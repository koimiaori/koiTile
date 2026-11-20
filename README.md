# koiTile

Windows 10 风格开始菜单磁贴的编辑工具：选图、写文字、挑图标，一键生成并注册磁贴包。

## 功能

- 通过创建应用的方式自定义Explorer Patcher上的磁贴

## 视频介绍

[![点击观看视频](https://i2.hdslb.com/bfs/archive/4378dd2f0f9af4cee66eef5892cc57c5bd3b24df.jpg)](https://www.bilibili.com/video/BV14oypzBE4w)

## 环境要求

- 任何能运行 Explorer Patcher 的 Windows 11 ？
- .NET 9 、Windows App Runtime 1.7

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
