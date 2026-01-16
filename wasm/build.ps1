# SonicKit WebAssembly Build Script (Windows PowerShell)
# Author: wangxuebing <lynnss.codeai@gmail.com>

param(
    [switch]$Debug,
    [switch]$MinSize,
    [switch]$NoOpus,
    [switch]$NoRNNoise,
    [switch]$Threads,
    [switch]$SIMD,
    [switch]$Clean,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

Write-Host "=== SonicKit WebAssembly Build ===" -ForegroundColor Green

if ($Help) {
    Write-Host "Usage: .\build.ps1 [options]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Debug         Debug build (default: Release)"
    Write-Host "  -MinSize       Optimize for size"
    Write-Host "  -NoOpus        Disable Opus codec"
    Write-Host "  -NoRNNoise     Disable RNNoise denoiser"
    Write-Host "  -Threads       Enable pthread (experimental)"
    Write-Host "  -SIMD          Enable WASM SIMD"
    Write-Host "  -Clean         Clean rebuild"
    Write-Host "  -Help          Show help"
    exit 0
}

# Check Emscripten
$emcc = Get-Command emcc -ErrorAction SilentlyContinue
if (-not $emcc) {
    Write-Host "Error: Emscripten not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Emscripten SDK:"
    Write-Host "  1. git clone https://github.com/emscripten-core/emsdk.git"
    Write-Host "  2. cd emsdk"
    Write-Host "  3. .\emsdk.bat install latest"
    Write-Host "  4. .\emsdk.bat activate latest"
    Write-Host "  5. .\emsdk_env.bat"
    exit 1
}

Write-Host "Emscripten version:" -ForegroundColor Green
emcc --version

# Build parameters
$BUILD_TYPE = if ($Debug) { "Debug" } elseif ($MinSize) { "MinSizeRel" } else { "Release" }
$ENABLE_OPUS = if ($NoOpus) { "OFF" } else { "ON" }
$ENABLE_RNNOISE = if ($NoRNNoise) { "OFF" } else { "ON" }
$ENABLE_THREADS = if ($Threads) { "ON" } else { "OFF" }
$ENABLE_SIMD = if ($SIMD) { "ON" } else { "OFF" }

Write-Host ""
Write-Host "Build config:" -ForegroundColor Yellow
Write-Host "  BUILD_TYPE:      $BUILD_TYPE"
Write-Host "  ENABLE_OPUS:     $ENABLE_OPUS"
Write-Host "  ENABLE_RNNOISE:  $ENABLE_RNNOISE"
Write-Host "  ENABLE_THREADS:  $ENABLE_THREADS"
Write-Host "  ENABLE_SIMD:     $ENABLE_SIMD"

# Clean build directory
if ($Clean -and (Test-Path "build")) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Push-Location "build"

try {
    # Configure CMake
    Write-Host ""
    Write-Host "Configuring CMake..." -ForegroundColor Green

    $cmakeArgs = @(
        "..",
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE",
        "-DWASM_ENABLE_OPUS=$ENABLE_OPUS",
        "-DWASM_ENABLE_RNNOISE=$ENABLE_RNNOISE",
        "-DWASM_ENABLE_THREADS=$ENABLE_THREADS",
        "-DWASM_ENABLE_SIMD=$ENABLE_SIMD"
    )

    & emcmake cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # Build
    Write-Host ""
    Write-Host "Building..." -ForegroundColor Green

    $numProcs = $env:NUMBER_OF_PROCESSORS
    if (-not $numProcs) { $numProcs = 4 }
    & emmake make -j $numProcs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    # Check output files
    if ((Test-Path "sonickit.js") -and (Test-Path "sonickit.wasm")) {
        Write-Host ""
        Write-Host "Build successful!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Output files:"

        $jsFile = Get-Item "sonickit.js"
        $wasmFile = Get-Item "sonickit.wasm"

        Write-Host "  sonickit.js:   $([math]::Round($jsFile.Length / 1KB, 2)) KB"
        Write-Host "  sonickit.wasm: $([math]::Round($wasmFile.Length / 1KB, 2)) KB"
        Write-Host "  Total:         $([math]::Round(($jsFile.Length + $wasmFile.Length) / 1KB, 2)) KB"

        # Copy to dist directory
        $distDir = "..\dist"
        if (-not (Test-Path $distDir)) {
            New-Item -ItemType Directory -Path $distDir | Out-Null
        }
        Copy-Item "sonickit.js", "sonickit.wasm" -Destination $distDir

        Write-Host ""
        Write-Host "Files copied to dist/" -ForegroundColor Green
    } else {
        throw "Output files not found"
    }

} catch {
    Write-Host ""
    Write-Host "Build failed: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "To test in browser:" -ForegroundColor Cyan
Write-Host "  cd examples"
Write-Host "  python -m http.server 8000"
Write-Host "  Open http://localhost:8000/demo_denoiser.html"
