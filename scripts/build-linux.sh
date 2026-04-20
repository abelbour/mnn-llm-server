#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "Building MNN LLM Server for Linux (x86_64)..."
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

mkdir -p "$PROJECT_ROOT/build"
cd "$PROJECT_ROOT/build"

cmake ../src \
    -DMNN_ROOT="$MNN_ROOT" \
    -DMNN_INCLUDE_DIR="$MNN_ROOT/include" \
    -DMNN_LLM_INCLUDE_DIR="$MNN_ROOT/transformers/llm/engine/include" \
    -DMNN_BUILD_DIR="$MNN_ROOT/build"

make -j$(nproc)

mkdir -p "$PROJECT_ROOT/bin"
cp mnn-server "$PROJECT_ROOT/bin/mnn-server-linux-x86_64"

echo "Build complete!"
echo "Binary: $PROJECT_ROOT/bin/mnn-server-linux-x86_64"
