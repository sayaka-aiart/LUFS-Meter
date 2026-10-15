#pragma once
#include <cmath>
#include <algorithm>
namespace lufs {
enum class TargetLevel { Unavailable, Below, Near, Over };
inline TargetLevel target_level(double value, double target, bool available) {
  if (!available || !std::isfinite(value) || !std::isfinite(target)) return TargetLevel::Unavailable;
  if (value > target) return TargetLevel::Over;
  if (value >= target - 3.0) return TargetLevel::Near;
  return TargetLevel::Below;
}
inline double peak_warning(double value) { return std::isfinite(value) ? std::clamp(value, -60.0, 0.0) : -6.0; }
inline TargetLevel true_peak_level(double value, bool available, double warning = -6) {
  if (!available || !std::isfinite(value)) return TargetLevel::Unavailable;
  if (value > 0) return TargetLevel::Over;
  return value >= peak_warning(warning) ? TargetLevel::Near : TargetLevel::Below;
}
}
