#Requires -Version 7.0
param(
  [ValidateSet('Inspect','SetVolume','SetFilter','CreateFilter','Restore','ResetMeters')][string]$Action = 'Inspect',
  [string]$Endpoint = 'ws://127.0.0.1:4455',
  [string]$Source = '',
  [string]$FilterName = '',
  [string]$FilterKind = '',
  [string]$SettingsJson = '{}',
  [double]$VolumeDb = [double]::NaN,
  [string]$BackupPath = '',
  [string]$FromBackup = '',
  [switch]$Apply
)
$ErrorActionPreference = 'Stop'
$socket = $null
$cancel = [Threading.CancellationTokenSource]::new()
$cancel.CancelAfter(15000)

function Receive-Obs {
  $buffer = [byte[]]::new(8192)
  $stream = [IO.MemoryStream]::new()
  try {
    do {
      $part = $socket.ReceiveAsync([ArraySegment[byte]]::new($buffer), $cancel.Token).GetAwaiter().GetResult()
      if ($part.MessageType -eq [Net.WebSockets.WebSocketMessageType]::Close) { throw 'OBS closed the WebSocket connection.' }
      if ($part.MessageType -ne [Net.WebSockets.WebSocketMessageType]::Text) { throw 'Expected OBS JSON text.' }
      $stream.Write($buffer, 0, $part.Count)
      if ($stream.Length -gt 1048576) { throw 'OBS response is too large.' }
    } while (!$part.EndOfMessage)
    return ([Text.Encoding]::UTF8.GetString($stream.ToArray()) | ConvertFrom-Json -AsHashtable)
  } finally { $stream.Dispose() }
}
function Send-Obs($Message) {
  $bytes = [Text.Encoding]::UTF8.GetBytes(($Message | ConvertTo-Json -Depth 50 -Compress))
  $null = $socket.SendAsync([ArraySegment[byte]]::new($bytes), [Net.WebSockets.WebSocketMessageType]::Text, $true, $cancel.Token).GetAwaiter().GetResult()
}
function Invoke-Obs([string]$Type, [hashtable]$Data = @{}) {
  $requestId = [guid]::NewGuid().ToString()
  Send-Obs @{ op=6; d=@{ requestType=$Type; requestId=$requestId; requestData=$Data } }
  do { $reply = Receive-Obs } while ($reply.op -ne 7 -or $reply.d.requestId -ne $requestId)
  if (!$reply.d.requestStatus.result) { throw "$Type failed: $($reply.d.requestStatus.comment)" }
  if ($reply.d.ContainsKey('responseData')) { return $reply.d.responseData }
  return @{}
}
function Get-Context {
  $profileInfo = Invoke-Obs 'GetProfileList'
  $collectionInfo = Invoke-Obs 'GetSceneCollectionList'
  return @{ profile=$profileInfo.currentProfileName; collection=$collectionInfo.currentSceneCollectionName }
}
function Save-Checkpoint($Checkpoint) {
  if (!$BackupPath) { throw 'A new -BackupPath is required for changes.' }
  $absolute = [IO.Path]::GetFullPath($BackupPath)
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($absolute)) | Out-Null
  $bytes = [Text.Encoding]::UTF8.GetBytes(($Checkpoint | ConvertTo-Json -Depth 50))
  $file = [IO.File]::Open($absolute, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
  try { $file.Write($bytes, 0, $bytes.Length); $file.Flush($true) } finally { $file.Dispose() }
  return $absolute
}
function Check-Settings($Settings, $Defaults, $Current) {
  if ($Settings -isnot [Collections.IDictionary]) { throw 'SettingsJson must be a JSON object.' }
  foreach ($key in $Settings.Keys) {
    if (!$Defaults.ContainsKey($key) -and !$Current.ContainsKey($key)) { throw "Unknown filter setting: $key" }
    $value = $Settings[$key]
    if ($null -eq $value -or $value -is [Collections.IDictionary] -or $value -is [array]) { throw 'Use scalar filter settings (numbers, strings or booleans).' }
    if (($value -is [double] -or $value -is [float]) -and ![double]::IsFinite($value)) { throw 'Filter values must be finite.' }
  }
}
function Verify-Settings($Expected, $Actual) {
  foreach ($key in $Expected.Keys) {
    if (!$Actual.ContainsKey($key) -or $Actual[$key] -ne $Expected[$key]) { throw "OBS did not retain requested value for: $key. The checkpoint can restore the previous settings." }
  }
}

