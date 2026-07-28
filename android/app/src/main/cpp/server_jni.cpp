#include <jni.h>
#include <string>
#include <android/log.h>
#include "server.h"

#define LOG_TAG "MnnServerNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static MnnServer* g_server = nullptr;
static JavaVM* g_jvm = nullptr;
static jobject g_callback_obj = nullptr;
static jmethodID g_callback_method = nullptr;

class JniLogCallback {
public:
    void operator()(const std::string& msg) {
        if (!g_jvm || !g_callback_obj || !g_callback_method) return;

        JNIEnv* env = nullptr;
        bool attached = false;
        int status = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        if (status == JNI_EDETACHED) {
            if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            } else {
                return;
            }
        } else if (status != JNI_OK) {
            return;
        }

        jstring jmsg = env->NewStringUTF(msg.c_str());
        env->CallVoidMethod(g_callback_obj, g_callback_method, jmsg);
        env->DeleteLocalRef(jmsg);

        if (attached) {
            g_jvm->DetachCurrentThread();
        }
    }
};

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    LOGI("JNI_OnLoad: JVM attached");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_mnn_server_ServerBridge_nativeStart(
    JNIEnv* env, jobject thiz,
    jstring modelPath, jstring modelsDir,
    jint threads, jstring backend, jboolean flashAttention,
    jint maxTokens, jfloat topP, jfloat temperature,
    jstring apiKey, jint port, jobject callback) {

    const char* modelPathStr = env->GetStringUTFChars(modelPath, nullptr);
    const char* modelsDirStr = env->GetStringUTFChars(modelsDir, nullptr);
    const char* backendStr = env->GetStringUTFChars(backend, nullptr);
    const char* apiKeyStr = apiKey ? env->GetStringUTFChars(apiKey, nullptr) : "";

    LOGI("nativeStart: model=%s threads=%d port=%d", modelPathStr, threads, port);

    // Clean up previous instance
    if (g_server) {
        g_server->stop();
        delete g_server;
        g_server = nullptr;
    }

    // Set up callback
    if (g_callback_obj) {
        env->DeleteGlobalRef(g_callback_obj);
        g_callback_obj = nullptr;
    }

    if (callback) {
        g_callback_obj = env->NewGlobalRef(callback);
        jclass cls = env->GetObjectClass(callback);
        g_callback_method = env->GetMethodID(cls, "onLog", "(Ljava/lang/String;)V");
    }

    // Create and start server
    g_server = new MnnServer(std::string(modelsDirStr));

    g_server->start(
        threads,
        std::string(backendStr),
        flashAttention == JNI_TRUE,
        std::string(modelPathStr),
        std::string(apiKeyStr),
        maxTokens,
        topP,
        temperature
    );

    // Load model
    if (!g_server->loadModel(std::string(modelPathStr))) {
        LOGE("Failed to load model: %s", modelPathStr);
    }

    // Start HTTP server
    std::string webPath = std::string(modelsDirStr) + "/../web/index.html";
    g_server->start("0.0.0.0", port, webPath, JniLogCallback());

    env->ReleaseStringUTFChars(modelPath, modelPathStr);
    env->ReleaseStringUTFChars(modelsDir, modelsDirStr);
    env->ReleaseStringUTFChars(backend, backendStr);
    if (apiKey) env->ReleaseStringUTFChars(apiKey, apiKeyStr);
}

JNIEXPORT void JNICALL
Java_com_mnn_server_ServerBridge_nativeStop(JNIEnv* env, jobject thiz) {
    LOGI("nativeStop");
    if (g_server) {
        g_server->stop();
        delete g_server;
        g_server = nullptr;
    }

    if (g_callback_obj) {
        env->DeleteGlobalRef(g_callback_obj);
        g_callback_obj = nullptr;
        g_callback_method = nullptr;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_mnn_server_ServerBridge_nativeIsRunning(JNIEnv* env, jobject thiz) {
    return g_server != nullptr ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_mnn_server_ServerBridge_nativeGetApiUrl(JNIEnv* env, jobject thiz, jint port) {
    if (!g_server) return env->NewStringUTF("");
    std::string ip = g_server->getLocalIp();
    std::string url = "http://" + ip + ":" + std::to_string(port);
    return env->NewStringUTF(url.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_mnn_server_ServerBridge_nativeGetWebUiUrl(JNIEnv* env, jobject thiz, jint port) {
    if (!g_server) return env->NewStringUTF("");
    std::string ip = g_server->getLocalIp();
    std::string url = "http://" + ip + ":" + std::to_string(port);
    return env->NewStringUTF(url.c_str());
}

} // extern "C"
