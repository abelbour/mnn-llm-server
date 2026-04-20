#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
MODELS_DIR="$PROJECT_ROOT/models"

list_models() {
    echo "Searching for MNN models on HuggingFace..."
    echo ""

    if command -v curl &> /dev/null; then
        MODELS=$(curl -s "https://huggingface.co/api/models?search=mnn&filter=transformers&sort=downloads&direction=-1&limit=20" | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    for model in data:
        print(model.get('id', ''))
except:
    print('ERROR')
" 2>/dev/null || echo "")

        if [ -n "$MODELS" ] && [ "$MODELS" != "ERROR" ]; then
            echo "Available MNN models:"
            echo ""
            for model in $MODELS; do
                echo "  $model"
            done
            echo ""
            echo "To download: $0 <model-name>"
            return 0
        fi
    fi

    echo "Could not fetch model list automatically."
    echo ""
    echo "Please visit https://huggingface.co/models?search=mnn"
    echo ""
    echo "Common models:"
    echo "  MNN/Llama-3.2-1B-Instruct-MNN"
    echo "  MNN/Qwen2.5-Coder-1.5B-Instruct-MNN"
    echo "  MNN/phi-2-MNN"
    echo "  MNN/Qwen2.5-7B-Instruct-MNN"
    echo "  MNN/phi-3.5-mini-instruct-MNN"
    echo ""
    echo "Usage: $0 <model-id>"
    echo ""
    echo "Example:"
    echo "  $0 MNN/Llama-3.2-1B-Instruct-MNN"
}

show_usage() {
    echo "MNN Model Download Script"
    echo ""
    echo "Usage: $0 [options] [model-name]"
    echo ""
    echo "Options:"
    echo "  --list          List available MNN models"
    echo "  -h, --help     Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 --list                    # List available models"
    echo "  $0 MNN/Llama-3.2-1B-Instruct-MNN"
    echo "  $0 Qwen2.5-Coder-1.5B-Instruct-MNN"
    exit 1
}

if [ "$1" = "--list" ] || [ "$1" = "-l" ]; then
    list_models
    exit 0
fi

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_usage
fi

if [ -z "$1" ]; then
    show_usage
fi

MODEL="$1"

normalize_model() {
    local model="$1"
    case "$model" in
        *-MNN)
            echo "MNN/$model"
            ;;
        MNN/*)
            echo "$model"
            ;;
        *)
            echo "MNN/$model"
            ;;
    esac
}

REPO=$(normalize_model "$MODEL")

echo "Downloading $MODEL..."
echo "Repository: https://huggingface.co/$REPO"
echo ""

mkdir -p "$MODELS_DIR"
cd "$MODELS_DIR"

MODEL_DIR="$MODEL"
if [[ "$MODEL" == MNN/* ]]; then
    MODEL_DIR=$(basename "$MODEL")
fi

if [ -d "$MODEL_DIR" ]; then
    echo "Model already exists: $MODEL_DIR"
    read -p "Re-download? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Skipping download."
        exit 0
    fi
    rm -rf "$MODEL_DIR"
fi

echo "Installing huggingface-hub..."
pip install huggingface-hub -q 2>/dev/null || true

echo "Downloading from HuggingFace..."
echo ""

python3 -c "
import os
from huggingface_hub import snapshot_download

try:
    snapshot_download(
        repo_id='$REPO',
        local_dir='$MODEL_DIR',
        local_dir_use_symlinks=False
    )
    print('Download complete!')
except Exception as e:
    print(f'Error: {e}')
    print('')
    print('Possible causes:')
    print('1. Model requires license acceptance at https://huggingface.co/$REPO')
    print('2. Model does not exist')
    print('')
    print('Manual download:')
    print('1. Go to https://huggingface.co/$REPO')
    print('2. Download files manually')
    print('3. Extract to ./models/$MODEL_DIR/')
    exit(1)
"

echo ""
echo "Download complete!"
echo "Model location: $MODELS_DIR/$MODEL_DIR"
echo ""
echo "To start the server:"
echo "  mnn-server -m $MODEL_DIR"