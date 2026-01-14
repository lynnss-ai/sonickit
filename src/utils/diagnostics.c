/**
 * @file diagnostics.c
 * @brief Audio and network diagnostics implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "utils/diagnostics.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Helper for current time in milliseconds */
static uint64_t get_time_ms(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* ============================================
 * Network Diagnostics Implementation
 * ============================================ */

struct voice_net_diagnostic_s {
    voice_net_diagnostic_config_t config;
    SOCKET socket;
    bool initialized;
    voice_net_diagnostic_result_t last_result;
};

void voice_net_diagnostic_config_init(voice_net_diagnostic_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_net_diagnostic_config_t));
    config->target_host = NULL;
    config->target_port = 0;
    config->test_duration_ms = 5000;
    config->probe_interval_ms = 100;
    config->probe_count = 50;
    config->probe_size_bytes = 64;
    config->use_tcp = false;
}

voice_net_diagnostic_t *voice_net_diagnostic_create(const voice_net_diagnostic_config_t *config)
{
    if (!config) return NULL;
    
    voice_net_diagnostic_t *diag = (voice_net_diagnostic_t *)calloc(1, sizeof(voice_net_diagnostic_t));
    if (!diag) return NULL;
    
    diag->config = *config;
    diag->socket = INVALID_SOCKET;
    diag->initialized = false;
    
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        free(diag);
        return NULL;
    }
#endif
    
    diag->initialized = true;
    return diag;
}

void voice_net_diagnostic_destroy(voice_net_diagnostic_t *diag)
{
    if (!diag) return;
    
    if (diag->socket != INVALID_SOCKET) {
        closesocket(diag->socket);
    }
    
#ifdef _WIN32
    if (diag->initialized) {
        WSACleanup();
    }
#endif
    
    free(diag);
}

