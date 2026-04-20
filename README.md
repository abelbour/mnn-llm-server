# MNN LLM Server

A high-performance, self-hosted LLM inference server built with Alibaba's MNN framework. Provides an OpenAI-compatible API with real-time streaming support and an integrated web UI.

## Features

- **Persistent Model Loading**: Models stay in memory for fast inference (~1-2s response after first load)
- **OpenAI-Compatible API**: Works with any OpenAI-compatible client
- **Real-time Streaming**: Token-by-token streaming via Server-Sent Events (SSE)
- **Web UI**: Built-in chat interface with markdown rendering
- **Multiple Model Support**: Switch between models easily
- **Interactive CLI**: Menu-driven or direct CLI commands
- **Dynamic Model Detection**: Automatically detects models in the models folder

## Quick Start

```bash
# Clone the repository
git clone https://github.com/yourusername/mnn-llm-server.git
cd mnn-llm-server

# Install dependencies (optional, for first-time setup)
./scripts/install-deps.sh

# Build the server
./scripts/start.sh --build

# Download models to ./models/ folder
# (see Models section below)

# Start the server
./scripts/start.sh --start
```

Then open http://localhost:8000 in your browser.

## CLI Usage

### Direct Commands

```bash
# Build
./scripts/start.sh --build              # Build server
./scripts/start.sh --build --all        # Build everything

# Start
./scripts/start.sh --start              # Start with default model
./scripts/start.sh -s                  # Short form
./scripts/start.sh -s -m "model-name"  # Specific model

# Stop
./scripts/start.sh --stop             # Stop server
./scripts/start.sh -x                 # Short form

# Status
./scripts/start.sh --status           # Check if server is running

# Logs
./scripts/start.sh --logs              # View last 50 lines
./scripts/start.sh --logs 100         # View last 100 lines

# Interactive menu
./scripts/start.sh                   # Show interactive menu
./scripts/start.sh -M                 # Same as above
```

### Interactive Menu

```
$ ./scripts/start.sh

==========================================
   MNN LLM Server - Interactive Menu
==========================================

1. Build          - Compile server & MNN
2. MNN Options   - MNN library submenu
3. Models        - Manage/download models
4. Start Server - Run the server
5. Server Options - Configure server
6. Stop Server  - Kill running server
7. View Logs    - Show server logs
8. Check Status - Server health
9. Exit
```

### MNN Options Submenu (Option 2)

```
==========================================
   MNN Options
==========================================

1. Build MNN Libraries   - Compile libMNN.so & libllm.so
2. Download Pre-built   - Download pre-built binaries
3. Back to Main Menu
```

### Server Options Submenu (Option 5)

```
==========================================
   Server Options
==========================================

1. Select Model     - Choose default model
2. Set Port        - Configure port
3. Back to Main Menu
```

## Configuration

### URL Parameters (Web UI)

The web UI supports URL parameters to pre-configure settings:

| Parameter | Description | Default |
|-----------|-------------|---------|
| `apiUrl` | API endpoint | `http://localhost:8000/v1` |
| `model` | Model name | Auto-detected from folder |
| `apiKey` | API key | `dummy` |

Examples:
```
http://localhost:8000/                      # Default (first model found)
http://192.168.1.100:8000/?model=phi-2       # Using phi-2 model
```

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web UI (index.html) |
| `/health` | GET | Server health |
| `/v1/models` | GET | List available models |
| `/v1/chat/completions` | POST | Chat completion (streaming supported) |

### Chat API Example

```bash
# Non-streaming
curl -X POST http://localhost:8000/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "Llama-3.2-1B-Instruct",
    "messages": [{"role": "user", "content": "Hello"}]
  }'

# Streaming
curl -N -X POST http://localhost:8000/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "Llama-3.2-1B-Instruct",
    "messages": [{"role": "user", "content": "Hello"}],
    "stream": true
  }'
```

## Models

### Adding Models

1. Create a folder in `./models/`:
   ```
   ./models/Llama-3.2-1B-Instruct-MNN/
   ```

2. Add model files:
   - `config.json`
   - `llm.mnn`
   - `tokenizer.txt`

### Download Models

Place MNN models in the `./models/` folder. Models can be downloaded from:

