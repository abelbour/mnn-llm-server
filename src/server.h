#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <map>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "httplib.h"
#include <nlohmann/json.hpp>
#include <llm/llm.hpp>

using json = nlohmann::json;

std::string getExeDir();

class MnnServer {
private:
    MNN::Transformer::Llm* llm_ = nullptr;
    std::string modelPath_;
    std::string modelsDir_;
    std::string activeModel_;
    bool isLoaded_ = false;
    std::atomic<bool> stopRequested_{false};
    std::mutex mutex_;
    std::string cachedModels_;
    bool modelsCached_ = false;
    std::string cachedHtml_;
    bool htmlCached_ = false;

public:
    MnnServer(const std::string& modelsDir);
    ~MnnServer();

    bool loadModel(const std::string& modelPath);
    std::string getModelName(const std::string& path) const;
    std::string chat(const std::string& prompt, int maxTokens = 256);
    void stopStreaming();
    void chatStreaming(const std::string& prompt, int maxTokens,
                       std::function<void(const std::string&, bool)> onToken);

    const std::string& getActiveModel() const;
    bool isLoaded() const;
    std::string scanModels();
    const std::string& getWebHtml(const std::string& webPath);

    // Server lifecycle
    void run(const std::string& host, int port, const std::string& webPath);
    void stop();

    // Config
    void loadConfig();
    void loadConfig(const std::string& configPath);
    std::string getLocalIp() const;
    std::string getConfigApiKey() const;
    int getConfigMaxTokens() const;
    float getConfigTemperature() const;
    float getConfigTopP() const;
    bool getConfigFlashAttention() const;
    int getConfigNThreads() const;
    std::string getConfigBackendType() const;
    int getConfigCacheType() const;
    std::string getConfigModelPath() const;
    std::string getConfigModelsDir() const;
    std::string getConfigHost() const;
    int getConfigPort() const;

    // For Android
    using LogCallback = std::function<void(const std::string&)>;
    void start(int threads, const std::string& backend, bool flashAttention,
               const std::string& modelPath, const std::string& apiKey,
               int maxTokens, float topP, float temperature);
    void start(const std::string& host, int port, const std::string& webPath, LogCallback logCb = nullptr);

private:
    struct {
        std::string host = "0.0.0.0";
        int port = 8080;
        std::string modelPath;
        std::string modelsDir;
        int nThreads = 4;
        std::string backendType = "auto";
        int cacheType = 0;
        std::string apiKey;
        int maxTokens = 4096;
        float topP = 0.9;
        float temperature = 0.7;
        bool flashAttention = true;
    } config_;

    void registerHandlers(httplib::Server& svr, const std::string& webPath, LogCallback logCb);
};
