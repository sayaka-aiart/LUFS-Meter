# LUFS Meter 0.1.0

Initial public preview for Windows x64 / OBS Studio 32.

## Download

Use **LUFS-Meter-Windows-x64.zip** in Assets. It contains the plugin DLL,
installation instructions, license notices and corresponding source.
SHA256SUMS.txt verifies the download.

## Features

- Independent source and stream-master LUFS metering with libebur128.
- Momentary / 3-second / Integrated display, configurable targets and source order.
- Compact horizontal or vertical meters; settings are grouped under the gear.

## Installation

Close OBS, extract the ZIP, and copy obs-plugins/64bit/obs-lufs-meter.dll
to the matching folder in your OBS installation. Open **Docks → LUFS Meter**.
OBS and Qt runtime DLLs are not included.

This is an early preview. Local OBS 32.2.2 source/mix checks and widget tests
passed; streaming to an actual platform and long-duration stability remain
unverified. See the repository's verification document for details.
