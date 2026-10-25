param(
    [string]$QtRoot = "E:/deepseek/tools/Qt/6.8.3/msvc2022_64",
    [string]$FFmpegRoot = "E:/deepseek/tools/ffmpeg-9.0.2-full_build-shared",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) {
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
}
if (-not $cmake) { throw "cmake not found" }

& $cmake -S $root -B "$root/build" -G "Visual Studio 17 2022" -A x64 `
    "-DCMAKE_PREFIX_PATH=$QtRoot" "-DFFMPEG_ROOT=$FFmpegRoot"
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

& $cmake --build "$root/build" --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed" }

"built: $root/build/$Configuration/ScreenCast.exe"
