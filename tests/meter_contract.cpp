#include "meter.hpp"
#include "routing.hpp"
#include "target.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
void tone(lufs::Meter& meter, float gain = 1, bool mute = false, double frequency = 1000, double amplitude = 0.1, double phase = 0) {
  std::vector<float> a(480), b(480);
  const float* planes[] = {a.data(), b.data()};
  for (int block = 0; block < 600; ++block) {
    for (int i = 0; i < 480; ++i) a[i] = b[i] = static_cast<float>(amplitude * std::sin(2 * 3.141592653589793 * frequency * (block * 480 + i) / 48000 + phase));
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
    check(std::abs(a.truePeak + 20.0) < 0.1, "true peak reference tone in dBTP");
    check(std::abs((b.truePeak - a.truePeak) + 6.0206) < 0.02, "true peak follows fader gain");
    check(std::abs(a.integrated - a.shortTerm) < 0.15, "integrated agreement");
    tone(*half, 1, true); b = half->reading();
    check(b.muted && (!std::isfinite(b.shortTerm) || b.shortTerm < -70), "mute is silent");
    check(!std::isfinite(b.truePeak) || b.truePeak < -100, "muting clears rolling true peak");
    full->reset(); full->process(); a = full->reading();
    check(a.frames == 0 && !std::isfinite(a.integrated), "reset clears integrated history");
    check(std::isnan(a.truePeak), "reset clears true peak window");
    auto mono = std::make_unique<lufs::Meter>(48000, 1); tone(*mono);
    check(std::abs(mono->reading().shortTerm + 23.01) < 0.15, "mono reference mapping");
    std::vector<float> large(4096, .1f); const float* planes[] = {large.data(), large.data()};
    check(full->capture(planes, 4096), "large source blocks split"); full->process();
    for (int i = 0; i < 16; ++i) full->capture(planes, 4096);
    check(!full->capture(planes, 4096), "bounded queue overflow"); full->process();
    check(full->reading().dropped == 1, "queue loss reported");
    large[0] = std::numeric_limits<float>::quiet_NaN();
    full->capture(planes, 4096); full->process(); check(!full->reading().error, "non-finite sanitized");
    auto intersample = std::make_unique<lufs::Meter>(48000, 2);
    tone(*intersample, 1, false, 12000, 1.2, 3.141592653589793 / 4);
    const double samplePeak = 20 * std::log10(1.2 / std::sqrt(2.0));
    const auto tp = intersample->reading().truePeak;
    check(samplePeak < 0 && tp > 1.4 && tp < 1.8, "intersample peak above 0 despite samples below 0 dBFS");
    const double initialMax = intersample->reading().truePeakMax;
    check(initialMax >= tp && initialMax < 3, "MAX captures intersample peak");
    tone(*intersample, 1, false, 1000, 0.1);
    check(std::abs(intersample->reading().truePeak + 20) < .1, "rolling peak releases earlier loud signal");
    const double held = intersample->reading().truePeakMax;
    check(held >= initialMax && held > intersample->reading().truePeak + 20, "MAX holds when current peak falls, including transition overs");
    tone(*intersample, 1, true);
    check(intersample->reading().truePeakMax == held, "MAX survives mute");
    for (int i = 0; i < 17; ++i) intersample->capture(planes, 4096);
    intersample->process();
    check(intersample->reading().dropped > 0 && intersample->reading().truePeakMax == held, "MAX survives queue-loss recovery");
    intersample->reset(); intersample->process();
    check(std::isnan(intersample->reading().truePeakMax), "manual reset clears MAX");
    tone(*intersample, 1, false, 1000, .1);
    check(intersample->reading().truePeakMax < -19, "MAX starts a new history after reset");
    auto startup = std::make_unique<lufs::Meter>(48000, 2);
    std::vector<float> first(480, .1f); const float* startPlanes[] = {first.data(), first.data()};
    startup->capture(startPlanes, first.size()); startup->process();
    check(std::isfinite(startup->reading().truePeak) && !std::isfinite(startup->reading().shortTerm), "true peak works before LUFS warmup");
    tone(*startup, 1, false, 1000, 0.00001);
    check(std::abs(startup->reading().truePeak + 100) < .1, "true peak measures below LUFS absolute gate");
    check(lufs::true_peak_level(-6.01, true) == lufs::TargetLevel::Below, "true peak below warning");
    check(lufs::true_peak_level(-6, true) == lufs::TargetLevel::Near, "true peak yellow starts at minus 6");
    check(lufs::true_peak_level(0, true) == lufs::TargetLevel::Near, "true peak zero is yellow");
    check(lufs::true_peak_level(.01, true) == lufs::TargetLevel::Over, "true peak above zero is red");
    check(lufs::true_peak_level(1, false) == lufs::TargetLevel::Unavailable, "stale peak does not warn");
    check(lufs::true_peak_level(-10, true, -12) == lufs::TargetLevel::Near, "custom warning changes color");
    check(lufs::true_peak_level(-12.01, true, -12) == lufs::TargetLevel::Below, "custom warning lower boundary");
    check(lufs::true_peak_level(-12, true, -12) == lufs::TargetLevel::Near, "custom warning exact boundary");
    check(lufs::true_peak_level(.01, true, -12) == lufs::TargetLevel::Over, "red remains above zero");
    check(lufs::peak_warning(12) == 0 && lufs::peak_warning(-80) == -60 && lufs::peak_warning(lufs::unavailable()) == -6, "warning sanitization");
    std::cout << "PASS: MAX hold/mute/loss/reset, configurable warning; True Peak intersample/gain/window/startup/reset/silence/thresholds; reference tone, gain, mono, mute, reset, chunking, overflow, PCM preservation, NaN\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
