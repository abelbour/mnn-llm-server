#define CPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND 120
#include "server.h"

#include <iostream>
#include <memory>
#include <string>
#include <csignal>

static MnnServer* g_server = nullptr;

static void signalHandler(int sig) {
    std::cout << "\nShutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string exeDir = getExeDir();
    std::string host = "0.0.0.0";
    int port = 8080;
    std::string modelPath = exeDir + "/models/Llama-3.2-1B-Instruct-MNN";
    std::string modelsDir = exeDir + "/models";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            try { port = std::stoi(argv[++i]); }
            catch (...) { std::cerr << "Invalid port, using 8080\n"; port = 8080; }
        } else if (arg == "-m" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == "-models" && i + 1 < argc) {
            modelsDir = argv[++i];
        }
    }

    // Try loading config file, then override with CLI args
    MnnServer server(modelsDir);
    server.loadConfig();

    // CLI args override config file values
    if (modelPath != exeDir + "/models/Llama-3.2-1B-Instruct-MNN") {
        // User explicitly set -m flag
    } else if (!server.getConfigModelPath().empty()) {
        modelPath = server.getConfigModelPath();
    }

    if (host == "0.0.0.0" && !server.getConfigHost().empty()) {
        host = server.getConfigHost();
    }
    if (port == 8080 && server.getConfigPort() != 8080) {
        port = server.getConfigPort();
    }

    std::cout << "Initializing MNN Server..." << std::endl;
    std::cout << "Model: " << modelPath << std::endl;
    std::cout << "Host: " << host << ":" << port << std::endl;

    g_server = &server;
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (!server.loadModel(modelPath)) {
        std::cerr << "Failed to load model, exiting..." << std::endl;
        return 1;
    }

    std::string webPath = exeDir + "/web/index.html";
    server.run(host, port, webPath);

    return 0;
}
