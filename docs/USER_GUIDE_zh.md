# SonicKit 用户指南

SonicKit 实时音频处理库的完整使用指南。

## 目录

1. [简介](#简介)
2. [安装](#安装)
3. [快速开始](#快速开始)
4. [核心模块](#核心模块)
5. [音频设备管理](#音频设备管理)
6. [DSP 处理](#dsp-处理)
7. [编解码器使用](#编解码器使用)
8. [网络传输](#网络传输)
9. [管线集成](#管线集成)
10. [平台适配](#平台适配)
11. [性能优化](#性能优化)
12. [常见问题](#常见问题)
13. [术语表](#术语表)

---

## 简介

SonicKit 是一个使用纯 C 语言编写的跨平台实时音频处理库。它为语音通信应用提供完整的解决方案，包括：

- **音频 I/O**：在所有主流平台上进行采集和播放
- **音频处理**：降噪、回声消除、AGC、重采样
- **编解码支持**：Opus、G.711、G.722
- **网络传输**：RTP/RTCP、SRTP、抖动缓冲、ICE/STUN/TURN
- **实用工具**：混音器、录音机、质量分析

### 设计理念

- **纯 C 实现**：最大兼容性，易于与其他语言集成
- **模块化架构**：按需使用
- **跨平台**：Windows、macOS、Linux、iOS、Android、WebAssembly
- **低延迟**：针对实时语音通信优化
- **线程安全**：安全的多线程使用

---

## 安装

### 前置要求

- CMake 3.16 或更高版本
- C11 兼容编译器（GCC 4.9+、Clang 3.4+、MSVC 2015+、MinGW）

### 从源码构建

```bash
# 克隆仓库
git clone https://github.com/your-repo/sonickit.git
cd sonickit

# 创建构建目录
mkdir build && cd build

# 配置
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build . --config Release

# 安装（可选）
cmake --install . --prefix /usr/local
```

### CMake 选项

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `SONICKIT_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `SONICKIT_BUILD_TESTS` | ON | 构建单元测试 |
| `SONICKIT_ENABLE_OPUS` | ON | 启用 Opus 编解码器 |
| `SONICKIT_ENABLE_G722` | OFF | 启用 G.722 编解码器 |
| `SONICKIT_ENABLE_RNNOISE` | ON | 启用 RNNoise 神经网络降噪器 |
| `SONICKIT_ENABLE_SRTP` | ON | 启用 SRTP 加密 |
| `SONICKIT_ENABLE_DTLS` | ON | 启用 DTLS-SRTP 密钥交换 |

### 在项目中链接

```cmake
find_package(SonicKit REQUIRED)
target_link_libraries(your_app PRIVATE SonicKit::SonicKit)
```

---

## 快速开始

### 最小示例：音频采集

```c
#include "voice/voice.h"
#include "audio/device.h"
#include <stdio.h>

// 采集音频的回调函数
void on_capture(voice_device_t *dev, const int16_t *samples,
                size_t count, void *user_data) {
    printf("采集了 %zu 个样本\n", count);
}

int main() {
    // 初始化库
    voice_init(NULL);

    // 配置设备
    voice_device_config_t config;
    voice_device_config_init(&config);
    config.mode = VOICE_DEVICE_MODE_CAPTURE;
    config.sample_rate = 48000;
    config.channels = 1;
    config.frame_size = 480;  // 48kHz 时 10ms
    config.capture_callback = on_capture;

    // 创建并启动设备
    voice_device_t *device = voice_device_create(&config);
    if (!device) {
        fprintf(stderr, "创建设备失败\n");
        return 1;
    }

    voice_device_start(device);

    // 采集 5 秒
    voice_sleep_ms(5000);

    // 清理
    voice_device_stop(device);
    voice_device_destroy(device);
    voice_deinit();

    return 0;
}
```

### 最小示例：降噪

```c
#include "dsp/denoiser.h"

// 创建降噪器
voice_denoiser_config_t config;
voice_denoiser_config_init(&config);
config.sample_rate = 16000;
config.frame_size = 160;  // 10ms 帧
config.engine = VOICE_DENOISE_RNNOISE;

voice_denoiser_t *denoiser = voice_denoiser_create(&config);

// 处理音频帧
int16_t frame[160];
// ... 用音频数据填充 frame ...
voice_denoiser_process(denoiser, frame, 160);

// 清理
voice_denoiser_destroy(denoiser);
```

---

## 核心模块

### 模块依赖关系

```
voice/voice.h          <- 主 API 入口
    |
    +-- audio/device.h       <- 音频 I/O
    +-- dsp/denoiser.h       <- 降噪
    +-- dsp/echo_canceller.h <- 回声消除
    +-- dsp/agc.h            <- 自动增益控制
    +-- dsp/resampler.h      <- 采样率转换
    +-- codec/codec.h        <- 音频编解码
    +-- network/rtp.h        <- RTP 打包
    +-- network/srtp.h       <- SRTP 加密
    +-- voice/pipeline.h     <- 完整处理管线
```

### 初始化

在使用任何模块之前，始终先初始化库：

```c
#include "voice/voice.h"

int main() {
    // 可选：配置日志
    voice_global_config_t global_config;
    voice_global_config_init(&global_config);
    global_config.log_level = VOICE_LOG_DEBUG;

    // 初始化
    voice_error_t err = voice_init(&global_config);
    if (err != VOICE_OK) {
        fprintf(stderr, "初始化失败: %s\n", voice_error_string(err));
        return 1;
    }

    // ... 使用库 ...

    // 清理
    voice_deinit();
    return 0;
}
```

---

## 音频设备管理

### 枚举设备

```c
#include "audio/device.h"

void list_devices() {
    voice_device_info_t devices[16];
    size_t count = 16;

    // 列出采集设备
    voice_device_enumerate(VOICE_DEVICE_MODE_CAPTURE, devices, &count);
    printf("采集设备:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  [%zu] %s (ID: %s)\n", i, devices[i].name, devices[i].id);
    }

    // 列出播放设备
    count = 16;
    voice_device_enumerate(VOICE_DEVICE_MODE_PLAYBACK, devices, &count);
    printf("播放设备:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  [%zu] %s (ID: %s)\n", i, devices[i].name, devices[i].id);
    }
}
```

### 全双工音频

```c
// 采集回调
void on_capture(voice_device_t *dev, const int16_t *input,
                size_t samples, void *user_data) {
    // 处理采集的音频
}

// 播放回调
void on_playback(voice_device_t *dev, int16_t *output,
                 size_t samples, void *user_data) {
    // 填充输出缓冲区
}

// 配置双工设备
voice_device_config_t config;
voice_device_config_init(&config);
config.mode = VOICE_DEVICE_MODE_DUPLEX;
config.sample_rate = 48000;
config.channels = 1;
config.frame_size = 480;
config.capture_callback = on_capture;
config.playback_callback = on_playback;
config.user_data = my_context;

voice_device_t *device = voice_device_create(&config);
voice_device_start(device);
```

---

## DSP 处理

### 降噪

SonicKit 支持两种降噪引擎：

| 引擎 | 描述 | 适用场景 |
|------|------|----------|
| `VOICE_DENOISE_SPEEX` | SpeexDSP 频谱减法 | 低 CPU，基本噪声 |
| `VOICE_DENOISE_RNNOISE` | 基于 RNN 的神经网络 | 高质量，复杂噪声 |

```c
voice_denoiser_config_t config;
voice_denoiser_config_init(&config);
config.sample_rate = 48000;
config.frame_size = 480;
config.engine = VOICE_DENOISE_RNNOISE;
config.denoise_level = 80;  // 0-100

voice_denoiser_t *dn = voice_denoiser_create(&config);

// 原地处理
voice_denoiser_process(dn, pcm_frame, frame_size);

// 获取降噪量
float reduction_db = voice_denoiser_get_reduction(dn);
```

### 回声消除 (AEC)

```c
voice_aec_config_t config;
voice_aec_config_init(&config);
config.sample_rate = 16000;
config.frame_size = 160;
config.filter_length_ms = 200;  // 回声尾长度
config.nlp_mode = VOICE_AEC_NLP_MODERATE;

voice_aec_t *aec = voice_aec_create(&config);

// 处理：带播放参考的采集
int16_t output[160];
voice_aec_process(aec, capture_frame, playback_frame, output, 160);
```

### 自动增益控制 (AGC)

```c
voice_agc_config_t config;
voice_agc_config_init(&config);
config.sample_rate = 16000;
config.frame_size = 160;
config.mode = VOICE_AGC_ADAPTIVE_DIGITAL;
config.target_level_dbfs = -3.0f;

voice_agc_t *agc = voice_agc_create(&config);

// 原地处理
voice_agc_process(agc, pcm_frame, frame_size);

// 获取当前增益
float gain = voice_agc_get_gain(agc);
```

### 重采样

```c
voice_resampler_config_t config;
voice_resampler_config_init(&config);
config.input_rate = 48000;
config.output_rate = 16000;
config.channels = 1;
config.quality = 5;  // 0-10，越高质量越好

voice_resampler_t *resampler = voice_resampler_create(&config);

// 计算输出大小
int output_frames = (input_frames * 16000) / 48000;
int16_t output[output_frames];
int actual_output;

voice_resampler_process(resampler, input, input_frames,
                        output, output_frames, &actual_output);
```

---

## 编解码器使用

### Opus 编解码

```c
// 编码器
voice_encoder_config_t enc_config;
voice_encoder_config_init(&enc_config);
enc_config.codec = VOICE_CODEC_OPUS;
enc_config.sample_rate = 48000;
enc_config.channels = 1;
enc_config.bitrate = 32000;
enc_config.application = VOICE_OPUS_APP_VOIP;

voice_encoder_t *encoder = voice_encoder_create(&enc_config);

// 编码
uint8_t encoded[256];
int encoded_size = voice_encoder_encode(encoder, pcm_frame, 960,
                                        encoded, sizeof(encoded));

// 解码器
voice_decoder_config_t dec_config;
voice_decoder_config_init(&dec_config);
dec_config.codec = VOICE_CODEC_OPUS;
dec_config.sample_rate = 48000;
dec_config.channels = 1;

voice_decoder_t *decoder = voice_decoder_create(&dec_config);

// 解码
int16_t decoded[960];
int decoded_samples = voice_decoder_decode(decoder, encoded, encoded_size,
                                           decoded, 960);
```

---

## 网络传输

### RTP 会话

```c
// 创建 RTP 会话
rtp_session_config_t config;
rtp_session_config_init(&config);
config.payload_type = 111;  // Opus
config.clock_rate = 48000;

rtp_session_t *rtp = rtp_session_create(&config);

// 创建 RTP 数据包
uint8_t packet[1500];
size_t packet_size;
rtp_session_create_packet(rtp, encoded_data, encoded_size,
                          timestamp, false, packet, &packet_size);

// 解析接收的数据包
rtp_packet_t parsed;
rtp_session_parse_packet(rtp, received_data, received_size, &parsed);
```

### 抖动缓冲

```c
jitter_buffer_config_t config;
jitter_buffer_config_init(&config);
config.sample_rate = 48000;
config.frame_size = 960;
config.min_delay_ms = 20;
config.max_delay_ms = 200;

jitter_buffer_t *jb = jitter_buffer_create(&config);

// 推入接收的数据包
jitter_buffer_push(jb, timestamp, encoded_data, encoded_size);

// 拉取用于播放的帧
uint8_t frame_data[256];
size_t frame_size;
jitter_buffer_status_t status;
jitter_buffer_pull(jb, frame_data, &frame_size, &status);
```

---

## 管线集成

管线模块提供完整的集成解决方案：

```c
voice_pipeline_config_t config;
voice_pipeline_config_init(&config);
config.mode = PIPELINE_MODE_DUPLEX;
config.sample_rate = 48000;
config.channels = 1;
config.frame_duration_ms = 20;

// 启用处理
config.enable_aec = true;
config.enable_denoise = true;
config.enable_agc = true;
config.denoise_engine = VOICE_DENOISE_RNNOISE;

// 配置编解码器
config.codec = VOICE_CODEC_OPUS;
config.bitrate = 32000;
config.enable_fec = true;

// 网络
config.enable_srtp = true;

voice_pipeline_t *pipeline = voice_pipeline_create(&config);

// 设置回调
voice_pipeline_set_encoded_callback(pipeline, on_encoded_packet, user_data);
voice_pipeline_set_state_callback(pipeline, on_state_change, user_data);

// 启动
voice_pipeline_start(pipeline);

// 输入接收的网络数据
voice_pipeline_receive_packet(pipeline, packet_data, packet_size);

// 停止并清理
voice_pipeline_stop(pipeline);
voice_pipeline_destroy(pipeline);
```

---

## 平台适配

### iOS

```objc
// 配置音频会话
voice_session_config_t config;
voice_session_config_init(&config);
config.category = VOICE_SESSION_CATEGORY_PLAY_AND_RECORD;
config.mode = VOICE_SESSION_MODE_VOICE_CHAT;
config.options = VOICE_SESSION_OPTION_DEFAULT_TO_SPEAKER;

voice_session_configure(&config);
voice_session_activate();

// 处理中断
void on_interrupt(voice_session_interruption_t type, void *user_data) {
    if (type == VOICE_SESSION_INTERRUPTION_BEGAN) {
        voice_pipeline_stop(pipeline);
    } else {
        voice_pipeline_start(pipeline);
    }
}
voice_session_set_interrupt_callback(on_interrupt, user_data);
```

### Android

```java
// Java 初始化
public class SonicKitLib {
    static {
        System.loadLibrary("sonickit");
    }

    public static native void nativeInit(Context context);
    public static native void nativeRelease();
}

// 在应用启动时调用
SonicKitLib.nativeInit(getApplicationContext());
```

### WebAssembly

SonicKit 支持编译为 WebAssembly，在浏览器中运行实时音频处理。

**构建 WASM:**

```bash
# 安装 Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh  # Linux/macOS
# 或 emsdk_env.bat      # Windows

# 构建
cd sonickit/wasm
mkdir build && cd build
emcmake cmake .. -DWASM_ENABLE_OPUS=OFF -DWASM_ENABLE_RNNOISE=OFF
emmake make -j8
```

**浏览器使用:**

```html
<script src="sonickit.js"></script>
<script>
// 加载模块
Module().then(sonicKit => {
    // 创建降噪器 (48kHz, 480 样本/帧)
    const denoiser = new sonicKit.Denoiser(48000, 480, 0);

    // 处理音频 (Int16Array)
    const input = new Int16Array(480);
    // ... 填充音频数据 ...
    const output = denoiser.process(input);

    // 清理
    denoiser.delete();
});
</script>
```

**可用的 JavaScript API 类:**

| 类名 | 构造函数 | 方法 |
|------|----------|------|
| `Denoiser` | `(sampleRate, frameSize, engine)` | `process()`, `reset()` |
| `EchoCanceller` | `(sampleRate, frameSize, filterLen)` | `process()`, `reset()` |
| `AGC` | `(sampleRate, frameSize, mode, target)` | `process()`, `getGain()`, `reset()` |
| `VAD` | `(sampleRate, frameSize, mode)` | `isSpeech()`, `getProbability()`, `reset()` |
| `Resampler` | `(channels, inRate, outRate, quality)` | `process()`, `reset()` |
| `G711Codec` | `(useAlaw)` | `encode()`, `decode()` |

更多详情请参阅 [wasm/README.md](../wasm/README.md)。

---

## 性能优化

### CPU 优化

1. **帧大小**：较大的帧减少开销但增加延迟
2. **质量设置**：降低重采样器质量可减少 CPU 使用
3. **引擎选择**：SpeexDSP 比 RNNoise 使用更少的 CPU
4. **SIMD**：使用 `-DVOICE_ENABLE_SIMD=ON` 启用

### 内存优化

1. **缓冲区大小**：根据实际需求匹配缓冲区大小
2. **对象池**：重用编码器/解码器实例
3. **静态分配**：使用 `_preallocated` 变体（如果可用）

### 延迟优化

1. **帧时长**：使用 10-20ms 的帧以获得低延迟
2. **抖动缓冲**：根据网络情况配置最小延迟
3. **处理顺序**：AEC -> 降噪 -> AGC 是最佳顺序

---

## 常见问题

### 问：为什么回声消除不起作用？

答：确保您向 AEC 提供了正确的播放参考信号。播放信号必须与通过扬声器输出的信号完全匹配。

### 问：如何减少 CPU 使用？

答：尝试使用 SpeexDSP 代替 RNNoise，降低重采样器质量，并使用较大的帧大小。

### 问：推荐的采样率是多少？

答：Opus 编解码器使用 48000 Hz，G.711/G.722 使用 16000 Hz。使用重采样器在不同采样率之间转换。

### 问：如何处理网络丢包？

答：在 Opus 编码器中启用 FEC，并使用抖动缓冲的 PLC 功能。

---

## 术语表

| 术语 | 英文全称 | 中文说明 |
|------|----------|----------|
| AEC | Acoustic Echo Cancellation | 声学回声消除，从麦克风输入中消除扬声器回声 |
| AGC | Automatic Gain Control | 自动增益控制，自动调节音频音量 |
| CNG | Comfort Noise Generation | 舒适噪声生成，在静音期间生成背景噪声 |
| DTMF | Dual-Tone Multi-Frequency | 双音多频，电话拨号音 |
| FEC | Forward Error Correction | 前向纠错，丢包错误恢复 |
| ICE | Interactive Connectivity Establishment | 交互式连接建立，NAT 穿透协议 |
| MOS | Mean Opinion Score | 平均意见得分，音频质量指标（1-5） |
| PLC | Packet Loss Concealment | 丢包隐藏，隐藏丢包的影响 |
| RTP | Real-time Transport Protocol | 实时传输协议，媒体流协议 |
| SRTP | Secure RTP | 安全 RTP，加密的 RTP |
| STUN | Session Traversal Utilities for NAT | NAT 会话穿越工具，NAT 穿透 |
| TURN | Traversal Using Relays around NAT | 使用中继穿越 NAT，用于 NAT 穿透的中继服务器 |
| VAD | Voice Activity Detection | 语音活动检测，检测语音存在 |

---

更多信息，请参阅：
- [API 参考](API_REFERENCE.md)
- [示例代码](../examples/)
- [源代码](../src/)
