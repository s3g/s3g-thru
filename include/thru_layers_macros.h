#pragma once
#include <algorithm>
#include <array>
namespace s3g_thru_layers {
// Positive Z amount increases this effect's characteristic transformation;
// negative amounts reverse the same coordinated movement. Type/pattern/seed
// choices remain fixed so pressure does not select a different algorithm.
inline void pressureMacro(int algorithm, float amount, std::array<float, 4> &p,
                          std::array<float, 4> &a) {
  std::array<float, 4> dp{}, da{};
  switch (algorithm) {
  case 1:
    dp = {-.3f, .5f, .5f, 0};
    break;
  case 2:
    dp = {.5f, .4f, -.2f, .3f};
    da = {.2f, .1f, 0, 0};
    break;
  case 3:
    dp = {0, .55f, -.25f, .4f};
    break;
  case 4:
    dp = {0, .4f, .4f, .5f};
    break;
  case 5:
    dp = {.6f, .5f, .25f, 0};
    da = {.2f, .15f, 0, 0};
    break;
  case 6:
    dp = {0, .6f, .25f, 0};
    da = {.2f, 0, .1f, .2f};
    break;
  case 7:
    dp = {0, .55f, .35f, .2f};
    da = {.2f, 0, .1f, .2f};
    break;
  case 8:
    dp = {-.45f, 0, .6f, .35f};
    break;
  case 9:
    dp = {0, .45f, -.3f, 0};
    da = {.2f, .3f, .2f, 0};
    break;
  case 10:
    dp = {0, .5f, .3f, .55f};
    break;
  case 11:
    dp = {0, .2f, -.2f, .7f};
    break;
  case 12:
    dp = {.5f, .35f, 0, .25f};
    break;
  case 13:
    dp = {-.45f, 0, -.45f, .25f};
    break;
  case 14:
    dp = {a[0] >= .5f ? -.4f : .4f, -.5f, 0, .25f};
    break;
  case 15:
    dp = {-.35f, 0, .6f, .5f};
    break;
  case 16:
    dp = {.2f, .6f, .5f, .25f};
    break;
  default:
    break;
  }
  for (int i = 0; i < 4; ++i) {
    p[i] = std::clamp(p[i] + amount * dp[i], 0.f, 1.f);
    a[i] = std::clamp(a[i] + amount * da[i], 0.f, 1.f);
  }
}
} // namespace s3g_thru_layers
