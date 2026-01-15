# SonicKit

跨平台实时音频处理库，使用纯C语言编写。

## 特性

### 音频处理
- 🎙️ **音频采集/播放** - 基于 miniaudio，支持所有主流平台
- 🔇 **降噪** - 双引擎 (SpeexDSP + RNNoise 神经网络)
- 📢 **回声消除** (AEC) - SpeexDSP 实现
- 🎚️ **自动增益控制** (AGC) - 固定/自适应/数字增益模式
- 🔄 **高质量重采样** - SpeexDSP 重采样器 (质量 0-10)
- 🎙️ **语音活动检测** (VAD) - 能量/过零率/谱熵分析
- 🎛️ **音频电平监测** - RMS/峰值/VU/LUFS 多种测量
- 📈 **音频质量分析** - POLQA/PESQ 风格 MOS 评分

### DSP 处理
- 🔊 **均衡器** - 多频段参数均衡，7种滤波器类型，预设配置
- 📉 **动态压缩器** - 压缩器/限制器/扩展器/门控，软拐点
- 📞 **DTMF** - 双音多频检测 (Goertzel算法) 和生成
- 🔉 **舒适噪声生成** (CNG) - 白/粉/棕/谱匹配噪声，RFC 3389 SID 帧

### 编解码器
- 🎵 **Opus** - 高质量语音/音乐编码 (6-510 kbps)
- 📞 **G.711** - A-law/μ-law 传统电话编码
- 📡 **G.722** - 宽带语音编码 (16kHz)

### 网络传输
- 📦 **RTP/RTCP** - 实时传输协议
- 🔐 **SRTP** - 加密传输 (AES-CM/AES-GCM)
- 📊 **自适应抖动缓冲** - 动态延迟调整
- 🔧 **丢包补偿** (PLC) - 多种算法
- 📶 **带宽估计** - GCC/REMB/BBR 拥塞控制
- 🌐 **ICE/STUN/TURN** - NAT 穿透和连接建立
- 🚀 **传输层抽象** - UDP/TCP 套接字封装，QoS/DSCP

### 音频工具
- 🎧 **音频混音器** - 多流混合，独立增益/静音
- 📼 **音频录制器** - WAV/RAW 文件录制，内存缓冲，回放控制
- 📊 **统计收集器** - 音频/编解码器/网络统计，MOS/R-Factor 计算

### 文件 I/O
- 📁 **WAV** - 读写支持
- 🎶 **MP3** - 解码支持
- 🎼 **FLAC** - 无损音频支持

### 平台支持
| 平台 | 音频后端 | 状态 |
|------|---------|------|
| Windows | WASAPI | ✅ |
| macOS | Core Audio | ✅ |
| Linux | ALSA/PulseAudio | ✅ |
| iOS | AVAudioSession + Core Audio | ✅ |
| Android | AAudio/OpenSL ES | ✅ |
| **WebAssembly** | Emscripten | ✅ |

## 项目结构

