#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BIN_DIR="$PROJECT_DIR/bin"
LIBS_DIR="$PROJECT_DIR/libs"
MODELS_DIR="$PROJECT_DIR/models"
LOGS_DIR="$PROJECT_DIR/logs"
WEB_DIR="$PROJECT_DIR/web"

DEFAULT_PORT=8000
CONFIG_FILE="$PROJECT_DIR/.config"

mkdir -p "$BIN_DIR" "$LOGS_DIR" "$MODELS_DIR" "$WEB_DIR"

load_config() {
    if [ -f "$CONFIG_FILE" ]; then
        source "$CONFIG_FILE"
    fi
    PORT="${PORT:-$DEFAULT_PORT}"
    MODEL="${MODEL:-Llama-3.2-1B-Instruct-MNN}"
}

save_config() {
    echo "PORT=$PORT" > "$CONFIG_FILE"
    echo "MODEL=$MODEL" >> "$CONFIG_FILE"
}

get_models() {
    if [ -d "$MODELS_DIR" ]; then
        ls -1 "$MODELS_DIR" 2>/dev/null | grep -v "^$" || echo ""
    else
        echo ""
    fi
}

get_model_path() {
    local model_name="$1"
    if [ -d "$MODELS_DIR/$model_name" ]; then
        echo "$MODELS_DIR/$model_name"
    else
        echo "$model_name"
    fi
}

build_server() {
    echo "Building MNN Server..."
    cd "$PROJECT_DIR/src"
    
    if [ ! -d "build" ]; then
        mkdir build
    fi
    
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    
    if [ -f "mnn-server" ]; then
        cp mnn-server "$BIN_DIR/"
        echo "Server built successfully!"
    else
        echo "Build failed!"
        exit 1
    fi
}

download_mnn_libs() {
    echo "Downloading pre-built MNN libraries..."
    echo "Note: This feature requires a download source."
    echo "Please build MNN from source or provide pre-built binaries."
    echo ""
    echo "To build MNN from source:"
    echo "1. Clone MNN: git clone https://github.com/alibaba/MNN.git"
    echo "2. Follow MNN build instructions"
    echo "3. Copy libMNN.so and libllm.so to $LIBS_DIR/"
}

start_server() {
    local model="$1"
    local port="$2"
    
    if [ -z "$model" ]; then
        model="$MODEL"
    fi
    if [ -z "$port" ]; then
        port="$PORT"
    fi
    
    if [ -z "$model" ]; then
        echo "Error: No model specified. Use -m flag or set MODEL in config."
        exit 1
    fi
    
    local model_path
    model_path=$(get_model_path "$model")
    
    if [ ! -d "$model_path" ] && [ ! -f "$model_path" ]; then
        echo "Error: Model not found: $model"
        echo "Available models:"
        list_models
        exit 1
    fi
    
    if [ -f "$BIN_DIR/mnn-server" ]; then
        cp "$BIN_DIR/mnn-server" "$PROJECT_DIR/mnn-server"
    elif [ -f "$PROJECT_DIR/mnn-server" ]; then
        echo "Using existing mnn-server"
    else
        echo "Error: mnn-server not found. Please build first."
        exit 1
    fi
    
    echo "Starting server..."
    echo "  Model: $model"
    echo "  Port: $port"
    
    pkill -f "mnn-server.*-p $port" 2>/dev/null || true
    sleep 1
    
    cd "$PROJECT_DIR"
    export LD_LIBRARY_PATH="$LIBS_DIR:$PROJECT_DIR:$LD_LIBRARY_PATH"
    
    if [ -d "$model_path" ]; then
        ./mnn-server -m "$model_path" -p "$port" > "$LOGS_DIR/server.log" 2>&1 &
    else
        ./mnn-server -m "$model_path" -p "$port" > "$LOGS_DIR/server.log" 2>&1 &
    fi
    
    sleep 3
    
    if curl -s "http://localhost:$port/health" > /dev/null 2>&1; then
        echo "Server started successfully!"
        echo "Web UI: http://localhost:$port/"
        echo "API: http://localhost:$port/v1/chat/completions"
    else
        echo "Server may have started. Check logs:"
        echo "  tail -f $LOGS_DIR/server.log"
    fi
}

stop_server() {
    local port="$1"
    if [ -z "$port" ]; then
        port="$PORT"
    fi
    
    echo "Stopping server on port $port..."
    pkill -f "mnn-server.*-p $port" 2>/dev/null
    
    if pgrep -f "mnn-server" > /dev/null; then
        echo "Server stopped."
    else
        echo "No server running."
    fi
}

