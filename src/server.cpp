#include "server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <csignal>
#include <algorithm>
#include <thread>

std::string getExeDir() {
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return std::filesystem::path(buf).parent_path().string();
    }
    return ".";
}

// MnnServer implementation

MnnServer::MnnServer(const std::string& modelsDir)
    : modelsDir_(modelsDir), activeModel_("") {}

MnnServer::~MnnServer() {
    if (llm_) {
        MNN::Transformer::Llm::destroy(llm_);
    }
}

bool MnnServer::loadModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isLoaded_ && modelPath == modelPath_) {
        std::cout << "Model already loaded: " << modelPath << std::endl;
        return true;
    }

    std::string configPath = modelPath;
    if (configPath.empty()) {
        configPath = getExeDir() + "/models/default";
    }
    if (configPath.back() != '/') {
        configPath += "/";
    }
    configPath += "config.json";

    std::cout << "Loading model from: " << configPath << std::endl;

    if (llm_) {
        MNN::Transformer::Llm::destroy(llm_);
        llm_ = nullptr;
        isLoaded_ = false;
    }

    llm_ = MNN::Transformer::Llm::createLLM(configPath);
    if (!llm_) {
        std::cerr << "Failed to create LLM instance" << std::endl;
        return false;
    }

    llm_->set_config(R"({"async": false, "thread": 4})");

    if (!llm_->load()) {
        std::cerr << "Failed to load model" << std::endl;
        MNN::Transformer::Llm::destroy(llm_);
        llm_ = nullptr;
        return false;
    }

    modelPath_ = modelPath;
    activeModel_ = getModelName(modelPath);
    isLoaded_ = true;
    modelsCached_ = false;

    std::cout << "Model loaded successfully: " << activeModel_ << std::endl;
    return true;
}

std::string MnnServer::getModelName(const std::string& path) const {
    std::string name = path;
    size_t pos = name.rfind('/');
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    if (name.find("-MNN") != std::string::npos) {
        name = name.substr(0, name.find("-MNN"));
    }
    return name;
}

std::string MnnServer::chat(const std::string& prompt, int maxTokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    stopRequested_ = false;

    if (!llm_ || !isLoaded_) {
        return "Error: No model loaded";
    }

    std::stringstream output;
    llm_->response(prompt, &output, nullptr, maxTokens);

    std::string result = output.str();
    return result;
}

void MnnServer::stopStreaming() {
    stopRequested_ = true;
}

void MnnServer::chatStreaming(const std::string& prompt, int maxTokens,
                               std::function<void(const std::string&, bool)> onToken) {
    std::lock_guard<std::mutex> lock(mutex_);
    stopRequested_ = false;

    if (!llm_ || !isLoaded_) {
        onToken("Error: No model loaded", true);
        return;
    }

    // Use stringstream to capture output token by token
    std::ostringstream outputStream;
    llm_->response(prompt, &outputStream, "<eop>", maxTokens);
    std::string fullOutput = outputStream.str();
    // Send full output as a single token
    if (!fullOutput.empty()) {
        onToken(fullOutput, false);
    }
    onToken("", true);
}

const std::string& MnnServer::getActiveModel() const {
    return activeModel_;
}

bool MnnServer::isLoaded() const {
    return isLoaded_;
}

std::string MnnServer::scanModels() {
    if (modelsCached_) return cachedModels_;

    json modelList = json::array();
    std::vector<std::string> files;

    DIR* dir = opendir(modelsDir_.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir))) {
            std::string name = entry->d_name;
            if (name.find("-MNN") != std::string::npos) {
                files.push_back(name);
            }
        }
        closedir(dir);
    }

    for (const auto& f : files) {
        std::string id = f;
        size_t pos = id.find("-MNN");
        if (pos != std::string::npos) {
            id = id.substr(0, pos);
        }
        if (id.find("-Instruct") != std::string::npos) {
            size_t iPos = id.find("-Instruct");
            id = id.substr(0, iPos);
        }
        modelList.push_back({
            {"id", id},
            {"object", "model"},
            {"owned_by", "local"}
        });
    }

    json result = {
        {"object", "list"},
        {"data", modelList}
    };
    cachedModels_ = result.dump();
    modelsCached_ = true;
    return cachedModels_;
}

