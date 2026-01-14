# SonicKit WebAssembly 构建脚本 (Windows PowerShell)
# 作者: wangxuebing <lynnss.codeai@gmail.com>

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

Write-Host "=== SonicKit WebAssembly 构建脚本 ===" -ForegroundColor Green

if ($Help) {
    Write-Host @"
使用方法: .\build.ps1 [选项]

选项:
  -Debug         Debug 模式构建 (默认: Release)
  -MinSize       最小大小优化
  -NoOpus        禁用 Opus 编解码器
  -NoRNNoise     禁用 RNNoise 降噪器
  -Threads       启用 pthread 支持 (实验性)
  -SIMD          启用 WASM SIMD
  -Clean         清理后重新构建
  -Help          显示帮助信息

前置要求:
  1. 安装 Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html
  2. 运行 emsdk_env.ps1 或 emsdk_env.bat 激活环境
"@
    exit 0
}

# 检查 Emscripten 环境
$emcc = Get-Command emcc -ErrorAction SilentlyContinue
if (-not $emcc) {
    Write-Host "错误: 未找到 Emscripten！" -ForegroundColor Red
    Write-Host @"

请安装 Emscripten SDK:
  1. git clone https://github.com/emscripten-core/emsdk.git
  2. cd emsdk
  3. .\emsdk.bat install latest
  4. .\emsdk.bat activate latest
  5. .\emsdk_env.bat  (或在 PowerShell 中运行 .\emsdk_env.ps1)

然后重新运行此脚本。
"@
    exit 1
}

Write-Host "Emscripten 版本:" -ForegroundColor Green
emcc --version

# 设置构建参数
$BUILD_TYPE = if ($Debug) { "Debug" } elseif ($MinSize) { "MinSizeRel" } else { "Release" }
$ENABLE_OPUS = if ($NoOpus) { "OFF" } else { "ON" }
$ENABLE_RNNOISE = if ($NoRNNoise) { "OFF" } else { "ON" }
$ENABLE_THREADS = if ($Threads) { "ON" } else { "OFF" }
$ENABLE_SIMD = if ($SIMD) { "ON" } else { "OFF" }

Write-Host ""
Write-Host "构建配置:" -ForegroundColor Yellow
Write-Host "  BUILD_TYPE:      $BUILD_TYPE"
Write-Host "  ENABLE_OPUS:     $ENABLE_OPUS"
Write-Host "  ENABLE_RNNOISE:  $ENABLE_RNNOISE"
Write-Host "  ENABLE_THREADS:  $ENABLE_THREADS"
Write-Host "  ENABLE_SIMD:     $ENABLE_SIMD"

# 清理构建目录
if ($Clean -and (Test-Path "build")) {
    Write-Host "清理构建目录..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

# 创建构建目录
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Push-Location "build"

try {
    # 配置 CMake
    Write-Host ""
    Write-Host "配置 CMake..." -ForegroundColor Green

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
        throw "CMake 配置失败"
    }

    # 编译
    Write-Host ""
    Write-Host "编译中..." -ForegroundColor Green

    & emmake make -j$env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) {
        throw "编译失败"
    }

    # 检查输出文件
    if ((Test-Path "sonickit.js") -and (Test-Path "sonickit.wasm")) {
        Write-Host ""
        Write-Host "构建成功！" -ForegroundColor Green
        Write-Host ""
        Write-Host "输出文件:"

        $jsFile = Get-Item "sonickit.js"
        $wasmFile = Get-Item "sonickit.wasm"

        Write-Host "  sonickit.js:   $([math]::Round($jsFile.Length / 1KB, 2)) KB"
        Write-Host "  sonickit.wasm: $([math]::Round($wasmFile.Length / 1KB, 2)) KB"
        Write-Host "  总计:          $([math]::Round(($jsFile.Length + $wasmFile.Length) / 1KB, 2)) KB"

        # 复制到 dist 目录
        $distDir = "..\dist"
        if (-not (Test-Path $distDir)) {
            New-Item -ItemType Directory -Path $distDir | Out-Null
        }
        Copy-Item "sonickit.js", "sonickit.wasm" -Destination $distDir

        Write-Host ""
        Write-Host "文件已复制到 dist/ 目录" -ForegroundColor Green
    } else {
        throw "输出文件未找到"
    }

} catch {
    Write-Host ""
    Write-Host "构建失败: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== 构建完成 ===" -ForegroundColor Green
Write-Host ""
Write-Host "在浏览器中测试:" -ForegroundColor Cyan
Write-Host "  cd examples"
Write-Host "  python -m http.server 8000"
Write-Host "  打开浏览器访问 http://localhost:8000/demo_denoiser.html"
