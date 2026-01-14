/**
 * @file platform_android.c
 * @brief Android-specific audio session management
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 使用 JNI 与 Android Audio 系统交互
 */

#if defined(__ANDROID__)

#include "voice/platform.h"
#include <jni.h>
#include <android/log.h>
#include <aaudio/AAudio.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sched.h>

#define LOG_TAG "VoiceLib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ============================================
 * Global State
 * ============================================ */

static JavaVM *g_jvm = NULL;
static jobject g_audio_manager = NULL;
static jclass g_audio_manager_class = NULL;
static bool g_low_latency_enabled = false;

static voice_interrupt_callback_t g_interrupt_cb = NULL;
static void *g_interrupt_cb_data = NULL;
static voice_route_change_callback_t g_route_change_cb = NULL;
static void *g_route_change_cb_data = NULL;

/* ============================================
 * JNI Helpers
 * ============================================ */

static JNIEnv *get_jni_env(void) {
    JNIEnv *env = NULL;
    if (g_jvm) {
        int status = (*g_jvm)->GetEnv(g_jvm, (void **)&env, JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
        }
    }
    return env;
}

/* ============================================
 * JNI Initialization (called from Java)
 * ============================================ */

JNIEXPORT void JNICALL
Java_com_voice_VoiceLib_nativeInit(JNIEnv *env, jclass clazz, jobject context) {
    (*env)->GetJavaVM(env, &g_jvm);
    
    /* Get AudioManager */
    jclass context_class = (*env)->GetObjectClass(env, context);
    jmethodID get_system_service = (*env)->GetMethodID(
        env, context_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    
    jstring audio_service = (*env)->NewStringUTF(env, "audio");
    jobject audio_manager = (*env)->CallObjectMethod(env, context, get_system_service, audio_service);
    
    if (audio_manager) {
        g_audio_manager = (*env)->NewGlobalRef(env, audio_manager);
        g_audio_manager_class = (*env)->NewGlobalRef(
            env, (*env)->GetObjectClass(env, audio_manager));
    }
    
    (*env)->DeleteLocalRef(env, audio_service);
    
    LOGD("VoiceLib native initialized");
}

JNIEXPORT void JNICALL
Java_com_voice_VoiceLib_nativeRelease(JNIEnv *env, jclass clazz) {
    if (g_audio_manager) {
        (*env)->DeleteGlobalRef(env, g_audio_manager);
        g_audio_manager = NULL;
    }
    if (g_audio_manager_class) {
        (*env)->DeleteGlobalRef(env, g_audio_manager_class);
        g_audio_manager_class = NULL;
    }
    g_jvm = NULL;
}

/* Audio focus change callback from Java */
JNIEXPORT void JNICALL
Java_com_voice_VoiceLib_onAudioFocusChange(JNIEnv *env, jclass clazz, jint focusChange) {
    if (!g_interrupt_cb) return;
    
    voice_interrupt_type_t type;
    voice_interrupt_reason_t reason = VOICE_INTERRUPT_REASON_DEFAULT;
    bool should_resume = false;
    
    /* AUDIOFOCUS_LOSS = -1, AUDIOFOCUS_LOSS_TRANSIENT = -2, 
       AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK = -3, AUDIOFOCUS_GAIN = 1 */
    switch (focusChange) {
        case 1: /* AUDIOFOCUS_GAIN */
            type = VOICE_INTERRUPT_ENDED;
            should_resume = true;
            break;
        case -1: /* AUDIOFOCUS_LOSS */
            type = VOICE_INTERRUPT_BEGAN;
            break;
        case -2: /* AUDIOFOCUS_LOSS_TRANSIENT */
        case -3: /* AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK */
            type = VOICE_INTERRUPT_BEGAN;
            should_resume = true;
            break;
        default:
            return;
    }
    
    g_interrupt_cb(type, reason, should_resume, g_interrupt_cb_data);
}

/* Audio route change callback from Java */
JNIEXPORT void JNICALL
Java_com_voice_VoiceLib_onAudioRouteChanged(JNIEnv *env, jclass clazz, jint type) {
    if (!g_route_change_cb) return;
    
    voice_route_change_reason_t reason;
    switch (type) {
        case 1: /* DEVICE_CONNECT */
            reason = VOICE_ROUTE_CHANGE_NEW_DEVICE;
            break;
        case 2: /* DEVICE_DISCONNECT */
            reason = VOICE_ROUTE_CHANGE_OLD_DEVICE_UNAVAILABLE;
            break;
        default:
            reason = VOICE_ROUTE_CHANGE_UNKNOWN;
            break;
    }
    
    voice_audio_route_t route = voice_session_get_current_route();
    g_route_change_cb(reason, route, g_route_change_cb_data);
}

/* ============================================
 * Platform API Implementation
 * ============================================ */

voice_error_t voice_session_configure(const voice_session_config_t *config) {
    if (!config) return VOICE_ERROR_NULL_POINTER;
    
    JNIEnv *env = get_jni_env();
    if (!env || !g_audio_manager) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    /* Configure AAudio for low-latency if in voice chat mode */
    if (config->mode == VOICE_SESSION_MODE_VOICE_CHAT) {
        voice_platform_enable_low_latency(true);
    }
    
    return VOICE_OK;
}

voice_error_t voice_session_activate(void) {
    JNIEnv *env = get_jni_env();
    if (!env || !g_audio_manager) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    /* Request audio focus */
    /* This should be called from Java side for proper callback handling */
    
    return VOICE_OK;
}

voice_error_t voice_session_deactivate(void) {
    JNIEnv *env = get_jni_env();
    if (!env || !g_audio_manager) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    /* Abandon audio focus */
    
    return VOICE_OK;
}

voice_audio_route_t voice_session_get_current_route(void) {
    JNIEnv *env = get_jni_env();
    if (!env || !g_audio_manager) {
        return VOICE_AUDIO_ROUTE_UNKNOWN;
    }
    
    /* Check various audio routes */
    jmethodID is_speaker = (*env)->GetMethodID(
        env, g_audio_manager_class, "isSpeakerphoneOn", "()Z");
    jmethodID is_bluetooth = (*env)->GetMethodID(
        env, g_audio_manager_class, "isBluetoothScoOn", "()Z");
    jmethodID is_wired = (*env)->GetMethodID(
        env, g_audio_manager_class, "isWiredHeadsetOn", "()Z");
    
    if (is_wired && (*env)->CallBooleanMethod(env, g_audio_manager, is_wired)) {
        return VOICE_AUDIO_ROUTE_HEADPHONES;
    }
    
    if (is_bluetooth && (*env)->CallBooleanMethod(env, g_audio_manager, is_bluetooth)) {
        return VOICE_AUDIO_ROUTE_BLUETOOTH_HFP;
    }
    
    if (is_speaker && (*env)->CallBooleanMethod(env, g_audio_manager, is_speaker)) {
        return VOICE_AUDIO_ROUTE_BUILTIN_SPEAKER;
    }
    
    return VOICE_AUDIO_ROUTE_BUILTIN_RECEIVER;
}

voice_error_t voice_session_override_output(voice_audio_route_t route) {
    JNIEnv *env = get_jni_env();
    if (!env || !g_audio_manager) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    jmethodID set_speaker = (*env)->GetMethodID(
        env, g_audio_manager_class, "setSpeakerphoneOn", "(Z)V");
    
    if (!set_speaker) {
        return VOICE_ERROR_NOT_SUPPORTED;
    }
    
    bool speaker_on = (route == VOICE_AUDIO_ROUTE_BUILTIN_SPEAKER);
    (*env)->CallVoidMethod(env, g_audio_manager, set_speaker, speaker_on);
    
    return VOICE_OK;
}

void voice_session_set_interrupt_callback(
    voice_interrupt_callback_t callback,
    void *user_data) {
    g_interrupt_cb = callback;
    g_interrupt_cb_data = user_data;
}

void voice_session_set_route_change_callback(
    voice_route_change_callback_t callback,
    void *user_data) {
    g_route_change_cb = callback;
    g_route_change_cb_data = user_data;
}

bool voice_session_request_mic_permission(
    void (*callback)(bool granted, void *user_data),
    void *user_data) {
    
    /* Permission must be requested from Java side */
    /* For now, assume we have permission if we're called */
    if (callback) {
        callback(true, user_data);
    }
    return true;
}

voice_permission_status_t voice_session_get_mic_permission(void) {
    /* This needs to be queried from Java side */
    return VOICE_PERMISSION_UNKNOWN;
}

voice_error_t voice_platform_enable_low_latency(bool enable) {
    g_low_latency_enabled = enable;
    
    /* AAudio performance mode is set when creating streams */
    /* AAUDIO_PERFORMANCE_MODE_LOW_LATENCY */
    
    LOGD("Low latency mode: %s", enable ? "enabled" : "disabled");
    return VOICE_OK;
}

voice_error_t voice_platform_get_optimal_parameters(
    uint32_t *sample_rate,
    uint32_t *frames_per_buffer) {
    
    JNIEnv *env = get_jni_env();
    if (!env || !g_audio_manager) {
        if (sample_rate) *sample_rate = 48000;
        if (frames_per_buffer) *frames_per_buffer = 960;
        return VOICE_OK;
    }
    
    /* Get optimal sample rate */
    if (sample_rate) {
        jmethodID get_property = (*env)->GetMethodID(
            env, g_audio_manager_class, "getProperty", 
            "(Ljava/lang/String;)Ljava/lang/String;");
        
        if (get_property) {
            jstring key = (*env)->NewStringUTF(env, "android.media.property.OUTPUT_SAMPLE_RATE");
            jstring value = (*env)->CallObjectMethod(env, g_audio_manager, get_property, key);
            
            if (value) {
                const char *str = (*env)->GetStringUTFChars(env, value, NULL);
                if (str) {
                    *sample_rate = (uint32_t)atoi(str);
                    (*env)->ReleaseStringUTFChars(env, value, str);
                }
                (*env)->DeleteLocalRef(env, value);
            }
            (*env)->DeleteLocalRef(env, key);
        }
        
        if (*sample_rate == 0) {
            *sample_rate = 48000;
        }
    }
    
    /* Get optimal frames per buffer */
    if (frames_per_buffer) {
        jmethodID get_property = (*env)->GetMethodID(
            env, g_audio_manager_class, "getProperty", 
            "(Ljava/lang/String;)Ljava/lang/String;");
        
        if (get_property) {
            jstring key = (*env)->NewStringUTF(env, "android.media.property.OUTPUT_FRAMES_PER_BUFFER");
            jstring value = (*env)->CallObjectMethod(env, g_audio_manager, get_property, key);
            
            if (value) {
                const char *str = (*env)->GetStringUTFChars(env, value, NULL);
                if (str) {
                    *frames_per_buffer = (uint32_t)atoi(str);
                    (*env)->ReleaseStringUTFChars(env, value, str);
                }
                (*env)->DeleteLocalRef(env, value);
            }
            (*env)->DeleteLocalRef(env, key);
        }
        
        if (*frames_per_buffer == 0) {
            *frames_per_buffer = 256;
        }
    }
    
    return VOICE_OK;
}

voice_error_t voice_platform_set_bluetooth_sco(bool enable) {
    JNIEnv *env = get_jni_env();
    if (!env || !g_audio_manager) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    jmethodID set_sco = (*env)->GetMethodID(
        env, g_audio_manager_class, 
        enable ? "startBluetoothSco" : "stopBluetoothSco", "()V");
    
    if (!set_sco) {
        return VOICE_ERROR_NOT_SUPPORTED;
    }
    
    (*env)->CallVoidMethod(env, g_audio_manager, set_sco);
    
    jmethodID set_sco_on = (*env)->GetMethodID(
        env, g_audio_manager_class, "setBluetoothScoOn", "(Z)V");
    
    if (set_sco_on) {
        (*env)->CallVoidMethod(env, g_audio_manager, set_sco_on, enable);
    }
    
    return VOICE_OK;
}

voice_error_t voice_platform_acquire_wake_lock(void) {
    /* Wake lock should be managed from Java side */
    return VOICE_OK;
}

voice_error_t voice_platform_release_wake_lock(void) {
    return VOICE_OK;
}

voice_error_t voice_platform_set_realtime_priority(void) {
    /* Set thread name for debugging */
    prctl(PR_SET_NAME, "voice_audio", 0, 0, 0);
    
    /* Try to set SCHED_FIFO */
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0) {
        /* Fall back to SCHED_RR */
        param.sched_priority = sched_get_priority_max(SCHED_RR);
        ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }
    
    if (ret != 0) {
        LOGD("Could not set realtime priority: %d", ret);
    }
    
    return VOICE_OK;
}

#endif /* __ANDROID__ */
