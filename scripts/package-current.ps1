param(
    [string]$BuildDir = "build",
    [string]$DistDir = "dist"
)

$ErrorActionPreference = "Stop"
& "$PSScriptRoot/build.ps1" -BuildDir $BuildDir
cmake --build $BuildDir --target package --config Release

$vsExe = Join-Path $BuildDir "Release/mybarrier.exe"
$ninjaExe = Join-Path $BuildDir "mybarrier.exe"
$exe = if (Test-Path $vsExe) { $vsExe } elseif (Test-Path $ninjaExe) { $ninjaExe } else { throw "Unable to find mybarrier.exe" }

$arch = if ([Environment]::Is64BitOperatingSystem) { "x86_64" } else { "x86" }
$name = "mybarrier-windows-$arch"
$out = Join-Path $DistDir $name
New-Item -ItemType Directory -Force -Path $out | Out-Null
Copy-Item $exe $out -Force
Copy-Item README.md $out -Force
Copy-Item docs/build-artifacts.md $out -Force
Copy-Item docs/implementation.md $out -Force

$zip = Join-Path $DistDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip
Write-Host "Packaged directory: $out"
Write-Host "Archive: $zip"
Write-Host "CPack packages:"
Get-ChildItem -Path $BuildDir -Filter "mybarrier-*" -File | ForEach-Object { Write-Host $_.FullName }
