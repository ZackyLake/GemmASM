#include "rely.h"
#include <intrin.h>
#include <cassert>
#include <type_traits>



constexpr uint32_t Weight4PerYmm = 256 / (8 * 4);
constexpr auto YWgtPerBlk = BlockOverK / Weight4PerYmm;
constexpr uint32_t AccumPerYmm = 256 / 32;
constexpr auto YAccPerBlk = BlockOverK / AccumPerYmm;

// Gemm 1xnx32
template<uint32_t CountK = 32, uint32_t BlockN = 32, uint32_t UnrollN = 4>
static void GemmW4A8(const uint8_t* __restrict ptrWeight, const uint32_t* __restrict ptrInput, float* __restrict ptrDst,
    const uint32_t* __restrict ptrSumIn, const uint8_t* __restrict ptrZpWgt, 
    const float* __restrict ptrScaleIn, const float* __restrict ptrScaleWgt,
    uint32_t totalN) noexcept
{
    static_assert(CountK % AccumPerYmm == 0);
    constexpr uint32_t YK = CountK / AccumPerYmm;
    static_assert(CountK % Weight4PerYmm == 0);
    constexpr uint32_t YWgt = CountK / Weight4PerYmm;
    static_assert(YWgt % YK == 0);
    constexpr uint32_t YWgtPerYK = YWgt / YK;
    constexpr uint32_t NPerCompute = 2 * 4;
    static_assert(BlockN % NPerCompute == 0);
    constexpr uint32_t LoopN = BlockN / NPerCompute;
    constexpr uint32_t IterN = BlockN * UnrollN;

    assert(totalN % IterN == 0);

    __m256 output[YK];
    for (uint32_t i = 0; i < YK; ++i)
        output[i] = _mm256_loadu_ps(ptrDst + i * AccumPerYmm);

    const auto maskLo = _mm256_set1_epi8(0xf);

    while (totalN)
    {
        // 1xIN * INxCK
        for (uint32_t unrollN = 0; unrollN < UnrollN; ++unrollN)
        {
            __m256i accum[YK];
            for (uint32_t i = 0; i < YK; ++i)
                accum[i] = _mm256_setzero_si256();

            _mm_prefetch(reinterpret_cast<const char*>(ptrWeight + (YK * YWgtPerYK * LoopN) * 256 / 8), _MM_HINT_T0);
            
            // 1xBN * BNxCK
            for (uint32_t loopN = 0; loopN < LoopN; ++loopN)
            {
                // 1x8 * 8xCK
                __m256i loadWgt[YWgt];
                for (uint32_t i = 0; i < YWgt; ++i)
                {
                    loadWgt[i] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptrWeight));
                    ptrWeight += 256 / 8;
                }

                __m256i loWgt[YWgt];
                __m256i hiWgt[YWgt];
                for (uint32_t i = 0; i < YWgt; ++i)
                {
                    loWgt[i] = _mm256_and_si256(loadWgt[i], maskLo);
                    hiWgt[i] = _mm256_and_si256(_mm256_srli_epi32(loadWgt[i], 4), maskLo);
                }

                const auto loIn4Bcast = _mm256_set1_epi32(*ptrInput++);
                const auto hiIn4Bcast = _mm256_set1_epi32(*ptrInput++);

                for (uint32_t i = 0; i < YK; ++i)
                {
                    for (uint32_t j = 0; j < YWgtPerYK; ++j)
                    {
                        accum[i] = _mm256_dpbusd_avx_epi32(accum[i], loWgt[i * YWgtPerYK + j], loIn4Bcast);
                        accum[i] = _mm256_dpbusd_avx_epi32(accum[i], hiWgt[i * YWgtPerYK + j], hiIn4Bcast);
                    }
                }
            }

            const auto zpWgt = _mm256_set1_epi32(*ptrZpWgt); // rsp+1B8h
            const auto sumIn = _mm256_set1_epi32(*ptrSumIn++); // rsp+1F8h
            const auto accumInWzp = _mm256_mullo_epi32(sumIn, zpWgt);
            for (uint32_t i = 0; i < YK; ++i)
            {
                accum[i] = _mm256_sub_epi32(accum[i], accumInWzp);
            }

            const auto scaleIn = _mm256_broadcast_ss(ptrScaleIn++); // rsp+1E0h
            for (uint32_t i = 0; i < YK; ++i)
            {
                const auto scaleWgt = _mm256_loadu_ps(ptrScaleWgt + i * AccumPerYmm); // rsp+1B0h
                const auto accumF32 = _mm256_cvtepi32_ps(accum[i]);
                const auto accumScaledIn = _mm256_mul_ps(accumF32, scaleIn);
                output[i] = _mm256_fmadd_ps(accumScaledIn, scaleWgt, output[i]);
            }
        }

        totalN -= IterN;
    }
    for (uint32_t i = 0; i < YK; ++i)
    {
        _mm256_storeu_ps(ptrDst + i * AccumPerYmm, output[i]);
    }
}


