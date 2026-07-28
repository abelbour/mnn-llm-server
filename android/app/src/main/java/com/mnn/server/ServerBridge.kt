package com.mnn.server

interface LogCallback {
    fun onLog(message: String)
}

object ServerBridge {
    init {
        System.loadLibrary("mnn_server")
    }

    external fun nativeStart(
        modelPath: String,
        modelsDir: String,
        threads: Int,
        backend: String,
        flashAttention: Boolean,
        maxTokens: Int,
        topP: Float,
        temperature: Float,
        apiKey: String,
        port: Int,
        callback: LogCallback?
    )

    external fun nativeStop()
    external fun nativeIsRunning(): Boolean
    external fun nativeGetApiUrl(port: Int): String
    external fun nativeGetWebUiUrl(port: Int): String

    fun start(
        modelPath: String,
        modelsDir: String,
        threads: Int = 4,
        backend: String = "auto",
        flashAttention: Boolean = true,
        maxTokens: Int = 4096,
        topP: Float = 0.9f,
        temperature: Float = 0.7f,
        apiKey: String = "",
        port: Int = 8080,
        callback: LogCallback? = null
    ) {
        nativeStart(
            modelPath, modelsDir, threads, backend, flashAttention,
            maxTokens, topP, temperature, apiKey, port, callback
        )
    }

    fun stop() = nativeStop()
    fun isRunning() = nativeIsRunning()
    fun getApiUrl(port: Int) = nativeGetApiUrl(port)
    fun getWebUiUrl(port: Int) = nativeGetWebUiUrl(port)
}
