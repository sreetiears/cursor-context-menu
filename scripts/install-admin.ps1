$root = Split-Path -Parent $PSScriptRoot
$cert = Join-Path $root "cert.cer"
$rootCer = Join-Path $root "root.cer"
if (-not (Test-Path $cert)) {
    Write-Error "找不到 cert.cer，请先运行 scripts\build.ps1 -Target all"
    exit 1
}
if (-not (Test-Path $rootCer)) { $rootCer = $cert }

& certutil.exe -addstore Root $rootCer
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& certutil.exe -addstore TrustedPeople $cert
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
