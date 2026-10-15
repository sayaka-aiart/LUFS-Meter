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
inline TargetLevel true_peak_level(double value, bool available) {
  if (!available || !std::isfinite(value)) return TargetLevel::Unavailable;
  if (value > 0) return TargetLevel::Over;
  return value >= -6 ? TargetLevel::Near : TargetLevel::Below;
}
}
