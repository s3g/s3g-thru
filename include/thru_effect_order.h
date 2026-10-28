#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
namespace s3g_daisy {
using ThruEffectOrder = std::array<int, 6>;
// One stepped permutation id stores the whole route atomically in clip data.
inline ThruEffectOrder decodeEffectOrder(int code) {
    constexpr int factorial[] = {120,24,6,2,1,1};
    ThruEffectOrder available {0,1,2,3,4,5}, result {};
    code = std::clamp(code, 0, 719);
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const int index = code / factorial[slot]; code %= factorial[slot];
        result[slot] = available[static_cast<std::size_t>(index)];
        for (std::size_t i = static_cast<std::size_t>(index); i + slot + 1 < 6; ++i)
            available[i] = available[i + 1];
    }
    return result;
}
} // namespace s3g_daisy
