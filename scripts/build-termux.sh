#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

show_usage() {
    echo "Usage: $0 [aarch64|arm|all]"
    echo ""
    echo "Options:"
    echo "  aarch64  Build for 64-bit ARM (most common)"
    echo "  arm      Build for 32-bit ARM"
    echo "  all      Build for both architectures (default)"
    exit 1
}

TARGET="${1:-all}"

if [[ ! "$TARGET" =~ ^(aarch64|arm|all)$ ]]; then
    show_usage
fi

echo "Building MNN LLM Server for Termux..."
echo "Project root: $PROJECT_ROOT"

if [ ! -d "$PROJECT_ROOT/src" ]; then
    echo "Error: src directory not found in $PROJECT_ROOT"
    exit 1
fi

if [ -z "$MNN_ROOT" ]; then
    if [ -d "/root/MNN" ]; then
        MNN_ROOT="/root/MNN"
    elif [ -d "$PROJECT_ROOT/../MNN" ]; then
        MNN_ROOT="$(dirname "$PROJECT_ROOT")/MNN"
    else
        echo "Error: MNN_ROOT not set and default locations not found"
        echo "Please set MNN_ROOT environment variable or clone MNN to /root/MNN"
        exit 1
    fi
fi

echo "Using MNN from: $MNN_ROOT"

build_arch() {
    local arch=$1
    local compiler_prefix

    if [ "$arch" = "aarch64" ]; then
        compiler_prefix="aarch64-linux-gnu"
    else
        compiler_prefix="arm-linux-gnueabihf"
    fi

    echo ">>> Building for $arch ($compiler_prefix)..."

    if ! command -v "${compiler_prefix}-gcc" &> /dev/null; then
        echo "Installing cross-compile toolchain for $arch..."
        sudo apt-get update
        sudo apt-get install -y "gcc-${compiler_prefix}" "g++-${compiler_prefix}"
    fi

    mkdir -p "$PROJECT_ROOT/build-$arch"
    cd "$PROJECT_ROOT/build-$arch"

    cmake ../src \
        -DCMAKE_C_COMPILER="${compiler_prefix}-gcc" \
        -DCMAKE_CXX_COMPILER="${compiler_prefix}-g++" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR="$arch" \
        -DMNN_ROOT="$MNN_ROOT" \
        -DMNN_INCLUDE_DIR="$MNN_ROOT/include" \
        -DMNN_LLM_INCLUDE_DIR="$MNN_ROOT/transformers/llm/engine/include" \
        -DMNN_BUILD_DIR="$MNN_ROOT/build"

    make -j$(nproc)

    mkdir -p "$PROJECT_ROOT/bin"
    cp mnn-server "$PROJECT_ROOT/bin/mnn-server-$arch"

    echo ">>> Built: $PROJECT_ROOT/bin/mnn-server-$arch"
    cd "$PROJECT_ROOT"
}

if [[ "$TARGET" == "all" ]]; then
    build_arch "aarch64"
    build_arch "arm"
elif [[ "$TARGET" == "aarch64" ]]; then
    build_arch "aarch64"
elif [[ "$TARGET" == "arm" ]]; then
    build_arch "arm"
fi

echo "Build complete!"
echo ""
echo "Binaries:"
ls -lh "$PROJECT_ROOT/bin"/mnn-server-*
echo ""
echo "Note: To create .deb package, use the GitHub Actions workflow on release."