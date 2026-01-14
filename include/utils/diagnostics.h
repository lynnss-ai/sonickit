/**
 * @file diagnostics.h
 * @brief Audio and network diagnostics tools
 * 
 * Provides comprehensive diagnostic capabilities for voice applications:
 * - Network quality testing (latency, jitter, packet loss)
 * - Echo detection and analysis
 * - Audio loopback testing
 * - Device health monitoring
 * - Real-time quality metrics
 */

#ifndef VOICE_DIAGNOSTICS_H
#define VOICE_DIAGNOSTICS_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Network Diagnostics
 * ============================================ */

/**
 * @brief Network quality test type
 */
typedef enum {
    VOICE_NET_TEST_LATENCY,      /**< Round-trip latency test */
    VOICE_NET_TEST_JITTER,       /**< Jitter measurement */
    VOICE_NET_TEST_PACKET_LOSS,  /**< Packet loss rate test */
    VOICE_NET_TEST_BANDWIDTH,    /**< Available bandwidth estimation */
    VOICE_NET_TEST_FULL          /**< Complete diagnostic suite */
} voice_net_test_type_t;

/**
 * @brief Network quality rating
 */
typedef enum {
    VOICE_NET_QUALITY_EXCELLENT,  /**< < 50ms latency, < 5% loss */
    VOICE_NET_QUALITY_GOOD,       /**< 50-100ms latency, 5-10% loss */
    VOICE_NET_QUALITY_FAIR,       /**< 100-200ms latency, 10-20% loss */
    VOICE_NET_QUALITY_POOR,       /**< 200-400ms latency, 20-40% loss */
    VOICE_NET_QUALITY_UNUSABLE    /**< > 400ms latency or > 40% loss */
} voice_net_quality_t;

/**
 * @brief Network diagnostic results
 */
typedef struct {
    /* Latency metrics (milliseconds) */
    float rtt_min;               /**< Minimum RTT */
    float rtt_max;               /**< Maximum RTT */
    float rtt_avg;               /**< Average RTT */
    float rtt_current;           /**< Current/last RTT */
    
    /* Jitter metrics (milliseconds) */
    float jitter_avg;            /**< Average jitter */
    float jitter_max;            /**< Maximum jitter observed */
    
    /* Packet loss */
    float packet_loss_rate;      /**< Loss rate (0.0 to 1.0) */
    uint32_t packets_sent;       /**< Total packets sent */
    uint32_t packets_received;   /**< Total packets received */
    uint32_t packets_lost;       /**< Total packets lost */
    
    /* Bandwidth estimation */
    uint32_t bandwidth_up_kbps;  /**< Estimated upload bandwidth */
    uint32_t bandwidth_down_kbps; /**< Estimated download bandwidth */
    
    /* Overall quality */
    voice_net_quality_t quality; /**< Overall quality rating */
    float mos_estimate;          /**< Estimated MOS score (1.0-5.0) */
    
    /* Test info */
    uint64_t test_duration_ms;   /**< Test duration */
    uint64_t timestamp;          /**< Test completion timestamp */
} voice_net_diagnostic_result_t;

/**
 * @brief Network diagnostic configuration
 */
typedef struct {
    const char *target_host;     /**< Target host for testing */
    uint16_t target_port;        /**< Target port */
    uint32_t test_duration_ms;   /**< Test duration (ms) */
    uint32_t probe_interval_ms;  /**< Interval between probes */
    uint32_t probe_count;        /**< Number of probes to send */
    uint32_t probe_size_bytes;   /**< Size of each probe packet */
    bool use_tcp;                /**< Use TCP instead of UDP */
} voice_net_diagnostic_config_t;

typedef struct voice_net_diagnostic_s voice_net_diagnostic_t;

/**
 * @brief Initialize network diagnostic config with defaults
 */
void voice_net_diagnostic_config_init(voice_net_diagnostic_config_t *config);

/**
 * @brief Create network diagnostic instance
 */
