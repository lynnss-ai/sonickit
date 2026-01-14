# Voice Library 优化与新功能迭代

## 📋 概述

本文档总结了对 Voice 库的代码审查结果，包含可实施的优化方案和新功能建议。

---

## ✅ 已完成的优化和新功能

### 1. 新增模块

| 模块 | 文件 | 描述 |
|------|------|------|
| **VAD (语音活动检测)** | `include/dsp/vad.h`, `src/dsp/vad.c` | 多模式 VAD 支持 (能量/SpeexDSP/RNNoise)，自适应阈值 |
| **音频混音器** | `include/audio/audio_mixer.h`, `src/audio/audio_mixer.c` | 多路音频混合，支持增益控制、限制器、多种混音算法 |
| **带宽估计器** | `include/network/bandwidth_estimator.h` | 网络带宽估计，AIMD 算法，自适应比特率控制 |
| **音频电平计** | `include/audio/audio_level.h` | 峰值/RMS 测量，RFC 6464 音频电平扩展支持 |
| **音频质量评估** | `include/audio/audio_quality.h`, `src/audio/audio_quality.c` | MOS 估计，E-Model (ITU-T G.107)，R-Factor 计算 |
| **SIMD 工具** | `include/utils/simd_utils.h`, `src/utils/simd_utils.c` | SSE2/AVX/NEON 优化的音频处理函数 |

### 2. SIMD 性能优化

新增 `simd_utils` 模块提供了以下优化函数：

- **格式转换** (int16 ↔ float)
  - SSE2 版本：每次处理 8 个样本
  - NEON 版本：ARM 平台优化
  - 约 4-8x 性能提升

- **音频处理**
  - `voice_apply_gain_float()` - SIMD 加速增益
  - `voice_mix_add_float()` - SIMD 加速混音
  - `voice_compute_energy_*()` - 能量计算

- **CPU 检测**
  - 运行时 SIMD 能力检测
  - 自动选择最优实现

---

## 🔧 建议的进一步优化

### 1. 音频缓冲区优化

**当前状态**: `audio_buffer.c` 使用 C11 原子操作实现无锁环形缓冲区

**建议优化**:

```c
// 添加缓存行对齐防止伪共享
typedef struct {
    alignas(64) _Atomic size_t write_pos;
    alignas(64) _Atomic size_t read_pos;
    // ...
} voice_audio_buffer_t;
```

```c
// 批量读写接口减少原子操作开销
size_t voice_audio_buffer_write_batch(
    voice_audio_buffer_t *buffer,
    const int16_t *samples,
    size_t num_samples,
    size_t batch_size  // 按批次更新 write_pos
);
```

### 2. 降噪器优化

**当前状态**: `denoiser.c` 支持 SpeexDSP 和 RNNoise 双引擎

**建议优化**:

```c
// 1. 使用 SIMD 加速 int16 ↔ float 转换
void voice_denoiser_process(voice_denoiser_t *denoiser, int16_t *samples, size_t count) {
    // 使用 simd_utils 替代手动转换
    voice_int16_to_float(samples, denoiser->float_buffer, count);
    // ... 处理 ...
    voice_float_to_int16(denoiser->float_buffer, samples, count);
}

// 2. 智能引擎切换
typedef struct {
    float noise_estimate_db;
    float speech_ratio;
    // 根据噪声水平自动切换引擎
} voice_denoiser_adaptive_t;
```

### 3. 抖动缓冲区优化

**当前状态**: `jitter_buffer.c` 使用槽位数组存储包

**建议优化**:

```c
// 1. 更好的丢包补偿
typedef enum {
    VOICE_PLC_SILENCE,      // 静音填充
    VOICE_PLC_REPEAT,       // 重复上一帧
    VOICE_PLC_INTERPOLATE,  // 插值 (新增)
    VOICE_PLC_CODEC_NATIVE, // 编解码器内置 PLC
} voice_plc_mode_t;

// 2. 包重排序优化
typedef struct {
    uint16_t seq_window[16];  // 滑动窗口跟踪
    size_t window_pos;
} voice_jb_reorder_t;
```

### 4. Pipeline 优化

**当前状态**: `pipeline.c` 同步处理音频

**建议优化**:

```c
// 异步处理线程
typedef struct {
    voice_audio_buffer_t *input_queue;
    voice_audio_buffer_t *output_queue;
    pthread_t process_thread;
    bool running;
} voice_async_pipeline_t;

// 单独的发送线程
void *voice_pipeline_process_thread(void *arg);
```

---

## 🆕 建议的新功能

### 1. WebRTC 风格音频处理

```c
// 完整的 WebRTC APM 风格处理链
typedef struct {
    voice_agc_t *agc;           // 自动增益控制
    voice_ns_t *noise_suppressor;// 噪声抑制
    voice_aec_t *echo_canceller; // 回声消除
    voice_vad_t *vad;           // 语音活动检测
    voice_comfort_noise_t *cng; // 舒适噪声生成
} voice_apm_t;
```

### 2. 录音/回放功能

```c
// Pipeline 内置录音
typedef struct {
    bool enabled;
    voice_file_writer_t *writer;
    voice_record_format_t format;
} voice_pipeline_recorder_t;

voice_error_t voice_pipeline_start_recording(
    voice_pipeline_t *pipeline,
    const char *filename,
    voice_record_format_t format
);
```

### 3. 音频效果器

```c
// 均衡器
typedef struct {
    float bands[10];  // 10 段均衡
} voice_equalizer_t;

// 混响
typedef struct {
    float room_size;
    float damping;
    float wet_mix;
} voice_reverb_t;

// 压缩器
typedef struct {
    float threshold_db;
    float ratio;
    float attack_ms;
    float release_ms;
} voice_compressor_t;
```

### 4. 网络传输增强

```c
// RED (Redundant Encoding)
typedef struct {
    bool enabled;
    uint8_t redundancy_count;
    voice_codec_t *primary_codec;
} voice_red_encoder_t;

// FEC (Forward Error Correction)
// Opus 已内置，G.711 可使用 RFC 5109
```

### 5. 多方会议支持

```c
// 会议桥
typedef struct {
    voice_mixer_t *mixer;
    voice_participant_t *participants;
    size_t participant_count;
    
    // 主讲人检测
    uint32_t current_speaker_id;
    float speaker_threshold_db;
} voice_conference_bridge_t;

// 选择性转发 (SFU 模式)
typedef struct {
    voice_participant_t *participants;
    voice_forwarding_rule_t *rules;
} voice_sfu_t;
```

---

## 📊 性能基准

### SIMD 优化效果 (预估)

| 操作 | 标量 | SSE2 | AVX2 | NEON |
|------|------|------|------|------|
| int16→float (1024样本) | 1.0x | 4x | 8x | 4x |
| 增益应用 | 1.0x | 4x | 8x | 4x |
| 混音 (2路) | 1.0x | 3x | 6x | 3x |
| 能量计算 | 1.0x | 4x | 8x | 4x |

### 内存优化

| 模块 | 当前 | 优化后 |
|------|------|--------|
| Jitter Buffer (100 slots) | ~200KB | ~150KB (池化) |
| Audio Buffer (4096 样本) | 8KB | 8KB + 128B 元数据 |
| RNNoise 状态 | ~100KB | ~100KB (无变化) |

---

## 🔜 下一步行动

1. **高优先级**
   - [ ] 在 denoiser.c 中集成 SIMD 转换函数
   - [ ] 实现带宽估计器 (bandwidth_estimator.c)
   - [ ] 添加音频电平计实现 (audio_level.c)

2. **中优先级**
   - [ ] 实现异步 Pipeline
   - [ ] 添加 WebRTC 风格 AGC
   - [ ] 改进 PLC 算法

3. **低优先级**
   - [ ] 多方会议支持
   - [ ] 录音功能
   - [ ] 均衡器等效果器

---

## 📁 新增文件清单

```
include/
├── audio/
│   ├── audio_mixer.h      (新增) 多路音频混音
│   ├── audio_level.h      (新增) 电平测量
│   └── audio_quality.h    (新增) 质量评估
├── dsp/
│   └── vad.h              (新增) 语音活动检测
├── network/
│   └── bandwidth_estimator.h (新增) 带宽估计
└── utils/
    └── simd_utils.h       (新增) SIMD 工具

src/
├── audio/
│   ├── audio_mixer.c      (新增)
│   └── audio_quality.c    (新增)
├── dsp/
│   └── vad.c              (新增)
└── utils/
    └── simd_utils.c       (新增)
```
