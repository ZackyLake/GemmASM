#include "rely.h"
#include "gtest-enhanced.h"
#include <random>
#include <string_view>
#include <charconv>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>

#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <Windows.h>
#include <ProcessThreadsApi.h>


ContextPack::ContextPack(uint32_t n, uint32_t k, uint32_t blockN, bool fillRef,
    std::function<void(std::vector<uint8_t>&, std::vector<uint8_t>&)> fillIn) noexcept
{
    assert(n > 0 && k > 0);
    assert(n % BlockOverN == 0);
    assert(k % BlockOverK == 0);
    assert(blockN % BlockOverN == 0);
    assert(n % blockN == 0);
    DataIn.resize(1 * n, 0);
    DataWgt.resize(n * k, 5);
    DataWgtReorder.resize(n * k / 2, 0);
    DataDest.resize(1 * k, 0.f);
    DataScaleIn.resize(1 * n / BlockOverN, 0.1f);
    DataScaleWgt.resize(k, 0.25f);
    DataZpWgt.resize(1, 4);
    DataSumIn.reserve(1 * n / BlockOverN);

    if (fillIn)
        fillIn(DataIn, DataWgtReorder);
    for (uint32_t i = 0; i < n; i += BlockOverN)
    {
        uint32_t sum = 0;
        for (uint32_t j = 0; j < BlockOverN; ++j)
        {
            sum += DataIn[i + j];
        }
        DataSumIn.push_back(sum);
    }
    {
        auto ptrWgtR = DataWgtReorder.data();
        for (uint32_t col = 0; col < k; col += BlockOverK)
        {
            for (uint32_t row = 0; row < n; row += 8)
            {
                const auto rowOffset = row * k;
                for (uint32_t i = 0; i < BlockOverK; ++i)
                {
                    const auto col_ = col + i;
                    const auto ptrWgt = DataWgt.data() + rowOffset + col_;
                    const uint8_t lo[4] = { ptrWgt[k * 0], ptrWgt[k * 1], ptrWgt[k * 2], ptrWgt[k * 3] };
                    const uint8_t hi[4] = { ptrWgt[k * 4], ptrWgt[k * 5], ptrWgt[k * 6], ptrWgt[k * 7] };
                    for (uint32_t j = 0; j < 4; ++j)
                    {
                        *ptrWgtR++ = (lo[j] & 0xfu) + ((hi[j] & 0xfu) << 4);
                    }
                }
            }
        }
        assert(ptrWgtR == DataWgtReorder.data() + DataWgtReorder.size());
    }

    SrcIn = DataIn.data();
    SrcWgt = DataWgtReorder.data();
    Dest = DataDest.data();
    SumBlockIn = DataSumIn.data();
    ZpBlockWgt = DataZpWgt.data();
    ScaleIn = DataScaleIn.data();
    ScaleWgt = DataScaleWgt.data();
    TotalN = n, TotalK = k;
    BlockN = blockN;

    // reference gemm
    if (fillRef)
    {
        DataRef.resize(1 * k, 0.f);
        for (uint32_t outK = 0; outK < k; ++outK)
        {
            float sum = 0.f;
            for (uint32_t curN = 0, blkN = 0; curN < n; blkN++)
            {
                int32_t blkSum = 0;
                for (uint32_t blk = 0; blk < BlockOverN; ++blk, ++curN)
                {
                    const auto loadIn = static_cast<int32_t>(DataIn[curN]);
                    const auto loadWgt = static_cast<int32_t>(DataWgt[curN * k + outK]);
                    const auto zpWgt = loadWgt - DataZpWgt[0];
                    blkSum += loadIn * zpWgt;
                }
                sum += blkSum * DataScaleIn[blkN] * DataScaleWgt[0];
            }
            DataRef[outK] = sum;
        }
    }

}

ContextPack::ContextPack(const ContextPack& other) noexcept : 
    DataIn(other.DataIn), DataWgt(other.DataWgt), DataWgtReorder(other.DataWgtReorder), DataDest(other.DataDest),
    DataSumIn(other.DataSumIn), DataZpWgt(other.DataZpWgt), DataScaleIn(other.DataScaleIn), DataScaleWgt(other.DataScaleWgt),
    DataRef(other.DataRef)
{
    SrcIn = DataIn.data();
    SrcWgt = DataWgtReorder.data();
    Dest = DataDest.data();
    SumBlockIn = DataSumIn.data();
    ZpBlockWgt = DataZpWgt.data();
    ScaleIn = DataScaleIn.data();
    ScaleWgt = DataScaleWgt.data();
    TotalN = other.TotalN, TotalK = other.TotalK;
    BlockN = other.BlockN;
}



