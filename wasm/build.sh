#!/bin/bash

# SonicKit WebAssembly 构建脚本
# 作者: wangxuebing <lynnss.codeai@gmail.com>

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # 无颜色

echo -e "${GREEN}=== SonicKit WebAssembly 构建脚本 ===${NC}"

# 检查 Emscripten 环境
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}错误: 未找到 Emscripten！${NC}"
    echo "请安装 Emscripten SDK:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo -e "${GREEN}Emscripten 版本:${NC}"
emcc --version

# 解析命令行参数
BUILD_TYPE="Release"
ENABLE_OPUS="ON"
ENABLE_RNNOISE="ON"
ENABLE_THREADS="OFF"
ENABLE_SIMD="OFF"
CLEAN_BUILD="OFF"

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --minsize)
            BUILD_TYPE="MinSizeRel"
            shift
            ;;
        --no-opus)
            ENABLE_OPUS="OFF"
            shift
            ;;
        --no-rnnoise)
            ENABLE_RNNOISE="OFF"
            shift
            ;;
        --threads)
            ENABLE_THREADS="ON"
            shift
            ;;
        --simd)
            ENABLE_SIMD="ON"
            shift
            ;;
        --clean)
            CLEAN_BUILD="ON"
            shift
            ;;
        --help)
            echo "使用方法: $0 [选项]"
            echo ""
            echo "选项:"
            echo "  --debug         Debug 模式构建 (默认: Release)"
            echo "  --minsize       最小大小优化"
            echo "  --no-opus       禁用 Opus 编解码器"
            echo "  --no-rnnoise    禁用 RNNoise 降噪器"
            echo "  --threads       启用 pthread 支持 (实验性)"
            echo "  --simd          启用 WASM SIMD"
            echo "  --clean         清理后重新构建"
            echo "  --help          显示帮助信息"
            exit 0
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            echo "使用 --help 查看帮助信息"
            exit 1
            ;;
    esac
done

# 清理构建目录
if [ "$CLEAN_BUILD" = "ON" ]; then
    echo -e "${YELLOW}清理构建目录...${NC}"
    rm -rf build
fi

# 创建构建目录
mkdir -p build
cd build

# 配置
echo -e "${GREEN}配置 CMake...${NC}"
emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DWASM_ENABLE_OPUS=$ENABLE_OPUS \
    -DWASM_ENABLE_RNNOISE=$ENABLE_RNNOISE \
    -DWASM_ENABLE_THREADS=$ENABLE_THREADS \
    -DWASM_ENABLE_SIMD=$ENABLE_SIMD

# 编译
echo -e "${GREEN}编译中...${NC}"
emmake make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 检查输出文件
if [ -f "sonickit.js" ] && [ -f "sonickit.wasm" ]; then
    echo -e "${GREEN}构建成功！${NC}"
    echo ""
    echo "输出文件:"
    ls -lh sonickit.js sonickit.wasm
    
    # 文件大小
    JS_SIZE=$(wc -c < sonickit.js | tr -d ' ')
    WASM_SIZE=$(wc -c < sonickit.wasm | tr -d ' ')
    TOTAL_SIZE=$((JS_SIZE + WASM_SIZE))
    
    echo ""
    echo "文件大小:"
    echo "  sonickit.js:   $(numfmt --to=iec-i --suffix=B $JS_SIZE 2>/dev/null || echo $JS_SIZE bytes)"
    echo "  sonickit.wasm: $(numfmt --to=iec-i --suffix=B $WASM_SIZE 2>/dev/null || echo $WASM_SIZE bytes)"
    echo "  总计:         $(numfmt --to=iec-i --suffix=B $TOTAL_SIZE 2>/dev/null || echo $TOTAL_SIZE bytes)"
    
    # 复制到 dist 目录
    mkdir -p ../dist
    cp sonickit.js sonickit.wasm ../dist/
    echo ""
    echo -e "${GREEN}文件已复制到 dist/ 目录${NC}"
else
    echo -e "${RED}构建失败！${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=== 构建完成 ===${NC}"
echo ""
echo "在浏览器中测试:"
echo "  cd ../examples"
echo "  python3 -m http.server 8000"
echo "  打开浏览器访问 http://localhost:8000"
