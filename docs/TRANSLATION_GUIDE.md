# SonicKit Translation Guide / 翻译指南

**Author:** wangxuebing <lynnss.codeai@gmail.com>  
**Date:** 2026-01-14

## Purpose / 目的

This document provides standardized English translations for technical terms used throughout the SonicKit codebase to ensure consistency and clarity.

本文档为 SonicKit 代码库中使用的技术术语提供标准化的英文翻译，以确保一致性和清晰度。

---

## Core Technical Terms / 核心技术术语

| 中文 | English | Context / 使用场景 |
|------|---------|-------------------|
| 采集 | Capture | Audio input / 音频输入 |
| 播放 | Playback | Audio output / 音频输出 |
| 重采样 | Resampling | DSP processing / DSP处理 |
| 重采样器 | Resampler | Component / 组件 |
| 降噪 | Denoising / Noise Reduction | DSP processing |
| 降噪器 | Denoiser | Component |
| 回声消除 | Echo Cancellation | DSP processing |
| 回声消除器 | Echo Canceller | Component (AEC) |
| 自动增益控制 | Automatic Gain Control | DSP processing (AGC) |
| 语音活动检测 | Voice Activity Detection | DSP processing (VAD) |
| 舒适噪声 | Comfort Noise | Audio processing |
| 压缩器 | Compressor | Dynamic range compressor |
| 均衡器 | Equalizer | Audio effects (EQ) |
| 音效/效果器 | Audio Effects | Effects processing |
| 水印 | Watermark | Audio watermarking |
| 双音多频 | Dual-Tone Multi-Frequency | DTMF |

## Audio Architecture / 音频架构

| 中文 | English | Notes |
|------|---------|-------|
| 管线/流水线 | Pipeline | Audio processing pipeline |
| 模块 | Module | Software module |
| 组件 | Component | System component |
| 音频设备 | Audio Device | Hardware device |
| 音频流 | Audio Stream | Data stream |
| 音频帧 | Audio Frame | Frame of samples |
| 样本 | Sample | Audio sample |
| 采样率 | Sample Rate | In Hz |
| 通道 | Channel | Mono/Stereo channels |
| 位深度 | Bit Depth | Sample precision |
| 音频缓冲区 | Audio Buffer | Buffer memory |
| 环形缓冲区 | Ring Buffer | Circular buffer |

## Codec Terms / 编解码术语

| 中文 | English | Notes |
|------|---------|-------|
| 编解码器 | Codec | Encoder/Decoder |
| 编码 | Encoding | Encode process |
| 解码 | Decoding | Decode process |
| 编码器 | Encoder | Encoding component |
| 解码器 | Decoder | Decoding component |
| 比特率 | Bit Rate | bps |
| 码率控制 | Rate Control | Bitrate management |
| 帧大小 | Frame Size | Samples per frame |
| 复杂度 | Complexity | Algorithm complexity |

## Network Terms / 网络术语

| 中文 | English | Notes |
|------|---------|-------|
| 抖动缓冲 | Jitter Buffer | Network buffering |
| 丢包 | Packet Loss | Lost packets |
| 丢包率 | Packet Loss Rate | Percentage |
| 延迟 | Latency / Delay | Network delay |
| 往返时延 | Round-Trip Time | RTT |
| 带宽 | Bandwidth | Network bandwidth |
| 带宽估计 | Bandwidth Estimation | BWE |
| 传输层 | Transport Layer | Network layer |
| 实时传输协议 | Real-time Transport Protocol | RTP |
| 安全实时传输协议 | Secure RTP | SRTP |
| 交互式连接建立 | Interactive Connectivity Establishment | ICE |
| 数据通道 | Data Channel | WebRTC data channel |

## Quality Metrics / 质量指标

| 中文 | English | Notes |
|------|---------|-------|
| 音频质量 | Audio Quality | Overall quality |
| 平均意见得分 | Mean Opinion Score | MOS |
| 信噪比 | Signal-to-Noise Ratio | SNR |
| 峰值信噪比 | Peak SNR | PSNR |
| 音频电平 | Audio Level | Volume level |
| 音频混音器 | Audio Mixer | Mixing component |
| 音频录制器 | Audio Recorder | Recording component |

## DSP Algorithms / DSP算法

| 中文 | English | Notes |
|------|---------|-------|
| 快速傅里叶变换 | Fast Fourier Transform | FFT |
| 滤波器 | Filter | Signal filter |
| 低通滤波器 | Low-Pass Filter | LPF |
| 高通滤波器 | High-Pass Filter | HPF |
| 带通滤波器 | Band-Pass Filter | BPF |
| 增益 | Gain | Amplification |
| 窗函数 | Window Function | Windowing |