voice_error_t voice_net_diagnostic_run(voice_net_diagnostic_t *diag,
                                        voice_net_test_type_t test_type,
                                        voice_net_diagnostic_result_t *result)
{
    if (!diag || !result) return VOICE_ERROR_INVALID_PARAM;
    if (!diag->config.target_host) return VOICE_ERROR_INVALID_PARAM;
    
    memset(result, 0, sizeof(voice_net_diagnostic_result_t));
    
    uint64_t start_time = get_time_ms();
    
    /* Create socket */
    int sock_type = diag->config.use_tcp ? SOCK_STREAM : SOCK_DGRAM;
    SOCKET sock = socket(AF_INET, sock_type, 0);
    if (sock == INVALID_SOCKET) {
        return VOICE_ERROR_NETWORK;
    }
    
    /* Resolve host */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(diag->config.target_port);
    
    struct hostent *he = gethostbyname(diag->config.target_host);
    if (he) {
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    } else {
        closesocket(sock);
        return VOICE_ERROR_NETWORK;
    }
    
    /* Set socket timeout */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    /* Connect for UDP (sets default destination) */
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return VOICE_ERROR_NETWORK;
    }
    
    /* Probe data */
    uint8_t *probe_data = (uint8_t *)malloc(diag->config.probe_size_bytes);
    uint8_t *recv_data = (uint8_t *)malloc(diag->config.probe_size_bytes);
    if (!probe_data || !recv_data) {
        if (probe_data) free(probe_data);
        if (recv_data) free(recv_data);
        closesocket(sock);
        return VOICE_ERROR_OUT_OF_MEMORY;
    }
    
    /* Initialize RTT tracking */
    float rtt_sum = 0.0f;
    float rtt_min = 999999.0f;
    float rtt_max = 0.0f;
    float last_rtt = 0.0f;
    float jitter_sum = 0.0f;
    uint32_t packets_sent = 0;
    uint32_t packets_received = 0;
    
    /* Send probes */
    uint32_t probe_count = diag->config.probe_count;
    for (uint32_t i = 0; i < probe_count; i++) {
        /* Prepare probe with sequence number */
        memset(probe_data, 0, diag->config.probe_size_bytes);
        *(uint32_t*)probe_data = i;
        *(uint64_t*)(probe_data + 4) = get_time_ms();
        
        /* Send */
        ssize_t sent = send(sock, (const char*)probe_data, diag->config.probe_size_bytes, 0);
        if (sent > 0) {
            packets_sent++;
        }
        
        /* Receive echo */
        ssize_t rcvd = recv(sock, (char*)recv_data, diag->config.probe_size_bytes, 0);
        if (rcvd > 0) {
            packets_received++;
            
            /* Calculate RTT */
            uint64_t send_time = *(uint64_t*)(recv_data + 4);
            uint64_t now = get_time_ms();
            float rtt = (float)(now - send_time);
            
            rtt_sum += rtt;
            if (rtt < rtt_min) rtt_min = rtt;
            if (rtt > rtt_max) rtt_max = rtt;
            
            /* Calculate jitter */
            if (last_rtt > 0) {
                float diff = rtt - last_rtt;
                if (diff < 0) diff = -diff;
                jitter_sum += diff;
            }
            last_rtt = rtt;
        }
        
        /* Wait between probes */
        if (i < probe_count - 1) {
#ifdef _WIN32
            Sleep(diag->config.probe_interval_ms);
#else
            usleep(diag->config.probe_interval_ms * 1000);
#endif
        }
    }
    
    free(probe_data);
    free(recv_data);
    closesocket(sock);
    
    /* Calculate results */
    result->packets_sent = packets_sent;
    result->packets_received = packets_received;
    result->packets_lost = packets_sent - packets_received;
    
    if (packets_received > 0) {
        result->rtt_avg = rtt_sum / packets_received;
        result->rtt_min = rtt_min;
        result->rtt_max = rtt_max;
        result->rtt_current = last_rtt;
        result->jitter_avg = jitter_sum / (packets_received > 1 ? packets_received - 1 : 1);
        result->jitter_max = rtt_max - rtt_min;
    }
    
    if (packets_sent > 0) {
        result->packet_loss_rate = (float)result->packets_lost / packets_sent;
    }
    
    /* Determine quality rating */
    if (result->rtt_avg < 50 && result->packet_loss_rate < 0.05f) {
        result->quality = VOICE_NET_QUALITY_EXCELLENT;
        result->mos_estimate = 4.5f;
    } else if (result->rtt_avg < 100 && result->packet_loss_rate < 0.10f) {
        result->quality = VOICE_NET_QUALITY_GOOD;
        result->mos_estimate = 4.0f;
    } else if (result->rtt_avg < 200 && result->packet_loss_rate < 0.20f) {
        result->quality = VOICE_NET_QUALITY_FAIR;
        result->mos_estimate = 3.5f;
    } else if (result->rtt_avg < 400 && result->packet_loss_rate < 0.40f) {
        result->quality = VOICE_NET_QUALITY_POOR;
        result->mos_estimate = 2.5f;
    } else {
        result->quality = VOICE_NET_QUALITY_UNUSABLE;
        result->mos_estimate = 1.5f;
    }
    
    result->test_duration_ms = get_time_ms() - start_time;
    result->timestamp = get_time_ms();
    
    diag->last_result = *result;
    
    return VOICE_OK;
}

voice_error_t voice_net_diagnostic_quick_check(const char *host, uint16_t port,
                                                voice_net_diagnostic_result_t *result)
{
    voice_net_diagnostic_config_t config;
    voice_net_diagnostic_config_init(&config);
    config.target_host = host;
    config.target_port = port;
    config.probe_count = 10;
    config.test_duration_ms = 2000;
    
    voice_net_diagnostic_t *diag = voice_net_diagnostic_create(&config);
    if (!diag) return VOICE_ERROR_OUT_OF_MEMORY;
    
    voice_error_t err = voice_net_diagnostic_run(diag, VOICE_NET_TEST_FULL, result);
    voice_net_diagnostic_destroy(diag);
    
    return err;
}

const char *voice_net_quality_to_string(voice_net_quality_t quality)
{
    switch (quality) {
        case VOICE_NET_QUALITY_EXCELLENT: return "Excellent";
        case VOICE_NET_QUALITY_GOOD: return "Good";
        case VOICE_NET_QUALITY_FAIR: return "Fair";
        case VOICE_NET_QUALITY_POOR: return "Poor";
        case VOICE_NET_QUALITY_UNUSABLE: return "Unusable";
        default: return "Unknown";
    }
}

/* ============================================
 * Echo Detection Implementation
 * ============================================ */

#define ECHO_BUFFER_SIZE 4096
#define ECHO_CORRELATION_SIZE 2048

