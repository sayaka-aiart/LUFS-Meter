# Building LUFS Meter

## Windows plugin

Requirements: Windows x64, PowerShell 7, Python 3.10+, Git, CMake 3.20+,
and Visual Studio 2019/2022 C++ Build Tools with a Windows SDK.

Run in the repository root:

When building from LUFS-Meter-Source-and-Licenses.zip, use the extracted
LUFS-Meter-VERSION-source/ directory as the repository root.

    .\scripts\fetch_obs.ps1
    .\scripts\fetch_qt.ps1
    .\scripts\build.ps1
    python scripts/package.py

The fetch scripts obtain the pinned OBS 32.0.4 SDK/runtime and Qt 6.8.3 SDK
from official OBS downloads, verify SHA-256, and extract into ignored deps/.
No global installation is changed. [Pinned dependency metadata](../cmake/dependencies.json).

The build script detects Visual Studio through vswhere. Override parameters
ObsSourceRoot, ObsRoot, QtRoot, BuildDir and Generator to use other compatible
local dependencies. For example:

    .\scripts\build.ps1 -ObsSourceRoot C:/dev/obs-studio -ObsRoot C:/dev/obs-portable -QtRoot C:/dev/qt6 -BuildDir build-custom -Generator "Visual Studio 17 2022"

The OBS runtime is used to generate import libraries, not redistributed.
The plugin uses the Qt libraries already provided by OBS.

Outputs:

- build/Release/obs-lufs-meter.dll
- dist/obs-lufs-meter-VERSION-windows-x64/
- dist/LUFS-Meter-Windows-x64.zip (DLL and Japanese installation instructions only)
- dist/LUFS-Meter-Source-and-Licenses.zip
- dist/SHA256SUMS.txt

The Windows DLL embeds the full GPL, MIT and BSD notices as RCDATA resources.
Packaging verifies their contents before creating either ZIP.

CI uses -QtPlatform minimal for the widget contract test.
This verifies controls and layout changes; it is not an OBS-host or live-stream test.

## Measurement tests without OBS

    cmake -S . -B build-core -DBUILD_OBS_PLUGIN=OFF
    cmake --build build-core --config Release
    ctest --test-dir build-core -C Release --output-on-failure

The core test can run on Windows or Linux. The OBS plugin is currently Windows-only.

## Optional isolated OBS check

After a normal plugin build:

    python scripts/prepare_qa.py --obs-root C:/dev/obs-portable
    .\scripts\run_qa.ps1 -Orientation horizontal -Metrics both
    .\scripts\run_qa.ps1 -Orientation vertical -Metrics both
    .\scripts\run_qa.ps1 -Orientation horizontal -Metrics truepeak
    .\scripts\run_qa.ps1 -Orientation horizontal -Metrics lufs

These scripts create a disposable runtime and synthetic tones under ignored qa/,
use its own profile, and collect measurements/screenshots. They do not start
an external stream or install into your normal OBS.
