$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$deps = Join-Path $projectRoot 'deps'
$spec = Get-Content -LiteralPath (Join-Path $projectRoot 'cmake/dependencies.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $deps | Out-Null
function Get-VerifiedArchive([string]$Url, [string]$Path, [string]$Hash) {
  if (!(Test-Path -LiteralPath $Path) -or (Get-FileHash -LiteralPath $Path).Hash.ToLower() -ne $Hash) {
    & curl.exe -L --fail --retry 5 --retry-all-errors --output $Path $Url
    if ($LASTEXITCODE) { throw "Download failed: $Url" }
  }
  if ((Get-FileHash -LiteralPath $Path).Hash.ToLower() -ne $Hash) { throw "SHA256 mismatch: $Path" }
}
$source = Join-Path $deps "obs-source-$($spec.obsVersion).zip"
$runtime = Join-Path $deps "OBS-Studio-$($spec.obsVersion)-Windows-x64.zip"
Get-VerifiedArchive $spec.obsSourceUrl $source $spec.obsSourceSha256
Get-VerifiedArchive $spec.obsRuntimeUrl $runtime $spec.obsRuntimeSha256
$sdkRoot = Join-Path $deps 'obs-sdk'
$runtimeRoot = Join-Path $deps 'obs-runtime'
if (!(Test-Path (Join-Path $sdkRoot "obs-studio-$($spec.obsVersion)/libobs/obs.h"))) {
  Expand-Archive -LiteralPath $source -DestinationPath $sdkRoot -Force
}
if (!(Test-Path (Join-Path $runtimeRoot 'bin/64bit/obs.dll'))) {
  Expand-Archive -LiteralPath $runtime -DestinationPath $runtimeRoot -Force
}
Write-Host "Verified OBS $($spec.obsVersion) SDK and runtime in deps/"
