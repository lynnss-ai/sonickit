/**
 * @file example_voicechat.c
 * @brief Full-duplex voice chat example
 * 
 * 演示完整的语音通话流程:
 * - 双工音频采集/播放
 * - 回声消除 (AEC)
 * - 降噪
 * - Opus编解码
 * - RTP/SRTP网络传输
 * - 抖动缓冲和丢包补偿
 */

#include "voice/voice.h"
#include "voice/pipeline.h"
#include "voice/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define close closesocket
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* ============================================
 * 配置
 * ============================================ */

#define DEFAULT_PORT 5004
#define MAX_PACKET_SIZE 1500

/* ============================================
 * 全局状态
 * ============================================ */

static volatile bool g_running = true;
static voice_pipeline_t *g_pipeline = NULL;
static int g_socket = -1;
static struct sockaddr_in g_remote_addr;
static bool g_remote_connected = false;

/* ============================================
 * 信号处理
 * ============================================ */

static void signal_handler(int sig) {
    (void)sig;
    printf("\nShutting down...\n");
    g_running = false;
}

/* ============================================
 * 回调函数
 * ============================================ */

static void on_encoded_data(
    voice_pipeline_t *pipeline,
    const uint8_t *data,
    size_t size,
    uint32_t timestamp,
    void *user_data)
{
    (void)pipeline;
    (void)timestamp;
    (void)user_data;
    
    if (g_socket >= 0 && g_remote_connected) {
        sendto(g_socket, (const char *)data, (int)size, 0,
            (struct sockaddr *)&g_remote_addr, sizeof(g_remote_addr));
    }
}

static void on_state_changed(
    voice_pipeline_t *pipeline,
    pipeline_state_t state,
    void *user_data)
{
    (void)pipeline;
    (void)user_data;
    
    const char *state_names[] = {
        "STOPPED", "STARTING", "RUNNING", "STOPPING", "ERROR"
    };
    printf("Pipeline state: %s\n", state_names[state]);
}

static void on_error(
    voice_pipeline_t *pipeline,
    voice_error_t error,
    const char *message,
    void *user_data)
{
    (void)pipeline;
    (void)user_data;
    
    fprintf(stderr, "Pipeline error %d: %s\n", error, message ? message : "");
}

/* ============================================
 * 网络接收线程
 * ============================================ */

#ifdef _WIN32
static DWORD WINAPI receive_thread(LPVOID arg)
#else
static void *receive_thread(void *arg)
#endif
{
    (void)arg;
    
    uint8_t buffer[MAX_PACKET_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    
    while (g_running) {
        int n = recvfrom(g_socket, (char *)buffer, MAX_PACKET_SIZE, 0,
            (struct sockaddr *)&sender_addr, &sender_len);
        
        if (n > 0 && g_pipeline) {
            /* 自动记录远端地址 */
            if (!g_remote_connected) {
                g_remote_addr = sender_addr;
                g_remote_connected = true;
                
                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sender_addr.sin_addr, addr_str, INET_ADDRSTRLEN);
                printf("Connected to: %s:%d\n", addr_str, ntohs(sender_addr.sin_port));
            }
            
            voice_pipeline_receive_packet(g_pipeline, buffer, n);
        }
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ============================================
 * 打印统计信息
 * ============================================ */

static void print_stats(void) {
    if (!g_pipeline) return;
    
    voice_pipeline_stats_t stats;
    if (voice_pipeline_get_stats(g_pipeline, &stats) == VOICE_OK) {
        printf("\r");
        printf("TX: %llu pkts | RX: %llu pkts | Lost: %llu (%.1f%%) | Jitter: %u ms | RTT: %u ms",
            (unsigned long long)stats.packets_sent,
            (unsigned long long)stats.packets_received,
            (unsigned long long)stats.packets_lost,
            stats.packet_loss_rate * 100,
            stats.jitter_ms,
            stats.rtt_ms);
        fflush(stdout);
    }
}

/* ============================================
 * 主程序
 * ============================================ */

int main(int argc, char *argv[])
{
    int local_port = DEFAULT_PORT;
    const char *remote_host = NULL;
    int remote_port = DEFAULT_PORT;
    bool enable_srtp = false;
    
    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            local_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            remote_host = argv[++i];
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            remote_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--srtp") == 0) {
            enable_srtp = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -p <port>      Local UDP port (default: %d)\n", DEFAULT_PORT);
            printf("  -c <host>      Remote host to connect to\n");
            printf("  -r <port>      Remote port (default: %d)\n", DEFAULT_PORT);
            printf("  --srtp         Enable SRTP encryption\n");
            printf("  -h, --help     Show this help\n");
            printf("\nExamples:\n");
            printf("  Server: %s -p 5004\n", argv[0]);
            printf("  Client: %s -p 5005 -c 192.168.1.100 -r 5004\n", argv[0]);
            return 0;
        }
    }
    
    printf("Voice Library - Voice Chat Example\n");
    printf("===================================\n");
    printf("Local port: %d\n", local_port);
    if (remote_host) {
        printf("Remote: %s:%d\n", remote_host, remote_port);
    } else {
        printf("Waiting for incoming connection...\n");
    }
    printf("SRTP: %s\n", enable_srtp ? "enabled" : "disabled");
    printf("\n");
    
    /* 初始化 WinSock */
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }
#endif
    
    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
