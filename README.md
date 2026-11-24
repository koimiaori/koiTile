# koiTile

Windows 10 风格开始菜单磁贴的编辑工具：选图、写文字、挑图标，一键生成并注册磁贴包。

## 功能

- 通过创建应用的方式自定义Explorer Patcher上的磁贴

## 视频介绍

[![点击观看视频](https://i2.hdslb.com/bfs/archive/4378dd2f0f9af4cee66eef5892cc57c5bd3b24df.jpg)](https://www.bilibili.com/video/BV14oypzBE4w)

## 环境要求

- Windows 10 1809+ / 任何能运行 Explorer Patcher 的 Windows 11 ？
- [访问ExplorerPatcher官方仓库](https://github.com/valinet/ExplorerPatcher/releases)
- [![下载 .NET 9 运行时](https://img.shields.io/badge/%E4%B8%8B%E8%BD%BD-.NET%209%20%E8%BF%90%E8%A1%8C%E6%97%B6-512BD4?style=for-the-badge&logo=dotnet&logoColor=white)]
- [访问 .NET 9 运行时 官方网站](https://dotnet.microsoft.com/zh-cn/download/dotnet/thank-you/runtime-desktop-9.0.23-windows-x64-installer)(https://builds.dotnet.microsoft.com/dotnet/WindowsDesktop/9.0.23/windowsdesktop-runtime-9.0.23-win-x64.exe)
- [![下载 Windows App SDK 1.7.3](https://img.shields.io/badge/%E4%B8%8B%E8%BD%BD-Windows%20App%20SDK%201.7.3-0078D4?style=for-the-badge&logo=windows11&logoColor=white)]
- [访问 Windows App SDK 官方网站](https://learn.microsoft.com/zh-cn/windows/apps/windows-app-sdk/downloads-archive)

## 使用

Download Release koiTile-v0.1.0-win-x64.zip，解压后双击 `koiTile.exe`，数据保存在 `%APPDATA%\koiTile`。

应用不可使用管理员权限运行。

需要开启开发者模式。在管理员权限的PowerShell中使用命令
```powershell
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock" /v AllowDevelopmentWithoutDevLicense /t REG_DWORD /d 1 /f
```

碎碎念...为什么在Winui3中添加ImageIconSource会报错啊...

## 构建
使用PowerShell
```powershell
.\koiTile-main\build.ps1
```
