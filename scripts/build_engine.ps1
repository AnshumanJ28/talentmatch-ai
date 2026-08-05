# scripts/build_engine.ps1
# Build the TalentMatch C++ engine on Windows (MSVC or MinGW).
#
# Prerequisites:
#   - CMake 3.18+
#   - Visual Studio 2022 with C++ workload  (or MinGW-w64 via MSYS2)
#   - Git (for FetchContent to download nlohmann/json)
#
# Usage (from repo root in PowerShell):
#   .\scripts\build_engine.ps1
#   .\scripts\build_engine.ps1 -EnableXGBoost
#   .\scripts\build_engine.ps1 -Clean

param(
    [switch]$EnableXGBoost = $false,
    [switch]$Clean = $false
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot "cpp_core\build"
$XGB         = if ($EnableXGBoost) { "ON" } else { "OFF" }

Write-Host "========================================"
Write-Host " TalentMatch Core — C++ Build (Windows)"
Write-Host "========================================"
Write-Host " Project root  : $ProjectRoot"
Write-Host " Build dir     : $BuildDir"
Write-Host " XGBoost       : $XGB"
Write-Host "========================================"
Write-Host ""

# Check cmake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake not found. Install from https://cmake.org/download/"
}

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..."
    Remove-Item -Recurse -Force $BuildDir
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# Configure
Write-Host "Configuring..."
& cmake -S "$ProjectRoot\cpp_core" `
        -B "$BuildDir" `
        -DCMAKE_BUILD_TYPE=Release `
        -DENABLE_XGBOOST=$XGB

if ($LASTEXITCODE -ne 0) { Write-Error "CMake configuration failed" }

# Build
Write-Host ""
Write-Host "Building..."
$cores = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
& cmake --build "$BuildDir" --config Release --parallel $cores

if ($LASTEXITCODE -ne 0) { Write-Error "Build failed" }

# Report
Write-Host ""
Write-Host "========================================"
$dll = Get-ChildItem -Path "$BuildDir" -Filter "talentmatch.dll" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($dll) {
    Write-Host " Build succeeded!"
    Write-Host " Library: $($dll.FullName)"
    Write-Host ""
    Write-Host " To use, set:"
    Write-Host "   `$env:TALENTMATCH_LIB_PATH = '$($dll.FullName)'"
    Write-Host ""
    Write-Host " Or run the API:"
    Write-Host "   uvicorn api:app --host 0.0.0.0 --port 8000"
} else {
    Write-Warning "talentmatch.dll not found in $BuildDir — check build output above"
}
Write-Host "========================================"
