# MNN LLM Server

A high-performance, self-hosted LLM inference server built with Alibaba's MNN framework. Provides an OpenAI-compatible API with real-time streaming support and an integrated web UI.

## Features

- **Persistent Model Loading**: Models stay in memory for fast inference (~1-2s response after first load)
- **OpenAI-Compatible API**: Works with any OpenAI-compatible client
- **Real-time Streaming**: Token-by-token streaming via Server-Sent Events (SSE)
- **Web UI**: Built-in chat interface with markdown rendering
- **Multiple GPU Backends**: Auto-detection (OpenCL → Vulkan → CPU)
- **Interactive CLI**: Menu-driven or direct CLI commands
- **Dynamic Model Detection**: Automatically detects models in the models folder

## Quick Start

### Termux (Android)

```bash
# Download the default package (includes all backends)
wget https://github.com/abelbour/mnn-llm-server/releases/latest/download/mnn-llm-server-cpu_all.deb

# Install
dpkg -i mnn-llm-server-cpu_all.deb
apt install -f  # Install dependencies

# Download a model
mnn-download-model Llama-3.2-1B-Instruct-MNN

# Start server
mnn-server
```

Then open http://localhost:8000 in your browser.

### Linux (ARM)

```bash
# Download desired variant
wget https://github.com/abelbour/mnn-llm-server/releases/latest/download/mnn-llm-server-vulkan-aarch64.zip

# Extract
unzip mnn-llm-server-vulkan-aarch64.zip
cd mnn-llm-server-vulkan-aarch64

# Download a model
./scripts/download-model.sh Llama-3.2-1B-Instruct-MNN

# Start server
./mnn-server
```

## Installation

### Termux Packages

| Package | Backends | Description |
|---------|----------|--------------|
| `mnn-llm-server-cpu_all.deb` | OpenCL + Vulkan + CPU | Default, auto-detects best backend |
| `mnn-llm-server-opencl_aarch64.deb` | OpenCL | OpenCL only |
| `mnn-llm-server-vulkan_aarch64.deb` | Vulkan | Vulkan only |

**Install:**
```bash
dpkg -i <package>.deb
apt install -f
```

### Linux Zip Packages

| Package | Backends | Architecture |
|---------|----------|--------------|
| `mnn-llm-server-opencl-aarch64.zip` | OpenCL | 64-bit ARM |
| `mnn-llm-server-vulkan-aarch64.zip` | Vulkan | 64-bit ARM |
| `mnn-llm-server-cpu-aarch64.zip` | CPU | 64-bit ARM |
| `mnn-llm-server-cpu-arm.zip` | CPU | 32-bit ARM |

**Extract:**
```bash
unzip <package>.zip
cd <package>
./mnn-server
```

## GPU Backend Selection

### Auto-Detection (Default)

The server automatically detects the best available backend in this order:
```
OpenCL → Vulkan → CPU
```

### Manual Selection

**Termux:**
```bash
# Set environment variable before running
MNN_BACKEND=opencl mnn-server
MNN_BACKEND=vulkan mnn-server
MNN_BACKEND=cpu mnn-server
```

**Linux:**
```bash
MNN_BACKEND=vulkan ./mnn-server
```

### Check Available Backend

```bash
# Run with specific backend to test
mnn-server-cpu --help  # or --test flag
```

## CLI Usage

### Direct Commands

```bash
# Build
./scripts/start.sh --build

# Start
./scripts/start.sh --start
./scripts/start.sh -s -m "model-name"

# Stop
./scripts/start.sh --stop

# Status
./scripts/start.sh --status

# Logs
./scripts/start.sh --logs

# Interactive menu
./scripts/start.sh
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

### Download Models

**Termux:**
```bash
mnn-download-model Llama-3.2-1B-Instruct-MNN
mnn-download-model Qwen2.5-Coder-1.5B-Instruct-MNN
```

**Linux:**
```bash
./scripts/download-model.sh Llama-3.2-1B-Instruct-MNN
```

**Manual:**
1. Download MNN model from HuggingFace
2. Extract to `models/` folder

### Available Models

| Model | Size | Notes |
|-------|------|-------|
| Llama-3.2-1B-Instruct-MNN | ~1.2GB | Recommended for mobile |
| Qwen2.5-Coder-1.5B-Instruct-MNN | ~3GB | Coding tasks |
| phi-2-MNN | ~2.7GB | Microsoft Phi-2 |

Download from: https://huggingface.co/models?search=mnn

### Model Folder Structure

```
models/
└── Llama-3.2-1B-Instruct-MNN/
    ├── config.json
    ├── llm.mnn
    └── tokenizer.txt