void GemmEntry(const Context* ctx, uint8_t method) noexcept;


struct TestInfos
{
    uint32_t N = 1536u;
    uint32_t K = 1536u;
    uint32_t Block = 512u;
    uint8_t Method = 255;
    bool InCache = false;
    bool PerfOnly = false;
};
static TestInfos TestInfo;


TEST(Gemm, Def)
{
    std::mt19937 gen(42);
#ifndef _DEBUG
    {
        std::vector<uint64_t> times;
        times.reserve(10000);
        ContextPack dummy(TestInfo.N, TestInfo.K, TestInfo.Block, false);
        if (!TestInfo.InCache)
        {
            std::vector<uint8_t> wgt(1024 * 1024 * 1024);
            const auto wgtSize = dummy.DataWgtReorder.size();
            const auto loopCnt = wgt.size() / wgtSize;
            // warm up
            for (uint32_t i = 0; i < 1000; ++i)
                GemmEntry(&dummy, TestInfo.Method);
            const auto t0 = std::chrono::high_resolution_clock::now();
            for (uint32_t i = 0; ; ++i)
            {
                const auto cntOffset = i % loopCnt;
                dummy.SrcWgt = wgt.data() + cntOffset * wgtSize;
                const auto tbegin = std::chrono::high_resolution_clock::now();
                GemmEntry(&dummy, TestInfo.Method);
                const auto tend = std::chrono::high_resolution_clock::now();
                const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tend - tbegin).count();
                times.push_back(ns);
                if (std::chrono::duration_cast<std::chrono::milliseconds>(tend - t0).count() >= 1500)
                    break;
            }
        }
        else
        {
            // warm up
            for (uint32_t i = 0; i < 1000; ++i)
                GemmEntry(&dummy, TestInfo.Method);
            const auto t0 = std::chrono::high_resolution_clock::now();
            for (uint32_t i = 0; ; ++i)
            {
                const auto tbegin = std::chrono::high_resolution_clock::now();
                GemmEntry(&dummy, TestInfo.Method);
                const auto tend = std::chrono::high_resolution_clock::now();
                const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tend - tbegin).count();
                times.push_back(ns);
                if (std::chrono::duration_cast<std::chrono::milliseconds>(tend - t0).count() >= 1600)
                    break;
            }
        }
        std::sort(times.begin(), times.end());
        const auto cnt = static_cast<uint32_t>(times.size() * 0.8);
        times.resize(cnt);
        const auto totalNs = std::accumulate(times.cbegin(), times.cend(), uint64_t(0));
        printf("perf[%s]: [%zu]ns/iter\n", TestInfo.InCache ? "cache" : "ram", static_cast<size_t>(totalNs / cnt));
    }
#endif
    if (!TestInfo.PerfOnly)
    {
        ContextPack ctx(TestInfo.N, TestInfo.K, TestInfo.Block, true, [&](auto& in, auto& wgt)
            {
                for (auto& val : in)
                {
                    val = static_cast<uint8_t>(gen() % 16);
                }
            });
        GemmEntry(&ctx, TestInfo.Method);
        for (uint32_t i = 0; i < ctx.DataRef.size(); ++i)
        {
            const auto dst = ctx.DataDest[i], ref = ctx.DataRef[i];
            const auto diff = dst - ref, base = ref == 0.f ? 1.0f : ref;
            if (std::abs(diff / base) > 1e-6f)
            {
                EXPECT_EQ(dst, ref) << "at index[" << i << "]";
                break;
            }
        }
    }
}


struct Reg
{
    uint32_t EAX = 0, EBC = 0, ECX = 0, EDX = 0;
};
static Reg cpuid(int32_t eax) noexcept
{
    Reg ret{};
    __cpuid(reinterpret_cast<int*>(&ret), eax);
    return ret;
}
static Reg cpuidex(int32_t eax, int32_t ecx) noexcept
{
    Reg ret{};
    __cpuidex(reinterpret_cast<int*>(&ret), eax, ecx);
    return ret;
}


