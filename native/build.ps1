$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$src = Join-Path $root 'src'
$inc = Join-Path $root 'include'
$out = Join-Path $root 'bin'

New-Item -ItemType Directory -Force -Path $out | Out-Null

function Find-Gcc {
    $cmd = Get-Command gcc.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $roots = @(
        "$env:LOCALAPPDATA\Microsoft\WinGet\Packages",
        "$env:ProgramFiles\WinLibs",
        'C:\mingw64',
        'D:\mingw64'
    )
    foreach ($r in $roots) {
        if (Test-Path $r) {
            $hit = Get-ChildItem -Path $r -Recurse -Filter 'gcc.exe' -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match 'bin\\gcc\.exe$' } | Select-Object -First 1
            if ($hit) { return $hit.FullName }
        }
    }
    return $null
}

$gcc = Find-Gcc
if (-not $gcc) { throw 'gcc not found' }

$sources = Get-ChildItem -Path $src -Filter '*.c' | ForEach-Object { $_.FullName }

$gccArgs = @(
    '-std=c11', '-O2', '-Wall', '-Wno-unused-parameter',
    '-DUNICODE', '-D_UNICODE', '-DWIN32_LEAN_AND_MEAN', '-DCOBJMACROS',
    "-I$inc", "-I$src",
    '-shared', '-o', (Join-Path $out 'koiTileCore.dll'),
    "-Wl,--out-implib,$(Join-Path $out 'libkoiTileCore.a')"
) + $sources + @(
    '-lole32', '-loleaut32', '-luuid', '-luser32', '-lshell32', '-lshlwapi',
    '-lgdi32', '-lwinhttp', '-ladvapi32', '-lcomctl32', '-lwindowscodecs'
)

& $gcc @gccArgs
if ($LASTEXITCODE -ne 0) { throw "gcc failed: $LASTEXITCODE" }

Write-Host "built $out\koiTileCore.dll"