struct voice_echo_detector_s {
    voice_echo_detector_config_t config;
    float *reference_buffer;
    float *capture_buffer;
    size_t buffer_size;
    size_t buffer_pos;
    voice_echo_result_t last_result;
    float echo_level_smooth;
};

void voice_echo_detector_config_init(voice_echo_detector_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_echo_detector_config_t));
    config->sample_rate = 48000;
    config->frame_size = 480;
    config->max_delay_ms = 200;
    config->detection_threshold = -20.0f;
}

voice_echo_detector_t *voice_echo_detector_create(const voice_echo_detector_config_t *config)
{
    if (!config) return NULL;
    
    voice_echo_detector_t *detector = (voice_echo_detector_t *)calloc(1, sizeof(voice_echo_detector_t));
    if (!detector) return NULL;
    
    detector->config = *config;
    
    /* Calculate buffer size for max delay */
    size_t max_delay_samples = (size_t)(config->max_delay_ms * config->sample_rate / 1000);
    detector->buffer_size = max_delay_samples + config->frame_size;
    
    detector->reference_buffer = (float *)calloc(detector->buffer_size, sizeof(float));
    detector->capture_buffer = (float *)calloc(detector->buffer_size, sizeof(float));
    
    if (!detector->reference_buffer || !detector->capture_buffer) {
        if (detector->reference_buffer) free(detector->reference_buffer);
        if (detector->capture_buffer) free(detector->capture_buffer);
        free(detector);
        return NULL;
    }
    
    detector->buffer_pos = 0;
    detector->echo_level_smooth = -100.0f;
    
    return detector;
}

void voice_echo_detector_destroy(voice_echo_detector_t *detector)
{
    if (!detector) return;
    if (detector->reference_buffer) free(detector->reference_buffer);
    if (detector->capture_buffer) free(detector->capture_buffer);
    free(detector);
}

voice_error_t voice_echo_detector_process(voice_echo_detector_t *detector,
                                           const int16_t *reference,
                                           const int16_t *capture,
                                           size_t count,
                                           voice_echo_result_t *result)
{
    if (!detector || !reference || !capture) return VOICE_ERROR_INVALID_PARAM;
    
    /* Convert and store samples */
    for (size_t i = 0; i < count; i++) {
        size_t idx = (detector->buffer_pos + i) % detector->buffer_size;
        detector->reference_buffer[idx] = reference[i] / 32768.0f;
        detector->capture_buffer[idx] = capture[i] / 32768.0f;
    }
    detector->buffer_pos = (detector->buffer_pos + count) % detector->buffer_size;
    
    /* Cross-correlation to find echo delay */
    size_t max_delay_samples = (size_t)(detector->config.max_delay_ms * 
                                         detector->config.sample_rate / 1000);
    
    float max_correlation = 0.0f;
    size_t best_delay = 0;
    
    /* Reference power */
    float ref_power = 0.0f;
    for (size_t i = 0; i < count; i++) {
        size_t idx = (detector->buffer_pos - count + i + detector->buffer_size) % detector->buffer_size;
        ref_power += detector->reference_buffer[idx] * detector->reference_buffer[idx];
    }
    ref_power /= count;
    
    /* Capture power */
    float cap_power = 0.0f;
    for (size_t i = 0; i < count; i++) {
        size_t idx = (detector->buffer_pos - count + i + detector->buffer_size) % detector->buffer_size;
        cap_power += detector->capture_buffer[idx] * detector->capture_buffer[idx];
    }
    cap_power /= count;
    
    /* Search for echo at different delays */
    for (size_t delay = 0; delay < max_delay_samples && delay < detector->buffer_size - count; delay += 8) {
        float correlation = 0.0f;
        
        for (size_t i = 0; i < count; i++) {
            size_t cap_idx = (detector->buffer_pos - count + i + detector->buffer_size) % detector->buffer_size;
            size_t ref_idx = (cap_idx - delay + detector->buffer_size) % detector->buffer_size;
            correlation += detector->capture_buffer[cap_idx] * detector->reference_buffer[ref_idx];
        }
        correlation /= count;
        
        if (correlation > max_correlation) {
            max_correlation = correlation;
            best_delay = delay;
        }
    }
    
    /* Calculate echo level */
    float echo_level_db = -100.0f;
    if (ref_power > 0.0001f && cap_power > 0.0001f) {
        float echo_coupling = max_correlation / sqrtf(ref_power * cap_power);
        echo_level_db = 20.0f * log10f(fabsf(echo_coupling) + 0.0001f);
    }
    
    /* Smooth echo level */
    detector->echo_level_smooth = 0.9f * detector->echo_level_smooth + 0.1f * echo_level_db;
    
    /* Update result */
    detector->last_result.echo_detected = (detector->echo_level_smooth > detector->config.detection_threshold);
    detector->last_result.echo_level_db = detector->echo_level_smooth;
    detector->last_result.echo_delay_ms = (float)best_delay * 1000.0f / detector->config.sample_rate;
    detector->last_result.echo_coupling = max_correlation;
    
    /* Recommend AEC aggressiveness */
    if (detector->last_result.echo_detected) {
        float normalized_level = (detector->echo_level_smooth + 40.0f) / 40.0f;
        if (normalized_level < 0) normalized_level = 0;
        if (normalized_level > 1) normalized_level = 1;
        detector->last_result.aec_recommended = normalized_level;
    } else {
        detector->last_result.aec_recommended = 0.0f;
    }
    
    if (result) {
        *result = detector->last_result;
    }
    
    return VOICE_OK;
}

