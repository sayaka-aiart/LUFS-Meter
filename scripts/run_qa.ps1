param([ValidateSet('horizontal','vertical')][string]$Orientation = 'horizontal', [string]$BuildDir = 'build', [ValidateSet('lufs','both','truepeak')][string]$Metrics = 'lufs', [ValidateRange(-60,0)][double]$TruePeakWarning = -6)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtimeRoot = Join-Path $projectRoot 'qa\obs-portable'
$exe = Join-Path $runtimeRoot 'bin\64bit\obs64.exe'
$settingsDir = Join-Path $runtimeRoot 'config\obs-studio\plugin_config\obs-lufs-meter'
New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
$settingsPath = Join-Path $settingsDir 'settings.ini'
if (Test-Path -LiteralPath $settingsPath) { Copy-Item -LiteralPath $settingsPath -Destination "$settingsPath.$Orientation.backup" -Force }
$direction = if ($Orientation -eq 'vertical') { 1 } else { 0 }
$showLufs = if ($Metrics -eq 'truepeak') { 'false' } else { 'true' }
$showPeak = if ($Metrics -eq 'lufs') { 'false' } else { 'true' }
@('[General]', "truePeakWarning=$($TruePeakWarning.ToString([Globalization.CultureInfo]::InvariantCulture))", "showLufs=$showLufs", "showTruePeak=$showPeak", "orientation=$direction", 'target=-14', 'displayMode=0') | Set-Content -LiteralPath $settingsPath -Encoding ascii
if (![IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $projectRoot $BuildDir }
Copy-Item -LiteralPath (Join-Path $BuildDir 'Release/obs-lufs-meter.dll') -Destination (Join-Path $runtimeRoot 'obs-plugins/64bit/obs-lufs-meter.dll') -Force
$env:LUFS_METER_QA_DIR = Join-Path $projectRoot "qa\actual-obs-$Metrics-$Orientation"
$env:LUFS_METER_QA_EXIT = '1'
$process = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -PassThru `
  -ArgumentList @('--portable','--multi','--profile','LUFS-QA','--collection','LUFS-QA','--disable-updater','--disable-missing-files-check')
if (!$process.WaitForExit(25000)) { throw "Isolated OBS did not exit; PID $($process.Id), path $exe" }
if ($process.ExitCode -ne 0) { throw "Isolated OBS exit $($process.ExitCode)" }
& python (Join-Path $PSScriptRoot 'verify_qa.py') $env:LUFS_METER_QA_DIR
if ($LASTEXITCODE) { throw 'Native measurement QA failed' }
Write-Host "$Metrics / $Orientation native OBS measurement PASS, exit 0"
