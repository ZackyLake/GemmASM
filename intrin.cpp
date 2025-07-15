#include "rely.h"
#include <intrin.h>
#include <cassert>
#include <type_traits>


// Gemm 1xnx32
template<uint32_t CountK = 32, uint32_t BlockN = 32, uint32_t UnrollN = 4>
static void GemmW4A8(const uint8_t* __restrict ptrWeight, const uint32_t* __restrict ptrInput, float* __restrict ptrDst,
    const uint32_t* __restrict ptrSumIn, const uint8_t* __restrict ptrZpWgt, 
    const float* __restrict ptrScaleIn, const float* __restrict ptrScaleWgt,
    uint32_t totalN) noexcept
{
    constexpr uint32_t AccumPerYmm = 256 / 32;
    static_assert(CountK % AccumPerYmm == 0);
    constexpr uint32_t YK = CountK / AccumPerYmm;
    constexpr uint32_t Weight4PerYmm = 256 / (8 * 4);
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


template<uint32_t YK, uint32_t YWgtPerYK, uint32_t idx>
static __forceinline void dp4a_lohi_(__m256i (&accum)[YK],
    const __m256i (&loWgt)[YK * YWgtPerYK], const __m256i (&hiWgt)[YK * YWgtPerYK],
    const __m256i& loIn4Bcast, const __m256i& hiIn4Bcast) noexcept
{
    constexpr const auto i = idx / YWgtPerYK, j = idx % YWgtPerYK;
    accum[i] = _mm256_dpbusd_avx_epi32(accum[i], loWgt[i * YWgtPerYK + j], loIn4Bcast);
    accum[i] = _mm256_dpbusd_avx_epi32(accum[i], hiWgt[i * YWgtPerYK + j], hiIn4Bcast);
}

template<uint32_t YK, uint32_t YWgtPerYK, uint32_t... Idxes>
static __forceinline void dp4a_lohi(__m256i (&accum)[YK],
    const __m256i (&loWgt)[YK * YWgtPerYK], const __m256i (&hiWgt)[YK * YWgtPerYK],
    const __m256i& loIn4Bcast, const __m256i& hiIn4Bcast,
    std::integer_sequence<uint32_t, Idxes...>) noexcept
{
    (..., dp4a_lohi_<YK, YWgtPerYK, Idxes>(accum, loWgt, hiWgt, loIn4Bcast, hiIn4Bcast));
}

template<uint32_t YK, uint32_t YWgtPerYK>
static __forceinline void gemm_loopN_(const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    __m256i (&accum)[YK], uint32_t) noexcept
{
    constexpr auto YWgt = YWgtPerYK * YK;
    const auto maskLo = _mm256_set1_epi8(0xf);
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

    dp4a_lohi<YK, YWgtPerYK>(accum, loWgt, hiWgt, loIn4Bcast, hiIn4Bcast, std::make_integer_sequence<uint32_t, YWgt>{});
}

template<uint32_t YK, uint32_t YWgtPerYK, uint32_t... loopN>
static __forceinline void gemm_loopN(const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    __m256i (&accum)[YK],
    std::integer_sequence<uint32_t, loopN...>) noexcept
{
    (..., gemm_loopN_<YK, YWgtPerYK>(ptrWeight, ptrInput, accum, loopN));
}

template<uint32_t YK, uint32_t YWgtPerYK, uint32_t LoopN>
static __forceinline void gemm_unrollN_(__m256 (&output)[YK],
    const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    const uint32_t* __restrict& ptrSumIn, const uint8_t* __restrict& ptrZpWgt,
    const float* __restrict& ptrScaleIn, const float* __restrict& ptrScaleWgt,
    uint32_t) noexcept
{
    constexpr uint32_t AccumPerYmm = 256 / 32;
    
    __m256i accum[YK];
    for (uint32_t i = 0; i < YK; ++i)
        accum[i] = _mm256_setzero_si256();

    _mm_prefetch(reinterpret_cast<const char*>(ptrWeight + (YK * YWgtPerYK * LoopN) * 256 / 8), _MM_HINT_T0);

    // 1xBN * BNxCK
    gemm_loopN<YK, YWgtPerYK>(ptrWeight, ptrInput, accum, std::make_integer_sequence<uint32_t, LoopN>{});

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

template<uint32_t YK, uint32_t YWgtPerYK, uint32_t LoopN, uint32_t... unrollN>
static __forceinline void gemm_unrollN(__m256 (&output)[YK],
    const uint8_t* __restrict& ptrWeight, const uint32_t* __restrict& ptrInput,
    const uint32_t* __restrict& ptrSumIn, const uint8_t* __restrict& ptrZpWgt,
    const float* __restrict& ptrScaleIn, const float* __restrict& ptrScaleWgt,
    std::integer_sequence<uint32_t, unrollN...>) noexcept
{
    (..., gemm_unrollN_<YK, YWgtPerYK, LoopN>(output, ptrWeight, ptrInput, 
        ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, unrollN));
}

template<uint32_t CountK = 32, uint32_t BlockN = 32, uint32_t UnrollN = 4>
static void GemmW4A8Unroll(const uint8_t* __restrict ptrWeight, const uint32_t* __restrict ptrInput, float* __restrict ptrDst,
    const uint32_t* __restrict ptrSumIn, const uint8_t* __restrict ptrZpWgt,
    const float* __restrict ptrScaleIn, const float* __restrict ptrScaleWgt,
    uint32_t totalN) noexcept
{
    constexpr uint32_t AccumPerYmm = 256 / 32;
    static_assert(CountK % AccumPerYmm == 0);
    constexpr uint32_t YK = CountK / AccumPerYmm;
    constexpr uint32_t Weight4PerYmm = 256 / (8 * 4);
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
        gemm_unrollN<YK, YWgtPerYK, LoopN>(output, ptrWeight, ptrInput,
            ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt,
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

void GemmEntry(const Context* ctx, uint8_t method) noexcept
{
    constexpr uint32_t BlockK = BlockOverK;
    constexpr uint32_t BlockNIn = BlockOverN;
    assert(ctx->TotalN % BlockNIn == 0);
    assert(ctx->TotalK % BlockK == 0);
    const auto totalK = ctx->TotalK / BlockK, totalN = ctx->TotalN / ctx->BlockN;
    const auto wgtColStride = ctx->TotalN * BlockK / 2;
    const auto wgtRowStride = ctx->BlockN * BlockK / 2;
    for (uint32_t k = 0; k < totalK; k++)
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
            case 1:
                GemmW4A8(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, ctx->BlockN);
                break;
            case 2:
                GemmW4A8Unroll(ptrWgt, ptrIn, ptrDst, ptrSumIn, ptrZpWgt, ptrScaleIn, ptrScaleWgt, ctx->BlockN);
                break;
            default: break;
            }
        }
    }
}