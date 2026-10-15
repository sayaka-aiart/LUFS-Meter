#include "meter.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace lufs {
std::uint64_t now_ms() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count());
}
Meter::Meter(unsigned rate, unsigned channels) : rate_(rate), channels_(channels) { restart(); }
Meter::~Meter() { if (state_) ebur128_destroy(&state_); }
void Meter::restart() {
  if (state_) ebur128_destroy(&state_);
  const auto resets = current_.resets + 1;
  current_ = Reading{};
  peaks_.clear();
  current_.resets = resets;
  if (rate_ < 8000 || rate_ > 384000 || channels_ < 1 || channels_ > MaxChannels) {
    current_.error = true; return;
  }
  state_ = ebur128_init(channels_, rate_, EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_HISTOGRAM | EBUR128_MODE_TRUE_PEAK);
  current_.error = !state_;
  // OBS uses FL, FR, FC, LFE, rear L/R, side L/R. Exclude LFE explicitly.
  if (state_ && channels_ > 2) {
    const int map[] = {EBUR128_LEFT, EBUR128_RIGHT, EBUR128_CENTER, EBUR128_UNUSED,
      EBUR128_LEFT_SURROUND, EBUR128_RIGHT_SURROUND, EBUR128_LEFT_SURROUND, EBUR128_RIGHT_SURROUND};
    for (unsigned i = 0; i < channels_; ++i) ebur128_set_channel(state_, i, map[i]);
  }
}
bool Meter::capture(const float* const* planes, std::size_t frames, float gain, bool muted) noexcept {
  if (!planes || !frames || channels_ < 1 || channels_ > MaxChannels || !std::isfinite(gain) || gain < 0) return false;
  for (unsigned c = 0; c < channels_; ++c) if (!planes[c]) return false;
  const auto needed = (frames + BlockFrames - 1) / BlockFrames;
  const auto w = write_.load(std::memory_order_relaxed), r = read_.load(std::memory_order_acquire);
  if (needed > QueueBlocks || w - r + needed > QueueBlocks) { dropped_.fetch_add(1); return false; }
  const auto ms = now_ms();
  for (std::size_t b = 0, offset = 0; b < needed; ++b) {
    auto& block = queue_[(w + b) % QueueBlocks];
    block.frames = static_cast<std::uint32_t>(std::min(BlockFrames, frames - offset));
    block.muted = muted; block.ms = ms;
    for (unsigned f = 0; f < block.frames; ++f) for (unsigned c = 0; c < channels_; ++c) {
      const float value = muted ? 0.f : planes[c][offset + f] * gain;
      block.pcm[f * channels_ + c] = std::isfinite(value) ? value : 0.f;
    }
    offset += block.frames;
  }
  write_.store(w + needed, std::memory_order_release);
  return true;
}
std::size_t Meter::process() {
  if (resetRequested_.exchange(false, std::memory_order_acq_rel)) {
    read_.store(write_.load(std::memory_order_acquire), std::memory_order_release);
    previousMs_ = 0; restart();
  }
  const auto drops = dropped_.load(std::memory_order_acquire);
  if (drops != seenDrops_) { restart(); seenDrops_ = drops; previousMs_ = 0; }
  auto r = read_.load(std::memory_order_relaxed);
  const auto w = write_.load(std::memory_order_acquire);
  std::size_t processed = 0;
  while (r != w) {
    const auto& block = queue_[r % QueueBlocks];
    if (previousMs_ && block.ms > previousMs_ + 1500) restart();
    previousMs_ = block.ms;
    current_.muted = block.muted;
    current_.lastMs = block.ms;
    if (state_ && ebur128_add_frames_float(state_, block.pcm.data(), block.frames) == EBUR128_SUCCESS) {
      current_.frames += block.frames;
      double peak = 0;
      for (unsigned channel = 0; channel < channels_; ++channel) {
        double channelPeak = 0;
        if (ebur128_prev_true_peak(state_, channel, &channelPeak) != EBUR128_SUCCESS) current_.error = true;
        peak = std::max(peak, channelPeak);
      }
      while (!peaks_.empty() && peaks_.back().second <= peak) peaks_.pop_back();
      peaks_.emplace_back(current_.frames, peak);
      const auto window = static_cast<std::uint64_t>(rate_) * 4 / 10;
      while (!peaks_.empty() && current_.frames - peaks_.front().first >= window) peaks_.pop_front();
      current_.truePeak = peaks_.front().second > 0 ? 20 * std::log10(peaks_.front().second) : -std::numeric_limits<double>::infinity();
    } else current_.error = true;
    ++r; ++processed;
    read_.store(r, std::memory_order_release);
  }
  current_.dropped = drops;
  if (state_ && !current_.error) {
    if (current_.frames >= rate_ * 0.4) ebur128_loudness_momentary(state_, &current_.momentary);
    if (current_.frames >= static_cast<std::uint64_t>(rate_) * 3) ebur128_loudness_shortterm(state_, &current_.shortTerm);
    if (current_.frames >= rate_ * 0.4) ebur128_loudness_global(state_, &current_.integrated);
  }
  { std::lock_guard<std::mutex> lock(snapshotMutex_); snapshot_ = current_; }
  return processed;
}
Reading Meter::reading() const { std::lock_guard<std::mutex> lock(snapshotMutex_); return snapshot_; }
}
