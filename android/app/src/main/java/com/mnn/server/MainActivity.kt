package com.mnn.server

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.IBinder
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : AppCompatActivity() {

    private lateinit var tvStatus: TextView
    private lateinit var tvApiUrl: TextView
    private lateinit var tvLogs: ScrollView
    private lateinit var tvLogText: TextView
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button
    private lateinit var btnOpenWebUi: Button
    private lateinit var btnCopyApiUrl: Button
    private lateinit var btnSettings: Button

    private var serverService: ServerService? = null
    private var bound = false

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as ServerService.LocalBinder
            serverService = binder.getService()
            bound = true
            serverService?.setLogCallback { message ->
                runOnUiThread { appendLog(message) }
            }
            updateUi()
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            serverService = null
            bound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvStatus = findViewById(R.id.tvStatus)
        tvApiUrl = findViewById(R.id.tvApiUrl)
        tvLogs = findViewById(R.id.tvLogs)
        tvLogText = findViewById(R.id.tvLogText)
        btnStart = findViewById(R.id.btnStart)
        btnStop = findViewById(R.id.btnStop)
        btnOpenWebUi = findViewById(R.id.btnOpenWebUi)
        btnCopyApiUrl = findViewById(R.id.btnCopyApiUrl)
        btnSettings = findViewById(R.id.btnSettings)

        btnStart.setOnClickListener { startServer() }
        btnStop.setOnClickListener { stopServer() }
        btnOpenWebUi.setOnClickListener { openWebUi() }
        btnCopyApiUrl.setOnClickListener { copyApiUrl() }
        btnSettings.setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }
    }

    override fun onStart() {
        super.onStart()
        Intent(this, ServerService::class.java).also { intent ->
            bindService(intent, connection, Context.BIND_AUTO_CREATE)
        }
    }

    override fun onStop() {
        super.onStop()
        if (bound) {
            unbindService(connection)
            bound = false
        }
    }

    private fun startServer() {
        val prefs = getSharedPreferences("mnn_server", MODE_PRIVATE)
        val modelPath = prefs.getString("model_path", "") ?: ""
        val modelsDir = prefs.getString("models_dir", "") ?: ""
        val threads = prefs.getInt("threads", 4)
        val backend = prefs.getString("backend", "auto") ?: "auto"
        val flashAttention = prefs.getBoolean("flash_attention", true)
        val maxTokens = prefs.getInt("max_tokens", 4096)
        val topP = prefs.getFloat("top_p", 0.9f)
        val temperature = prefs.getFloat("temperature", 0.7f)
        val apiKey = prefs.getString("api_key", "") ?: ""
        val port = prefs.getInt("port", 8080)

        if (modelPath.isEmpty()) {
            appendLog("Error: No model path configured. Go to Settings first.")
            return
        }

        val intent = Intent(this, ServerService::class.java).apply {
            action = ServerService.ACTION_START
            putExtra(ServerService.EXTRA_MODEL_PATH, modelPath)
            putExtra(ServerService.EXTRA_MODELS_DIR, modelsDir)
            putExtra(ServerService.EXTRA_THREADS, threads)
            putExtra(ServerService.EXTRA_BACKEND, backend)
            putExtra(ServerService.EXTRA_FLASH_ATTENTION, flashAttention)
            putExtra(ServerService.EXTRA_MAX_TOKENS, maxTokens)
            putExtra(ServerService.EXTRA_TOP_P, topP)
            putExtra(ServerService.EXTRA_TEMPERATURE, temperature)
            putExtra(ServerService.EXTRA_API_KEY, apiKey)
            putExtra(ServerService.EXTRA_PORT, port)
        }
        startForegroundService(intent)
        appendLog("Starting server...")
    }

    private fun stopServer() {
        val intent = Intent(this, ServerService::class.java).apply {
            action = ServerService.ACTION_STOP
        }
        startService(intent)
    }

    private fun openWebUi() {
        val port = getSharedPreferences("mnn_server", MODE_PRIVATE).getInt("port", 8080)
        val url = ServerBridge.getWebUiUrl(port)
        if (url.isNotEmpty()) {
            val intent = Intent(this, WebUiActivity::class.java).apply {
                putExtra("url", url)
            }
            startActivity(intent)
        }
    }

    private fun copyApiUrl() {
        val port = getSharedPreferences("mnn_server", MODE_PRIVATE).getInt("port", 8080)
        val url = ServerBridge.getApiUrl(port)
        if (url.isNotEmpty()) {
            val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
            val clip = android.content.ClipData.newPlainText("API URL", url)
            clipboard.setPrimaryClip(clip)
            appendLog("API URL copied to clipboard")
        }
    }

    private fun updateUi() {
        val running = serverService?.isRunning == true
        btnStart.isEnabled = !running
        btnStop.isEnabled = running
        tvStatus.text = if (running) "Running" else "Stopped"
        tvStatus.setTextColor(getColor(if (running) R.color.status_running else R.color.status_stopped))

        if (running) {
            val port = getSharedPreferences("mnn_server", MODE_PRIVATE).getInt("port", 8080)
            tvApiUrl.text = ServerBridge.getApiUrl(port)
            tvApiUrl.visibility = android.view.View.VISIBLE
        } else {
            tvApiUrl.visibility = android.view.View.GONE
        }
    }

    private fun appendLog(message: String) {
        val timestamp = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())
        val logLine = "[$timestamp] $message\n"
        tvLogText.append(logLine)
        tvLogs.post { tvLogs.fullScroll(ScrollView.FOCUS_DOWN) }
    }
}
