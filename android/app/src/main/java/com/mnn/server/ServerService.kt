package com.mnn.server

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat

class ServerService : Service(), LogCallback {

    companion object {
        private const val TAG = "ServerService"
        private const val CHANNEL_ID = "mnn_server_channel"
        private const val NOTIFICATION_ID = 1

        const val ACTION_START = "com.mnn.server.START"
        const val ACTION_STOP = "com.mnn.server.STOP"
        const val EXTRA_MODEL_PATH = "model_path"
        const val EXTRA_MODELS_DIR = "models_dir"
        const val EXTRA_THREADS = "threads"
        const val EXTRA_BACKEND = "backend"
        const val EXTRA_FLASH_ATTENTION = "flash_attention"
        const val EXTRA_MAX_TOKENS = "max_tokens"
        const val EXTRA_TOP_P = "top_p"
        const val EXTRA_TEMPERATURE = "temperature"
        const val EXTRA_API_KEY = "api_key"
        const val EXTRA_PORT = "port"
    }

    private val binder = LocalBinder()
    private var logCallback: ((String) -> Unit)? = null
    var isRunning = false
        private set

    inner class LocalBinder : Binder() {
        fun getService(): ServerService = this@ServerService
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                startForeground(NOTIFICATION_ID, buildNotification("Starting server..."))
                startServer(intent)
            }
            ACTION_STOP -> {
                stopServer()
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }
        return START_STICKY
    }

    private fun startServer(intent: Intent) {
        val modelPath = intent.getStringExtra(EXTRA_MODEL_PATH) ?: return
        val modelsDir = intent.getStringExtra(EXTRA_MODELS_DIR) ?: return
        val threads = intent.getIntExtra(EXTRA_THREADS, 4)
        val backend = intent.getStringExtra(EXTRA_BACKEND) ?: "auto"
        val flashAttention = intent.getBooleanExtra(EXTRA_FLASH_ATTENTION, true)
        val maxTokens = intent.getIntExtra(EXTRA_MAX_TOKENS, 4096)
        val topP = intent.getFloatExtra(EXTRA_TOP_P, 0.9f)
        val temperature = intent.getFloatExtra(EXTRA_TEMPERATURE, 0.7f)
        val apiKey = intent.getStringExtra(EXTRA_API_KEY) ?: ""
        val port = intent.getIntExtra(EXTRA_PORT, 8080)

        Thread {
            try {
                ServerBridge.start(
                    modelPath = modelPath,
                    modelsDir = modelsDir,
                    threads = threads,
                    backend = backend,
                    flashAttention = flashAttention,
                    maxTokens = maxTokens,
                    topP = topP,
                    temperature = temperature,
                    apiKey = apiKey,
                    port = port,
                    callback = this
                )
                isRunning = true
                updateNotification("Server running on port $port")
                logCallback?.invoke("Server started on port $port")
            } catch (e: Exception) {
                Log.e(TAG, "Failed to start server", e)
                logCallback?.invoke("Error: ${e.message}")
                isRunning = false
            }
        }.start()
    }

    fun stopServer() {
        ServerBridge.stop()
        isRunning = false
        logCallback?.invoke("Server stopped")
    }

    fun setLogCallback(callback: (String) -> Unit) {
        logCallback = callback
    }

    override fun onLog(message: String) {
        logCallback?.invoke(message)
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "MNN Server",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "MNN LLM Server running in background"
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(text: String): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = PendingIntent.getService(
            this, 1,
            Intent(this, ServerService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("MNN Server")
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentIntent(pendingIntent)
            .addAction(R.drawable.ic_notification, "Stop", stopIntent)
            .setOngoing(true)
            .build()
    }

    private fun updateNotification(text: String) {
        val manager = getSystemService(NotificationManager::class.java)
        manager.notify(NOTIFICATION_ID, buildNotification(text))
    }
}
