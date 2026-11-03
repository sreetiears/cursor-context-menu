# 用 Cursor 打开项目

Windows 11 资源管理器右键菜单。在文件夹上右键，或在文件夹空白处右键，选择 **Open project in Cursor**，用 Cursor 打开这个目录。可以同时选中多个文件夹。

菜单由一个稀疏 MSIX 包注册。命令实现在 `CursorExplorerCommand.dll` 里，它先读同目录的 `cursor.path`，找不到再使用 Cursor 的默认安装位置 `%LOCALAPPDATA%\Programs\cursor\Cursor.exe`。

## 要求

- Windows 11（版本 22000 或更高）
- [MinGW-w64](https://www.mingw-w64.org/)，并且 `gcc` 和 `windres` 在 `PATH` 里
- 已安装 Cursor

## 配置 Cursor 路径

Cursor 装在默认位置时不用额外配置。装在其他位置时，把 `cursor.path.example` 复制为 `cursor.path`，写上 `Cursor.exe` 的完整路径。这个文件放在 `CursorExplorerCommand.dll` 旁边。路径里可以写 `%LOCALAPPDATA%` 这样的环境变量。

`cursor.path` 只在本机使用，不会提交到仓库。

## 构建

在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

这会生成右键命令 `CursorExplorerCommand.dll`、包宿主 `CursorContextHost.exe`，以及用来检查命令的 `test_menu.exe`。

检查命令是否加载成功：

```powershell
.\test_menu.exe
```

输出里应有标题 `Open project in Cursor`。`state` 的值是 `0` 表示已经找到 Cursor，`2` 表示没找到，菜单会隐藏。

打包并签名：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Target all
```

签名使用当前用户证书存储里的 `CN=CursorOpenProject`。没有这张证书时，脚本会新建一张自签名证书，并导出 `cert.cer` 和 `root.cer`。

`tools\packsign.cs` 是同一套打包和签名的 C# 写法。构建脚本走 C 版本，入口是 `packsign pack` 和 `packsign sign`。

## 安装

```powershell
powershell -ExecutionPolicy Bypass -File scripts\install.ps1
```

安装会请求管理员权限，把签名证书放进本机「受信任的根证书」和「受信任的人」，然后注册右键菜单。这张证书只用来给这个菜单包签名。

装好后关掉已经打开的资源管理器窗口，再重新打开，右键文件夹即可看到菜单。

安装之后不要移动这个目录。菜单从这个目录加载 DLL。换了位置就再运行一次安装脚本。

## 卸载

```powershell
Get-AppxPackage Cursor.OpenProject | Remove-AppxPackage
```

## 目录

```
src/                 右键命令、包宿主、加载测试
tools/               打包和签名
scripts/build.ps1    编译，-Target all 时同时打包签名
scripts/install.ps1  构建、安装证书并注册菜单
Assets/              包图标
AppxManifest.xml     稀疏包清单
```
