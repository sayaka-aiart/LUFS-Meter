#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
#include <utility>
#include "ebur128.h"

namespace lufs {
constexpr std::size_t MaxChannels = 8, BlockFrames = 1024, QueueBlocks = 64;
inline double unavailable() { return std::numeric_limits<double>::quiet_NaN(); }
std::uint64_t now_ms();
struct Reading {
  double momentary = unavailable(), shortTerm = unavailable(), integrated = unavailable();
  double truePeak = unavailable();
  std::uint64_t frames = 0, lastMs = 0, dropped = 0, resets = 0;
  bool muted = false, error = false;
};
// One producer (one OBS source/output callback), one consumer (meter worker).
// The callback copies into preallocated storage; only the worker calls libebur128.
class Meter {
public:
  Meter(unsigned rate, unsigned channels);
  ~Meter();
  Meter(const Meter&) = delete;
  bool capture(const float* const* planes, std::size_t frames, float gain = 1.0f, bool muted = false) noexcept;
  std::size_t process();
  void reset() { resetRequested_.store(true, std::memory_order_release); }
  Reading reading() const;
private:
  struct Block { std::uint32_t frames = 0; bool muted = false; std::uint64_t ms = 0; std::array<float, BlockFrames * MaxChannels> pcm{}; };
  void restart();
  unsigned rate_, channels_;
  ebur128_state* state_ = nullptr;
  std::array<Block, QueueBlocks> queue_{};
  alignas(64) std::atomic<std::uint64_t> write_{0};
  alignas(64) std::atomic<std::uint64_t> read_{0};
  std::atomic<std::uint64_t> dropped_{0};
  std::atomic<bool> resetRequested_{false};
  std::uint64_t seenDrops_ = 0, previousMs_ = 0;
  mutable std::mutex snapshotMutex_;
  Reading snapshot_, current_;
  // Monotonic queue of block maxima over the most recent 400 ms of PCM.
  std::deque<std::pair<std::uint64_t, double>> peaks_;
};
}