```
sonickit/
├── include/
│   ├── voice/           # 核心头文件
│   │   ├── types.h      # 基础类型定义
│   │   ├── error.h      # 错误码和日志
│   │   ├── config.h     # 配置结构
│   │   ├── voice.h      # 主 API
│   │   ├── pipeline.h   # 处理管线
│   │   ├── platform.h   # 平台抽象
│   │   └── statistics.h # 统计收集
│   ├── audio/           # 音频模块
│   │   ├── audio_buffer.h    # 环形缓冲区
│   │   ├── device.h          # 设备管理
│   │   ├── file_io.h         # 文件读写
│   │   ├── audio_mixer.h     # 音频混音
│   │   ├── audio_level.h     # 电平监测
│   │   ├── audio_quality.h   # 质量分析
│   │   └── audio_recorder.h  # 录制回放
│   ├── dsp/             # DSP 模块
│   │   ├── resampler.h       # 重采样
│   │   ├── denoiser.h        # 降噪
│   │   ├── echo_canceller.h  # 回声消除
│   │   ├── vad.h             # 语音活动检测
│   │   ├── agc.h             # 自动增益控制
│   │   ├── comfort_noise.h   # 舒适噪声
│   │   ├── dtmf.h            # DTMF检测/生成
│   │   ├── equalizer.h       # 参数均衡器
│   │   └── compressor.h      # 动态压缩器
│   ├── codec/           # 编解码器
│   │   ├── codec.h           # 编解码接口
│   │   ├── g711.h            # G.711 编解码
│   │   ├── g722.h            # G.722 编解码
│   │   └── opus.h            # Opus 编解码
│   ├── network/         # 网络模块
│   │   ├── rtp.h             # RTP 协议
│   │   ├── srtp.h            # SRTP 加密
│   │   ├── jitter_buffer.h   # 抖动缓冲
│   │   ├── bandwidth_estimator.h  # 带宽估计
│   │   ├── ice.h             # ICE/STUN/TURN
│   │   └── transport.h       # 传输层抽象
│   ├── sip/             # SIP 协议模块
│   │   ├── sip_core.h        # SIP 核心协议
│   │   └── sip_ua.h          # SIP 用户代理
│   └── utils/           # 工具模块
│       ├── diagnostics.h     # 诊断工具
│       └── simd_utils.h      # SIMD 优化
├── src/                 # 实现文件
├── examples/            # 示例程序
├── wasm/                # WebAssembly 支持
│   ├── api/                  # JS 绑定 API
│   ├── platform/             # WASM 平台层
│   ├── stubs/                # 桩函数
│   └── examples/             # 浏览器演示
├── docs/                # 文档
│   └── NEW_FEATURES.md  # 新功能详细文档
├── third_party/         # 第三方库
└── CMakeLists.txt
```

## 构建

### 依赖项

**必需:**
- CMake 3.16+
- C11 编译器 (GCC, Clang, MSVC 或 MinGW)

**可选 (自动检测或手动启用):**
- SpeexDSP - 重采样、降噪、AEC
- RNNoise - 神经网络降噪
- Opus - Opus 编解码器支持
- libsrtp2 - SRTP 加密
- OpenSSL - DTLS-SRTP 密钥交换

### 快速构建 (最小化，无外部依赖)

```bash
# 克隆仓库
git clone <repo>
cd sonickit

# 创建构建目录
mkdir build && cd build

# 配置最小功能 (不需要外部依赖)
cmake .. -DSONICKIT_ENABLE_OPUS=OFF \
         -DSONICKIT_ENABLE_RNNOISE=OFF \
         -DSONICKIT_ENABLE_SRTP=OFF \
         -DSONICKIT_ENABLE_DTLS=OFF

# 编译
cmake --build .
```

### 完整功能构建

```bash
# 配置所有功能 (需要外部依赖)
cmake .. -DSONICKIT_ENABLE_OPUS=ON \
         -DSONICKIT_ENABLE_RNNOISE=ON \
         -DSONICKIT_ENABLE_SRTP=ON \
         -DSONICKIT_ENABLE_DTLS=ON \
         -DSONICKIT_ENABLE_G722=ON

# 编译
cmake --build . --config Release

# 运行测试
ctest -C Release
```

### 平台特定说明

#### Windows (MinGW)

```powershell
# 使用 MinGW Makefiles
mkdir build; cd build
cmake .. -G "MinGW Makefiles" -DSONICKIT_BUILD_TESTS=ON -DSONICKIT_BUILD_EXAMPLES=ON
cmake --build .
```

#### Windows (Visual Studio)

