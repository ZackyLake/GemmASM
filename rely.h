#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <functional>


struct Context
{
    const uint8_t* __restrict SrcIn = nullptr; // U8, [1, n]
    const uint8_t* __restrict SrcWgt = nullptr; // U4, reordered [n, k] => [k/32, n/8, 128]
    float* __restrict Dest = nullptr; // [1, k]
    const uint32_t* __restrict SumBlockIn = nullptr; // [1, n/32]
    const uint8_t* __restrict ZpBlockWgt = nullptr; // [1, 1]
    const float* __restrict ScaleIn = nullptr; // [1, n/32]
    const float* __restrict ScaleWgt = nullptr; // [1, k]
    uint32_t TotalN = 0, TotalK = 0;
    uint32_t BlockN = 0;
};


inline constexpr uint32_t BlockOverN = 32;
inline constexpr uint32_t BlockOverK = 32;


struct ContextPack : public Context
{
    std::vector<uint8_t> DataIn;
    std::vector<uint8_t> DataWgt;
    std::vector<uint8_t> DataWgtReorder;
    std::vector<float> DataDest;
    std::vector<uint32_t> DataSumIn;
    std::vector<uint8_t> DataZpWgt;
    std::vector<float> DataScaleIn;
    std::vector<float> DataScaleWgt;
    std::vector<float> DataRef;
    ContextPack(uint32_t n, uint32_t k, uint32_t blockN, bool fillRef = true,
        std::function<void(std::vector<uint8_t>&, std::vector<uint8_t>&)> fillIn = {}) noexcept;
    ContextPack(const ContextPack& other) noexcept;
};