```

## Configuration

### URL Parameters (Web UI)

| Parameter | Description | Default |
|-----------|-------------|---------|
| `apiUrl` | API endpoint | `http://localhost:8000/v1` |
| `model` | Model name | Auto-detected |
| `apiKey` | API key | `dummy` |

Examples:
```
http://localhost:8000/
http://localhost:8000/?model=phi-2
```

### Server Options

```bash
# Custom port
mnn-server -p 8080

# Custom model
mnn-server -m "phi-2-MNN"

# Custom models directory
mnn-server -models /path/to/models
```

## Project Structure

```
mnn-llm-server/
├── README.md
├── LICENSE
├── .gitignore
├── .github/
│   └── workflows/
│       └── build.yml
├── src/
│   ├── main.cpp
│   └── CMakeLists.txt
├── web/
│   └── index.html
├── scripts/
│   ├── start.sh
│   ├── build.sh
│   ├── model-download.sh
│   └── install-deps.sh
├── models/
└── logs/
```

## Troubleshooting

### Server won't start

1. Check if model is downloaded:
   ```bash
   ls models/
   ```

2. Check logs:
   ```bash
   ./scripts/start.sh --logs
   ```

3. Check port availability:
   ```bash
   ./scripts/start.sh --status
   ```

### Slow first response

The first request may take 30+ seconds as the model loads into memory. Subsequent requests should be much faster (~1-2s).

### No GPU acceleration

Check which backend is being used:
```bash
# Try specific backend
MNN_BACKEND=opencl mnn-server
# or
MNN_BACKEND=vulkan mnn-server
```

### Web UI not loading

- Make sure firewall allows the port
- Check if server is running: `./scripts/start.sh --status`

## Building from Source

### Termux Build

```bash
# Install dependencies
pkg install build-essential cmake ninja git curl wget python3

# Clone and build
git clone https://github.com/abelbour/mnn-llm-server.git
cd mnn-llm-server
./scripts/build.sh termux aarch64
```

### Linux Build

```bash
# Install dependencies
sudo apt install build-essential cmake ninja git curl wget python3

# Clone and build
git clone https://github.com/abelbour/mnn-llm-server.git
cd mnn-llm-server
./scripts/build.sh linux
```

## GitHub Releases

Pre-built binaries and packages are available on GitHub releases.

### Release Assets

Each release includes:

| Asset | Type | Platform |
|-------|------|----------|
| `mnn-llm-server-cpu_all.deb` | .deb | Termux (all backends) |
| `mnn-llm-server-opencl_aarch64.deb` | .deb | Termux (OpenCL) |
| `mnn-llm-server-vulkan_aarch64.deb` | .deb | Termux (Vulkan) |
| `mnn-llm-server-opencl-aarch64.zip` | .zip | Linux |
| `mnn-llm-server-vulkan-aarch64.zip` | .zip | Linux |
| `mnn-llm-server-cpu-aarch64.zip` | .zip | Linux |
| `mnn-llm-server-cpu-arm.zip` | .zip | Linux |

### Creating a Release

1. Go to https://github.com/abelbour/mnn-llm-server/releases/new
2. Click "Draft a new release"
3. Set tag version (e.g., v1.0.0)
4. Add title and description
5. Click "Publish release"

GitHub Actions will automatically build and upload all packages.

## License

MIT License - See [LICENSE](LICENSE) file.

## Credits

- [Alibaba MNN](https://github.com/alibaba/MNN) - High-performance deep learning framework
- [xsukax WebUI](https://github.com/xsukax/xsukax-Custom-OpenAI-WebUI) - Beautiful OpenAI-compatible web interface

## Support

If you find this project useful, please consider:
- Starring the repository
- Reporting issues
- Contributing improvements