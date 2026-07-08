param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --config Release -j

$vsExe = Join-Path $BuildDir "Release/mybarrier.exe"
$ninjaExe = Join-Path $BuildDir "mybarrier.exe"
if (Test-Path $vsExe) {
    Write-Host "Built: $vsExe"
} elseif (Test-Path $ninjaExe) {
    Write-Host "Built: $ninjaExe"
} else {
    throw "Unable to find mybarrier.exe under $BuildDir"
}