template<uint8_t VNNI>
static __forceinline void dp4a_lohi_single(__m256i& accum,
    const __m256i& wgt, const __m256i& loIn4Bcast, const __m256i& hiIn4Bcast) noexcept
{
    const auto maskLo = _mm256_set1_epi8(0xf);
    const auto loWgt = _mm256_and_si256(wgt, maskLo);
    const auto hiWgt = _mm256_and_si256(_mm256_srli_epi32(wgt, 4), maskLo);
    if constexpr (VNNI == 1)
    {
        accum = _mm256_dpbusd_avx_epi32(accum, loWgt, loIn4Bcast);
        accum = _mm256_dpbusd_avx_epi32(accum, hiWgt, hiIn4Bcast);
    }
    else if constexpr (VNNI == 2)
    {
        accum = _mm256_dpbusd_epi32(accum, loWgt, loIn4Bcast);
        accum = _mm256_dpbusd_epi32(accum, hiWgt, hiIn4Bcast);
    }
    else
    {
        const auto allone = _mm256_set1_epi16(1);
        const auto lo = _mm256_maddubs_epi16(loWgt, loIn4Bcast);
        const auto hi = _mm256_maddubs_epi16(hiWgt, hiIn4Bcast);
        // W4, won't overflow
        const auto lohi = _mm256_madd_epi16(_mm256_add_epi16(lo, hi), allone);
        accum = _mm256_add_epi32(accum, lohi);
    }
}

template<uint8_t VNNI, uint32_t AccumOffset, uint32_t YWgtPerYK, uint32_t YK, uint32_t YG, uint32_t... Idxes>
static __forceinline void dp4a_lohi(__m256i (&accum)[YK],
    const __m256i (&wgt)[YG],const __m256i& loIn4Bcast, const __m256i& hiIn4Bcast,
    std::integer_sequence<uint32_t, Idxes...>) noexcept
{
    (..., dp4a_lohi_single<VNNI>(accum[AccumOffset + Idxes / YWgtPerYK], wgt[Idxes], loIn4Bcast, hiIn4Bcast));
}

template<uint8_t VNNI, uint32_t AccumOffset, uint32_t YWgtPerYK, uint32_t YK>
static __forceinline void gemmBlk_(const uint8_t* __restrict ptrWeight,
    const __m256i& loIn4Bcast, const __m256i& hiIn4Bcast, __m256i (&accum)[YK]) noexcept
{
    __m256i loadWgt[YWgtPerBlk];
    for (uint32_t i = 0; i < YWgtPerBlk; ++i)
        loadWgt[i] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptrWeight + i * (256 / 8)));

    dp4a_lohi<VNNI, AccumOffset, YWgtPerYK>(accum,
        loadWgt, loIn4Bcast, hiIn4Bcast, std::make_integer_sequence<uint32_t, YWgtPerBlk>{});
}

template<uint8_t VNNI, uint32_t YWgtPerYK, uint32_t YK, uint32_t... Idxes>
static __forceinline void gemmBlk(const uint8_t* __restrict ptrWeight, const size_t strideWgtBlk,
    const __m256i& loIn4Bcast, const __m256i& hiIn4Bcast, __m256i (&accum)[YK],
    std::integer_sequence<uint32_t, Idxes...>) noexcept
{
    (..., gemmBlk_<VNNI, Idxes * YAccPerBlk, YWgtPerYK>(ptrWeight + Idxes * strideWgtBlk, loIn4Bcast, hiIn4Bcast, accum));
}

template<uint8_t VNNI, bool PerBlock, uint32_t YK, uint32_t YWgtPerYK>
static __forceinline void gemm_loopN_(const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    __m256i (&accum)[YK], const size_t strideWgtBlk, uint32_t) noexcept
{
    constexpr auto YWgt = YWgtPerYK * YK;
    static_assert(YWgt % YWgtPerBlk == 0);
    constexpr auto WgtBlk = YWgt / YWgtPerBlk;

    // 1x8 * 8xCK
    const auto loIn4Bcast = _mm256_set1_epi32(*ptrInput++);
    const auto hiIn4Bcast = _mm256_set1_epi32(*ptrInput++);
    if constexpr (PerBlock)
    {
        gemmBlk<VNNI, YWgtPerYK>(ptrWeight, strideWgtBlk, loIn4Bcast, hiIn4Bcast, accum,
            std::make_integer_sequence<uint32_t, WgtBlk>{});
    }
    else
    {
        __m256i loadWgt[YWgt];
        for (uint32_t i = 0; i < WgtBlk; ++i)
        {
            for (uint32_t j = 0; j < YWgtPerBlk; ++j)
                loadWgt[i * YWgtPerBlk + j] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>
                    (ptrWeight + i * strideWgtBlk + j * (256 / 8)));
        }
        dp4a_lohi<VNNI, 0, YWgtPerYK>(accum, loadWgt, loIn4Bcast, hiIn4Bcast, std::make_integer_sequence<uint32_t, YWgt>{});
    }
    ptrWeight += YWgtPerBlk * (256 / 8);
}