check_status() {
    local port="$1"
    if [ -z "$port" ]; then
        port="$PORT"
    fi
    
    echo "=========================================="
    echo "   Server Status"
    echo "=========================================="
    echo ""
    
    if pgrep -f "mnn-server" > /dev/null; then
        echo "Status: RUNNING"
        echo "Port: $port"
        
        local health
        health=$(curl -s "http://localhost:$port/health" 2>/dev/null || echo '{"error": "Failed to connect"}')
        
        if command -v python3 > /dev/null 2>&1; then
            echo "$health" | python3 -m json.tool 2>/dev/null || echo "$health"
        else
            echo "$health"
        fi
    else
        echo "Status: STOPPED"
    fi
    
    echo ""
    echo "Available models:"
    list_models
}

list_models() {
    local models
    models=$(get_models)
    
    if [ -n "$models" ]; then
        local i=1
        while IFS= read -r model; do
            if [ -n "$model" ]; then
                echo "  $i. $model"
                i=$((i + 1))
            fi
        done <<< "$models"
    else
        echo "  No models found in $MODELS_DIR"
        echo "  Download models to this folder"
    fi
}

view_logs() {
    local lines="${1:-50}"
    
    if [ -f "$LOGS_DIR/server.log" ]; then
        tail -n "$lines" "$LOGS_DIR/server.log"
    else
        echo "No logs found."
    fi
}

show_help() {
    echo "MNN LLM Server - Interactive CLI"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --build, -b              Build the server"
    echo "  --build-mnn              Build MNN libraries"
    echo "  --build-all              Build everything"
    echo "  --start, -s [model]      Start server (optional: model name)"
    echo "  --stop, -x               Stop server"
    echo "  --status                 Check server status"
    echo "  --logs [lines]           View server logs (default: 50)"
    echo "  -m, --model <name>       Set model"
    echo "  -p, --port <number>      Set port (default: 8000)"
    echo "  --menu, -M               Show interactive menu"
    echo "  --help, -h               Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 --build                # Build server"
    echo "  $0 -s                     # Start with default model"
    echo "  $0 -s -m Qwen2.5-Coder-1.5B-Instruct-MNN -p 8001"
    echo "  $0 -x                     # Stop server"
    echo "  $0 --status               # Check status"
    echo ""
    echo "Without options, shows interactive menu."
}

show_menu() {
    load_config
    
    local models
    models=$(get_models)
    local model_count
    model_count=$(echo "$models" | grep -c "^" || echo 0)
    
    while true; do
        echo ""
        echo "=========================================="
        echo "   MNN LLM Server - Interactive Menu"
        echo "=========================================="
        echo ""
        echo "1. Build          - Compile server & MNN"
        echo "2. MNN Options   - MNN library submenu"
        echo "3. Models         - Manage models"
        echo "4. Start Server  - Run the server"
        echo "5. Server Options - Configure server"
        echo "6. Stop Server   - Kill running server"
        echo "7. View Logs     - Show server logs"
        echo "8. Check Status  - Server health"
        echo "9. Exit"
        echo ""
        
        read -p "Choice: " choice
        
        case "$choice" in
            1)
                build_server
                ;;
            2)
                show_mnn_menu
                ;;
            3)
                show_models_menu
                ;;
            4)
                start_server_menu
                ;;
            5)
                show_server_options_menu
                ;;
            6)
                stop_server
                ;;
            7)
                echo ""
                echo "Showing last 50 lines of logs (Ctrl+C to exit):"
                tail -f "$LOGS_DIR/server.log" 2>/dev/null || echo "No logs found."
                ;;
            8)
                check_status
                ;;
            9)
                echo "Goodbye!"
                exit 0
                ;;
            *)
                echo "Invalid choice. Please try again."
                ;;
        esac
    done
}

show_mnn_menu() {
    while true; do
        echo ""
        echo "=========================================="
        echo "   MNN Options"
        echo "=========================================="
        echo ""
        echo "1. Build MNN Libraries   - Compile libMNN.so & libllm.so"
        echo "2. Download Pre-built   - Download pre-built binaries"
        echo "3. Back to Main Menu"
        echo ""
        
        read -p "Choice: " choice
        
        case "$choice" in
            1)
                echo "Building MNN libraries..."
                echo "Note: This requires MNN source. Clone from:"
                echo "https://github.com/alibaba/MNN"
                ;;
            2)
                download_mnn_libs
                ;;
            3)
                break
                ;;
            *)
                echo "Invalid choice."
                ;;
        esac
    done
}