const std::string& MnnServer::getWebHtml(const std::string& webPath) {
    if (!htmlCached_) {
        std::ifstream file(webPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            cachedHtml_ = buffer.str();
        }
        htmlCached_ = true;
    }
    return cachedHtml_;
}

// Helper functions

static std::string parseMessages(const json& messages) {
    std::string fullPrompt;
    std::string systemPrompt;
    const int MAX_CONTEXT_CHARS = 8000 * 4; // rough estimate: 8k tokens * 4 chars/token

    for (const auto& msg : messages) {
        std::string role = msg.value("role", "user");
        std::string content = msg.value("content", "");

        if (role == "system") {
            systemPrompt = content;
        }
    }

    if (!systemPrompt.empty()) {
        fullPrompt += "System: " + systemPrompt + "\n\n";
    }

    for (const auto& msg : messages) {
        std::string role = msg.value("role", "user");
        std::string content = msg.value("content", "");

        if (role == "system") continue;

        if (role == "user") {
            fullPrompt += "User: " + content + "\n\n";
        } else if (role == "assistant") {
            fullPrompt += "Assistant: " + content + "\n\n";
        }
    }

    fullPrompt += "Assistant:";

    // Truncate from the beginning if exceeding context window
    if ((int)fullPrompt.size() > MAX_CONTEXT_CHARS) {
        fullPrompt = fullPrompt.substr(fullPrompt.size() - MAX_CONTEXT_CHARS);
        size_t firstNewline = fullPrompt.find('\n');
        if (firstNewline != std::string::npos) {
            fullPrompt = fullPrompt.substr(firstNewline + 1);
        }
    }

    return fullPrompt;
}

static std::string buildChatCompletion(const std::string& id,
                              const std::string& model,
                              const std::string& content,
                              const std::string& finishReason = "stop") {
    json result = {
        {"id", id},
        {"object", "chat.completion"},
        {"created", (int)time(nullptr)},
        {"model", model},
        {"choices", json::array({
            {
                {"index", 0},
                {"message", {
                    {"role", "assistant"},
                    {"content", content}
                }},
                {"finish_reason", finishReason}
            }
        })},
        {"usage", {
            {"prompt_tokens", 0},
            {"completion_tokens", 0},
            {"total_tokens", 0}
        }}
    };
    return result.dump();
}

static std::string buildChatCompletionChunk(const std::string& id,
                               const std::string& model,
                               const std::string& content,
                               int choiceIndex) {
    json result = {
        {"id", id},
        {"object", "chat.completion.chunk"},
        {"created", (int)time(nullptr)},
        {"model", model},
        {"choices", json::array({
            {
                {"index", choiceIndex},
                {"delta", {
                    {"content", content}
                }}
            }
        })}
    };
    return result.dump();
}

// Config methods

void MnnServer::loadConfig() {
    std::string configPath = getExeDir() + "/server_config.json";
    loadConfig(configPath);
}

void MnnServer::loadConfig(const std::string& configPath) {
    std::ifstream f(configPath);
    if (!f.is_open()) {
        std::cerr << "[Server] config not found at " << configPath << ", using defaults" << std::endl;
        return;
    }
    try {
        json j;
        f >> j;
        if (j.contains("host")) config_.host = j["host"].get<std::string>();
        if (j.contains("port")) config_.port = j["port"].get<int>();
        if (j.contains("model")) config_.modelPath = j["model"].get<std::string>();
        if (j.contains("modelsDir")) config_.modelsDir = j["modelsDir"].get<std::string>();
        if (j.contains("nThreads")) config_.nThreads = j["nThreads"].get<int>();
        if (j.contains("backend")) config_.backendType = j["backend"].get<std::string>();
        if (j.contains("cacheType")) config_.cacheType = j["cacheType"].get<int>();
        if (j.contains("apiKey")) config_.apiKey = j["apiKey"].get<std::string>();
        if (j.contains("maxTokens")) config_.maxTokens = j["maxTokens"].get<int>();
        if (j.contains("topP")) config_.topP = j["topP"].get<float>();
        if (j.contains("temperature")) config_.temperature = j["temperature"].get<float>();
        if (j.contains("flashAttention")) config_.flashAttention = j["flashAttention"].get<bool>();
        std::cerr << "[Server] config loaded from " << configPath << std::endl;
    } catch (...) {
        std::cerr << "[Server] config parse error at " << configPath << std::endl;
    }
}