try {
  $uri = [Uri]$Endpoint
  if (!$uri.IsLoopback -or $uri.Scheme -notin @('ws','wss') -or $uri.UserInfo -or $uri.Query -or $uri.Fragment) { throw 'Use a local OBS WebSocket endpoint without URL credentials.' }
  if ($Action -ne 'Inspect' -and !$Apply) { throw 'Changes require the explicit -Apply switch.' }
  $restore = $null
  if ($Action -eq 'Restore') {
    if (!$FromBackup) { throw 'Restore requires -FromBackup.' }
    $restore = Get-Content -LiteralPath $FromBackup -Raw | ConvertFrom-Json -AsHashtable
    if ($restore.schemaVersion -ne 1 -or $restore.app -ne 'LUFS Meter OBS bridge' -or $restore.endpoint -ne $uri.AbsoluteUri) { throw 'Checkpoint schema or endpoint does not match.' }
    $Source = $restore.sourceUuid
  }
  $socket = [Net.WebSockets.ClientWebSocket]::new()
  $null = $socket.ConnectAsync($uri, $cancel.Token).GetAwaiter().GetResult()
  $hello = Receive-Obs
  if ($hello.op -ne 0) { throw 'Expected OBS WebSocket 5 Hello.' }
  $identify = @{ rpcVersion=1; eventSubscriptions=0 }
  if ($hello.d.ContainsKey('authentication')) {
    $password = [Environment]::GetEnvironmentVariable('OBS_WEBSOCKET_PASSWORD')
    if (!$password) { throw 'Set OBS_WEBSOCKET_PASSWORD for this OBS server.' }
    $secret = [Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($password + $hello.d.authentication.salt)))
    $identify.authentication = [Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($secret + $hello.d.authentication.challenge)))
    $password = $null; $secret = $null
  }
  Send-Obs @{ op=1; d=$identify }
  $identified = Receive-Obs
  if ($identified.op -ne 2) { throw 'OBS identification failed.' }
  $inputList = Invoke-Obs 'GetInputList'
  if ($Action -eq 'ResetMeters') {
    $hotkeys = (Invoke-Obs 'GetHotkeyList').hotkeys
    if ('obs-lufs-meter.reset' -notin $hotkeys) { throw 'LUFS Meter reset hotkey is unavailable.' }
    $null = Invoke-Obs 'TriggerHotkeyByName' @{ hotkeyName='obs-lufs-meter.reset' }
    @{ action=$Action; requested=$true } | ConvertTo-Json
    exit 0
  }
  if (!$Source) {
    if ($Action -ne 'Inspect') { throw 'Select one source by UUID or exact name.' }
    @{ action=$Action; inputs=$inputList.inputs; context=(Get-Context) } | ConvertTo-Json -Depth 50
    exit 0
  }
  $sourceMatches = @($inputList.inputs | Where-Object { $_.inputUuid -ceq $Source -or $_.inputName -ceq $Source })
  if ($sourceMatches.Count -ne 1) { throw 'Select one existing input by UUID or unique exact name.' }
  $inputInfo = $sourceMatches[0]
  $inputRef = @{ inputUuid=$inputInfo.inputUuid }
  $sourceRef = @{ sourceUuid=$inputInfo.inputUuid }
  $context = Get-Context
  if ($Action -eq 'Inspect') {
    @{ action=$Action; source=$inputInfo; context=$context; volume=(Invoke-Obs 'GetInputVolume' $inputRef); mute=(Invoke-Obs 'GetInputMute' $inputRef); filters=(Invoke-Obs 'GetSourceFilterList' $sourceRef).filters } | ConvertTo-Json -Depth 50
    exit 0
  }
  if ($restore -and ($restore.context.profile -ne $context.profile -or $restore.context.collection -ne $context.collection)) { throw 'OBS profile or scene collection differs from the checkpoint.' }
  $checkpoint = @{ schemaVersion=1; app='LUFS Meter OBS bridge'; endpoint=$uri.AbsoluteUri; utc=[DateTime]::UtcNow.ToString('o'); sourceUuid=$inputInfo.inputUuid; sourceName=$inputInfo.inputName; context=$context }
  $type = if ($Action -eq 'Restore') { $restore.type } elseif ($Action -eq 'SetVolume') { 'volume' } else { 'filter' }
  if ($type -eq 'volume') {
    if ($Action -ne 'Restore' -and (![double]::IsFinite($VolumeDb) -or $VolumeDb -lt -60 -or $VolumeDb -gt 6)) { throw 'VolumeDb must be between -60 and +6 dB.' }
    $checkpoint.type = 'volume'; $checkpoint.before = Invoke-Obs 'GetInputVolume' $inputRef
    $savedPath = Save-Checkpoint $checkpoint
    $change = @{ inputUuid=$inputInfo.inputUuid }
    if ($restore) { $change.inputVolumeMul = $restore.before.inputVolumeMul } else { $change.inputVolumeDb = $VolumeDb }
    $null = Invoke-Obs 'SetInputVolume' $change
    $after = Invoke-Obs 'GetInputVolume' $inputRef
    $expectedGain = if ($restore) { [double]$restore.before.inputVolumeMul } else { [math]::Pow(10, $VolumeDb/20) }
    if ([math]::Abs($after.inputVolumeMul - $expectedGain) -gt .0001) { throw 'Volume verification failed; restore from the checkpoint.' }
  } elseif ($type -eq 'filter') {
    if ($restore) { $FilterName = $restore.filterName }
    if (!$FilterName) { throw 'FilterName is required.' }
    $filters = (Invoke-Obs 'GetSourceFilterList' $sourceRef).filters
    $existing = @($filters | Where-Object { $_.filterName -ceq $FilterName })
    if ($existing.Count -gt 1) { throw 'Ambiguous filter name.' }
    $checkpoint.type = 'filter'; $checkpoint.filterName = $FilterName; $checkpoint.existed = ($existing.Count -eq 1)
    $checkpoint.before = if ($checkpoint.existed) { Invoke-Obs 'GetSourceFilter' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName } } else { $null }
    if ($restore) {
      if ($restore.existed -and $checkpoint.existed -and $checkpoint.before.filterKind -ne $restore.before.filterKind) { throw 'Filter no longer matches the saved kind.' }
      if (!$restore.existed -and (!$checkpoint.existed -or $checkpoint.before.filterKind -ne $restore.createdKind)) { throw 'Created filter no longer matches the checkpoint.' }
    } elseif ($Action -eq 'CreateFilter') {
      if ($checkpoint.existed -or !$FilterKind) { throw 'CreateFilter needs a new filter name and a FilterKind.' }
    } elseif (!$checkpoint.existed) { throw 'SetFilter needs an existing filter.' }
    if (!$restore) {
      $kind = if ($checkpoint.existed) { $checkpoint.before.filterKind } else { $FilterKind }
      $defaults = (Invoke-Obs 'GetSourceFilterDefaultSettings' @{ filterKind=$kind }).defaultFilterSettings
      $changes = $SettingsJson | ConvertFrom-Json -AsHashtable
      Check-Settings $changes $defaults $(if ($checkpoint.existed) { $checkpoint.before.filterSettings } else { @{} })
      $checkpoint.createdKind = $kind
    } else { $checkpoint.createdKind = if ($restore.existed) { $restore.before.filterKind } else { $checkpoint.before.filterKind } }
    $savedPath = Save-Checkpoint $checkpoint
    if ($restore -and !$restore.existed) {
      $null = Invoke-Obs 'RemoveSourceFilter' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName }
      $remaining = (Invoke-Obs 'GetSourceFilterList' $sourceRef).filters
      if (@($remaining | Where-Object { $_.filterName -ceq $FilterName }).Count) { throw 'Filter removal verification failed.' }
      $after = @{ removed=$FilterName }
    } elseif ($restore) {
      if (!$checkpoint.existed) { $null = Invoke-Obs 'CreateSourceFilter' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName; filterKind=$restore.before.filterKind; filterSettings=$restore.before.filterSettings } }
      $null = Invoke-Obs 'SetSourceFilterSettings' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName; filterSettings=$restore.before.filterSettings; overlay=$false }
      $null = Invoke-Obs 'SetSourceFilterEnabled' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName; filterEnabled=$restore.before.filterEnabled }
      $after = Invoke-Obs 'GetSourceFilter' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName }
      Verify-Settings $restore.before.filterSettings $after.filterSettings
      if ($after.filterEnabled -ne $restore.before.filterEnabled) { throw 'Filter enable state verification failed.' }
    } elseif ($Action -eq 'CreateFilter') {
      $null = Invoke-Obs 'CreateSourceFilter' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName; filterKind=$FilterKind; filterSettings=$changes }
      $after = Invoke-Obs 'GetSourceFilter' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName }
      Verify-Settings $changes $after.filterSettings
    } else {
      $null = Invoke-Obs 'SetSourceFilterSettings' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName; filterSettings=$changes; overlay=$true }
      $after = Invoke-Obs 'GetSourceFilter' @{ sourceUuid=$inputInfo.inputUuid; filterName=$FilterName }
      Verify-Settings $changes $after.filterSettings
    }
  } else { throw 'Unsupported checkpoint type.' }
  @{ action=$Action; source=$inputInfo; backup=$savedPath; verified=$true; after=$after } | ConvertTo-Json -Depth 50
} catch {
  [Console]::Error.WriteLine((@{ error=$_.Exception.Message } | ConvertTo-Json -Compress))
  exit 1
} finally {
  if ($socket) { $socket.Dispose() }
  $cancel.Dispose()
}