show_models_menu() {
    while true; do
        echo ""
        echo "=========================================="
        echo "   Models"
        echo "=========================================="
        echo ""
        echo "Available models in $MODELS_DIR:"
        echo ""
        list_models
        echo ""
        echo "1. Refresh Models  - Scan for new models"
        echo "2. Set Default    - Set default model"
        echo "3. Back to Main Menu"
        echo ""
        
        read -p "Choice: " choice
        
        case "$choice" in
            1)
                echo "Scanning for models..."
                list_models
                ;;
            2)
                echo "Current default: $MODEL"
                echo ""
                list_models
                echo ""
                read -p "Enter model number or name: " model_input
                
                if [[ "$model_input" =~ ^[0-9]+$ ]]; then
                    local i=1
                    while IFS= read -r m; do
                        if [ -n "$m" ] && [ "$i" -eq "$model_input" ]; then
                            MODEL="$m"
                            break
                        fi
                        i=$((i + 1))
                    done <<< "$models"
                else
                    MODEL="$model_input"
                fi
                
                save_config
                echo "Default model set to: $MODEL"
                ;;
            3)
                break
                ;;
            *)
                echo "Invalid choice."
                ;;
        esac
    done
}

start_server_menu() {
    load_config
    
    local models
    models=$(get_models)
    
    echo ""
    echo "=========================================="
    echo "   Start Server"
    echo "=========================================="
    echo ""
    echo "Available models:"
    echo ""
    
    local i=1
    while IFS= read -r model; do
        if [ -n "$model" ]; then
            echo "  $i. $model"
            i=$((i + 1))
        fi
    done <<< "$models"
    
    echo "  $i. Custom path"
    echo ""
    echo "Default: $MODEL"
    echo ""
    read -p "Select model [1-$i] or enter name: " model_input
    
    local selected_model="$MODEL"
    
    if [[ "$model_input" =~ ^[0-9]+$ ]]; then
        local idx=1
        while IFS= read -r m; do
            if [ -n "$m" ] && [ "$idx" -eq "$model_input" ]; then
                selected_model="$m"
                break
            fi
            idx=$((idx + 1))
        done <<< "$models"
    elif [ -n "$model_input" ]; then
        selected_model="$model_input"
    fi
    
    echo ""
    read -p "Port [$PORT]: " port_input
    if [ -n "$port_input" ]; then
        PORT="$port_input"
    fi
    
    save_config
    start_server "$selected_model" "$PORT"
}

show_server_options_menu() {
    load_config
    
    while true; do
        echo ""
        echo "=========================================="
        echo "   Server Options"
        echo "=========================================="
        echo ""
        echo "Current settings:"
        echo "  Model: $MODEL"
        echo "  Port: $PORT"
        echo ""
        echo "1. Select Model     - Choose default model"
        echo "2. Set Port         - Change server port"
        echo "3. Back to Main Menu"
        echo ""
        
        read -p "Choice: " choice
        
        case "$choice" in
            1)
                list_models
                echo ""
                read -p "Enter model name: " MODEL
                save_config
                echo "Model set to: $MODEL"
                ;;
            2)
                read -p "Enter port: " PORT
                save_config
                echo "Port set to: $PORT"
                ;;
            3)
                break
                ;;
            *)
                echo "Invalid choice."
                ;;
        esac
    done
}

# Main
load_config

# Parse command line arguments
if [ $# -eq 0 ]; then
    show_menu
    exit 0
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build|-b)
            build_server
            ;;
        --build-mnn)
            echo "Building MNN libraries..."
            echo "Note: This requires MNN source code."
            ;;
        --build-all|--all)
            build_server
            ;;
        --start|-s)
            shift
            local model=""
            if [ -n "$1" ] && [[ ! "$1" =~ ^- ]]; then
                model="$1"
            fi
            start_server "$model" ""
            ;;
        --stop|-x)
            stop_server
            ;;
        --status)
            check_status
            ;;
        --logs)
            shift
            local lines="${1:-50}"
            view_logs "$lines"
            ;;
        -m|--model)
            shift
            MODEL="$1"
            save_config
            ;;
        -p|--port)
            shift
            PORT="$1"
            save_config
            ;;
        --menu|-M)
            show_menu
            ;;
        --help|-h)
            show_help
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
    shift
done
