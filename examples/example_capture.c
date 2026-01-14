/**
 * @file example_capture.c
 * @brief Simple audio capture to WAV file example
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 演示从麦克风采集音频并保存为WAV文件
 */

#include "voice/voice.h"
#include "voice/platform.h"
#include "audio/device.h"
#include "audio/file_io.h"
#include "dsp/denoiser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* ============================================
 * 全局变量
 * ============================================ */

static volatile bool g_running = true;
static audio_writer_t *g_writer = NULL;
static voice_denoiser_t *g_denoiser = NULL;
static int16_t *g_process_buffer = NULL;
static uint64_t g_total_samples = 0;

/* ============================================
 * 信号处理
 * ============================================ */

static void signal_handler(int sig) {
    (void)sig;
    printf("\nStopping capture...\n");
    g_running = false;
}

/* ============================================
 * 音频回调
 * ============================================ */

static void capture_callback(
    voice_device_t *device,
    const int16_t *input,
    size_t frame_count,
    void *user_data)
{
    (void)device;
    (void)user_data;

    /* 复制到处理缓冲区 */
    memcpy(g_process_buffer, input, frame_count * sizeof(int16_t));

    /* 降噪处理 */
    if (g_denoiser) {
        voice_denoiser_process(g_denoiser, g_process_buffer, frame_count);
    }

    /* 写入文件 */
    if (g_writer) {
        audio_writer_write_s16(g_writer, g_process_buffer, frame_count);
        g_total_samples += frame_count;
    }
}

/* ============================================
 * 主程序
 * ============================================ */

int main(int argc, char *argv[])
{
    const char *output_file = "capture.wav";
    int duration_sec = 10;
    bool enable_denoise = true;

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            duration_sec = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-denoise") == 0) {
            enable_denoise = false;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -o <file>      Output WAV file (default: capture.wav)\n");
            printf("  -d <seconds>   Recording duration (default: 10)\n");
            printf("  --no-denoise   Disable noise reduction\n");
            printf("  -h, --help     Show this help\n");
            return 0;
        }
    }

    printf("Voice Library - Audio Capture Example\n");
    printf("======================================\n");
    printf("Output: %s\n", output_file);
    printf("Duration: %d seconds\n", duration_sec);
    printf("Denoise: %s\n", enable_denoise ? "enabled" : "disabled");
    printf("\n");

    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
#ifndef _WIN32
    signal(SIGTERM, signal_handler);
#endif

    /* 配置音频参数 */
    const uint32_t sample_rate = 48000;
    const uint8_t channels = 1;
    const uint32_t frame_size = 960; /* 20ms at 48kHz */

    /* 分配处理缓冲区 */
    g_process_buffer = (int16_t *)malloc(frame_size * sizeof(int16_t));
    if (!g_process_buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return 1;
    }

    /* 创建降噪器 */
    if (enable_denoise) {
        voice_denoiser_config_t dn_config;
        voice_denoiser_config_init(&dn_config);
        dn_config.sample_rate = sample_rate;
        dn_config.frame_size = frame_size;
        dn_config.engine = VOICE_DENOISE_SPEEX;
        dn_config.enable_vad = true;
        dn_config.enable_agc = true;

        g_denoiser = voice_denoiser_create(&dn_config);
        if (!g_denoiser) {
            fprintf(stderr, "Warning: Failed to create denoiser\n");
        }
    }

    /* 创建音频写入器 */
    g_writer = audio_writer_create_simple(output_file, sample_rate, channels);
    if (!g_writer) {
        fprintf(stderr, "Failed to create audio writer: %s\n", output_file);
        goto cleanup;
    }

    /* 枚举并显示可用设备 */
    printf("Available capture devices:\n");
    voice_device_info_t devices[10];
    size_t device_count = 10;

    if (voice_device_enumerate(VOICE_DEVICE_MODE_CAPTURE, devices, &device_count) == VOICE_OK) {
        for (size_t i = 0; i < device_count; i++) {
            printf("  [%zu] %s%s\n", i, devices[i].name,
                devices[i].is_default ? " (default)" : "");
        }
    }
    printf("\n");

    /* 创建采集设备 */
    voice_device_ext_config_t dev_config;
    voice_device_config_init(&dev_config);
    dev_config.mode = VOICE_DEVICE_MODE_CAPTURE;
    dev_config.sample_rate = sample_rate;
    dev_config.channels = channels;
    dev_config.frame_size = frame_size;
    dev_config.capture_callback = capture_callback;
    dev_config.capture_user_data = NULL;

    voice_device_t *device = voice_device_create_simple(&dev_config);
    if (!device) {
        fprintf(stderr, "Failed to create capture device\n");
        goto cleanup;
    }

    /* 启动采集 */
    voice_error_t err = voice_device_start(device);
    if (err != VOICE_OK) {
        fprintf(stderr, "Failed to start capture: %d\n", err);
        voice_device_destroy(device);
        goto cleanup;
    }

    printf("Recording... Press Ctrl+C to stop\n");

    /* 录制循环 */
    int elapsed_sec = 0;
    while (g_running && elapsed_sec < duration_sec) {
        sleep_ms(1000);
        elapsed_sec++;

        double recorded_sec = (double)g_total_samples / sample_rate;
        printf("\rRecorded: %.1f seconds", recorded_sec);
        fflush(stdout);
    }

    printf("\n");

    /* 停止并清理 */
    voice_device_stop(device);
    voice_device_destroy(device);

    printf("Saved %llu samples (%.1f seconds) to %s\n",
        (unsigned long long)g_total_samples,
        (double)g_total_samples / sample_rate,
        output_file);

cleanup:
    if (g_writer) {
        audio_writer_close(g_writer);
    }

    if (g_denoiser) {
        voice_denoiser_destroy(g_denoiser);
    }

    if (g_process_buffer) {
        free(g_process_buffer);
    }

    return 0;
}