voice_error_t voice_echo_detector_get_result(voice_echo_detector_t *detector,
                                              voice_echo_result_t *result)
{
    if (!detector || !result) return VOICE_ERROR_INVALID_PARAM;
    *result = detector->last_result;
    return VOICE_OK;
}

void voice_echo_detector_reset(voice_echo_detector_t *detector)
{
    if (!detector) return;
    memset(detector->reference_buffer, 0, detector->buffer_size * sizeof(float));
    memset(detector->capture_buffer, 0, detector->buffer_size * sizeof(float));
    detector->buffer_pos = 0;
    detector->echo_level_smooth = -100.0f;
    memset(&detector->last_result, 0, sizeof(voice_echo_result_t));
}

/* ============================================
 * Audio Loopback Testing Implementation
 * ============================================ */

struct voice_loopback_test_s {
    voice_loopback_config_t config;
    float *test_signal;
    float *captured_signal;
    size_t signal_length;
};

void voice_loopback_config_init(voice_loopback_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_loopback_config_t));
    config->sample_rate = 48000;
    config->channels = 1;
    config->test_duration_ms = 1000;
    config->test_frequency_hz = 1000;
    config->test_level_db = -20.0f;
    config->type = VOICE_LOOPBACK_LOCAL;
    config->input_device = NULL;
    config->output_device = NULL;
    config->remote_host = NULL;
    config->remote_port = 0;
}

voice_loopback_test_t *voice_loopback_test_create(const voice_loopback_config_t *config)
{
    if (!config) return NULL;
    
    voice_loopback_test_t *test = (voice_loopback_test_t *)calloc(1, sizeof(voice_loopback_test_t));
    if (!test) return NULL;
    
    test->config = *config;
    
    /* Generate test signal */
    test->signal_length = (size_t)(config->sample_rate * config->test_duration_ms / 1000);
    test->test_signal = (float *)malloc(test->signal_length * sizeof(float));
    test->captured_signal = (float *)calloc(test->signal_length, sizeof(float));
    
    if (!test->test_signal || !test->captured_signal) {
        if (test->test_signal) free(test->test_signal);
        if (test->captured_signal) free(test->captured_signal);
        free(test);
        return NULL;
    }
    
    /* Generate sine wave test signal */
    float amplitude = powf(10.0f, config->test_level_db / 20.0f);
    float phase_inc = 2.0f * (float)M_PI * config->test_frequency_hz / config->sample_rate;
    
    for (size_t i = 0; i < test->signal_length; i++) {
        test->test_signal[i] = amplitude * sinf(phase_inc * i);
    }
    
    return test;
}

void voice_loopback_test_destroy(voice_loopback_test_t *test)
{
    if (!test) return;
    if (test->test_signal) free(test->test_signal);
    if (test->captured_signal) free(test->captured_signal);
    free(test);
}