```powershell
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

#### Linux / macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### CMake 选项

| 选项 | 默认 | 描述 |
|------|------|------|
| `SONICKIT_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `SONICKIT_BUILD_TESTS` | ON | 构建测试程序 |
| `SONICKIT_ENABLE_OPUS` | ON | 启用 Opus 编解码器 (需要 libopus) |
| `SONICKIT_ENABLE_G722` | OFF | 启用 G.722 编解码器 |
| `SONICKIT_ENABLE_SPEEX` | OFF | 启用 Speex 编解码器 |
| `SONICKIT_ENABLE_RNNOISE` | ON | 启用 RNNoise 神经网络降噪 |
| `SONICKIT_ENABLE_SRTP` | ON | 启用 SRTP 加密 (需要 libsrtp2) |
| `SONICKIT_ENABLE_DTLS` | ON | 启用 DTLS-SRTP 密钥交换 (需要 OpenSSL) |
| `SONICKIT_ENABLE_LTO` | ON | 启用链接时优化 (Release 构建) |

### 构建输出

成功构建后，您将得到：

```
build/
├── libvoice.a              # 静态库
├── test_buffer.exe         # 环形缓冲区测试
├── test_resampler.exe      # 重采样器测试
├── test_codec.exe          # 编解码器测试
├── test_dsp.exe            # DSP 模块测试
├── test_network.exe        # 网络模块测试
├── test_effects.exe        # 音频效果测试
├── test_watermark.exe      # 水印测试
├── test_diagnostics.exe    # 诊断测试
├── test_datachannel.exe    # DataChannel 测试
├── test_sip.exe            # SIP 协议测试
├── benchmark_simd.exe      # SIMD 性能基准测试
├── benchmark_dsp.exe       # DSP 性能基准测试
└── examples/
    ├── example_capture.exe     # 音频采集示例
    ├── example_playback.exe    # 音频播放示例
    ├── example_file_convert.exe # 文件转换示例
    └── example_voicechat.exe   # 语音聊天示例
```

### 运行测试

```bash
# 运行所有测试
ctest --test-dir build -C Debug --output-on-failure

# 运行特定测试
./build/test_codec.exe
```

### 性能基准测试

SonicKit 包含 SIMD 优化和 DSP 模块的性能基准测试。

```bash
# SIMD 基准测试 (格式转换、增益、混音等)
./build/benchmark_simd.exe

# DSP 基准测试 (AEC、时间拉伸、延迟估计)
./build/benchmark_dsp.exe
```

**命令行选项:**

| 选项 | 说明 |
|------|------|
| `-n, --iterations N` | 设置迭代次数 (默认: SIMD 10000, DSP 1000) |
| `-h, --help` | 显示帮助信息 |

**使用示例:**

```bash
# 使用 50000 次迭代运行 SIMD 基准测试
./build/benchmark_simd.exe -n 50000

# 使用 5000 次迭代运行 DSP 基准测试
./build/benchmark_dsp.exe --iterations 5000
```

**示例输出:**

```
+================================================================+
|          SonicKit SIMD Performance Benchmark                   |
+================================================================+

SIMD Capabilities: AVX2 SSE4.1 SSE2
Test buffer size: 4096 samples
Iterations: 10000

===================================================================
                     Format Conversion Tests
===================================================================

Benchmark: int16_to_float
  Iterations: 10000
  Mean:     245.32 ns
  Median:   241.00 ns
  Std Dev:   32.45 ns
  Min/Max:  235.00 / 412.00 ns
  P95/P99:  298.00 / 345.00 ns
  Throughput: 16.70 Gsamples/sec
```

### WebAssembly (浏览器) 构建

SonicKit 支持编译为 WebAssembly，在浏览器中运行实时音频处理。

**前置条件:**
- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)

```bash
# 安装 Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh  # Linux/macOS
# 或 emsdk_env.bat      # Windows

# 构建 WASM
cd sonickit/wasm
mkdir build && cd build
emcmake cmake .. -DWASM_ENABLE_OPUS=OFF -DWASM_ENABLE_RNNOISE=OFF
emmake make -j8

# 输出文件
# build/sonickit.js
# build/sonickit.wasm
```

**浏览器使用示例:**