std::string MnnServer::getLocalIp() const {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return "127.0.0.1";

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "127.0.0.1";
    }

    struct sockaddr_in local_addr{};
    socklen_t len = sizeof(local_addr);
    getsockname(sock, (struct sockaddr*)&local_addr, &len);
    close(sock);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ip, sizeof(ip));
    return ip;
}

std::string MnnServer::getConfigApiKey() const { return config_.apiKey; }
int MnnServer::getConfigMaxTokens() const { return config_.maxTokens; }
float MnnServer::getConfigTemperature() const { return config_.temperature; }
float MnnServer::getConfigTopP() const { return config_.topP; }
bool MnnServer::getConfigFlashAttention() const { return config_.flashAttention; }
int MnnServer::getConfigNThreads() const { return config_.nThreads; }
std::string MnnServer::getConfigBackendType() const { return config_.backendType; }
int MnnServer::getConfigCacheType() const { return config_.cacheType; }
std::string MnnServer::getConfigModelPath() const { return config_.modelPath; }
std::string MnnServer::getConfigModelsDir() const { return config_.modelsDir; }
std::string MnnServer::getConfigHost() const { return config_.host; }
int MnnServer::getConfigPort() const { return config_.port; }

void MnnServer::start(int threads, const std::string& backend, bool flashAttention,
                      const std::string& modelPath, const std::string& apiKey,
                      int maxTokens, float topP, float temperature) {
    config_.nThreads = threads;
    config_.backendType = backend;
    config_.flashAttention = flashAttention;
    config_.modelPath = modelPath;
    config_.apiKey = apiKey;
    config_.maxTokens = maxTokens;
    config_.topP = topP;
    config_.temperature = temperature;
}

