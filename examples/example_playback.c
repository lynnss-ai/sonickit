/**
 * @file example_playback.c
 * @brief Simple audio file playback example
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 演示从音频文件播放到扬声器
 */

#include "voice/voice.h"
#include "audio/device.h"
#include "audio/file_io.h"
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
static audio_reader_t *g_reader = NULL;
static uint64_t g_total_samples = 0;
static uint64_t g_file_samples = 0;

/* ============================================
 * 信号处理
 * ============================================ */

static void signal_handler(int sig) {
    (void)sig;
    printf("\nStopping playback...\n");
    g_running = false;
}

/* ============================================
 * 音频回调
 * ============================================ */

static void playback_callback(
    voice_device_t *device,
    int16_t *output,
    size_t frame_count,
    void *user_data)
{
    (void)device;
    (void)user_data;

    if (!g_reader || !g_running) {
        memset(output, 0, frame_count * sizeof(int16_t));
        return;
    }

    size_t samples_read = 0;
    voice_error_t err = audio_reader_read_s16(g_reader, output, frame_count, &samples_read);

    if (err != VOICE_OK || samples_read == 0) {
        /* 文件结束 */
        memset(output, 0, frame_count * sizeof(int16_t));
        g_running = false;
        return;
    }

    /* 如果读取的样本少于请求的，填充静音 */
    if (samples_read < frame_count) {
        memset(output + samples_read, 0, (frame_count - samples_read) * sizeof(int16_t));
    }

    g_total_samples += samples_read;
}

/* ============================================
 * 主程序
 * ============================================ */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <audio_file> [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -v <volume>    Playback volume 0.0-1.0 (default: 1.0)\n");
        printf("\nSupported formats: WAV, MP3, FLAC\n");
        return 1;
    }

    const char *input_file = argv[1];
    float volume = 1.0f;

    /* 解析选项 */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            volume = (float)atof(argv[++i]);
            if (volume < 0.0f) volume = 0.0f;
            if (volume > 1.0f) volume = 1.0f;
        }
    }

    printf("Voice Library - Audio Playback Example\n");
    printf("=======================================\n");
    printf("File: %s\n", input_file);
    printf("Volume: %.0f%%\n", volume * 100);
    printf("\n");

    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
#ifndef _WIN32
    signal(SIGTERM, signal_handler);
#endif

    /* 打开音频文件 */
    g_reader = audio_reader_open(input_file);
    if (!g_reader) {
        fprintf(stderr, "Failed to open audio file: %s\n", input_file);
        return 1;
    }

    /* 获取文件信息 */
    uint32_t sample_rate;
    uint8_t channels;
    audio_reader_get_info_ex(g_reader, &sample_rate, &channels, &g_file_samples);

    printf("Sample rate: %u Hz\n", sample_rate);
    printf("Channels: %u\n", (unsigned)channels);
    printf("Duration: %.1f seconds\n", (double)g_file_samples / sample_rate);
    printf("\n");

    /* 帧大小 */
    const uint32_t frame_size = sample_rate / 50; /* 20ms */

    /* 枚举并显示可用设备 */
    printf("Available playback devices:\n");
    voice_device_info_t devices[10];
    size_t device_count = 10;

    if (voice_device_enumerate(VOICE_DEVICE_MODE_PLAYBACK, devices, &device_count) == VOICE_OK) {
        for (size_t i = 0; i < device_count; i++) {
            printf("  [%zu] %s%s\n", i, devices[i].name,
                devices[i].is_default ? " (default)" : "");
        }
    }
    printf("\n");

    /* 创建播放设备 */
    voice_device_ext_config_t dev_config;
    voice_device_config_init(&dev_config);
    dev_config.mode = VOICE_DEVICE_MODE_PLAYBACK;
    dev_config.sample_rate = sample_rate;
    dev_config.channels = channels;
    dev_config.frame_size = frame_size;
    dev_config.playback_callback = playback_callback;
    dev_config.playback_user_data = NULL;

    voice_device_t *device = voice_device_create_simple(&dev_config);
    if (!device) {
        fprintf(stderr, "Failed to create playback device\n");
        audio_reader_close(g_reader);
        return 1;
    }

    /* 启动播放 */
    voice_error_t err = voice_device_start(device);
    if (err != VOICE_OK) {
        fprintf(stderr, "Failed to start playback: %d\n", err);
        voice_device_destroy(device);
        audio_reader_close(g_reader);
        return 1;
    }

    printf("Playing... Press Ctrl+C to stop\n");

    /* 播放循环 */
    while (g_running) {
        sleep_ms(100);

        double played_sec = (double)g_total_samples / sample_rate;
        double total_sec = (double)g_file_samples / sample_rate;
        int progress = (int)((played_sec / total_sec) * 50);

        printf("\r[");
        for (int i = 0; i < 50; i++) {
            printf(i < progress ? "=" : " ");
        }
        printf("] %.1f / %.1f sec", played_sec, total_sec);
        fflush(stdout);
    }

    printf("\n\nPlayback complete.\n");

    /* 清理 */
    voice_device_stop(device);
    voice_device_destroy(device);
    audio_reader_close(g_reader);

    return 0;
}
