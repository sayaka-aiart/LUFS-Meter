#Requires -Version 7.0
param([string]$BuildDir = 'build')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtimeRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'qa/obs-portable'))
if (!$runtimeRoot.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid isolated runtime path.' }
$exe = Join-Path $runtimeRoot 'bin/64bit/obs64.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Prepare the isolated OBS runtime first.' }
if (@(Get-Process -Name obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }).Count) { throw 'The isolated OBS runtime is already running.' }
if (Get-NetTCPConnection -LocalPort 45773 -State Listen -ErrorAction SilentlyContinue) { throw 'QA port 45773 is occupied.' }
if (![IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $projectRoot $BuildDir }
$runDir = Join-Path $projectRoot ("qa/external-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDir | Out-Null
$webConfig = Join-Path $runtimeRoot 'config/obs-studio/plugin_config/obs-websocket/config.json'
$meterConfig = Join-Path $runtimeRoot 'config/obs-studio/plugin_config/obs-lufs-meter/settings.ini'
$readingPath = Join-Path (Split-Path $meterConfig) 'readings.json'
$bridge = Join-Path $PSScriptRoot 'obs_control.ps1'
$reader = Join-Path $PSScriptRoot 'read_meter.py'
$endpoint = 'ws://127.0.0.1:45773'
$obsProcess = $null
$originalPassword = $env:OBS_WEBSOCKET_PASSWORD
$originalQaExit = $env:LUFS_METER_QA_EXIT
$originalQaDir = $env:LUFS_METER_QA_DIR
$originalStopFile = $env:LUFS_METER_QA_STOP_FILE
$stopFile = Join-Path $runDir 'stop.marker'
$hadEnabledWebDll = Test-Path -LiteralPath (Join-Path $runtimeRoot 'obs-plugins/64bit/obs-websocket.dll')
$hadWeb = Test-Path -LiteralPath $webConfig
$hadMeter = Test-Path -LiteralPath $meterConfig
if ($hadWeb) { Copy-Item -LiteralPath $webConfig -Destination (Join-Path $runDir 'web-config.backup') }
if ($hadMeter) { Copy-Item -LiteralPath $meterConfig -Destination (Join-Path $runDir 'meter-config.backup') }
function Invoke-Bridge([string[]]$BridgeArgs) {
  $output = & pwsh -NoProfile -File $bridge -Endpoint $endpoint @BridgeArgs
  if ($LASTEXITCODE) { throw "Bridge failed: $($BridgeArgs[0])" }
  return (($output -join [Environment]::NewLine) | ConvertFrom-Json -AsHashtable)
}
function Read-Live {
  $output = & python -X utf8 $reader --file $readingPath
  if ($LASTEXITCODE) { throw 'Live meter read failed.' }
  return (($output -join [Environment]::NewLine) | ConvertFrom-Json -AsHashtable)
}
function Wait-Lufs([double]$Expected) {
  for ($attempt = 0; $attempt -lt 30; ++$attempt) {
    try {
      $data = Get-Content -LiteralPath $readingPath -Raw | ConvertFrom-Json -AsHashtable
      $voice = @($data.sources | Where-Object { $_.name -eq 'マイク' })[0]
      $age = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() - $data.updatedAtUnixMs
      if ($data.status -eq 'running' -and $age -ge -1000 -and $age -lt 2000 -and $null -ne $voice.lufs.shortTerm -and [math]::Abs($voice.lufs.shortTerm - $Expected) -lt .15) { return (Read-Live) }
    } catch { }
    Start-Sleep -Milliseconds 500
  }
  throw "Meter did not reach expected LUFS: $Expected"
}
try {
  $disabledDll = Join-Path $runtimeRoot 'obs-plugins/64bit/obs-websocket.dll.disabled'
  $enabledDll = Join-Path $runtimeRoot 'obs-plugins/64bit/obs-websocket.dll'
  if (!(Test-Path -LiteralPath $enabledDll)) { Copy-Item -LiteralPath $disabledDll -Destination $enabledDll }
  Copy-Item -LiteralPath (Join-Path $BuildDir 'Release/obs-lufs-meter.dll') -Destination (Join-Path $runtimeRoot 'obs-plugins/64bit/obs-lufs-meter.dll') -Force
  $env:OBS_WEBSOCKET_PASSWORD = [guid]::NewGuid().ToString('N') + [guid]::NewGuid().ToString('N')
  @{ server_enabled=$true; auth_required=$true; server_password=$env:OBS_WEBSOCKET_PASSWORD; server_port=45773; first_load=$false; alerts_enabled=$false } | ConvertTo-Json | Set-Content -LiteralPath $webConfig -Encoding utf8
  @('[General]', 'exportEnabled=true', 'showLufs=true', 'showTruePeak=true', 'orientation=0', 'target=-14', 'displayMode=0') | Set-Content -LiteralPath $meterConfig -Encoding ascii
  $env:LUFS_METER_QA_EXIT = $null
  $env:LUFS_METER_QA_DIR = $runDir
  $env:LUFS_METER_QA_STOP_FILE = $stopFile
  $obsProcess = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -PassThru -ArgumentList @('--portable','--multi','--profile','LUFS-QA','--collection','LUFS-QA','--disable-updater','--disable-missing-files-check')
  $before = Wait-Lufs -16.47
  $voice = @($before.sources | Where-Object { $_.name -eq 'マイク' })[0]
  $sourceId = $voice.id
  $inspection = Invoke-Bridge @('-Action','Inspect','-Source',$sourceId)
  if ([math]::Abs($inspection.volume.inputVolumeMul - .5) -gt .001) { throw 'Unexpected initial QA volume.' }
  $firstSequence = $before.sequence
  Start-Sleep -Milliseconds 650
  if ((Read-Live).sequence -le $firstSequence) { throw 'External snapshot did not update.' }
  $volumeBackup = Join-Path $runDir 'volume-before.json'
  $null = Invoke-Bridge @('-Action','SetVolume','-Source',$sourceId,'-VolumeDb','-12.0412','-Apply','-BackupPath',$volumeBackup)
  $null = Invoke-Bridge @('-Action','ResetMeters','-Apply')
  $lower = Wait-Lufs -22.49
  $null = Invoke-Bridge @('-Action','Restore','-FromBackup',$volumeBackup,'-Apply','-BackupPath',(Join-Path $runDir 'volume-undo.json'))
  $null = Invoke-Bridge @('-Action','ResetMeters','-Apply')
  $restored = Wait-Lufs -16.47
  $created = Join-Path $runDir 'filter-created.json'
  $null = Invoke-Bridge @('-Action','CreateFilter','-Source',$sourceId,'-FilterName','LUFS-QA-Gain','-FilterKind','gain_filter','-SettingsJson','{"db":0}','-Apply','-BackupPath',$created)
  $edited = Join-Path $runDir 'filter-edited.json'
  $null = Invoke-Bridge @('-Action','SetFilter','-Source',$sourceId,'-FilterName','LUFS-QA-Gain','-SettingsJson','{"db":-3}','-Apply','-BackupPath',$edited)
  $null = Invoke-Bridge @('-Action','ResetMeters','-Apply')
  $filtered = Wait-Lufs -19.47
  $null = Invoke-Bridge @('-Action','Restore','-FromBackup',$edited,'-Apply','-BackupPath',(Join-Path $runDir 'filter-edit-undo.json'))
  $null = Invoke-Bridge @('-Action','Restore','-FromBackup',$created,'-Apply','-BackupPath',(Join-Path $runDir 'filter-create-undo.json'))
  $null = Invoke-Bridge @('-Action','ResetMeters','-Apply')
  $final = Wait-Lufs -16.47
  $finalInspection = Invoke-Bridge @('-Action','Inspect','-Source',$sourceId)
  if (@($finalInspection.filters | Where-Object { $_.filterName -eq 'LUFS-QA-Gain' }).Count -or [math]::Abs($finalInspection.volume.inputVolumeMul - .5) -gt .001) { throw 'QA settings were not restored.' }
  foreach ($sample in @(@('before',$before),@('lower',$lower),@('restored',$restored),@('filtered',$filtered),@('final',$final))) { $sample[1] | ConvertTo-Json -Depth 50 | Set-Content -LiteralPath (Join-Path $runDir ($sample[0] + '.json')) -Encoding utf8 }
  [IO.File]::WriteAllText($stopFile, 'stop')
  if (!$obsProcess.WaitForExit(10000) -or $obsProcess.ExitCode -ne 0) { throw 'Isolated OBS did not close cleanly.' }
  $terminal = Get-Content -LiteralPath $readingPath -Raw | ConvertFrom-Json -AsHashtable
  if ($terminal.status -ne 'stopped') { throw 'Missing stopped export state.' }
  $null = & python -X utf8 $reader --file $readingPath 2> (Join-Path $runDir 'stopped-reader.log')
  if ($LASTEXITCODE -ne 2) { throw 'Stopped OBS was accepted as live.' }
  @{ status='PASS'; scope='isolated OBS 32.2.2, local synthetic audio'; authenticated=$true; liveUpdates=$true; volumeDeltaDb=-6.0206; filterDeltaDb=-3; resetViaWebSocket=$true; settingsRestored=$true; stoppedRejected=$true; directory=$runDir } | ConvertTo-Json | Tee-Object -FilePath (Join-Path $runDir 'verification.json')
} finally {
  if ($obsProcess -and !$obsProcess.HasExited) {
    [IO.File]::WriteAllText($stopFile, 'stop')
    if (!$obsProcess.WaitForExit(10000)) { $obsProcess.Kill(); $obsProcess.WaitForExit() }
  }
  if (!$hadEnabledWebDll -and (Test-Path -LiteralPath (Join-Path $runtimeRoot 'obs-plugins/64bit/obs-websocket.dll'))) { Remove-Item -LiteralPath (Join-Path $runtimeRoot 'obs-plugins/64bit/obs-websocket.dll') }
  if ($hadWeb) { Copy-Item -LiteralPath (Join-Path $runDir 'web-config.backup') -Destination $webConfig -Force }
  if ($hadMeter) { Copy-Item -LiteralPath (Join-Path $runDir 'meter-config.backup') -Destination $meterConfig -Force }
  $env:OBS_WEBSOCKET_PASSWORD = $originalPassword
  $env:LUFS_METER_QA_EXIT = $originalQaExit
  $env:LUFS_METER_QA_DIR = $originalQaDir
  $env:LUFS_METER_QA_STOP_FILE = $originalStopFile
}
