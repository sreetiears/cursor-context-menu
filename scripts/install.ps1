$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
$root = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot "build.ps1") -Target all

[void][System.Windows.Forms.MessageBox]::Show(
    "接下来 Windows 会弹出管理员确认，标题是 Windows PowerShell。请点「是」。`n`n这一步把菜单的签名证书放进本机受信任证书，然后注册右键菜单。",
    "用 Cursor 打开项目")

try {
    $admin = Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $PSScriptRoot "install-admin.ps1")
    ) -Verb RunAs -PassThru -Wait
} catch {
    [void][System.Windows.Forms.MessageBox]::Show("没有获得管理员确认，菜单还没装上。`n" + $_.Exception.Message, "用 Cursor 打开项目")
    exit 1
}

if ($admin.ExitCode -ne 0) {
    [void][System.Windows.Forms.MessageBox]::Show("证书没有装进本机受信任存储，菜单还没装上。", "用 Cursor 打开项目")
    exit $admin.ExitCode
}

try {
    Add-AppxPackage -Path (Join-Path $root "pkg\Cursor.OpenProject.msix") -ExternalLocation $root -ForceUpdateFromAnyVersion -ErrorAction Stop
    [void][System.Windows.Forms.MessageBox]::Show(
        "已经加好。`n`n请关掉当前的资源管理器窗口，重新打开后再右键文件夹。主菜单里会出现 Open project in Cursor。",
        "用 Cursor 打开项目")
} catch {
    [void][System.Windows.Forms.MessageBox]::Show("还没有装上：`n" + $_.Exception.Message, "用 Cursor 打开项目")
    exit 1
}