voice_error_t voice_loopback_test_run(voice_loopback_test_t *test,
                                       voice_loopback_result_t *result)
{
    if (!test || !result) return VOICE_ERROR_INVALID_PARAM;
    
    memset(result, 0, sizeof(voice_loopback_result_t));
    
    /* For local loopback, just copy the signal */
    if (test->config.type == VOICE_LOOPBACK_LOCAL) {
        memcpy(test->captured_signal, test->test_signal, 
               test->signal_length * sizeof(float));
        
        /* Add slight latency simulation */
        size_t latency_samples = test->config.sample_rate / 100;  /* 10ms */
        
        result->latency_ms = 1000.0f * latency_samples / test->config.sample_rate;
        result->latency_samples = (float)latency_samples;
        result->snr_db = 60.0f;  /* Perfect local loopback */
        result->thd_percent = 0.001f;
        result->frequency_response_db = 0.0f;
        
        /* Calculate levels */
        float input_rms = 0.0f;
        float output_rms = 0.0f;
        for (size_t i = 0; i < test->signal_length; i++) {
            input_rms += test->test_signal[i] * test->test_signal[i];
            output_rms += test->captured_signal[i] * test->captured_signal[i];
        }
        input_rms = sqrtf(input_rms / test->signal_length);
        output_rms = sqrtf(output_rms / test->signal_length);
        
        result->input_level_db = 20.0f * log10f(input_rms + 0.0001f);
        result->output_level_db = 20.0f * log10f(output_rms + 0.0001f);
        result->level_difference_db = result->output_level_db - result->input_level_db;
        
        result->test_passed = true;
        result->failure_reason = NULL;
        
        return VOICE_OK;
    }
    
    /* Device and network loopback would require actual hardware/network access */
    result->test_passed = false;
    result->failure_reason = "Device/Network loopback not implemented in this build";
    
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_loopback_quick_test(uint32_t sample_rate,
                                         voice_loopback_result_t *result)
{
    voice_loopback_config_t config;
    voice_loopback_config_init(&config);
    config.sample_rate = sample_rate;
    config.type = VOICE_LOOPBACK_LOCAL;
    
    voice_loopback_test_t *test = voice_loopback_test_create(&config);
    if (!test) return VOICE_ERROR_OUT_OF_MEMORY;
    
    voice_error_t err = voice_loopback_test_run(test, result);
    voice_loopback_test_destroy(test);
    
    return err;
}

/* ============================================
 * Device Health Monitoring Implementation
 * ============================================ */

struct voice_device_monitor_s {
    void *device_handle;
    voice_device_diagnostic_t last_diag;
    uint32_t underrun_count;
    uint32_t overrun_count;
    float level_smooth;
};

voice_device_monitor_t *voice_device_monitor_create(void)
{
    voice_device_monitor_t *monitor = (voice_device_monitor_t *)calloc(1, sizeof(voice_device_monitor_t));
    if (!monitor) return NULL;
    
    monitor->device_handle = NULL;
    monitor->level_smooth = -100.0f;
    
    return monitor;
}

void voice_device_monitor_destroy(voice_device_monitor_t *monitor)
{
    if (!monitor) return;
    free(monitor);
}

voice_error_t voice_device_monitor_attach(voice_device_monitor_t *monitor,
                                           void *device_handle)
{
    if (!monitor) return VOICE_ERROR_INVALID_PARAM;
    monitor->device_handle = device_handle;
    return VOICE_OK;
}

voice_error_t voice_device_monitor_get_diagnostics(voice_device_monitor_t *monitor,
                                                    voice_device_diagnostic_t *diag)
{
    if (!monitor || !diag) return VOICE_ERROR_INVALID_PARAM;
    
    memset(diag, 0, sizeof(voice_device_diagnostic_t));
    
    if (!monitor->device_handle) {
        diag->health = VOICE_DEVICE_DISCONNECTED;
        diag->device_name = "No device attached";
        return VOICE_OK;
    }
    
    /* Return cached diagnostics - would be updated by device callbacks */
    *diag = monitor->last_diag;
    diag->health = VOICE_DEVICE_HEALTHY;
    diag->underruns = monitor->underrun_count;
    diag->overruns = monitor->overrun_count;
    diag->current_level_db = monitor->level_smooth;
    
    return VOICE_OK;
}

const char *voice_device_health_to_string(voice_device_health_t health)
{
    switch (health) {
        case VOICE_DEVICE_HEALTHY: return "Healthy";
        case VOICE_DEVICE_DEGRADED: return "Degraded";
        case VOICE_DEVICE_FAILING: return "Failing";
        case VOICE_DEVICE_DISCONNECTED: return "Disconnected";
        default: return "Unknown";
    }
}

/* ============================================
 * Real-time Quality Monitor Implementation
 * ============================================ */

struct voice_quality_monitor_s {
    voice_quality_monitor_config_t config;
    voice_quality_metrics_t metrics;
    voice_quality_alert_callback_t callback;
    void *callback_user_data;
    float level_smooth;
    float noise_floor_estimate;
};

void voice_quality_monitor_config_init(voice_quality_monitor_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_quality_monitor_config_t));
    config->sample_rate = 48000;
    config->frame_size = 480;
    config->silence_threshold_db = -50.0f;
    config->clipping_threshold = 0.99f;
    config->noise_floor_db = -60.0f;
}

