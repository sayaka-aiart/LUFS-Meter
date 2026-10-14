# Contributing

Please describe the OBS version, audio layout and reproduction steps for bugs.
Changes to metering should preserve the original audio samples and keep
allocation, libebur128 processing and blocking work out of audio callbacks.

Run the measurement contract for core changes, and the Windows build/widget
contract for UI/plugin changes. Host-test audio routing or lifetime changes
in an isolated OBS profile. Document what was and was not tested.

Original contributions use GPL-2.0-or-later. Preserve third-party notices.
Do not include SDKs, OBS profiles, build outputs or raw local logs in commits.