```javascript
// 加载 SonicKit WASM
const sonicKit = await Module();

// 创建降噪器 (48kHz, 480 样本/帧)
const denoiser = new sonicKit.Denoiser(48000, 480, 0);

// 处理音频 (Int16Array)
const processedSamples = denoiser.process(inputSamples);

// 清理
denoiser.delete();
```

**JavaScript API 类:**

| 类名 | 构造函数 | 方法 |
|------|----------|------|
| `Denoiser` | `(sampleRate, frameSize, engine)` | `process()`, `reset()` |
| `EchoCanceller` | `(sampleRate, frameSize, filterLen)` | `process()`, `reset()` |
| `AGC` | `(sampleRate, frameSize, mode, target)` | `process()`, `getGain()`, `reset()` |
| `VAD` | `(sampleRate, frameSize, mode)` | `isSpeech()`, `getProbability()`, `reset()` |
| `Resampler` | `(channels, inRate, outRate, quality)` | `process()`, `reset()` |
| `G711Codec` | `(useAlaw)` | `encode()`, `decode()` |

更多 WASM 详情请参阅 [wasm/README.md](wasm/README.md)。

## 模块概览

| 模块 | 功能 | 主要特性 |
|------|------|----------|
| **audio_device** | 音频设备 | 采集/播放，跨平台 |
| **audio_buffer** | 环形缓冲 | 线程安全，零拷贝 |
| **audio_mixer** | 音频混音 | 多流混合，独立增益 |
| **audio_level** | 电平监测 | RMS/峰值/VU/LUFS |
| **audio_quality** | 质量分析 | MOS 评分，SNR 估计 |
| **audio_recorder** | 录制回放 | WAV 文件，内存缓冲 |
| **resampler** | 重采样 | 质量 0-10，任意比率 |
| **denoiser** | 降噪 | SpeexDSP + RNNoise |
| **echo_canceller** | 回声消除 | 自适应滤波 |
| **vad** | 语音检测 | 能量/过零率/谱熵 |
| **agc** | 增益控制 | 固定/自适应/数字 |
| **comfort_noise** | 舒适噪声 | 白/粉/棕/谱匹配 |
| **dtmf** | DTMF | Goertzel 检测/生成 |
| **equalizer** | 均衡器 | 多频段，7种滤波器 |
| **compressor** | 压缩器 | 软拐点，侧链 |
| **codec** | 编解码 | Opus/G.711/G.722 |
| **rtp** | RTP 协议 | 打包/解析，RTCP |
| **srtp** | SRTP 加密 | AES-CM/AES-GCM |
| **jitter_buffer** | 抖动缓冲 | 自适应，PLC |
| **bandwidth_estimator** | 带宽估计 | GCC/REMB/BBR |
| **ice** | NAT 穿透 | ICE/STUN/TURN |
| **transport** | 传输层 | UDP/TCP，QoS |
| **sip_core** | SIP 协议 | RFC 3261，注册/呼叫 |
| **sip_ua** | SIP 用户代理 | 呼叫管理，会话控制 |
| **diagnostics** | 诊断工具 | 性能分析，调试日志 |
| **simd_utils** | SIMD 优化 | SSE/AVX/NEON 加速 |
| **statistics** | 统计收集 | MOS/R-Factor，JSON |

## 快速开始

### 简单采集

```c
#include "voice/voice.h"
#include "audio/device.h"

void capture_callback(voice_device_t *dev, const int16_t *input,
                     size_t samples, void *user_data) {
    // 处理采集的音频数据
}

int main() {
    voice_device_config_t config;
    voice_device_config_init(&config);
    config.mode = VOICE_DEVICE_MODE_CAPTURE;
    config.sample_rate = 48000;
    config.channels = 1;
    config.capture_callback = capture_callback;

    voice_device_t *device = voice_device_create(&config);
    voice_device_start(device);

    // ... 运行 ...

    voice_device_stop(device);
    voice_device_destroy(device);
    return 0;
}
```

