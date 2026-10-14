param(
  [string]$ObsSourceRoot = '',
  [string]$ObsRoot = '',
  [string]$QtRoot = '',
  [string]$BuildDir = 'build',
  [string]$Generator = '',
  [string]$VcToolsBin = '',
  [ValidateSet('windows','minimal')][string]$QtPlatform = 'windows'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$spec = Get-Content -LiteralPath (Join-Path $projectRoot 'cmake/dependencies.json') -Raw | ConvertFrom-Json
if (!$ObsSourceRoot) { $ObsSourceRoot = Join-Path $projectRoot "deps/obs-sdk/obs-studio-$($spec.obsVersion)" }
if (!$ObsRoot) { $ObsRoot = Join-Path $projectRoot 'deps/obs-runtime' }
if (!$QtRoot) { $QtRoot = Join-Path $projectRoot 'deps/qt6' }
if (![IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $projectRoot $BuildDir }
foreach ($path in @((Join-Path $ObsSourceRoot 'libobs/obs.h'), (Join-Path $ObsRoot 'bin/64bit/obs.dll'), (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
  if (!(Test-Path -LiteralPath $path)) { throw "Missing dependency: $path. Run fetch_obs.ps1 / fetch_qt.ps1, or supply explicit paths." }
}
if (!$VcToolsBin) {
  $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio/Installer/vswhere.exe'
  if (!(Test-Path $vswhere)) { throw 'Visual Studio C++ Build Tools and vswhere are required.' }
  $dumpbinPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe'
  $dumpbinPath = $dumpbinPath | Select-Object -First 1
  if (!$dumpbinPath) { throw 'No Visual Studio x64 C++ toolchain found.' }
  $VcToolsBin = Split-Path $dumpbinPath
}
$importDir = Join-Path $projectRoot 'deps/imports'
New-Item -ItemType Directory -Force -Path $importDir | Out-Null
foreach ($name in @('obs', 'obs-frontend-api')) {
  $dll = Join-Path $ObsRoot "bin/64bit/$name.dll"
  $exports = & (Join-Path $VcToolsBin 'dumpbin.exe') /exports $dll
  if ($LASTEXITCODE) { throw "Cannot inspect $dll" }
  $symbols = $exports | Where-Object { $_ -match '^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+\S+' } | ForEach-Object { ($_ -split '\s+')[-1] } | Where-Object { $_ -match '^(obs_|audio_|config_|blog$|bfree$|bzalloc$|bmalloc$|brealloc$)' }
  $def = Join-Path $importDir "$name.def"
  @("LIBRARY $name.dll", 'EXPORTS') + $symbols | Set-Content -LiteralPath $def -Encoding ascii
  & (Join-Path $VcToolsBin 'lib.exe') "/def:$def" /machine:x64 "/out:$(Join-Path $importDir "$name.lib")"
  if ($LASTEXITCODE) { throw "Cannot generate import library for $name" }
}
$configure = @('-S', $projectRoot, '-B', $BuildDir, "-DOBS_SOURCE_ROOT=$ObsSourceRoot", "-DOBS_IMPORT_LIBRARY=$(Join-Path $importDir 'obs.lib')", "-DOBS_FRONTEND_LIBRARY=$(Join-Path $importDir 'obs-frontend-api.lib')", "-DCMAKE_PREFIX_PATH=$QtRoot")
if ($Generator) { $configure += @('-G', $Generator, '-A', 'x64') }
elseif (!(Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))) { $configure += @('-A', 'x64') }
& cmake @configure
if ($LASTEXITCODE) { throw 'Configure failed' }
& cmake --build $BuildDir --config Release --parallel 2
if ($LASTEXITCODE) { throw 'Build failed' }
& ctest --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Measurement tests failed' }
$qaDir = Join-Path $projectRoot 'qa'
New-Item -ItemType Directory -Force -Path $qaDir | Out-Null
$oldPath = $env:PATH
$oldPlatform = $env:QT_QPA_PLATFORM
$oldPluginPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
try {
  $env:PATH = "$(Join-Path $QtRoot 'bin');$oldPath"
  $env:QT_QPA_PLATFORM = $QtPlatform
  $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtRoot 'plugins/platforms'
  $uiTest = Start-Process -FilePath (Join-Path $BuildDir 'Release/ui_contract.exe') -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput (Join-Path $qaDir 'ui-contract.log') -RedirectStandardError (Join-Path $qaDir 'ui-contract-error.log')
  if ($uiTest.ExitCode -ne 0) { throw "Widget contract failed; see qa/ui-contract-error.log (exit $($uiTest.ExitCode))" }
  Get-Content -LiteralPath (Join-Path $qaDir 'ui-contract.log')
} finally {
  $env:PATH = $oldPath
  $env:QT_QPA_PLATFORM = $oldPlatform
  $env:QT_QPA_PLATFORM_PLUGIN_PATH = $oldPluginPath
}
$versionMatch = [regex]::Match((Get-Content (Join-Path $projectRoot 'CMakeLists.txt') -Raw), 'project\(obs_lufs_meter VERSION ([0-9.]+)')
if (!$versionMatch.Success) { throw 'Cannot determine project version' }
$packageRoot = Join-Path $projectRoot "dist/obs-lufs-meter-$($versionMatch.Groups[1].Value)-windows-x64"
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot 'obs-plugins/64bit') | Out-Null
Copy-Item -LiteralPath (Join-Path $BuildDir 'Release/obs-lufs-meter.dll') -Destination (Join-Path $packageRoot 'obs-plugins/64bit/obs-lufs-meter.dll') -Force
Write-Host "Verified plugin staged at $packageRoot"
