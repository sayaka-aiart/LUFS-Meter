#include "meter.hpp"
#include "routing.hpp"
#include "target.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
void tone(lufs::Meter& meter, float gain = 1, bool mute = false, double frequency = 1000) {
  std::vector<float> a(480), b(480);
  const float* planes[] = {a.data(), b.data()};
  for (int block = 0; block < 600; ++block) {
    for (int i = 0; i < 480; ++i) a[i] = b[i] = static_cast<float>(0.1 * std::sin(2 * 3.141592653589793 * frequency * (block * 480 + i) / 48000));
    const auto original = a;
    check(meter.capture(planes, 480, gain, mute), "capture");
    check(a == original, "input PCM mutated");
    meter.process();
  }
}
int main() {
  try {
    check(lufs::target_level(-17.01, -14, true) == lufs::TargetLevel::Below, "below yellow boundary");
    check(lufs::target_level(-17, -14, true) == lufs::TargetLevel::Near, "yellow starts at target minus 3");
    check(lufs::target_level(-14, -14, true) == lufs::TargetLevel::Near, "exact target is yellow");
    check(lufs::target_level(-13.99, -14, true) == lufs::TargetLevel::Over, "red only above target");
    check(lufs::target_level(-10, -14, false) == lufs::TargetLevel::Unavailable, "muted/stale values do not warn");
    check(lufs::target_level(-27, -30, true) == lufs::TargetLevel::Over, "individual BGM target");
    check(lufs::configured_tracks(false, false, 4, 0) == 1, "simple streaming is track1");
    check(lufs::configured_tracks(true, false, 2, 0) == 2, "advanced streaming track2");
    check(lufs::configured_tracks(true, false, 0, 0) == 0, "invalid track not silently aliased");
    check(lufs::configured_tracks(true, true, 1, 10) == 10, "SRT multi-track mask");
    check(lufs::effective_tracks(1, true, 4) == 4, "active runtime encoder wins");
    check(lufs::effective_tracks(4, false, 1) == 4, "inactive stale encoder ignored");
    check(lufs::effective_tracks(1, true, 0) == 0, "unobservable live route unavailable");
    auto full = std::make_unique<lufs::Meter>(48000, 2), half = std::make_unique<lufs::Meter>(48000, 2);
    tone(*full); tone(*half, 0.5f);
    auto a = full->reading(), b = half->reading();
    check(std::abs(a.shortTerm + 20.0) < 0.15, "stereo reference tone LUFS");
    check(std::abs((b.shortTerm - a.shortTerm) + 6.0206) < 0.02, "fader gain LUFS delta");
    check(std::abs(a.integrated - a.shortTerm) < 0.15, "integrated agreement");
    tone(*half, 1, true); b = half->reading();
    check(b.muted && (!std::isfinite(b.shortTerm) || b.shortTerm < -70), "mute is silent");
    full->reset(); full->process(); a = full->reading();
    check(a.frames == 0 && !std::isfinite(a.integrated), "reset clears integrated history");
    auto mono = std::make_unique<lufs::Meter>(48000, 1); tone(*mono);
    check(std::abs(mono->reading().shortTerm + 23.01) < 0.15, "mono reference mapping");
    std::vector<float> large(4096, .1f); const float* planes[] = {large.data(), large.data()};
    check(full->capture(planes, 4096), "large source blocks split"); full->process();
    for (int i = 0; i < 16; ++i) full->capture(planes, 4096);
    check(!full->capture(planes, 4096), "bounded queue overflow"); full->process();
    check(full->reading().dropped == 1, "queue loss reported");
    large[0] = std::numeric_limits<float>::quiet_NaN();
    full->capture(planes, 4096); full->process(); check(!full->reading().error, "non-finite sanitized");
    std::cout << "PASS: reference tone, gain, mono, mute, reset, chunking, overflow, PCM preservation, NaN\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