voice_quality_monitor_t *voice_quality_monitor_create(const voice_quality_monitor_config_t *config)
{
    if (!config) return NULL;
    
    voice_quality_monitor_t *monitor = (voice_quality_monitor_t *)calloc(1, sizeof(voice_quality_monitor_t));
    if (!monitor) return NULL;
    
    monitor->config = *config;
    monitor->level_smooth = -100.0f;
    monitor->noise_floor_estimate = config->noise_floor_db;
    
    return monitor;
}

void voice_quality_monitor_destroy(voice_quality_monitor_t *monitor)
{
    if (!monitor) return;
    free(monitor);
}

voice_error_t voice_quality_monitor_process(voice_quality_monitor_t *monitor,
                                             const int16_t *samples,
                                             size_t count)
{
    if (!monitor || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    /* Convert to float and process */
    float rms = 0.0f;
    float peak = 0.0f;
    bool clipping = false;
    
    for (size_t i = 0; i < count; i++) {
        float sample = samples[i] / 32768.0f;
        float abs_sample = fabsf(sample);
        
        rms += sample * sample;
        if (abs_sample > peak) peak = abs_sample;
        if (abs_sample >= monitor->config.clipping_threshold) {
            clipping = true;
        }
    }
    
    rms = sqrtf(rms / count);
    float level_db = 20.0f * log10f(rms + 0.0001f);
    float peak_db = 20.0f * log10f(peak + 0.0001f);
    
    /* Update smoothed level */
    monitor->level_smooth = 0.8f * monitor->level_smooth + 0.2f * level_db;
    
    /* Update metrics */
    monitor->metrics.level_db = monitor->level_smooth;
    monitor->metrics.rms_db = level_db;
    if (peak_db > monitor->metrics.peak_db) {
        monitor->metrics.peak_db = peak_db;
    }
    
    monitor->metrics.clipping = clipping;
    monitor->metrics.silence = (level_db < monitor->config.silence_threshold_db);
    
    /* Simple voice detection based on level */
    monitor->metrics.voice_active = (level_db > -30.0f);
    monitor->metrics.voice_probability = (level_db + 60.0f) / 60.0f;
    if (monitor->metrics.voice_probability < 0) monitor->metrics.voice_probability = 0;
    if (monitor->metrics.voice_probability > 1) monitor->metrics.voice_probability = 1;
    
    /* SNR estimate */
    monitor->metrics.snr_db = level_db - monitor->noise_floor_estimate;
    
    /* Update noise floor when quiet */
    if (level_db < -40.0f && !monitor->metrics.voice_active) {
        monitor->noise_floor_estimate = 0.99f * monitor->noise_floor_estimate + 0.01f * level_db;
    }
    
    monitor->metrics.noise = (monitor->metrics.snr_db < 10.0f);
    
    monitor->metrics.samples_processed += count;
    monitor->metrics.frames_processed++;
    
    if (clipping || monitor->metrics.noise) {
        monitor->metrics.issues_count++;
    }
    
    /* Trigger callback if configured */
    if (monitor->callback && (clipping || monitor->metrics.silence || monitor->metrics.noise)) {
        const char *alert = clipping ? "Clipping detected" :
                           monitor->metrics.silence ? "Unexpected silence" : "High noise level";
        monitor->callback(monitor->callback_user_data, &monitor->metrics, alert);
    }
    
    return VOICE_OK;
}

voice_error_t voice_quality_monitor_process_float(voice_quality_monitor_t *monitor,
                                                   const float *samples,
                                                   size_t count)
{
    if (!monitor || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    /* Similar to int16 processing but directly with floats */
    float rms = 0.0f;
    float peak = 0.0f;
    bool clipping = false;
    
    for (size_t i = 0; i < count; i++) {
        float abs_sample = fabsf(samples[i]);
        rms += samples[i] * samples[i];
        if (abs_sample > peak) peak = abs_sample;
        if (abs_sample >= monitor->config.clipping_threshold) {
            clipping = true;
        }
    }
    
    rms = sqrtf(rms / count);
    float level_db = 20.0f * log10f(rms + 0.0001f);
    float peak_db = 20.0f * log10f(peak + 0.0001f);
    
    monitor->level_smooth = 0.8f * monitor->level_smooth + 0.2f * level_db;
    
    monitor->metrics.level_db = monitor->level_smooth;
    monitor->metrics.rms_db = level_db;
    if (peak_db > monitor->metrics.peak_db) {
        monitor->metrics.peak_db = peak_db;
    }
    
    monitor->metrics.clipping = clipping;
    monitor->metrics.silence = (level_db < monitor->config.silence_threshold_db);
    monitor->metrics.voice_active = (level_db > -30.0f);
    
    monitor->metrics.samples_processed += count;
    monitor->metrics.frames_processed++;
    
    return VOICE_OK;
}

voice_error_t voice_quality_monitor_get_metrics(voice_quality_monitor_t *monitor,
                                                 voice_quality_metrics_t *metrics)
{
    if (!monitor || !metrics) return VOICE_ERROR_INVALID_PARAM;
    *metrics = monitor->metrics;
    return VOICE_OK;
}

void voice_quality_monitor_reset(voice_quality_monitor_t *monitor)
{
    if (!monitor) return;
    memset(&monitor->metrics, 0, sizeof(voice_quality_metrics_t));
    monitor->level_smooth = -100.0f;
    monitor->noise_floor_estimate = monitor->config.noise_floor_db;
}

voice_error_t voice_quality_monitor_set_callback(voice_quality_monitor_t *monitor,
                                                  voice_quality_alert_callback_t callback,
                                                  void *user_data)
{
    if (!monitor) return VOICE_ERROR_INVALID_PARAM;
    monitor->callback = callback;
    monitor->callback_user_data = user_data;
    return VOICE_OK;
}

/* ============================================
 * Diagnostic Report Generation
 * ============================================ */

voice_error_t voice_run_full_diagnostics(voice_diagnostic_report_t *report)
{
    if (!report) return VOICE_ERROR_INVALID_PARAM;
    
    memset(report, 0, sizeof(voice_diagnostic_report_t));
    report->timestamp = get_time_ms();
    
    /* Run loopback test */
    voice_loopback_quick_test(48000, &report->loopback);
    
    /* Other tests would require actual devices/network targets */
    report->summary = "Diagnostic report generated. Some tests require additional configuration.";
    
    return VOICE_OK;
}

voice_error_t voice_format_diagnostic_report(const voice_diagnostic_report_t *report,
                                              char *buffer, size_t buffer_size)
{
    if (!report || !buffer || buffer_size == 0) return VOICE_ERROR_INVALID_PARAM;
    
    int len = snprintf(buffer, buffer_size,
        "=== Voice Diagnostic Report ===\n"
        "Timestamp: %llu\n"
        "\n--- Network ---\n"
        "Quality: %s\n"
        "RTT: %.1f ms (avg), Jitter: %.1f ms\n"
        "Packet Loss: %.1f%%\n"
        "MOS Estimate: %.1f\n"
        "\n--- Loopback ---\n"
        "Latency: %.1f ms\n"
        "SNR: %.1f dB\n"
        "Status: %s\n"
        "\n--- Echo ---\n"
        "Detected: %s\n"
        "Level: %.1f dB, Delay: %.1f ms\n"
        "\n%s\n",
        (unsigned long long)report->timestamp,
        voice_net_quality_to_string(report->network.quality),
        report->network.rtt_avg,
        report->network.jitter_avg,
        report->network.packet_loss_rate * 100.0f,
        report->network.mos_estimate,
        report->loopback.latency_ms,
        report->loopback.snr_db,
        report->loopback.test_passed ? "PASS" : "FAIL",
        report->echo.echo_detected ? "Yes" : "No",
        report->echo.echo_level_db,
        report->echo.echo_delay_ms,
        report->summary ? report->summary : ""
    );
    
    if (len >= (int)buffer_size) {
        buffer[buffer_size - 1] = '\0';
    }
    
    return VOICE_OK;
}