## Platform / 平台

| 中文 | English | Notes |
|------|---------|-------|
| 平台 | Platform | OS platform |
| 线程 | Thread | Threading |
| 互斥锁 | Mutex | Mutual exclusion |
| 条件变量 | Condition Variable | Thread sync |
| 原子操作 | Atomic Operation | Lock-free ops |
| 内存对齐 | Memory Alignment | Data alignment |
| SIMD指令 | SIMD Instructions | Vectorization |

## Configuration / 配置

| 中文 | English | Notes |
|------|---------|-------|
| 配置 | Configuration | Settings |
| 参数 | Parameter | Function parameter |
| 选项 | Option | Config option |
| 默认 | Default | Default value |
| 启用 | Enable | Turn on |
| 禁用 | Disable | Turn off |
| 初始化 | Initialize / Init | Initialization |
| 销毁 | Destroy | Cleanup |
| 创建 | Create | Object creation |
| 释放 | Release / Free | Memory release |

## Error Handling / 错误处理

| 中文 | English | Notes |
|------|---------|-------|
| 错误 | Error | Error condition |
| 错误码 | Error Code | Return code |
| 成功 | Success | Successful operation |
| 失败 | Failure / Failed | Failed operation |
| 无效参数 | Invalid Parameter | Bad argument |
| 内存不足 | Out of Memory | Memory exhausted |
| 超时 | Timeout | Operation timeout |
| 不支持 | Unsupported | Not supported |

## State & Status / 状态

| 中文 | English | Notes |
|------|---------|-------|
| 状态 | State / Status | Current state |
| 运行中 | Running | Active state |
| 已停止 | Stopped | Inactive state |
| 暂停 | Paused | Suspended state |
| 就绪 | Ready | Ready state |
| 忙碌 | Busy | Busy state |
| 空闲 | Idle | Idle state |

## Documentation / 文档

| 中文 | English | Notes |
|------|---------|-------|
| 简介 | Brief | Short description |
| 描述 | Description | Detailed description |
| 返回值 | Return Value | Function return |
| 参见 | See Also | Cross-reference |
| 注意 | Note / Warning | Important note |
| 示例 | Example | Code example |
| 用法 | Usage | How to use |

---

## Translation Guidelines / 翻译指南

### General Principles / 一般原则

1. **Consistency First** - Use the same English term for each Chinese term throughout the codebase
   **一致性优先** - 在整个代码库中对每个中文术语使用相同的英文术语

2. **Technical Accuracy** - Ensure translations match industry-standard terminology
   **技术准确性** - 确保翻译符合行业标准术语

3. **Clarity Over Literal Translation** - Prioritize clear communication over word-for-word translation
   **清晰性优于直译** - 优先考虑清晰的沟通而不是逐字翻译

4. **Preserve Code Structure** - Keep comment structure (Doxygen tags, formatting) unchanged
   **保持代码结构** - 保持注释结构（Doxygen标签、格式）不变

### Comment Types / 注释类型

#### Section Headers / 章节标题
```c
/* ============================================
 * 全局状态  →  Global State
 * ============================================ */
```

#### Function Documentation / 函数文档
```c
/**
 * @brief 初始化语音库  →  Initialize voice library
 * @param config 全局配置  →  Global configuration
 * @return 错误码  →  Error code
 */
```

#### Inline Comments / 行内注释
```c
int channels;  /* 通道数  →  Number of channels */
```

#### Doxygen Member Comments / Doxygen成员注释
```c
uint32_t sample_rate;  /**< 采样率  →  Sample rate */
```

---

## Abbreviations / 缩略语

| Abbreviation | Full Form | 中文 |
|--------------|-----------|------|
| AEC | Acoustic Echo Cancellation | 回声消除 |
| AGC | Automatic Gain Control | 自动增益控制 |
| VAD | Voice Activity Detection | 语音活动检测 |
| DSP | Digital Signal Processing | 数字信号处理 |
| FFT | Fast Fourier Transform | 快速傅里叶变换 |
| RTP | Real-time Transport Protocol | 实时传输协议 |
| SRTP | Secure RTP | 安全RTP |
| ICE | Interactive Connectivity Establishment | 交互式连接建立 |
| MOS | Mean Opinion Score | 平均意见得分 |
| SNR | Signal-to-Noise Ratio | 信噪比 |
| DTMF | Dual-Tone Multi-Frequency | 双音多频 |
| BWE | Bandwidth Estimation | 带宽估计 |
| SIMD | Single Instruction Multiple Data | 单指令多数据 |

---

## Version History / 版本历史

- **v1.0** (2026-01-14): Initial translation guide created
- 首次创建翻译指南
