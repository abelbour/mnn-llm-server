#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

show_usage() {
    echo "MNN LLM Server Build Script"
    echo ""
    echo "Usage: $0 [platform] [arch]"
    echo ""
    echo "Platforms:"
    echo "  linux    Build for native Linux (x86_64)"
    echo "  termux  Build for Termux (Android)"
    echo ""
    echo "Linux architectures:"
    echo "  x86_64  64-bit x86 (default)"
    echo ""
    echo "Termux architectures:"
    echo "  aarch64 64-bit ARM (most common)"
    echo "  arm     32-bit ARM"
    echo "  all     Both ARM architectures"
    echo ""
    echo "Examples:"
    echo "  $0 linux                    # Build for Linux x86_64"
    echo "  $0 termux aarch64          # Build for Termux aarch64"
    echo "  $0 termux arm              # Build for Termux arm"
    echo "  $0 termux all             # Build for both Termux architectures"
    exit 1
}

PLATFORM="${1:-}"
ARCH="${2:-}"

if [ -z "$PLATFORM" ]; then
    show_usage
fi

case "$PLATFORM" in
    linux)
        "$SCRIPT_DIR/build-linux.sh"
        ;;
    termux)
        "$SCRIPT_DIR/build-termux.sh" "${ARCH:-all}"
        ;;
    *)
        show_usage
        ;;
esac