voice_net_diagnostic_t *voice_net_diagnostic_create(const voice_net_diagnostic_config_t *config);

/**
 * @brief Destroy network diagnostic instance
 */
void voice_net_diagnostic_destroy(voice_net_diagnostic_t *diag);

/**
 * @brief Run a specific network test
 */
voice_error_t voice_net_diagnostic_run(voice_net_diagnostic_t *diag,
                                        voice_net_test_type_t test_type,
                                        voice_net_diagnostic_result_t *result);

/**
 * @brief Run quick network quality check
 */
voice_error_t voice_net_diagnostic_quick_check(const char *host, uint16_t port,
                                                voice_net_diagnostic_result_t *result);

/**
 * @brief Get quality description string
 */
const char *voice_net_quality_to_string(voice_net_quality_t quality);

/* ============================================
 * Echo Detection
 * ============================================ */

/**
 * @brief Echo detection result
 */
typedef struct {
    bool echo_detected;          /**< Whether echo was detected */
    float echo_level_db;         /**< Echo level in dB */
    float echo_delay_ms;         /**< Estimated echo delay */
    float echo_coupling;         /**< Echo return loss enhancement */
    float aec_recommended;       /**< AEC aggressiveness recommended (0-1) */
} voice_echo_result_t;

/**
 * @brief Echo detection configuration
 */
typedef struct {
    uint32_t sample_rate;        /**< Sample rate */
    uint32_t frame_size;         /**< Analysis frame size */
    uint32_t max_delay_ms;       /**< Maximum echo delay to search */
    float detection_threshold;   /**< Echo detection threshold (dB) */
} voice_echo_detector_config_t;

typedef struct voice_echo_detector_s voice_echo_detector_t;

/**
 * @brief Initialize echo detector config
 */
void voice_echo_detector_config_init(voice_echo_detector_config_t *config);

/**
 * @brief Create echo detector
 */
voice_echo_detector_t *voice_echo_detector_create(const voice_echo_detector_config_t *config);

/**
 * @brief Destroy echo detector
 */
void voice_echo_detector_destroy(voice_echo_detector_t *detector);

/**
 * @brief Process audio pair for echo detection
 * @param detector Echo detector instance
 * @param reference Reference signal (speaker output)
 * @param capture Captured signal (microphone input)
 * @param count Number of samples
 * @param result Echo detection result (optional)
 */
voice_error_t voice_echo_detector_process(voice_echo_detector_t *detector,
                                           const int16_t *reference,
                                           const int16_t *capture,
                                           size_t count,
                                           voice_echo_result_t *result);

/**
 * @brief Get current echo metrics
 */
voice_error_t voice_echo_detector_get_result(voice_echo_detector_t *detector,
                                              voice_echo_result_t *result);

/**
 * @brief Reset echo detector
 */
void voice_echo_detector_reset(voice_echo_detector_t *detector);

/* ============================================
 * Audio Loopback Testing
 * ============================================ */

/**
 * @brief Loopback test type
 */
typedef enum {
    VOICE_LOOPBACK_LOCAL,        /**< Local software loopback */
    VOICE_LOOPBACK_DEVICE,       /**< Device hardware loopback */
    VOICE_LOOPBACK_NETWORK       /**< Network round-trip loopback */
} voice_loopback_type_t;

/**
 * @brief Loopback test results
 */
typedef struct {
    /* Latency */
    float latency_ms;            /**< Round-trip latency */
    float latency_samples;       /**< Latency in samples */
    
    /* Signal quality */
    float snr_db;                /**< Signal-to-noise ratio */
    float thd_percent;           /**< Total harmonic distortion */
    float frequency_response_db; /**< Frequency response deviation */
    
    /* Audio levels */
    float input_level_db;        /**< Input signal level */
    float output_level_db;       /**< Output signal level */
    float level_difference_db;   /**< Gain/attenuation in path */
    
    /* Status */
    bool test_passed;            /**< Overall pass/fail status */
    const char *failure_reason;  /**< Reason if test failed */
} voice_loopback_result_t;

