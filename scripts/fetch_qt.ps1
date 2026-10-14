$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$deps = Join-Path $projectRoot 'deps'
$spec = Get-Content -LiteralPath (Join-Path $projectRoot 'cmake/dependencies.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $deps | Out-Null
$archive = Join-Path $deps 'qt6.zip'
if (!(Test-Path -LiteralPath $archive) -or (Get-FileHash -LiteralPath $archive).Hash.ToLower() -ne $spec.qtSha256) {
  & curl.exe -L --fail --retry 5 --retry-all-errors --output $archive $spec.qtUrl
  if ($LASTEXITCODE) { throw 'Qt SDK download failed' }
}
if ((Get-FileHash -LiteralPath $archive).Hash.ToLower() -ne $spec.qtSha256) { throw 'Qt SDK hash mismatch' }
if (!(Test-Path (Join-Path $deps 'qt6/lib/cmake/Qt6/Qt6Config.cmake'))) {
  Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $deps 'qt6') -Force
}
Write-Host "Verified Qt $($spec.qtVersion) SDK in deps/qt6"
