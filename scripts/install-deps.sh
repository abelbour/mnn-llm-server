#!/bin/bash

echo "=========================================="
echo "   MNN LLM Server - Install Dependencies"
echo "=========================================="
echo ""

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    OS="unknown"
fi

echo "Detected OS: $OS"
echo ""

install_apt() {
    echo "Installing dependencies for Debian/Ubuntu..."
    echo ""
    
    sudo apt update
    
    sudo apt install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        curl \
        wget \
        python3 \
        tmux
    
    echo ""
    echo "Dependencies installed!"
}

install_termux() {
    echo "Installing dependencies for Termux..."
    echo ""
    
    pkg update
    pkg install -y \
        build-essential \
        cmake \
        ninja \
        git \
        curl \
        wget \
        python3 \
        tmux
    
    echo ""
    echo "Dependencies installed!"
}

install_alpine() {
    echo "Installing dependencies for Alpine..."
    echo ""
    
    apk add --no-cache \
        build-base \
        cmake \
        ninja \
        git \
        curl \
        wget \
        python3 \
        tmux
    
    echo ""
    echo "Dependencies installed!"
}

install_arch() {
    echo "Installing dependencies for Arch Linux..."
    echo ""
    
    sudo pacman -Sy --noconfirm \
        base-devel \
        cmake \
        ninja \
        git \
        curl \
        wget \
        python3 \
        tmux
    
    echo ""
    echo "Dependencies installed!"
}

install_macos() {
    echo "Installing dependencies for macOS..."
    echo ""
    
    if command -v brew >/dev/null 2>&1; then
        brew install cmake ninja python3 tmux git curl wget
    else
        echo "Homebrew not found. Install from: https://brew.sh"
    fi
    
    echo ""
    echo "Dependencies installed!"
}

install_generic() {
    echo "Installing generic dependencies..."
    echo ""
    
    echo "Please install the following packages manually:"
    echo "  - build-essential / base-devel / Development Tools"
    echo "  - cmake"
    echo "  - ninja-build / ninja"
    echo "  - git"
    echo "  - curl"
    echo "  - wget"
    echo "  - python3"
    echo "  - tmux"
}

check_install() {
    local cmd="$1"
    local name="$2"
    
    if command -v "$cmd" >/dev/null 2>&1; then
        echo "  ✓ $name"
        return 0
    else
        echo "  ✗ $name (not found)"
        return 1
    fi
}

echo "Checking installed dependencies..."
echo ""

check_install gcc "GCC/Clang"
check_install cmake "CMake"
check_install ninja "Ninja"
check_install git "Git"
check_install curl "cURL"
check_install wget "wget"
check_install python3 "Python3"
check_install tmux "Tmux"

echo ""
echo "=========================================="
echo "Would you like to install missing dependencies? [y/N]"
read -r response

if [[ "$response" =~ ^[Yy]$ ]]; then
    case "$OS" in
        debian|ubuntu|linuxmint)
            install_apt
            ;;
        termux)
            install_termux
            ;;
        alpine)
            install_alpine
            ;;
        arch|manjaro)
            install_arch
            ;;
        darwin)
            install_macos
            ;;
        *)
            install_generic
            ;;
    esac
else
    echo "Skipping dependency installation."
fi

echo ""
echo "Setup complete!"
echo ""
echo "Next steps:"
echo "1. Run: ./scripts/build.sh --server"
echo "2. Download models to ./models/"
echo "3. Run: ./scripts/start.sh --start"