/**
 * @brief Loopback test configuration
 */
typedef struct {
    uint32_t sample_rate;        /**< Sample rate */
    uint32_t channels;           /**< Number of channels */
    uint32_t test_duration_ms;   /**< Test duration */
    uint32_t test_frequency_hz;  /**< Test tone frequency */
    float test_level_db;         /**< Test signal level */
    voice_loopback_type_t type;  /**< Loopback type */
    
    /* For device loopback */
    const char *input_device;    /**< Input device ID (NULL for default) */
    const char *output_device;   /**< Output device ID (NULL for default) */
    
    /* For network loopback */
    const char *remote_host;     /**< Remote loopback host */
    uint16_t remote_port;        /**< Remote loopback port */
} voice_loopback_config_t;

typedef struct voice_loopback_test_s voice_loopback_test_t;

/**
 * @brief Initialize loopback config
 */
void voice_loopback_config_init(voice_loopback_config_t *config);

/**
 * @brief Create loopback test
 */
voice_loopback_test_t *voice_loopback_test_create(const voice_loopback_config_t *config);

/**
 * @brief Destroy loopback test
 */
void voice_loopback_test_destroy(voice_loopback_test_t *test);

/**
 * @brief Run loopback test
 */
voice_error_t voice_loopback_test_run(voice_loopback_test_t *test,
                                       voice_loopback_result_t *result);

/**
 * @brief Run quick local loopback test
 */
voice_error_t voice_loopback_quick_test(uint32_t sample_rate,
                                         voice_loopback_result_t *result);

/* ============================================
 * Device Health Monitoring
 * ============================================ */

/**
 * @brief Device health status
 */
typedef enum {
    VOICE_DEVICE_HEALTHY,        /**< Device working normally */
    VOICE_DEVICE_DEGRADED,       /**< Device working but with issues */
    VOICE_DEVICE_FAILING,        /**< Device experiencing failures */
    VOICE_DEVICE_DISCONNECTED,   /**< Device disconnected */
    VOICE_DEVICE_UNKNOWN         /**< Status unknown */
} voice_device_health_t;

/**
 * @brief Device diagnostic results
 */
typedef struct {
    voice_device_health_t health;    /**< Overall health status */
    
    /* Performance metrics */
    float cpu_usage_percent;         /**< CPU usage by audio processing */
    float buffer_utilization;        /**< Buffer usage (0-1) */
    uint32_t underruns;              /**< Buffer underrun count */
    uint32_t overruns;               /**< Buffer overrun count */
    
    /* Audio quality */
    float current_level_db;          /**< Current signal level */
    bool clipping_detected;          /**< Clipping in recent frames */
    bool silence_detected;           /**< Unexpected silence */
    
    /* Latency */
    float input_latency_ms;          /**< Input chain latency */
    float output_latency_ms;         /**< Output chain latency */
    float processing_latency_ms;     /**< Processing latency */
    
    /* Device info */
    const char *device_name;         /**< Device name */
    uint32_t sample_rate;            /**< Active sample rate */
    uint32_t channels;               /**< Active channel count */
    uint32_t buffer_size;            /**< Active buffer size */
} voice_device_diagnostic_t;

typedef struct voice_device_monitor_s voice_device_monitor_t;

/**
 * @brief Create device health monitor
 */
voice_device_monitor_t *voice_device_monitor_create(void);

/**
 * @brief Destroy device monitor
 */
void voice_device_monitor_destroy(voice_device_monitor_t *monitor);

/**
 * @brief Attach monitor to active audio device
 */
voice_error_t voice_device_monitor_attach(voice_device_monitor_t *monitor,
                                           void *device_handle);

/**
 * @brief Get current device diagnostics
 */
voice_error_t voice_device_monitor_get_diagnostics(voice_device_monitor_t *monitor,
                                                    voice_device_diagnostic_t *diag);

