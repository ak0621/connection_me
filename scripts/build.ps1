param(
    [string]$BuildDir = "build",
    [string]$BuildGui = $env:MYBARRIER_BUILD_GUI
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($BuildGui)) {
    $BuildGui = "AUTO"
}

cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=Release -DMYBARRIER_BUILD_GUI=$BuildGui
cmake --build $BuildDir --config Release -j

$cliCandidates = @(
    (Join-Path $BuildDir "Release/mybarrier.exe"),
    (Join-Path $BuildDir "mybarrier.exe")
)
$guiCandidates = @(
    (Join-Path $BuildDir "Release/MyBarrier.exe"),
    (Join-Path $BuildDir "MyBarrier.exe")
)

$cli = $cliCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($cli) {
    Write-Host "Built CLI: $cli"
} else {
    throw "Unable to find mybarrier.exe under $BuildDir"
}

$gui = $guiCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($gui) {
    Write-Host "Built desktop UI: $gui"
} else {
    Write-Host "Desktop UI not built (MYBARRIER_BUILD_GUI=$BuildGui)"
}
