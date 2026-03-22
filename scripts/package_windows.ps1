# Windows Release Packaging Script
param(
    [string]$Version = "dev"
)

Write-Host "Packaging Amplitron for Windows..." -ForegroundColor Cyan

# Create release directory
$releaseDir = "release/Amplitron-Windows"
New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null

# Copy executable
Copy-Item "build/guitar-amp.exe" "$releaseDir/Amplitron.exe"

# Copy documentation
Copy-Item "README.md" "$releaseDir/"
Copy-Item "LICENSE" "$releaseDir/" -ErrorAction SilentlyContinue

# Copy assets and presets
Copy-Item -Recurse "assets" "$releaseDir/" -ErrorAction SilentlyContinue
Copy-Item -Recurse "presets" "$releaseDir/" -ErrorAction SilentlyContinue

# Copy MinGW runtime DLLs
$mingwBin = "C:\msys64\mingw64\bin"
if (Test-Path $mingwBin) {
    Copy-Item "$mingwBin/libgcc_s_seh-1.dll" "$releaseDir/"
    Copy-Item "$mingwBin/libstdc++-6.dll" "$releaseDir/"
    Copy-Item "$mingwBin/libwinpthread-1.dll" "$releaseDir/"
    Copy-Item "$mingwBin/libportaudio-2.dll" "$releaseDir/"
    Copy-Item "$mingwBin/SDL2.dll" "$releaseDir/"
}

# Create README for the package
@"
# Amplitron - Guitar Amp Simulator

Version: $Version

## Quick Start

1. Double-click Amplitron.exe to launch
2. Connect your guitar interface
3. Select your audio devices in Settings
4. Start playing!

## Features

- Real-time audio processing with ultra-low latency
- 9 studio-quality effects pedals
- Preset management system
- Multi-track recording

## Support

- GitHub: https://github.com/sudip-mondal-2002/Amplitron
- Email: sudmondal2002@gmail.com

"@ | Out-File -FilePath "$releaseDir/README.txt" -Encoding UTF8

# Create ZIP archive
Compress-Archive -Path $releaseDir -DestinationPath "release/Amplitron-Windows-x64.zip" -Force

Write-Host "Package created: release/Amplitron-Windows-x64.zip" -ForegroundColor Green