/**
 * @brief Get health status string
 */
const char *voice_device_health_to_string(voice_device_health_t health);

/* ============================================
 * Real-time Quality Monitor
 * ============================================ */

/**
 * @brief Audio quality metrics (updated in real-time)
 */
typedef struct {
    /* Signal levels */
    float level_db;              /**< Current level (dBFS) */
    float peak_db;               /**< Peak level since reset */
    float rms_db;                /**< RMS level */
    
    /* Quality indicators */
    float snr_db;                /**< Signal-to-noise ratio */
    bool voice_active;           /**< Voice activity detected */
    float voice_probability;     /**< Voice probability (0-1) */
    
    /* Issues detected */
    bool clipping;               /**< Clipping detected */
    bool silence;                /**< Unexpected silence */
    bool noise;                  /**< High noise level */
    bool echo;                   /**< Echo detected */
    
    /* Statistics */
    uint64_t samples_processed;  /**< Total samples processed */
    uint64_t frames_processed;   /**< Total frames processed */
    uint32_t issues_count;       /**< Total issues detected */
} voice_quality_metrics_t;

typedef struct voice_quality_monitor_s voice_quality_monitor_t;

/**
 * @brief Quality monitor configuration
 */
typedef struct {
    uint32_t sample_rate;            /**< Sample rate */
    uint32_t frame_size;             /**< Analysis frame size */
    float silence_threshold_db;      /**< Silence detection threshold */
    float clipping_threshold;        /**< Clipping threshold (0-1) */
    float noise_floor_db;            /**< Expected noise floor */
} voice_quality_monitor_config_t;

/**
 * @brief Initialize quality monitor config
 */
void voice_quality_monitor_config_init(voice_quality_monitor_config_t *config);

/**
 * @brief Create quality monitor
 */
voice_quality_monitor_t *voice_quality_monitor_create(const voice_quality_monitor_config_t *config);

/**
 * @brief Destroy quality monitor
 */
void voice_quality_monitor_destroy(voice_quality_monitor_t *monitor);

/**
 * @brief Process audio frame and update metrics
 */
voice_error_t voice_quality_monitor_process(voice_quality_monitor_t *monitor,
                                             const int16_t *samples,
                                             size_t count);

/**
 * @brief Process float audio frame
 */
voice_error_t voice_quality_monitor_process_float(voice_quality_monitor_t *monitor,
                                                   const float *samples,
                                                   size_t count);

/**
 * @brief Get current quality metrics
 */
voice_error_t voice_quality_monitor_get_metrics(voice_quality_monitor_t *monitor,
                                                 voice_quality_metrics_t *metrics);

/**
 * @brief Reset quality monitor statistics
 */
void voice_quality_monitor_reset(voice_quality_monitor_t *monitor);

/**
 * @brief Set callback for quality alerts
 */
typedef void (*voice_quality_alert_callback_t)(void *user_data,
                                                const voice_quality_metrics_t *metrics,
                                                const char *alert_message);

voice_error_t voice_quality_monitor_set_callback(voice_quality_monitor_t *monitor,
                                                  voice_quality_alert_callback_t callback,
                                                  void *user_data);

/* ============================================
 * Diagnostic Report Generation
 * ============================================ */

/**
 * @brief Generate comprehensive diagnostic report
 */
typedef struct {
    voice_net_diagnostic_result_t network;
    voice_device_diagnostic_t device;
    voice_quality_metrics_t quality;
    voice_loopback_result_t loopback;
    voice_echo_result_t echo;
    uint64_t timestamp;
    const char *summary;
} voice_diagnostic_report_t;

/**
 * @brief Run full diagnostic suite
 */
voice_error_t voice_run_full_diagnostics(voice_diagnostic_report_t *report);

/**
 * @brief Format diagnostic report as string
 */
voice_error_t voice_format_diagnostic_report(const voice_diagnostic_report_t *report,
                                              char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_DIAGNOSTICS_H */
