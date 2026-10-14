#pragma once
#include <cmath>
namespace lufs {
enum class TargetLevel { Unavailable, Below, Near, Over };
inline TargetLevel target_level(double value, double target, bool available) {
  if (!available || !std::isfinite(value) || !std::isfinite(target)) return TargetLevel::Unavailable;
  if (value > target) return TargetLevel::Over;
  if (value >= target - 3.0) return TargetLevel::Near;
  return TargetLevel::Below;
}
}