#ifndef _WIN32
    signal(SIGTERM, signal_handler);
#endif
    
    /* 创建 UDP socket */
    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_socket < 0) {
        fprintf(stderr, "Failed to create socket\n");
        goto cleanup;
    }
    
    /* 绑定本地端口 */
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(local_port);
    
    if (bind(g_socket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        fprintf(stderr, "Failed to bind to port %d\n", local_port);
        goto cleanup;
    }
    
    /* 设置非阻塞超时 */
#ifdef _WIN32
    DWORD timeout = 100;
    setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    
    /* 设置远端地址 */
    if (remote_host) {
        memset(&g_remote_addr, 0, sizeof(g_remote_addr));
        g_remote_addr.sin_family = AF_INET;
        inet_pton(AF_INET, remote_host, &g_remote_addr.sin_addr);
        g_remote_addr.sin_port = htons(remote_port);
        g_remote_connected = true;
    }
    
    /* 配置管线 */
    voice_pipeline_config_t config;
    voice_pipeline_config_init(&config);
    config.mode = PIPELINE_MODE_DUPLEX;
    config.sample_rate = 48000;
    config.channels = 1;
    config.frame_duration_ms = 20;
    config.enable_aec = true;
    config.enable_denoise = true;
    config.enable_agc = true;
    config.denoise_engine = VOICE_DENOISE_SPEEX;
    config.codec = VOICE_CODEC_OPUS;
    config.bitrate = 32000;
    config.enable_fec = true;
    config.enable_srtp = enable_srtp;
    
    /* 创建管线 */
    g_pipeline = voice_pipeline_create(&config);
    if (!g_pipeline) {
        fprintf(stderr, "Failed to create pipeline\n");
        goto cleanup;
    }
    
    /* 设置回调 */
    voice_pipeline_set_encoded_callback(g_pipeline, on_encoded_data, NULL);
    voice_pipeline_set_state_callback(g_pipeline, on_state_changed, NULL);
    voice_pipeline_set_error_callback(g_pipeline, on_error, NULL);
    
    /* 如果启用 SRTP，设置测试密钥 (实际应用中应使用安全的密钥交换) */
    if (enable_srtp) {
        uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
        uint8_t salt[14] = {0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                           0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae};
        
        voice_pipeline_set_srtp_send_key(g_pipeline, key, 16, salt, 14);
        voice_pipeline_set_srtp_recv_key(g_pipeline, key, 16, salt, 14);
    }
    
    /* 启动接收线程 */
#ifdef _WIN32
    HANDLE recv_thread = CreateThread(NULL, 0, receive_thread, NULL, 0, NULL);
#else
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_thread, NULL);
#endif
    
    /* 启动管线 */
    voice_error_t err = voice_pipeline_start(g_pipeline);
    if (err != VOICE_OK) {
        fprintf(stderr, "Failed to start pipeline: %d\n", err);
        goto cleanup;
    }
    
    printf("Voice chat active. Press Ctrl+C to stop.\n\n");
    
    /* 主循环 */
    while (g_running) {
        sleep_ms(1000);
        print_stats();
    }
    
    printf("\n");
    
cleanup:
    /* 停止管线 */
    if (g_pipeline) {
        voice_pipeline_stop(g_pipeline);
        voice_pipeline_destroy(g_pipeline);
    }
    
    /* 关闭 socket */
    if (g_socket >= 0) {
        close(g_socket);
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    printf("Goodbye!\n");
    return 0;
}
