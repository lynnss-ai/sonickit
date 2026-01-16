# SonicKit WebAssembly

> 作者: wangxuebing <lynnss.codeai@gmail.com>

SonicKit 的 WebAssembly 版本，将音频处理能力带到浏览器端。

## 📦 功能特性

### DSP 处理
- ✅ **降噪** - SpeexDSP + RNNoise 双引擎
- ✅ **回声消除** (AEC) - SpeexDSP 实现
- ✅ **自动增益控制** (AGC) - 多种模式
- ✅ **重采样** - 高质量采样率转换
- ✅ **语音活动检测** (VAD) - 实时语音检测
- ✅ **DTMF 检测/生成** - 电话按键音处理
- ✅ **延时估计** - 信号延时测量
- ✅ **时间拉伸** - 变速不变调处理

### 编解码器
- ✅ **Opus** - 高质量语音编码
- ✅ **G.711** - A-law/μ-law 编码

### 音频处理
- ✅ **音频缓冲** - 环形缓冲区
- ✅ **混音器** - 多流混合
- ✅ **音频质量分析** - MOS 评分
- ✅ **电平监测** - RMS/峰值测量
- ✅ **抖动缓冲** - 网络音频缓冲

### 音频效果
- ✅ **均衡器** - 多频段参数均衡
- ✅ **压缩器** - 动态范围控制
- ✅ **限幅器** - 峰值限制
- ✅ **噪声门** - 背景噪音消除
- ✅ **舒适噪声** - CNG 生成
- ✅ **混响** - 房间模拟效果
- ✅ **延迟** - 回声效果
- ✅ **变调** - 音调变换
- ✅ **合唱** - 声音加厚效果
- ✅ **镶边** - 调制效果

### 空间音频
- ✅ **空间渲染** - 3D 音频定位
- ✅ **HRTF** - 头部相关传输函数

### 音频水印
- ✅ **水印嵌入** - 隐藏数据嵌入
- ✅ **水印检测** - 隐藏数据提取

## 🚀 快速开始

### 前置要求

1. **Emscripten SDK**
```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

2. **CMake** (>= 3.16)

### 编译

```bash
cd wasm
chmod +x build.sh
./build.sh
```

编译选项:
```bash
./build.sh --debug        # Debug 模式
./build.sh --minsize      # 最小大小优化
./build.sh --no-opus      # 禁用 Opus
./build.sh --threads      # 启用多线程
./build.sh --simd         # 启用 WASM SIMD
./build.sh --clean        # 清理后重新编译
```

### 使用示例

#### 1. 加载模块

```html
<script src="sonickit.js"></script>
<script>
  let sonicKit = null;

  async function init() {
    sonicKit = await createSonicKit();
    console.log('SonicKit WASM loaded!');
  }

  init();
</script>
```

#### 2. 实时降噪

```javascript
// 创建降噪器
const denoiser = new sonicKit.Denoiser(
  48000,  // 采样率
  480,    // 帧大小 (10ms @ 48kHz)
  1       // 引擎类型 (0=自动, 1=SpeexDSP, 2=RNNoise)
);

// 处理音频
const inputAudio = new Int16Array(480);  // 输入音频
const outputAudio = denoiser.process(inputAudio);

// 清理
denoiser.delete();
```

#### 3. 回声消除

```javascript
const aec = new sonicKit.EchoCanceller(
  16000,  // 采样率
  160,    // 帧大小
  200     // 滤波器长度 (ms)
);

const captured = new Int16Array(160);  // 麦克风音频
const playback = new Int16Array(160);  // 扬声器音频

const output = aec.process(captured, playback);
```

#### 4. Opus 编解码

```javascript
// 编码器
const encoder = new sonicKit.OpusEncoder(
  48000,  // 采样率
  1,      // 声道数
  64000   // 码率
);

const pcm = new Int16Array(960);  // 20ms @ 48kHz
const encoded = encoder.encode(pcm, 960);

// 解码器
const decoder = new sonicKit.OpusDecoder(48000, 1);
const decoded = decoder.decode(encoded, encoded.length);
```

#### 5. 重采样

```javascript
const resampler = new sonicKit.Resampler(
  16000,  // 输入采样率
  48000,  // 输出采样率
  1,      // 声道数
  5       // 质量 (0-10)
);

const input = new Int16Array(160);   // 10ms @ 16kHz
const output = resampler.process(input, 160);
```

#### 6. 语音活动检测

```javascript
const vad = new sonicKit.VAD(
  16000,  // 采样率
  160,    // 帧大小
  1       // 模式 (0=低灵敏度, 3=高灵敏度)
);

