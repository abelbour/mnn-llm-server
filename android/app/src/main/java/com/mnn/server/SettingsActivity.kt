package com.mnn.server

import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.Switch
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

class SettingsActivity : AppCompatActivity() {

    private lateinit var etModelPath: EditText
    private lateinit var etModelsDir: EditText
    private lateinit var etPort: EditText
    private lateinit var etThreads: EditText
    private lateinit var etBackend: EditText
    private lateinit var switchFlashAttention: Switch
    private lateinit var etMaxTokens: EditText
    private lateinit var etTopP: EditText
    private lateinit var etTemperature: EditText
    private lateinit var etApiKey: EditText
    private lateinit var btnSave: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)

        etModelPath = findViewById(R.id.etModelPath)
        etModelsDir = findViewById(R.id.etModelsDir)
        etPort = findViewById(R.id.etPort)
        etThreads = findViewById(R.id.etThreads)
        etBackend = findViewById(R.id.etBackend)
        switchFlashAttention = findViewById(R.id.switchFlashAttention)
        etMaxTokens = findViewById(R.id.etMaxTokens)
        etTopP = findViewById(R.id.etTopP)
        etTemperature = findViewById(R.id.etTemperature)
        etApiKey = findViewById(R.id.etApiKey)
        btnSave = findViewById(R.id.btnSave)

        loadSettings()

        btnSave.setOnClickListener { saveSettings() }
    }

    private fun loadSettings() {
        val prefs = getSharedPreferences("mnn_server", MODE_PRIVATE)
        etModelPath.setText(prefs.getString("model_path", ""))
        etModelsDir.setText(prefs.getString("models_dir", ""))
        etPort.setText(prefs.getInt("port", 8080).toString())
        etThreads.setText(prefs.getInt("threads", 4).toString())
        etBackend.setText(prefs.getString("backend", "auto"))
        switchFlashAttention.isChecked = prefs.getBoolean("flash_attention", true)
        etMaxTokens.setText(prefs.getInt("max_tokens", 4096).toString())
        etTopP.setText(prefs.getFloat("top_p", 0.9f).toString())
        etTemperature.setText(prefs.getFloat("temperature", 0.7f).toString())
        etApiKey.setText(prefs.getString("api_key", ""))
    }

    private fun saveSettings() {
        val prefs = getSharedPreferences("mnn_server", MODE_PRIVATE)
        val editor = prefs.edit()

        editor.putString("model_path", etModelPath.text.toString().trim())
        editor.putString("models_dir", etModelsDir.text.toString().trim())

        val port = etPort.text.toString().toIntOrNull() ?: 8080
        editor.putInt("port", port)

        val threads = etThreads.text.toString().toIntOrNull() ?: 4
        editor.putInt("threads", threads)

        editor.putString("backend", etBackend.text.toString().trim())
        editor.putBoolean("flash_attention", switchFlashAttention.isChecked)

        val maxTokens = etMaxTokens.text.toString().toIntOrNull() ?: 4096
        editor.putInt("max_tokens", maxTokens)

        val topP = etTopP.text.toString().toFloatOrNull() ?: 0.9f
        editor.putFloat("top_p", topP)

        val temperature = etTemperature.text.toString().toFloatOrNull() ?: 0.7f
        editor.putFloat("temperature", temperature)

        editor.putString("api_key", etApiKey.text.toString().trim())

        editor.apply()
        Toast.makeText(this, "Settings saved", Toast.LENGTH_SHORT).show()
        finish()
    }
}
