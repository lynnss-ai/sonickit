# 新增功能说明

本文档描述了语音库新增的高级功能模块。

## 目录

1. [AGC - 自动增益控制](#agc---自动增益控制)
2. [CNG - 舒适噪声生成](#cng---舒适噪声生成)
3. [DTMF - 双音多频信号](#dtmf---双音多频信号)
4. [Equalizer - 参数均衡器](#equalizer---参数均衡器)
5. [Compressor - 动态范围压缩器](#compressor---动态范围压缩器)
6. [ICE/STUN/TURN - NAT穿透](#icestunturn---nat穿透)
7. [Transport - 网络传输层](#transport---网络传输层)
8. [统计收集器](#统计收集器)
9. [音频录制/播放](#音频录制播放)

---

## AGC - 自动增益控制

### 概述

AGC (Automatic Gain Control) 自动调整音频信号的增益，确保输出电平保持在目标范围内。

### 特性

- **多种模式**:
  - Fixed: 固定增益
  - Adaptive: 自适应增益 (推荐)
  - Digital: 数字增益控制
  - Limiter: 仅限制器模式

- **压缩功能**:
  - 可配置的压缩阈值和比率
  - 低/中/高三档压缩强度

- **噪声门**:
  - 可配置的门限阈值
  - 避免放大静音时的噪声

- **限制器**:
  - 防止削波失真
  - 可配置的限制阈值

### 使用示例

```c
#include "dsp/agc.h"

// 创建 AGC
voice_agc_config_t config;
voice_agc_config_init(&config);
config.target_level_dbfs = -6.0f;
config.mode = VOICE_AGC_ADAPTIVE;

voice_agc_t *agc = voice_agc_create(&config);

// 处理音频
int16_t samples[160];
voice_agc_process(agc, samples, 160);

// 获取状态
voice_agc_state_t state;
voice_agc_get_state(agc, &state);
printf("Current gain: %.1f dB\n", state.current_gain_db);

// 清理
voice_agc_destroy(agc);
```

---

## CNG - 舒适噪声生成

### 概述

在静音期间生成低电平背景噪声,避免完全静音带来的不适感。符合 RFC 3389 规范。

### 噪声类型

- **White**: 白噪声 (平坦频谱)
- **Pink**: 粉红噪声 (1/f 频谱)
- **Brown**: 布朗噪声 (1/f² 频谱)
- **Spectral**: 基于输入信号频谱的噪声

### SID 帧

支持 RFC 3389 静音插入描述符 (SID) 帧的编解码:

```c
#include "dsp/comfort_noise.h"

voice_cng_config_t config;
voice_cng_config_init(&config);
config.noise_type = VOICE_CNG_PINK;
config.auto_level = true;

voice_cng_t *cng = voice_cng_create(&config);

// 分析输入 (VAD 检测到语音时)
voice_cng_analyze(cng, input_samples, 160);

// 生成舒适噪声 (VAD 检测到静音时)
int16_t noise[160];
voice_cng_generate(cng, noise, 160);

// 编码 SID 帧用于网络传输
voice_sid_frame_t sid;
voice_cng_encode_sid(cng, &sid);

voice_cng_destroy(cng);
```

---

## DTMF - 双音多频信号

### 概述

支持 DTMF (Dual-Tone Multi-Frequency) 信号的检测和生成。

### 检测器

使用 Goertzel 算法高效检测 DTMF 音调:

```c
#include "dsp/dtmf.h"

voice_dtmf_detector_config_t config;
voice_dtmf_detector_config_init(&config);
config.sample_rate = 8000;
config.min_on_time_ms = 40;

voice_dtmf_detector_t *det = voice_dtmf_detector_create(&config);

// 处理音频
int16_t samples[160];
voice_dtmf_result_t result;
voice_dtmf_digit_t digit = voice_dtmf_detector_process(det, samples, 160, &result);

if (digit != VOICE_DTMF_NONE) {
    printf("Detected: %c\n", (char)digit);
}

// 获取累积的数字
char buffer[64];
voice_dtmf_detector_get_digits(det, buffer, sizeof(buffer));
printf("Digits: %s\n", buffer);

voice_dtmf_detector_destroy(det);
```

### 生成器

```c
voice_dtmf_generator_config_t config;
voice_dtmf_generator_config_init(&config);
config.tone_duration_ms = 100;
config.pause_duration_ms = 100;

voice_dtmf_generator_t *gen = voice_dtmf_generator_create(&config);

// 生成单个数字
int16_t output[800];
size_t generated = voice_dtmf_generator_generate(gen, '5', output, 800);

// 生成序列
int16_t sequence[8000];
size_t len = voice_dtmf_generator_generate_sequence(gen, "123#", sequence, 8000);

voice_dtmf_generator_destroy(gen);
```

---

## Equalizer - 参数均衡器

### 概述

多频段参数均衡器 (Parametric EQ)，使用级联 Biquad 滤波器实现各类频率调整。

### 滤波器类型

- **Lowpass**: 低通滤波器
- **Highpass**: 高通滤波器
- **Bandpass**: 带通滤波器
- **Notch**: 陷波滤波器
- **Peak**: 峰值滤波器（参数均衡）
- **Lowshelf**: 低架滤波器
- **Highshelf**: 高架滤波器

### 预设模式

```c
typedef enum {
    VOICE_EQ_PRESET_FLAT,           // 平坦响应
    VOICE_EQ_PRESET_VOICE_ENHANCE,  // 语音增强
    VOICE_EQ_PRESET_TELEPHONE,      // 电话音质 (300-3400Hz)
    VOICE_EQ_PRESET_BASS_BOOST,     // 低音增强
    VOICE_EQ_PRESET_TREBLE_BOOST,   // 高音增强
    VOICE_EQ_PRESET_REDUCE_NOISE,   // 降噪倾向
    VOICE_EQ_PRESET_CLARITY         // 清晰度增强
} voice_eq_preset_t;
```

### 使用示例

```c
#include "dsp/equalizer.h"

// 使用预设创建均衡器
voice_eq_config_t config;
voice_eq_config_from_preset(&config, VOICE_EQ_PRESET_VOICE_ENHANCE, 16000);

voice_eq_t *eq = voice_eq_create(&config);

// 处理音频
int16_t samples[160];
voice_eq_process(eq, samples, 160);

// 动态调整频段
voice_eq_band_t band = {
    .enabled = true,
    .type = VOICE_EQ_PEAK,
    .frequency = 2000.0f,
    .gain_db = 4.0f,
    .q = 1.5f
};
voice_eq_set_band(eq, 0, &band);

// 获取频率响应曲线
float freqs[] = {100, 500, 1000, 2000, 4000, 8000};
float responses[6];
voice_eq_get_frequency_response(eq, freqs, responses, 6);

voice_eq_destroy(eq);
```

---

## Compressor - 动态范围压缩器

### 概述

动态范围处理器，包括压缩器、扩展器、限制器和噪声门功能。

### 处理器类型

- **Compressor**: 压缩器 - 减少动态范围，使响度更均匀
- **Expander**: 扩展器 - 增加动态范围
- **Limiter**: 限制器 - 硬限制峰值防止削波
- **Gate**: 噪声门 - 静音低于阈值的信号

### 压缩器配置

```c
#include "dsp/compressor.h"

voice_compressor_config_t config;
voice_compressor_config_init(&config);
config.threshold_db = -20.0f;   // 阈值
config.ratio = 4.0f;            // 压缩比 4:1
config.attack_ms = 10.0f;       // 启动时间
config.release_ms = 100.0f;     // 释放时间
config.knee_type = VOICE_DRC_SOFT_KNEE;
config.auto_makeup = true;      // 自动补偿增益

voice_compressor_t *comp = voice_compressor_create(&config);

// 处理音频
int16_t samples[160];
voice_compressor_process(comp, samples, 160);

// 获取状态
voice_compressor_state_t state;
voice_compressor_get_state(comp, &state);
printf("Gain reduction: %.1f dB\n", state.gain_reduction_db);

voice_compressor_destroy(comp);
```

### 限制器配置

```c
voice_compressor_config_t config;
voice_limiter_config_init(&config);  // 使用限制器预设
config.threshold_db = -1.0f;         // 接近 0dBFS

voice_compressor_t *limiter = voice_compressor_create(&config);
```

### 侧链压缩

支持外部侧链信号控制压缩：

```c
int16_t main_audio[160];
int16_t sidechain[160];  // 控制信号

// 使用侧链信号控制主音频的压缩
voice_compressor_process_sidechain(comp, main_audio, sidechain, 160);
```

---

## ICE/STUN/TURN - NAT穿透

### 概述

实现 NAT 穿透功能,支持:
- **ICE**: Interactive Connectivity Establishment (RFC 5245)
- **STUN**: Session Traversal Utilities for NAT (RFC 5389)
- **TURN**: Traversal Using Relays around NAT (RFC 5766)

### STUN 绑定

获取公网映射地址:

```c
#include "network/ice.h"

voice_stun_config_t config;
voice_stun_config_init(&config);
strcpy(config.server, "stun.l.google.com");
config.port = 19302;

voice_stun_client_t *stun = voice_stun_client_create(&config);

voice_ice_candidate_t result;
if (voice_stun_binding_request(stun, &result) == VOICE_SUCCESS) {
    printf("Public address: %s:%u\n", result.address, result.port);
}

voice_stun_client_destroy(stun);
```

### ICE Agent

完整的 ICE 协商:

```c
voice_ice_config_t config;
voice_ice_config_init(&config);
config.mode = VOICE_ICE_FULL;

// 添加 STUN 服务器
strcpy(config.stun_servers[0], "stun.l.google.com:19302");
config.stun_server_count = 1;

voice_ice_agent_t *agent = voice_ice_agent_create(&config);

// 收集候选
voice_ice_gather_candidates(agent);

// 获取本地候选
voice_ice_candidate_t candidates[16];
size_t count = voice_ice_get_local_candidates(agent, candidates, 16);

// 转换为 SDP
char sdp[256];
for (size_t i = 0; i < count; i++) {
    voice_ice_candidate_to_sdp(&candidates[i], sdp, sizeof(sdp));
    printf("a=%s\n", sdp);
}

// 添加远程候选 (从对端接收)
voice_ice_candidate_t remote;
voice_ice_candidate_from_sdp("candidate:host0 1 UDP 2130706431 192.168.1.100 5000 typ host", &remote);
voice_ice_add_remote_candidate(agent, &remote);

// 开始连接检查
voice_ice_start(agent);

voice_ice_agent_destroy(agent);
```

---

## Transport - 网络传输层

### 概述

网络传输层抽象，封装 UDP/TCP Socket 操作，提供统一的网络接口。

### 传输类型

- **UDP**: 无连接数据报传输（实时语音首选）
- **TCP**: 面向连接的流传输
- **TLS**: 加密 TCP 传输
- **DTLS**: 加密 UDP 传输

### 使用示例

```c
#include "network/transport.h"

// 创建 UDP 传输
voice_transport_config_t config;
voice_transport_config_init(&config);
config.type = VOICE_TRANSPORT_UDP;
config.non_blocking = true;
config.tos = 0xB8;  // EF DSCP for voice

voice_transport_t *transport = voice_transport_create(&config);

// 绑定本地端口
voice_transport_bind(transport, NULL, 5000);

// 连接到远端（设置默认目标）
voice_transport_connect(transport, "192.168.1.100", 5000);

// 发送数据
uint8_t packet[172];
voice_transport_send(transport, packet, sizeof(packet));

// 接收数据
uint8_t buffer[1500];
voice_net_address_t from;
ssize_t received = voice_transport_recvfrom(transport, buffer, sizeof(buffer), &from);

// 获取统计
voice_transport_stats_t stats;
voice_transport_get_stats(transport, &stats);
printf("Sent: %llu packets, %llu bytes\n", 
    stats.packets_sent, stats.bytes_sent);

voice_transport_destroy(transport);
```

### 事件驱动模式

```c
// 配置回调
void on_receive(const uint8_t *data, size_t size,
                const voice_net_address_t *from, void *user_data) {
    printf("Received %zu bytes from %s:%u\n", 
           size, from->address, from->port);
}

config.on_receive = on_receive;
config.callback_user_data = my_context;

voice_transport_t *transport = voice_transport_create(&config);
voice_transport_bind(transport, NULL, 5000);

// 事件循环
while (running) {
    voice_transport_poll(transport, 100);  // 100ms 超时
}
```

### QoS 设置

为实时语音设置适当的 DSCP 标记：

```c
// EF (Expedited Forwarding) - 推荐用于语音
voice_transport_set_qos(transport, 0xB8);

// AF41 (Assured Forwarding) - 视频
// voice_transport_set_qos(transport, 0x88);
```

---

## 统计收集器

### 概述

全面收集和分析语音通话的各项统计指标。

### 统计类型

- **音频统计**: 电平、帧数、丢帧
- **编解码器统计**: 编解码帧数、比特率
- **网络统计**: 丢包、抖动、RTT、带宽
- **会话统计**: MOS、R-Factor、质量等级

### 使用示例

```c
#include "voice/statistics.h"

voice_stats_config_t config;
voice_stats_config_init(&config);
config.snapshot_interval_ms = 1000;
config.auto_calculate_quality = true;

voice_stats_collector_t *stats = voice_stats_collector_create(&config);

// 更新统计
voice_stats_add_packet(stats, 172, true);   // 发送
voice_stats_add_packet(stats, 172, false);  // 接收
voice_stats_add_loss(stats, 1);             // 丢包
voice_stats_add_rtt(stats, 50);             // RTT

// 定期快照
voice_stats_take_snapshot(stats);

// 获取会话统计
voice_session_stats_t session;
voice_stats_get_session(stats, &session);
printf("MOS: %.2f, Quality: %s\n", 
    session.mos,
    session.quality == VOICE_QUALITY_GOOD ? "Good" : "Fair");

// 导出 JSON
char json[1024];
voice_stats_export_json(stats, json, sizeof(json));
printf("%s\n", json);

voice_stats_collector_destroy(stats);
```

### 质量评估

基于 E-Model (ITU-T G.107) 计算 R-Factor 和 MOS:

| 质量等级 | R-Factor | MOS |
|---------|----------|-----|
| Excellent | ≥ 90 | ≥ 4.3 |
| Good | 80-90 | 4.0-4.3 |
| Fair | 70-80 | 3.6-4.0 |
| Poor | 60-70 | 2.6-3.6 |
| Bad | < 60 | < 2.6 |

---

## 音频录制/播放

### 概述

提供音频录制到文件或内存,以及音频文件播放功能。

### 录制器

```c
#include "audio/audio_recorder.h"

voice_recorder_config_t config;
voice_recorder_config_init(&config);
config.format = VOICE_RECORD_WAV;
config.sample_rate = 16000;
config.max_duration_ms = 60000;  // 最多60秒

voice_recorder_t *rec = voice_recorder_create(&config);

// 开始录制到文件
voice_recorder_start(rec, "recording.wav");

// 写入音频数据
int16_t samples[160];
voice_recorder_write(rec, samples, 160);

// 获取状态
voice_recorder_status_t status;
voice_recorder_get_status(rec, &status);
printf("Duration: %u ms\n", status.duration_ms);

// 停止录制
voice_recorder_stop(rec);

voice_recorder_destroy(rec);
```

### 内存录制 (环形缓冲区)

```c
voice_recorder_config_t config;
voice_recorder_config_init(&config);
config.circular_buffer = true;
config.buffer_size = 16000 * 10;  // 10秒缓冲区

voice_recorder_t *rec = voice_recorder_create(&config);
voice_recorder_start(rec, NULL);  // 无文件名表示录制到内存

// ... 写入数据 ...

// 获取缓冲区内容
int16_t buffer[160000];
size_t count = 160000;
voice_recorder_get_buffer(rec, buffer, &count);
```

### 播放器

```c
voice_player_config_t config;
voice_player_config_init(&config);
config.volume = 0.8f;
config.loop = false;

voice_player_t *player = voice_player_create_from_file("audio.wav", &config);

// 播放控制
voice_player_play(player);
voice_player_pause(player);
voice_player_seek(player, 5000);  // 跳转到5秒

// 调整参数
voice_player_set_volume(player, 1.0f);
voice_player_set_speed(player, 1.5f);  // 1.5倍速

// 读取音频数据 (用于输出)
int16_t output[160];
size_t read;
voice_player_read(player, output, 160, &read);

// 状态
voice_player_status_t status;
voice_player_get_status(player, &status);
printf("Position: %u/%u ms\n", status.position_ms, status.total_duration_ms);

voice_player_destroy(player);
```

---

## 集成建议

### Pipeline 集成顺序

典型的音频处理 Pipeline:

```
输入 -> AGC -> VAD -> [Denoiser] -> [AEC] -> Encoder
                 |
                 v (静音时)
                CNG
```

### 统计集成

在关键节点收集统计:

```c
// 编码后
voice_stats_update_codec(stats, &codec_stats);

// 发送后
voice_stats_add_packet(stats, packet_size, true);

// 接收后
voice_stats_add_packet(stats, packet_size, false);
voice_stats_add_rtt(stats, rtt);

// 定期
voice_stats_take_snapshot(stats);
```

---

## 性能考虑

| 模块 | CPU 开销 | 内存 | 说明 |
|------|---------|------|------|
| AGC | 低 | ~1 KB | 每样本简单计算 |
| CNG | 极低 | ~256 B | 噪声生成器状态 |
| DTMF 检测 | 低 | ~512 B | Goertzel 滤波器状态 |
| DTMF 生成 | 极低 | ~128 B | 相位累加器 |
| Equalizer | 低-中 | ~2 KB | 取决于频段数量 |
| Compressor | 低 | ~512 B | 包络跟踪状态 |
| ICE Agent | 中 | ~4 KB | 候选管理 |
| Transport | 极低 | ~1 KB | Socket 封装 |
| 统计收集器 | 极低 | ~8 KB | 含快照历史 |
| 录制器 | 低 | 可配置 | 取决于缓冲区大小 |
| 播放器 | 低 | ~1 KB | 文件读取缓冲 |

---

## 完整 Pipeline 示例

以下展示了一个完整的语音处理 Pipeline:

```c
// 输入链
input -> equalizer -> agc -> vad -> denoiser -> aec -> compressor -> encoder

// 输出链  
decoder -> jitter_buffer -> plc -> denoiser -> equalizer -> output

// 网络层
rtp <-> srtp <-> transport <-> ice
```

### 初始化示例

```c
#include "voice/voice.h"

// 创建所有处理器
voice_eq_t *eq_in = voice_eq_create(&eq_config);
voice_agc_t *agc = voice_agc_create(&agc_config);
voice_vad_t *vad = voice_vad_create(&vad_config);
voice_compressor_t *comp = voice_compressor_create(&comp_config);
voice_transport_t *transport = voice_transport_create(&net_config);
voice_stats_collector_t *stats = voice_stats_collector_create(&stats_config);

// 处理循环
while (running) {
    // 1. 从设备读取
    voice_device_read(device, samples, frame_size);
    
    // 2. 输入均衡
    voice_eq_process(eq_in, samples, frame_size);
    
    // 3. AGC
    voice_agc_process(agc, samples, frame_size);
    
    // 4. VAD
    bool is_speech = voice_vad_process(vad, samples, frame_size);
    
    // 5. 压缩器
    voice_compressor_process(comp, samples, frame_size);
    
    // 6. 编码 & 发送
    if (is_speech) {
        voice_codec_encode(codec, samples, frame_size, packet, &packet_size);
        voice_transport_send(transport, packet, packet_size);
        voice_stats_add_packet(stats, packet_size, true);
    } else {
        // 发送 CNG SID 帧
        voice_cng_encode_sid(cng, &sid);
        // ...
    }
}
```