### 完整语音通话

```c
#include "voice/pipeline.h"

int main() {
    voice_pipeline_config_t config;
    voice_pipeline_config_init(&config);
    config.mode = PIPELINE_MODE_DUPLEX;
    config.enable_aec = true;
    config.enable_denoise = true;
    config.codec = VOICE_CODEC_OPUS;

    voice_pipeline_t *pipeline = voice_pipeline_create(&config);

    voice_pipeline_set_encoded_callback(pipeline, on_send_packet, NULL);
    voice_pipeline_start(pipeline);

    // 接收远端数据
    voice_pipeline_receive_packet(pipeline, data, size);

    // ... 运行 ...

    voice_pipeline_stop(pipeline);
    voice_pipeline_destroy(pipeline);
    return 0;
}
```

## 示例程序

- **example_capture** - 麦克风采集录音
- **example_playback** - 音频文件播放
- **example_file_convert** - 音频格式转换
- **example_voicechat** - 完整双工语音通话

```bash
# 录音 10 秒
./example_capture -o recording.wav -d 10

# 播放音频
./example_playback music.mp3

# 格式转换 + 降噪
./example_file_convert input.mp3 output.wav -r 16000 -n

# 语音通话 (两台机器)
# 机器 A
./example_voicechat -p 5004

# 机器 B
./example_voicechat -p 5005 -c <机器A的IP> -r 5004
```

## API 文档

### 音频设备

```c
// 枚举设备
voice_device_info_t devices[10];
size_t count = 10;
voice_device_enumerate(VOICE_DEVICE_MODE_CAPTURE, devices, &count);

// 创建设备
voice_device_t *device = voice_device_create(&config);
voice_device_start(device);
voice_device_stop(device);
voice_device_destroy(device);
```

### 降噪

```c
voice_denoiser_config_t config;
voice_denoiser_config_init(&config);
config.sample_rate = 48000;
config.engine = VOICE_DENOISE_RNNOISE;  // 或 VOICE_DENOISE_SPEEX

voice_denoiser_t *dn = voice_denoiser_create(&config);
voice_denoiser_process(dn, pcm_buffer, sample_count);
voice_denoiser_destroy(dn);
```

### 编解码

```c
// 编码
voice_encoder_t *enc = voice_encoder_create(&codec_config);
voice_encoder_encode(enc, pcm, samples, output, &output_size);

// 解码
voice_decoder_t *dec = voice_decoder_create(&codec_config);
voice_decoder_decode(dec, input, input_size, pcm, &samples);
```

### 网络

```c
// RTP
rtp_session_t *rtp = rtp_session_create(&config);
rtp_session_create_packet(rtp, payload, len, timestamp, marker, packet, &size);
rtp_session_parse_packet(rtp, data, len, &packet);

// SRTP
srtp_session_t *srtp = srtp_session_create(&config);
srtp_protect(srtp, packet, &size, max_size);
srtp_unprotect(srtp, packet, &size);
```

## 移动平台注意事项

### iOS

```objc
// 在使用音频前配置会话
voice_session_config_t config;
voice_session_config_init(&config);
config.category = VOICE_SESSION_CATEGORY_PLAY_AND_RECORD;
config.mode = VOICE_SESSION_MODE_VOICE_CHAT;

voice_session_configure(&config);
voice_session_activate();

// 处理中断
voice_session_set_interrupt_callback(on_interrupt, user_data);
```

### Android

需要在 Java 层初始化:

```java
public class VoiceLib {
    static {
        System.loadLibrary("voice");
    }

    public static native void nativeInit(Context context);
    public static native void nativeRelease();
}

// 初始化
VoiceLib.nativeInit(getApplicationContext());
```

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request!

## 详细文档

更多功能详情请参阅 [docs/NEW_FEATURES.md](docs/NEW_FEATURES.md)，包括：
- 各模块详细 API 说明
- 完整使用示例
- 性能优化建议
- 移动平台适配指南
