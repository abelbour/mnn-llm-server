#define CPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND 120
#include "httplib.h"
#include "json.hpp"

#include <llm/llm.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <ctime>

using json = nlohmann::json;

class MnnServer {
private:
    MNN::Transformer::Llm* llm_ = nullptr;
    std::string modelPath_;
    std::string modelsDir_;
    std::string activeModel_;
    bool isLoaded_ = false;
    std::mutex mutex_;

public:
    MnnServer(const std::string& modelsDir)
        : modelsDir_(modelsDir), activeModel_("") {}

    ~MnnServer() {
        if (llm_) {
            MNN::Transformer::Llm::destroy(llm_);
        }
    }

    bool loadModel(const std::string& modelPath) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (isLoaded_ && modelPath == modelPath_) {
            std::cout << "Model already loaded: " << modelPath << std::endl;
            return true;
        }

        std::string configPath = modelPath;
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

        std::cout << "Model loaded successfully: " << activeModel_ << std::endl;
        return true;
    }

    std::string getModelName(const std::string& path) {
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

    std::string chat(const std::string& prompt) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!llm_ || !isLoaded_) {
            return "Error: No model loaded";
        }

        std::stringstream output;
        llm_->response(prompt, &output, nullptr, 256);

        std::string result = output.str();
        return result;
    }

    const std::string& getActiveModel() const { return activeModel_; }
    bool isLoaded() const { return isLoaded_; }

    std::string scanModels() {
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
        return result.dump();
    }
};

std::string parseMessages(const json& messages) {
    std::string systemPrompt;
    std::string userPrompt;
    std::string fullPrompt;

    for (const auto& msg : messages) {
        std::string role = msg.value("role", "user");
        std::string content = msg.value("content", "");

        if (role == "system") {
            systemPrompt = content;
        } else if (role == "user") {
            userPrompt = content;
        }
    }

    if (!systemPrompt.empty()) {
        fullPrompt += "System: " + systemPrompt + "\n\n";
    }
    fullPrompt += "User: " + userPrompt + "\n\nAssistant:";

    return fullPrompt;
}

std::string buildChatCompletion(const std::string& id,
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

std::string buildChatCompletionChunk(const std::string& id,
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

#include <unistd.h>
#include <libgen.h>

std::string getExeDir() {
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return dirname(dirname(buf));
    }
    return ".";
}

int main(int argc, char* argv[]) {
    std::string exeDir = getExeDir();
    std::string webPath = exeDir + "/web/index.html";
    
    std::string host = "0.0.0.0";
    int port = 8000;
    std::string modelPath = exeDir + "/models/Llama-3.2-1B-Instruct-MNN";
    std::string modelsDir = exeDir + "/models";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "-m" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == "-models" && i + 1 < argc) {
            modelsDir = argv[++i];
        }
    }

    std::cout << "Initializing MNN Server..." << std::endl;
    std::cout << "Model: " << modelPath << std::endl;
    std::cout << "Host: " << host << ":" << port << std::endl;

    auto server = std::make_unique<MnnServer>(modelsDir);

    if (!server->loadModel(modelPath)) {
        std::cerr << "Failed to load model, exiting..." << std::endl;
        return 1;
    }

    std::cout << "Starting HTTP server..." << std::endl;

    httplib::Server svr;

    svr.Get("/health", [&](const httplib::Request& req, httplib::Response& res) {
        json health = {
            {"status", "ok"},
            {"model", server->getActiveModel()},
            {"loaded", server->isLoaded()}
        };
        res.set_content(health.dump(), "application/json");
    });

    svr.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
        std::ifstream file(webPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else {
            res.status = 404;
            res.set_content("Chat UI not found at: " + webPath, "text/plain");
        }
    });

    svr.Get("/v1/models", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(server->scanModels(), "application/json");
    });

    svr.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            std::string model = body.value("model", "");
            bool stream = body.value("stream", false);

            if (!body.contains("messages")) {
                res.status = 400;
                json error = {
                    {"error", {
                        {"message", "Missing messages"},
                        {"type", "invalid_request_error"}
                    }}
                };
                res.set_content(error.dump(), "application/json");
                return;
            }

            const json& messages = body["messages"];
            std::string prompt = parseMessages(messages);

            time_t now = time(nullptr);
            std::string id = "chat-" + std::to_string((int)now);
            std::string activeModel = server->getActiveModel();

            if (stream) {
                res.set_header("Content-Type", "text/event-stream");
                res.set_header("Cache-Control", "no-cache");
                res.set_header("Connection", "keep-alive");
                res.set_header("X-Accel-Buffering", "no");

                std::string response = server->chat(prompt);
                
                size_t pos = 0;
                int index = 0;
                std::string result;
                
                while (pos < response.size()) {
                    size_t chunk_len = std::min((size_t)10, response.size() - pos);
                    std::string token = response.substr(pos, chunk_len);
                    std::string chunk = buildChatCompletionChunk(id, activeModel, token, index);
                    result += "data: " + chunk + "\n\n";
                    pos += chunk_len;
                    index++;
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                result += "data: [DONE]\n\n";
                res.set_content(result, "text/event-stream");
            } else {
                std::string response = server->chat(prompt);
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

    std::cout << "Server listening on " << host << ":" << port << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  GET  /health" << std::endl;
    std::cout << "  GET  /v1/models" << std::endl;
    std::cout << "  POST /v1/chat/completions" << std::endl;

    if (!svr.bind_to_port(host, port)) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        return 1;
    }

    svr.listen_after_bind();

    return 0;
}