const audio = new Int16Array(160);
const isSpeech = vad.isSpeech(audio);
const probability = vad.getProbability();
```

## 🎨 演示示例

启动本地服务器查看演示：

```bash
cd examples
python3 -m http.server 8000
# 打开浏览器访问 http://localhost:8000
```

演示页面：
- **demo_denoiser.html** - 实时降噪演示
- **demo_codec.html** - 编解码器演示（待创建）
- **demo_voicechat.html** - 语音聊天演示（待创建）

## 📐 架构说明

### 目录结构

```
wasm/
├── platform/           # WASM 平台适配层
│   ├── wasm_platform.c    # 平台实现
│   ├── wasm_time.c        # 时间函数
│   └── wasm_threading.h   # 线程抽象
├── stubs/             # 桩实现
│   ├── device_stub.c      # 设备 I/O 桩
│   └── network_stub.c     # 网络桩
├── api/               # JavaScript API
│   └── sonickit_api.cpp   # Embind 绑定
├── examples/          # 演示示例
├── dist/              # 编译输出
├── CMakeLists.txt     # 构建配置
└── build.sh           # 构建脚本
```

### 设计原则

1. **零侵入** - 不修改 `../src/` 和 `../include/` 中的代码
2. **模块化** - 仅包含需要的功能模块
3. **性能优先** - 启用 LTO 和 WASM 优化
4. **易用性** - 提供友好的 JavaScript API

### 平台适配

- **音频 I/O**: 由 Web Audio API 处理
- **线程**: 默认单线程，可选 pthread
- **网络**: 通过 WebSocket/WebRTC 桥接
- **文件系统**: Emscripten 虚拟文件系统

## 🔧 API 参考

### Denoiser (降噪器)

```typescript
class Denoiser {
  constructor(sampleRate: number, frameSize: number, engine: number);
  process(input: Int16Array): Int16Array;
  reset(): void;
  delete(): void;
}
```

### EchoCanceller (回声消除)

```typescript
class EchoCanceller {
  constructor(sampleRate: number, frameSize: number, filterLength: number);
  process(captured: Int16Array, playback: Int16Array): Int16Array;
  reset(): void;
  delete(): void;
}
```

### AGC (自动增益控制)

```typescript
class AGC {
  constructor(sampleRate: number, frameSize: number, mode: number, targetLevel: number);
  process(input: Int16Array): Int16Array;
  setTargetLevel(level: number): void;
  getGain(): number;
  delete(): void;
}
```

### Resampler (重采样器)

```typescript
class Resampler {
  constructor(inputRate: number, outputRate: number, channels: number, quality: number);
  process(input: Int16Array, inputFrames: number): Int16Array;
  reset(): void;
  delete(): void;
}
```

### VAD (语音活动检测)

```typescript
class VAD {
  constructor(sampleRate: number, frameSize: number, mode: number);
  isSpeech(input: Int16Array): boolean;
  getProbability(): number;
  delete(): void;
}
```

### OpusEncoder / OpusDecoder

```typescript
class OpusEncoder {
  constructor(sampleRate: number, channels: number, bitrate: number);
  encode(pcm: Int16Array, samples: number): Uint8Array;
  setBitrate(bitrate: number): void;
  delete(): void;
}

class OpusDecoder {
  constructor(sampleRate: number, channels: number);
  decode(data: Uint8Array, size: number): Int16Array;
  delete(): void;
}
```

## 📊 性能

### 文件大小（压缩前）

| 配置 | JS | WASM | 总计 |
|------|-----|------|------|
| 基础版 | ~50KB | ~400KB | ~450KB |
| 完整版 | ~50KB | ~800KB | ~850KB |

### 处理延迟

| 操作 | 延迟 |
|------|------|
| 降噪 (SpeexDSP) | < 3ms |
| 降噪 (RNNoise) | < 8ms |
| 回声消除 | < 5ms |
| Opus 编码 | < 2ms |
| 重采样 | < 2ms |

## 🔗 集成指南

### 在 React 中使用

```jsx
import { useEffect, useState } from 'react';

function AudioProcessor() {
  const [sonicKit, setSonicKit] = useState(null);
  const [denoiser, setDenoiser] = useState(null);

  useEffect(() => {
    const script = document.createElement('script');
    script.src = '/sonickit.js';
    script.onload = async () => {
      const kit = await window.createSonicKit();
      setSonicKit(kit);
    };
    document.body.appendChild(script);
  }, []);

  const startDenoising = () => {
    if (sonicKit) {
      const den = new sonicKit.Denoiser(48000, 480, 1);
      setDenoiser(den);
    }
  };

  return <button onClick={startDenoising}>Start</button>;
}
```

### 在 Vue 中使用

```vue
<script setup>
import { ref, onMounted } from 'vue';

const sonicKit = ref(null);
const denoiser = ref(null);

onMounted(async () => {
  const script = document.createElement('script');
  script.src = '/sonickit.js';
  await new Promise(resolve => {
    script.onload = resolve;
    document.body.appendChild(script);
  });

  sonicKit.value = await window.createSonicKit();
});

function startDenoising() {
  denoiser.value = new sonicKit.value.Denoiser(48000, 480, 1);
}
</script>
```

## 🛠️ 故障排除

### 1. 模块加载失败

确保 `.wasm` 文件与 `.js` 文件在同一目录：
```
dist/
  ├── sonickit.js
  └── sonickit.wasm
```

### 2. 内存不足

增加初始内存：
```javascript
createSonicKit({
  INITIAL_MEMORY: 128 * 1024 * 1024  // 128MB
});
```

### 3. CORS 错误

启动本地服务器：
```bash
python3 -m http.server 8000
```

## 📄 许可证

与主项目相同的许可证。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📞 支持

- GitHub Issues: https://github.com/lynnss-ai/sonickit/issues
- 邮箱: support@example.com
