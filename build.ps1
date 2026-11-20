$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$dist = Join-Path (Split-Path $root -Parent) 'koiTile'

Write-Host '[1/3] native core'
& (Join-Path $root 'native\build.ps1')

Write-Host '[2/3] winui app'
$project = Join-Path $root 'src\koiTile.App\koiTile.App.csproj'
& dotnet build $project -c Release -p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "dotnet build failed: $LASTEXITCODE" }

Write-Host '[3/3] publish'
$bin = Join-Path $root 'src\koiTile.App\bin\x64\Release\net9.0-windows10.0.19041.0\win-x64'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$null = robocopy $bin $dist /MIR /NFL /NDL /NJH /NJS /NP
if ($LASTEXITCODE -ge 8) { throw "robocopy failed: $LASTEXITCODE" }

foreach ($path in @(
    (Join-Path $root 'src\koiTile.App\bin'),
    (Join-Path $root 'src\koiTile.App\obj'),
    (Join-Path $root 'src\koiTileLaunch\bin'),
    (Join-Path $root 'src\koiTileLaunch\obj'),
    (Join-Path $root 'native\bin'),
    (Join-Path $root 'data'))) {
    if (Test-Path $path) { Remove-Item $path -Recurse -Force }
}

Write-Host "output: $(Join-Path $dist 'koiTile.exe')"