// Internal: register all HTTP handlers on a httplib::Server
void MnnServer::registerHandlers(httplib::Server& svr, const std::string& webPath,
                                  LogCallback logCb) {
    svr.set_payload_max_length(1024 * 1024);

    svr.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        json health = {
            {"status", "ok"},
            {"model", this->getActiveModel()},
            {"loaded", this->isLoaded()}
        };
        res.set_content(health.dump(), "application/json");
    });

    svr.Get("/", [this, &webPath](const httplib::Request&, httplib::Response& res) {
        const std::string& html = this->getWebHtml(webPath);
        if (!html.empty()) {
            res.set_content(html, "text/html");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });

    svr.Get("/v1/models", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(this->scanModels(), "application/json");
    });

    svr.Post("/v1/chat/completions", [this, logCb](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            bool stream = body.value("stream", false);

            if (!body.contains("messages") || !body["messages"].is_array()) {
                res.status = 400;
                json error = {
                    {"error", {
                        {"message", "messages must be a JSON array"},
                        {"type", "invalid_request_error"}
                    }}
                };
                res.set_content(error.dump(), "application/json");
                return;
            }

            const json& messages = body["messages"];
            int maxTokens = std::clamp(body.value("max_tokens", 256), 1, 8192);

            for (const char* param : {"temperature", "top_p", "frequency_penalty", "presence_penalty"}) {
                if (body.contains(param)) {
                    std::string msg = "Warning: " + std::string(param) + " is not supported, ignoring";
                    std::cerr << msg << std::endl;
                    if (logCb) logCb(msg);
                }
            }
            std::string prompt = parseMessages(messages);

            time_t now = time(nullptr);
            std::string id = "chat-" + std::to_string((int)now);
            std::string activeModel = this->getActiveModel();

            if (stream) {
                res.set_header("Content-Type", "text/event-stream");
                res.set_header("Cache-Control", "no-cache");
                res.set_header("Connection", "keep-alive");
                res.set_header("X-Accel-Buffering", "no");

                res.set_chunked_content_provider(
                    "text/event-stream",
                    [this, prompt, maxTokens, id, activeModel](
                        size_t /*offset*/, httplib::DataSink& sink) {
                        this->chatStreaming(prompt, maxTokens,
                            [&](const std::string& token, bool isEnd) {
                                if (isEnd) {
                                    json finishChunk = {
                                        {"id", id},
                                        {"object", "chat.completion.chunk"},
                                        {"created", (int)time(nullptr)},
                                        {"model", activeModel},
                                        {"choices", json::array({{
                                            {"index", 0},
                                            {"delta", json::object()},
                                            {"finish_reason", "stop"}
                                        }})}
                                    };
                                    sink.os << "data: " << finishChunk.dump() << "\n\n";
                                    sink.os << "data: [DONE]\n\n";
                                    sink.os.flush();
                                    sink.done();
                                    return;
                                }
                                sink.os << "data: "
                                        << buildChatCompletionChunk(
                                               id, activeModel, token, 0)
                                        << "\n\n";
                                sink.os.flush();
                            }
                        );
                        return false;
                    }
                );
            } else {
                std::string response = this->chat(prompt, maxTokens);
                std::string result = buildChatCompletion(id, activeModel, response);
                res.set_content(result, "application/json");
            }
        }
        catch (const std::exception& e) {
            res.status = 500;
            json error = {
                {"error", {
                    {"message", e.what()},
                    {"type", "server_error"}
                }}
            };
            res.set_content(error.dump(), "application/json");
        }
    });

    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        if (req.method == "POST") {
            std::cerr << req.method << " " << req.path << " " << res.status << std::endl;
        }
    });

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        std::cerr << "HTTP " << res.status << " error" << std::endl;
    });
}

// CLI: blocking listen
void MnnServer::run(const std::string& host, int port, const std::string& webPath) {
    httplib::Server svr;
    registerHandlers(svr, webPath, nullptr);

    std::cout << "Server listening on " << host << ":" << port << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  GET  /health" << std::endl;
    std::cout << "  GET  /v1/models" << std::endl;
    std::cout << "  POST /v1/chat/completions" << std::endl;

    if (!svr.bind_to_port(host, port)) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        return;
    }

    svr.listen_after_bind();
}

// Android: non-blocking listen with optional log callback
void MnnServer::start(const std::string& host, int port, const std::string& webPath, LogCallback logCb) {
    auto log = [&](const std::string& msg) {
        std::cerr << msg << std::endl;
        if (logCb) logCb(msg);
    };

    auto svr = std::make_shared<httplib::Server>();
    registerHandlers(*svr, webPath, logCb);

    log("[Server] listening on " + host + ":" + std::to_string(port));

    if (host == "0.0.0.0") {
        std::string localIp = this->getLocalIp();
        log("[Server] API URL: http://" + localIp + ":" + std::to_string(port));
        log("[Server] Web UI:  http://" + localIp + ":" + std::to_string(port));
    } else {
        log("[Server] API URL: http://" + host + ":" + std::to_string(port));
        log("[Server] Web UI:  http://" + host + ":" + std::to_string(port));
    }
    if (!config_.apiKey.empty()) {
        log("[Server] API key: " + config_.apiKey.substr(0, 4) + "..." + config_.apiKey.substr(config_.apiKey.size() - 4));
    }

    if (!svr->bind_to_port(host, port)) {
        log("[ERROR] failed to bind to port " + std::to_string(port));
        return;
    }

    // Run in background thread for Android
    std::thread serverThread([svr]() {
        svr->listen_after_bind();
    });
    serverThread.detach();
}

void MnnServer::stop() {
    stopRequested_.store(true);
}
