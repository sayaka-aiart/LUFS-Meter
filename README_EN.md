# LUFS Meter

[日本語](README.md) · English

A compact LUFS / True Peak meter for individual OBS audio sources and the stream master.
For Windows x64 / OBS Studio 32. No additional audio plugin is required.

[![Download for Windows x64](https://img.shields.io/badge/Download-Windows_x64-81ada4?style=for-the-badge)](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Windows-x64.zip)

**[Download the latest ZIP](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Windows-x64.zip)** ·
[Releases](https://github.com/sayaka-aiart/LUFS-Meter/releases) ·
[Detailed guide (Japanese)](README_JA.md)

The installation ZIP contains only **obs-lufs-meter.dll** and **導入方法.txt** (installation instructions in Japanese).
The current plugin interface is in Japanese; this README provides English instructions.

| Horizontal | Vertical |
| --- | --- |
| ![Horizontal meters](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.4.0/docs/images/horizontal.png) | ![Vertical meters](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.4.0/docs/images/vertical.png) |

True Peak only, with the yellow warning threshold set to −12 dBTP:

![True Peak only](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.4.0/docs/images/truepeak.png)

Screenshots show v0.4.0 measuring test tones in a separate OBS instance.

## Features

- Measure the stream-track master and selected audio sources separately.
- Display Momentary, Short-term (3 seconds), or Integrated LUFS, plus True Peak in dBTP.
- Select **LUFS only / LUFS + True Peak / True Peak only** in settings.
- When both are enabled, horizontal meters stack LUFS above True Peak; vertical meters place LUFS on the left and True Peak on the right. LUFS uses a thick teal bar; True Peak uses a thin lavender bar.
- The default LUFS target is **−14 LUFS**. The meter turns yellow from 3 LU below the target through the target, and red above it.
- Set the True Peak yellow warning threshold independently (default **−6 dBTP**). Values above 0 dBTP turn red.
- **True Peak MAX hold** appears as a number and a bar marker. Clear it with the reset button to the left of the gear.
- Use the top-right gear to set individual source targets, select and reorder sources, and switch orientation.
- Includes libebur128. The meter measures audio without changing it.

## Codex and external tools

Open the gear → **外部連携** (External integration) → enable **JSON出力** (JSON export).
The plugin exports source and master LUFS, True Peak, and MAX readings every 500 ms.
Use **パスをコピー** (Copy path) to copy the location of `readings.json`.

The source ZIP includes a Python reader and a PowerShell script for inspecting and changing OBS volume and filters through OBS WebSocket.
JSON export is off by default. The plugin itself does not adjust volume or filters.

[External integration guide (Japanese)](docs/EXTERNAL_CONTROL.md) ·
[Source ZIP with integration scripts](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Source-and-Licenses.zip)

For MCP, use the [stdio server](scripts/lufs_meter_mcp.py) and [configuration template](mcp/codex-config.toml) from this repository.
The latest released source ZIP may predate these MCP additions. See the [BOOTH kit manual (Japanese)](docs/BOOTH_MANUAL_JA.txt) and [API guide (Japanese)](docs/BOOTH_API_JA.md).

### Example prompt for Codex

Enable JSON export, replace the two path placeholders below, and paste the prompt into Codex.
For the scripts, use Python 3.10 or later and PowerShell 7 or later.
Enable the OBS WebSocket server under **Tools → WebSocket Server Settings**, and make its password available through the `OBS_WEBSOCKET_PASSWORD` environment variable in the environment running the script.
The default local endpoint is `ws://127.0.0.1:4455`; change it below if your OBS uses another port.

```text
Adjust my OBS stream audio using LUFS Meter readings.

Integration scripts: <folder where the source ZIP was extracted>/scripts
Meter JSON: <path to readings.json copied from LUFS Meter's External integration settings>
OBS WebSocket: ws://127.0.0.1:4455
Authentication: use the OBS_WEBSOCKET_PASSWORD environment variable. Do not display the password in replies or logs.

Targets (editable):
- Keep the microphone prominent, aiming for −17 to −16 LUFS Short-term during speech.
- Keep background music and game audio quiet enough for my voice to remain clearly audible.
- Keep the stream master's Integrated loudness at or below −14 LUFS. This is a ceiling; do not raise the mix just to reach −14 LUFS.
- During representative speech + music + game audio, also check that the master's Short-term loudness stays around or below −14 LUFS.
- Aim for True Peak MAX at or below −1 dBTP after adjustments.
- Use the voice as the reference and adjust its loudness difference from music and game audio. Do not normalize all three to the same LUFS.
- Music should sit quietly behind my voice. Game audio should remain understandable, without effects masking my voice.
- Check in-game dialogue and cutscenes separately from gameplay effects, since I also want viewers to hear that content.

Workflow:
1. Read docs/EXTERNAL_CONTROL.md and the scripts' usage instructions.
   Read measurements with read_meter.py, and inspect current volume and filters with obs_control.ps1 -Action Inspect.
   Match source UUIDs, profile, and scene collection to confirm that both tools refer to the same OBS instance.
   Identify the microphone, music, and game audio sources, recording their roles and UUIDs.
   If the game audio includes music, do not count it again as a separate music source.
2. Save the original volume and filter settings to new, timestamped backups before making changes.
   Work on sources routed to the stream master. Limit changes to the volume, gain, compressor, and limiter settings that are needed.
3. Reset measurements and announce the start of a 30-second measurement period.
   I will play the same music and game audio used in my streams, speaking at my normal volume and slightly louder.
   Record source and master LUFS, True Peak, and MAX during this period.
   Use Short-term readings during speech to compare microphone-minus-music and microphone-minus-game loudness differences in LU.
   Do not judge the microphone as too quiet based only on Integrated readings that include pauses.
   Check normal gameplay, loud effects, and in-game dialogue, noting problems for each situation separately.
4. Make the necessary adjustments based on the measurements.
   Use obs_control.ps1 with -Apply and a new -BackupPath for every settings change.
   Change volume or gain by about 1–2 dB at a time, then read back the settings to verify the change.
   Do not increase gain based on silence, muted sources, warmup readings, or stale JSON.
5. After changes, use ResetMeters to clear LUFS history and True Peak MAX, then measure again under the same conditions.
   Compare the microphone target, voice-to-music/game differences, and master ceiling.
   If the master exceeds −14 LUFS, first lower music and game audio to preserve the voice. Control loud microphone passages if needed.
   Restore changes that made the result worse, using backups in reverse order.
   Do not chase every momentary change in speaking volume; settle on settings that work consistently for a normal stream.
   Do not infer speech intelligibility from loudness differences alone. Use a short recording or my listening feedback when needed.
6. Briefly report the settings changed, before/after source and master LUFS and True Peak MAX,
   voice-to-music/game loudness differences, and backup locations.

This request includes the settings changes, remeasurement, and restoration needed within the scope above.
Perform the checks and adjustments wherever the connection permits, rather than stopping at instructions.
```

LUFS and True Peak alone cannot establish perceived voice quality or distortion.
If you also want EQ or other tonal changes, provide recordings that can be compared.

## Installation

1. Download and extract **LUFS-Meter-Windows-x64.zip** using the button above.
2. Close OBS and copy **obs-lufs-meter.dll** into **obs-plugins/64bit/** inside your OBS installation.
3. Start OBS and open **Docks → LUFS Meter**.

The usual destination is `C:/Program Files/obs-studio/obs-plugins/64bit/`.
No additional Qt or OBS DLLs need to be copied.
To uninstall, close OBS and remove this plugin's DLL.

GitHub's automatically generated “Source code” ZIPs are for development.
Choose **LUFS-Meter-Windows-x64.zip** to install the plugin.

## Measurement scope

The master measures the combined audio on the OBS track selected for streaming, before encoding.
Individual source readings use audio after filters and the source's current OBS volume gain.
The master is measured from the actual mix; source LUFS values are not added together.
Mono and stereo are supported.

[Measurement details (Japanese)](README_JA.md#測定範囲) · [Verification record](docs/VERIFICATION.md)

## Development

[Build on Windows](docs/BUILDING.md) · [Release procedure](docs/RELEASING.md) · [Changelog](CHANGELOG.md)

The measurement core can be tested without Qt or OBS:

```sh
cmake -S . -B build-core -DBUILD_OBS_PLUGIN=OFF
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## License

Original plugin code is licensed under **GPL-2.0-or-later**.
libebur128 uses the MIT license; the bundled queue.h uses BSD-3-Clause.
Full license texts and copyright notices are also embedded in the DLL.
Download the corresponding source and licenses as a [separate ZIP](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Source-and-Licenses.zip).

[LICENSE](LICENSE) · [Third-party notices](THIRD_PARTY_NOTICES.md)