- [Hugging Face](https://huggingface.co/models?search=mnn) - Search for "MNN" models
- [MNN Model Zoo](https://github.com/alibaba/MNN/blob/master/benchmark/DOWNLOAD.md)

### Available Models

The server automatically detects models in the `./models/` folder. Simply add a model folder and restart.

Commonly used models:

- `Llama-3.2-1B-Instruct-MNN` (~1.2GB)
- `Qwen2.5-Coder-1.5B-Instruct-MNN` (~3GB)
- `Qwen3.5-4B-MNN` (~8GB)
- `phi-2-MNN` (~2.7GB)

## Project Structure

```
mnn-llm-server/
├── README.md              # This file
├── LICENSE               # MIT License
├── .gitignore            # Git ignore rules
├── src/                  # Server source code
│   ├── main.cpp          # Server (dynamic paths)
│   └── CMakeLists.txt   # Build config
├── web/                  # Web UI
│   └── index.html        # xsukax-based chat UI
├── scripts/              # CLI scripts
│   ├── start.sh        # Main CLI (interactive + CLI)
│   ├── build.sh        # Build script
│   └── install-deps.sh # Dependency installer
├── bin/                  # Compiled binaries
│   └── mnn-server
├── libs/                 # MNN libraries
│   ├── libMNN.so       # MNN core
│   └── libllm.so       # LLM engine
├── models/              # Model files (download here)
│   └── [model folders]
└── logs/               # Server logs
    └── server.log
```

## Requirements

### Build Requirements

- C++ compiler (GCC/Clang)
- CMake 3.16+
- Ninja build (optional, faster)
- Git

### Runtime Requirements

- Linux / Termux (Android) / macOS
- libMNN.so (MNN core library)
- libllm.so (LLM engine)

### Installing Dependencies

```bash
# Interactive installer
./scripts/install-deps.sh

# Or manually:
# Debian/Ubuntu:
sudo apt install build-essential cmake ninja-build git curl wget python3 tmux

# Termux (Android):
pkg install build-essential cmake ninja git curl wget python3 tmux

# macOS:
brew install cmake ninja python3 tmux git curl wget
```

## Building from Source

### Quick Build

```bash
# Build for current platform
./scripts/build.sh linux

# Build for Termux (all architectures)
./scripts/build.sh termux all

# Build for specific Termux architecture
./scripts/build.sh termux aarch64
./scripts/build.sh termux arm
```

### Build Output

After building, binaries are placed in `bin/`:

| Binary | Platform |
|--------|----------|
| `bin/mnn-server-linux-x86_64` | Linux x86_64 |
| `bin/mnn-server-aarch64` | Termux 64-bit ARM |
| `bin/mnn-server-arm` | Termux 32-bit ARM |

### Building MNN Libraries

Building MNN from source is required only if you need custom optimizations.

```bash
# Clone MNN
git clone https://github.com/alibaba/MNN.git

# Build MNN core
cd MNN
./backend/cpu/build.sh

# Build LLM engine
./tools/llm/build.sh

# Copy libraries to libs folder
cp build/libMNN.so /path/to/mnn-llm-server/libs/
cp build/libllm.so /path/to/mnn-llm-server/libs/
```

## Troubleshooting

### Server won't start

1. Check if port is already in use:
   ```bash
   ./scripts/start.sh --status
   ```

2. Check logs:
   ```bash
   ./scripts/start.sh --logs
   ```

3. Verify models exist:
   ```bash
   ls models/
   ```

### Slow first response

The first request may take 30+ seconds as the model loads into memory. Subsequent requests should be much faster (~1-2s).

### Web UI not loading

- Make sure firewall allows the port
- Check if server is actually running: `./scripts/start.sh --status`

### Model not found

1. Verify model is in `./models/` folder
2. Check model folder structure:
   ```
   models/
   └── Llama-3.2-1B-Instruct-MNN/
       ├── config.json
       ├── llm.mnn
       └── tokenizer.txt
   ```

## License

MIT License - See [LICENSE](LICENSE) file.

## GitHub Releases

Pre-built binaries and Termux packages are available on GitHub releases.

### Release Assets

Each release includes:

| Asset | Description |
|-------|-------------|
| `mnn-llm-server-aarch64` | Binary for 64-bit ARM |
| `mnn-llm-server-arm` | Binary for 32-bit ARM |
| `mnn-llm-server_X.X.X_all.deb` | Termux package (auto-selects correct binary) |

### Building Releases

1. Create a new release on GitHub with a tag (e.g., `v1.0.0`)
2. GitHub Actions will automatically build both architectures
3. Binaries and .deb package will be uploaded as release assets
4. Users can download the .deb package and install via `dpkg -i`

### Manual Build (without release)

```bash
# Build for both Termux architectures
./scripts/build.sh termux all
```

## Credits

- [Alibaba MNN](https://github.com/alibaba/MNN) - High-performance deep learning framework
- [xsukax WebUI](https://github.com/xsukax/xsukax-Custom-OpenAI-WebUI) - Beautiful OpenAI-compatible web interface

## Support

If you find this project useful, please consider:
- ⭐ Starring the repository
- 🐛 Reporting issues
- 💡 Contributing improvements