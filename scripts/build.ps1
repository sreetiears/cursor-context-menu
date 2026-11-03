param(
    [ValidateSet("menu", "pack", "all")]
    [string]$Target = "menu"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root "src"
$tools = Join-Path $root "tools"

function Invoke-Checked {
    param([string]$File, [string[]]$CommandArgs)
    & $File @CommandArgs
    if ($LASTEXITCODE -ne 0) { throw "$File 失败，退出码 $LASTEXITCODE" }
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$windres = (Get-Command windres -ErrorAction Stop).Source

Push-Location $src
try {
    Invoke-Checked $windres @("-O", "coff", "-i", "command.rc", "-o", "command.res")
    Invoke-Checked $gcc @(
        "-shared", "-O2", "-s", "-static-libgcc",
        "-o", (Join-Path $root "CursorExplorerCommand.dll"),
        "command.c", "command.res", "command.def",
        "-lole32", "-lshlwapi"
    )
    Invoke-Checked $gcc @(
        "-O2", "-s", "-static-libgcc", "-mwindows",
        "-o", (Join-Path $root "CursorContextHost.exe"),
        "host.c"
    )
    Invoke-Checked $gcc @(
        "-O2", "-s", "-static-libgcc",
        "-o", (Join-Path $root "test_menu.exe"),
        "test.c",
        "-lole32", "-lshlwapi"
    )
} finally {
    Pop-Location
}

if ($Target -eq "menu") { return }

Invoke-Checked $gcc @(
    "-O2", "-s", "-static-libgcc",
    "-o", (Join-Path $root "packsign.exe"),
    (Join-Path $tools "driver.c"),
    (Join-Path $tools "pack.c"),
    (Join-Path $tools "sign.c"),
    "-lole32", "-lshlwapi", "-lurlmon", "-lcrypt32"
)

function Ensure-DevCert {
    $cert = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq "CN=CursorOpenProject" -and $_.HasPrivateKey } |
        Select-Object -First 1
    if (-not $cert) {
        $cert = New-SelfSignedCertificate `
            -Type Custom `
            -Subject "CN=CursorOpenProject" `
            -KeyUsage DigitalSignature `
            -FriendlyName "Cursor Open Project" `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}") `
            -NotAfter (Get-Date).AddYears(5)
    }

    $cer = Join-Path $root "cert.cer"
    Export-Certificate -Cert $cert -FilePath $cer -Force | Out-Null
    if ($cert.Subject -eq $cert.Issuer) {
        Copy-Item $cer (Join-Path $root "root.cer") -Force
    }
}

Ensure-DevCert
Push-Location $root
try {
    Invoke-Checked (Join-Path $root "packsign.exe") @("pack")
    Invoke-Checked (Join-Path $root "packsign.exe") @("sign")
} finally {
    Pop-Location
}
