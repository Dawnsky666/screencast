param(
    [string]$QtRoot = "C:/Qt/6.8.3/msvc2022_64",
    [string]$FFmpegRoot = "C:/ffmpeg",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot "build.ps1") -QtRoot $QtRoot -FFmpegRoot $FFmpegRoot -Configuration $Configuration

$src = Join-Path $root "build/$Configuration"
$dst = Join-Path $root "dist"
if (-not (Test-Path (Join-Path $src "ScreenCast.exe"))) {
    throw "ScreenCast.exe not found in $src"
}

if (Test-Path $dst) {
    Remove-Item $dst -Recurse -Force
}
New-Item -ItemType Directory -Path $dst | Out-Null
Copy-Item (Join-Path $src "*") $dst -Recurse -Force

"packaged: $dst"
