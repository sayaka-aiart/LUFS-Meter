# Verification

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
