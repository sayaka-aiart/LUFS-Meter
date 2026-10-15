# Verification

## External integration 0.4.0 — 2026-10-15

- Reader tests cover freshness, stopped/disabled/changing states, invalid JSON,
  unknown schemas and ambiguous source names.
- Widget/serialization tests cover export save/cancel, full metrics, null values,
  warmup/mute, preserved MAX, atomic replacement and write failure.
- Authenticated OBS WebSocket integration in isolated OBS 32.2.2 verified a live
  JSON sequence, -6.0206 dB input volume change, -3 dB gain-filter change, source
  filter creation, settings checkpoints and restoration of original state.
- WebSocket-triggered measurement reset and explicit stopped state were verified.
  The reader rejected the stopped process. The isolated OBS exited with code 0.
- This used local synthetic tones and did not change the normal OBS installation.

## MAX hold and configurable warning 0.3.0 — 2026-10-15

- Measurement tests verify MAX persistence after lower input, mute and queue-loss
  recovery, and explicit reset followed by a new measurement history.
- Warning tests cover custom boundaries, the unchanged >0 dBTP red threshold,
  defaults and invalid/out-of-range configuration values.
- Widget tests verify the header reset button is left of the gear and invokes reset.
- Widget tests verify MAX labels during mute, warning save/cancel, immediate
  recoloring and enable/disable state for LUFS-only / True-Peak-only / both.
- Isolated OBS 32.2.2 passed both orientations plus True-Peak-only with a saved
  -12 dBTP warning setting. MAX was never below the current True Peak.
- Current docs/images show these native readings; settings.png is a widget preview.

## True Peak and metric selection 0.2.0 — 2026-10-15

- Measurement and widget contracts passed with the new binary.
- The 12 kHz phase-offset test at 48 kHz has sample peaks below 0 dBFS
  and reconstructed True Peak above +1.4 dBTP; oversampling detects the overs.
- Reference dBTP, fader gain, mute/reset, rolling peak release, startup before
  LUFS warmup, values below the LUFS absolute gate and -6/0 boundaries passed.
- All three display combinations passed in both orientations, including cancel,
  the at-least-one-metric rule, stale/muted values and above/below vs left/right layout.
- Isolated OBS 32.2.2 loaded both-metric horizontal/vertical, True-Peak-only and
  LUFS-only displays. Voice True Peak was -16.48 dBTP, matching amplitude 0.15.
  Master mix routing and muted-source exclusion also passed; all checks exited 0.
- The screenshots in docs/images/ show the native OBS measurements; the settings
  image is a standalone widget preview. These checks do not connect a streaming service.

## Minimal install package 0.1.1 — 2026-10-15

- Windows Release build, measurement contract and widget contract passed.
- The new DLL loaded in isolated OBS 32.2.2 and exited with code 0.
  Voice, BGM and Track 2 measured -16.47, -17.13 and -13.78 LUFS;
  Track-1-only and muted sources were excluded from the master.
- Packaging checks the full embedded GPL/MIT/BSD notices against source files,
  ZIP integrity and the exact two-file install layout: obs-lufs-meter.dll
  and 導入方法.txt. Matching source/licenses are packaged in a separate ZIP.

## Initial preview — 2026-10-14

- Windows x64 Release build and measurement contract passed.
- The contract covers reference tones, mono/stereo mapping, fader gain,
  mute, reset, block splitting, bounded queue overflow, invalid samples and
  preservation of input PCM.
- Routing tests cover Simple/Advanced selection, invalid track numbers,
  multi-track masks, active encoder priority and inactive stale encoders.
- Target boundaries and modal settings save/cancel, source selection and
  horizontal/vertical changes passed the widget contract.
- Isolated OBS 32.2.2 loaded the dock, measured synthetic sources and Track 2,
  rendered both orientations, and exited with code 0.
- The voice tone measured about −16.47 LUFS, BGM −17.13 LUFS, and their
  mixed Track 2 −13.78 LUFS. Track-1-only game audio and muted FX were excluded.
- Screenshots in docs/images/ show these local test measurements.

## Limits

Actual streaming to a platform, SRT/RIST live output, 32 simultaneous sources
and long-duration stability have not been verified. The live encoder-following
path has contract coverage, not a live streaming acceptance result.

Source gain is the current OBS user gain applied to the measurement copy.
Sample-timed gain changes, source sync offsets and transition mixing may differ
from this per-source estimate. Use the master for the final OBS mix measurement.

OBS's allocation counter reported one remaining allocation on shutdown.
Its origin has not been isolated; a clean process exit is not a memory-leak test.

CI builds and tests are separate from these OBS-host checks. See each workflow
run for the results of that particular revision.
