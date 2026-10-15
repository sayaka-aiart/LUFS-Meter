# Changelog

## 0.3.0 — 2026-10-15

- Put the measurement reset button immediately to the left of the settings gear.
- Add True Peak MAX hold values and bar markers for each source and stream master.
- Keep MAX through mute, inactivity and queue-loss recovery; clear on measurement reset.
- Configure and save the True Peak yellow threshold (-60 to 0 dBTP, default -6).
- Apply the threshold consistently to current readings, bars and MAX; red remains above 0 dBTP.

## 0.2.1 — 2026-10-15

- Make True Peak numbers about 12.5% smaller than LUFS in both orientations.

## 0.2.0 — 2026-10-15

- Add per-source and stream-master True Peak metering in dBTP with libebur128.
- Select LUFS only, LUFS + True Peak, or True Peak only in settings.
- Save metric visibility, support both meter orientations and keep LUFS-only as the default.
- True Peak shows a rolling ~400 ms maximum, warns at -6 dBTP and turns red above 0 dBTP.
- Verify intersample overs, gain, mute/reset, rolling peak release and all display combinations.

## 0.1.1 — 2026-10-15

- Simplify the install ZIP to obs-lufs-meter.dll and 導入方法.txt.
- Provide matching source and licenses as a separate release download.
- Embed full license notices in the DLL and verify them during packaging.
- Update README download links and installation instructions.

## 0.1.0 — 2026-10-14

- Independent Windows OBS dock for source and stream-master LUFS.
- Momentary, Short-term and Integrated measurements using libebur128 1.2.6.
- Configurable targets, source selection/order, and horizontal/vertical meters.
- Compact dark UI with a single settings gear.
- Source and license bundle included with Windows releases.

This is an early preview. See [verification and limitations](docs/VERIFICATION.md).
