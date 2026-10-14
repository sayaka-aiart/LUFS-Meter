#pragma once
#include <cstdint>
namespace lufs {
inline std::uint8_t configured_tracks(bool advanced, bool multiple, unsigned track, unsigned mask) {
  if (!advanced) return 1;
  if (multiple) return static_cast<std::uint8_t>(mask & 63);
  return track >= 1 && track <= 6 ? static_cast<std::uint8_t>(1u << (track - 1)) : 0;
}
inline std::uint8_t effective_tracks(std::uint8_t configured, bool active, std::uint8_t observed) {
  return active ? observed : configured; // Live output with unknown routing must not claim a guessed master.
}
}