template<uint8_t VNNI, bool PerBlock, uint32_t YK, uint32_t YWgtPerYK, uint32_t... loopN>
static __forceinline void gemm_loopN(const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    __m256i (&accum)[YK], const size_t strideWgtBlk,
    std::integer_sequence<uint32_t, loopN...>) noexcept
{
    (..., gemm_loopN_<VNNI, PerBlock, YK, YWgtPerYK>(ptrWeight, ptrInput, accum, strideWgtBlk, loopN));
}

template<uint8_t VNNI, bool PerBlock, uint32_t YK, uint32_t YWgtPerYK, uint32_t LoopN>
static __forceinline void gemm_unrollN_(__m256 (&output)[YK],
    const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    const uint32_t* __restrict& ptrSumIn, const uint8_t* __restrict& ptrZpWgt,
    const float* __restrict& ptrScaleIn, const float* __restrict& ptrScaleWgt,
    const size_t strideWgtBlk,
    uint32_t) noexcept
{
    __m256i accum[YK];
    for (uint32_t i = 0; i < YK; ++i)
        accum[i] = _mm256_setzero_si256();

    _mm_prefetch(reinterpret_cast<const char*>(ptrWeight + (YK * YWgtPerYK * LoopN) * 256 / 8), _MM_HINT_T0);

    // 1xBN * BNxCK
    gemm_loopN<VNNI, PerBlock, YK, YWgtPerYK>(ptrWeight, ptrInput, accum, strideWgtBlk, std::make_integer_sequence<uint32_t, LoopN>{});

    const auto zpWgt = _mm256_set1_epi32(*ptrZpWgt); // rsp+1B8h
    const auto sumIn = _mm256_set1_epi32(*ptrSumIn++); // rsp+1F8h
    const auto accumInWzp = _mm256_mullo_epi32(sumIn, zpWgt);
    for (uint32_t i = 0; i < YK; ++i)
    {
        accum[i] = _mm256_sub_epi32(accum[i], accumInWzp);
    }

    const auto scaleIn = _mm256_broadcast_ss(ptrScaleIn++); // rsp+1E0h
    for (uint32_t i = 0; i < YK; ++i)
    {
        const auto scaleWgt = _mm256_loadu_ps(ptrScaleWgt + i * AccumPerYmm); // rsp+1B0h
        const auto accumF32 = _mm256_cvtepi32_ps(accum[i]);
        const auto accumScaledIn = _mm256_mul_ps(accumF32, scaleIn);
        output[i] = _mm256_fmadd_ps(accumScaledIn, scaleWgt, output[i]);
    }
}

template<uint8_t VNNI, bool PerBlock, uint32_t YK, uint32_t YWgtPerYK, uint32_t LoopN, uint32_t... unrollN>
static __forceinline void gemm_unrollN(__m256 (&output)[YK],
    const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    const uint32_t* __restrict& ptrSumIn, const uint8_t* __restrict& ptrZpWgt,
    const float* __restrict& ptrScaleIn, const float* __restrict& ptrScaleWgt,
    const size_t strideWgtBlk, 
    std::integer_sequence<uint32_t, unrollN...>) noexcept
{
    (..., gemm_unrollN_<VNNI, PerBlock, YK, YWgtPerYK, LoopN>(output, ptrWeight, ptrInput, 
        ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, strideWgtBlk, unrollN));
}