int main(int argc, char** argv)
{
    std::thread([]() 
    {
        const auto handle = GetCurrentThread();
        SYSTEM_INFO sysInfo{};
        GetSystemInfo(&sysInfo);
        GROUP_AFFINITY mask{};
        using T = decltype(mask.Mask);
        mask.Group = 0;
        std::string cpuTypes;
        for (uint32_t i = 0; i < sysInfo.dwNumberOfProcessors; ++i)
        {
            mask.Mask = T(1) << i;
            char type = '?';
            if (const auto ret = SetThreadGroupAffinity(handle, &mask, nullptr); ret != 0)
            {
                if (const uint32_t max_base_index = cpuid(0).EAX; max_base_index >= 0x1au)
                {
                    const auto reg = cpuidex(0x1au, 0);
                    if (reg.EAX != 0) // it's hybrid
                    {
                        // LP-E has no L3, see https://community.intel.com/t5/Processors/Detecting-LP-E-Cores-on-Meteor-Lake-in-software/m-p/1584555/highlight/true#M70732
                        bool hasL3 = false;
                        for (uint32_t i = 0; i < UINT32_MAX; ++i)
                        {
                            const auto cacheReg = cpuidex(0x4u, i);
                            const auto cacheType = cacheReg.EAX & 0x1fu;
                            const auto cacheLevel = (cacheReg.EAX & 0xe0u) >> 5;
                            if (cacheType == 0)
                                break;
                            if (cacheType == 3 /*unifed cache*/ && cacheLevel == 3)
                            {
                                hasL3 = true;
                                break;
                            }
                        }
                        switch (reg.EAX >> 24)
                        {
                        case 0x20u: type = hasL3 ? 'E' : 'L'; break;
                        case 0x40u: type = 'P'; break;
                        default: break;
                        }
                    }
                    else
                        type = '.';
                }
            }
            cpuTypes.push_back(type);
        }
        printf("CPU[%u] Types:[%s]\n", sysInfo.dwNumberOfProcessors, cpuTypes.c_str());
    }).join();

    testing::InitGoogleTest(&argc, argv);
    std::optional<uint32_t> pinCore;
    for (int i = 0; i < argc; ++i)
    {
        constexpr std::errc NoErr{};
        std::string_view arg(argv[i]);
        if (arg.starts_with("-dims="))
        {
            arg.remove_prefix(6);
            const char* str = arg.data(), *strend = arg.data() + arg.size();
            if (const auto res = std::from_chars(str, strend, TestInfo.N, 10); res.ec == NoErr && res.ptr + 1 < strend)
            {
                str = res.ptr + 1;
                if (const auto res = std::from_chars(str, strend, TestInfo.K, 10); res.ec == NoErr && res.ptr + 1 < strend)
                {
                    str = res.ptr + 1;
                    std::from_chars(str, strend, TestInfo.Block, 10);
                }
            }
        }
        else if (arg.starts_with("-method="))
        {
            arg.remove_prefix(8);
            std::from_chars(arg.data(), arg.data() + arg.size(), TestInfo.Method, 10);
        }
        else if (arg.starts_with("-core="))
        {
            arg.remove_prefix(6);
            uint8_t coreid = 0;
            if (std::from_chars(arg.data(), arg.data() + arg.size(), coreid, 10).ec == NoErr && coreid < 64)
                pinCore.emplace(coreid);
        }
        else if (arg == "-incache")
        {
            TestInfo.InCache = true;
        }
        else if (arg == "-perfonly")
        {
            TestInfo.PerfOnly = true;
        }
    }

    if (TestInfo.Method == 255)
    {
        printf(
            "alg:\n"
            "  [0]: OV\n"
            "  [1]: W4A8\n"
            "  [2]: Unroll32-VNNI\n"
            "  [3]: Unroll32-AVX2\n"
            "  [4]: Unroll64-VNNI\n"
            "  [5]: Unroll64-AVX2\n"
            "  [6]: Unroll64-VNNI-Block8\n"
            "  [7]: Unroll64-AVX2-Block8\n"
        );
        exit(-1);
    }

    printf("Test dims: [1]x[%u]x[%u] @ [%u]\n", TestInfo.N, TestInfo.K, TestInfo.Block);

    // thread pinning
    {
        const auto handle = GetCurrentThread();
        THREAD_POWER_THROTTLING_STATE powerThrottling;
        ZeroMemory(&powerThrottling, sizeof(powerThrottling));
        powerThrottling.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
        powerThrottling.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
        powerThrottling.StateMask = 0;
        if (const auto ret = SetThreadInformation(handle, ThreadPowerThrottling, &powerThrottling, sizeof(powerThrottling)); ret == 0)
        {
            printf("!!Failed to set qos\n");
        }
        if (pinCore)
        {
            GROUP_AFFINITY mask{};
            using T = decltype(mask.Mask);
            mask.Group = 0;
            mask.Mask = T(1) << *pinCore;
            if (const auto ret = SetThreadGroupAffinity(handle, &mask, nullptr); ret == 0)
            {
                printf("!!Failed to set thread affinity\n");
            }
        }
    }

    return RUN_ALL_TESTS();
}
