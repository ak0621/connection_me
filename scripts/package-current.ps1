param(
    [string]$BuildDir = "build",
    [string]$DistDir = "dist",
    [string]$BuildGui = $env:MYBARRIER_BUILD_GUI
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($BuildGui)) {
    $BuildGui = "AUTO"
}

& "$PSScriptRoot/build.ps1" -BuildDir $BuildDir -BuildGui $BuildGui
cmake --build $BuildDir --target package --config Release

$cmakeLists = Get-Content CMakeLists.txt -Raw
$version = if ($cmakeLists -match 'project\(mybarrier VERSION ([^ ]+)') { $Matches[1] } else { "dev" }
$arch = if ([Environment]::Is64BitOperatingSystem) { "x86-64" } else { "x86" }
$name = "MyBarrier-$version-windows-$arch"
$out = Join-Path $DistDir $name
New-Item -ItemType Directory -Force -Path $out | Out-Null

$cliCandidates = @(
    (Join-Path $BuildDir "Release/mybarrier.exe"),
    (Join-Path $BuildDir "mybarrier.exe")
)
$guiCandidates = @(
    (Join-Path $BuildDir "Release/MyBarrier.exe"),
    (Join-Path $BuildDir "MyBarrier.exe")
)
$cli = $cliCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$gui = $guiCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cli) { throw "Unable to find mybarrier.exe under $BuildDir" }

Copy-Item $cli $out -Force
if ($gui) { Copy-Item $gui $out -Force }
Copy-Item README.md $out -Force
Copy-Item docs/build-artifacts.md $out -Force
Copy-Item docs/implementation.md $out -Force

$zip = Join-Path $DistDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip
Write-Host "Packaged directory: $out"
if ($gui) {
    Write-Host "Runnable desktop UI included: $gui"
} else {
    Write-Host "Desktop UI not included (MYBARRIER_BUILD_GUI=$BuildGui)"
}
Write-Host "Archive: $zip"
Write-Host "CPack packages:"
Get-ChildItem -Path $BuildDir -Filter "MyBarrier-*" -File | ForEach-Object { Write-Host $_.FullName }