template<uint8_t VNNI, bool PerBlock, uint32_t CountK = 32, uint32_t BlockN = 32, uint32_t UnrollN = 4>
static void GemmW4A8Unroll(const uint8_t* __restrict ptrWeight, const uint32_t* __restrict ptrInput, float* __restrict ptrDst,
    const uint32_t* __restrict ptrSumIn, const uint8_t* __restrict ptrZpWgt,
    const float* __restrict ptrScaleIn, const float* __restrict ptrScaleWgt,
    const size_t strideWgtBlk, uint32_t totalN) noexcept
{
    static_assert(CountK % AccumPerYmm == 0);
    constexpr uint32_t YK = CountK / AccumPerYmm;
    static_assert(CountK % Weight4PerYmm == 0);
    constexpr uint32_t YWgt = CountK / Weight4PerYmm;
    static_assert(YWgt % YK == 0);
    constexpr uint32_t YWgtPerYK = YWgt / YK;
    constexpr uint32_t NPerCompute = 2 * 4;
    static_assert(BlockN % NPerCompute == 0);
    constexpr uint32_t LoopN = BlockN / NPerCompute;
    constexpr uint32_t IterN = BlockN * UnrollN;

    assert(totalN % IterN == 0);

    __m256 output[YK];
    for (uint32_t i = 0; i < YK; ++i)
        output[i] = _mm256_loadu_ps(ptrDst + i * AccumPerYmm);

    const auto maskLo = _mm256_set1_epi8(0xf);

    while (totalN)
    {
        // 1xIN * INxCK
        gemm_unrollN<VNNI, PerBlock && (CountK != 32), YK, YWgtPerYK, LoopN>(output, ptrWeight, ptrInput,
            ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, 
            strideWgtBlk,
            std::make_integer_sequence<uint32_t, UnrollN>{});

        totalN -= IterN;
    }
    for (uint32_t i = 0; i < YK; ++i)
    {
        _mm256_storeu_ps(ptrDst + i * AccumPerYmm, output[i]);
    }
}


extern "C" void GemmW4A8OVAsm(const uint8_t* __restrict ptrWeight, const uint32_t* __restrict ptrInput, float* __restrict ptrDst,
    const uint32_t* __restrict ptrSumIn, const uint8_t* __restrict ptrZpWgt,
    const float* __restrict ptrScaleIn, const float* __restrict ptrScaleWgt,
    uint32_t totalN) noexcept;

extern "C" void GemmW4A8OVAsm512(const uint8_t* __restrict ptrWeight, const uint32_t* __restrict ptrInput, float* __restrict ptrDst,
    const uint32_t* __restrict ptrSumIn, const uint8_t* __restrict ptrZpWgt,
    const float* __restrict ptrScaleIn, const float* __restrict ptrScaleWgt,
    uint32_t totalN) noexcept;

std::pair<bool, bool> SupportVNNI() noexcept;


void GemmEntry(const Context* ctx, uint8_t method) noexcept
{
    constexpr uint32_t BlockK = BlockOverK;
    constexpr uint32_t BlockNIn = BlockOverN;
    assert(ctx->TotalN % BlockNIn == 0);
    assert(ctx->TotalK % BlockK == 0);
    const auto totalK = ctx->TotalK / BlockK, totalN = ctx->TotalN / ctx->BlockN;
    const auto wgtColStride = ctx->TotalN * BlockK / 2;
    const auto wgtRowStride = ctx->BlockN * BlockK / 2;
    const auto kPerLoop = (method >= 4 && method <= 7) ? 2 : 1;
    if (const auto [avx512, avx2] = SupportVNNI(); avx512 && !avx2)
    {
        if (method == 0 || method == 2 || method == 4 || method == 6)
            method += 128;
    }
    for (uint32_t k = 0; k < totalK; k += kPerLoop)
    {
        const auto kOffset = k * BlockK;
        const auto wgtColOffset = k * wgtColStride;
        for (uint32_t n = 0; n < totalN; n++)
        {
            const auto nOffset = n * ctx->BlockN;
            const auto wgtRowOffset = n * wgtRowStride;
            const auto ptrIn = reinterpret_cast<const uint32_t*>(ctx->SrcIn + nOffset);
            const auto ptrWgt = ctx->SrcWgt + wgtColOffset + wgtRowOffset;
            const auto ptrDst = ctx->Dest + kOffset;
            const auto ptrSumIn = ctx->SumBlockIn + (nOffset / BlockNIn);
            const auto ptrZpWgt = ctx->ZpBlockWgt;
            const auto ptrScaleIn = ctx->ScaleIn + (nOffset / BlockNIn);
            const auto ptrScaleWgt = ctx->ScaleWgt + kOffset;
            switch (method)
            {
            case 0:
                GemmW4A8OVAsm(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, ctx->BlockN);
                break;
            case 0 + 128:
                GemmW4A8OVAsm512(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, ctx->BlockN);
                break;
            case 1:
                GemmW4A8(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, ctx->BlockN);
                break;
            case 2:
                GemmW4A8Unroll<1, false>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 2 + 128:
                GemmW4A8Unroll<2, false>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 3:
                GemmW4A8Unroll<0, false>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 4:
                GemmW4A8Unroll<1, false, 64>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 4 + 128:
                GemmW4A8Unroll<2, false, 64>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 5:
                GemmW4A8Unroll<0, false, 64>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 6:
                GemmW4A8Unroll<1, true, 64>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 6 + 128:
                GemmW4A8Unroll<2, true, 64>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            case 7:
                GemmW4A8Unroll<0, true, 64>(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, wgtColStride, ctx->BlockN);
                break;
            default:
                exit(-1);
            }
        }
    }
}