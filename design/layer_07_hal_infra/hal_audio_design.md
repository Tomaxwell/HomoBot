# HAL_Audio 模块详细设计文档

## 1. 模块概述与定位

**模块全称**：Audio Hardware Abstraction Layer（音频硬件抽象层）

**模块缩写**：HAL_Audio

**定位**：HAL_Audio 是端侧音频子系统的唯一入口，负责麦克风阵列与扬声器的硬件抽象、音频信号预处理、语音识别（ASR）与语音合成（TTS）。向上为 Interaction 模块提供标准化音频事件与语音文本，向下屏蔽具体音频芯片（如 XMOS、TI AIC3204、RK3308）的差异。

**核心价值**：
- 将音频硬件细节（驱动、采样率、通道数）与上层应用解耦
- 在端侧完成低延迟 ASR（唤醒词/命令词）与 TTS 推理，减少云端往返
- 提供回声消除（AEC）与降噪（NS），保障嘈杂环境下的交互质量

**所处层次**：HAL & Infra 层（与 HAL_EtherCAT、HAL_Sensor 同级）

## 2. 职责边界

| 边界 | HAL_Audio 负责 | 对方负责 |
|------|---------------|---------|
| HAL_Audio ↔ HAL_Sensor | 音频设备状态上报 | 摄像头/Lidar 等其他传感器管理 |
| HAL_Audio ↔ Interaction | 提供 ASR 文本、TTS 播放状态、音频事件 | 多模态意图整合、交互状态管理 |
| HAL_Audio ↔ Gateway | 大模型 ASR/TTS 云端调用（弱网 fallback） | 云端通信、APP 消息路由 |
| HAL_Audio ↔ SM | 音频模块状态上报 | 全局状态机管理 |
| HAL_Audio ↔ HDS | 音频健康数据上报 | 故障定级 |

**明确不负责的领域**：
- 不负责自然语言理解（NLU）—— 由 Agent 负责
- 不负责多模态意图融合 —— 由 Interaction 负责
- 不直接控制机器人运动 —— 由 MC/MS/MP 负责
- 不直接对接 APP/云端 —— 由 Gateway 负责

## 3. 状态机设计

```
                    ┌──────────────┐
                    │              │
         ┌─────────►│    IDLE      │◄────────┐
         │          │              │         │
         │          └──────┬───────┘         │
         │                 │ init()          │ cleanup()
         │                 ▼                 │
    fault_recover()   ┌──────────────┐      │
         │            │ INITIALIZING │      │
         │            │              │      │
         │            └──────┬───────┘      │
         │                   │ ready        │
         │                   ▼              │
         │            ┌──────────────┐      │
         │            │    READY     │──────┘
         │            │              │ stop()
         │            └──────┬───────┘
         │                   │ start_recording()
         │                   ▼
         │            ┌──────────────┐
         │            │  RECORDING   │
         │            │  (ASR active)│
         │            └──────┬───────┘
         │                   │ start_playing()
         │                   ▼
         │            ┌──────────────┐
         │            │   PLAYING    │
         │            │  (TTS active)│
         │            └──────┬───────┘
         │                   │ error
         │                   ▼
         └────────────┐ ┌──────────────┐
                      │ │    FAULT     │
                      │ │              │
                      │ └──────────────┘
```

**状态说明**：

| 状态 | 说明 |
|------|------|
| IDLE | 初始状态，音频设备未初始化 |
| INITIALIZING | 正在初始化音频硬件、加载 ASR/TTS 模型 |
| READY | 音频设备就绪，等待指令 |
| RECORDING | 正在录音并进行 ASR 推理 |
| PLAYING | 正在播放 TTS 音频 |
| FAULT | 音频硬件故障或模型加载失败 |

**状态转换条件**：
- `init()`：EM 启动 HAL_Audio 节点时触发
- `ready`：硬件初始化完成、模型加载成功
- `start_recording()`：Interaction 请求开始语音识别
- `start_playing()`：Interaction 请求播放语音合成
- `stop()`：停止当前录音或播放
- `error`：硬件错误、模型推理异常、内存不足
- `fault_recover()`：EM 触发重启或热恢复
- `cleanup()`：EM 停止节点时触发

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# hal_audio_msgs/msg/AudioDeviceState.msg
# 音频设备状态广播

uint8 state                 # 当前状态
uint8 prev_state
builtin_interfaces/Time state_changed_at
bool mic_available          # 麦克风是否可用
bool speaker_available      # 扬声器是否可用
float32 input_gain_db       # 输入增益（dB）
float32 output_volume       # 输出音量（0.0-1.0）
uint32 sample_rate_hz       # 当前采样率
uint8 channels              # 通道数
```

```
# hal_audio_msgs/msg/AudioFrame.msg
# 原始音频帧

builtin_interfaces/Time stamp
int16[] samples             # PCM 采样数据
uint32 sample_rate
uint8 channels
uint32 frame_id
```

```
# hal_audio_msgs/msg/VadEvent.msg
# 语音活动检测事件

builtin_interfaces/Time stamp
bool is_speech              # 是否检测到语音
float32 speech_prob         # 语音概率（0.0-1.0）
builtin_interfaces/Time speech_start
builtin_interfaces/Time speech_end
float32 duration_sec
```

```
# hal_audio_msgs/msg/SpeechRecognitionResult.msg
# ASR 识别结果

builtin_interfaces/Time stamp
string text                 # 识别文本
string language             # 语言代码（"zh", "en"）
float32 confidence          # 置信度（0.0-1.0）
bool is_final               # 是否为最终结果
bool is_wake_word           # 是否为唤醒词
string wake_word            # 唤醒词内容（如"你好小步"）
float32 audio_duration_sec  # 音频时长
```

```
# hal_audio_msgs/msg/TtsRequest.msg
# TTS 合成请求（内部 Topic，由 Interaction 下发）

builtin_interfaces/Time stamp
string text                 # 待合成文本
string language             # 目标语言
float32 speed               # 语速（0.5-2.0，1.0=正常）
float32 pitch               # 音调（0.5-2.0）
uint8 priority              # 优先级（打断当前播放）
```

```
# hal_audio_msgs/msg/Heartbeat.msg
# HAL_Audio 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
float32 cpu_usage_percent
float32 memory_usage_mb
```

### 4.2 Action 定义 (action)

```
# hal_audio_msgs/action/Speak.action
# TTS 语音合成并播放（长耗时操作）

# Goal
string text
string language
float32 speed
float32 pitch
bool interrupt_current      # 是否打断当前播放

---
# Result
bool success
uint16 error_code
string message
builtin_interfaces/Time actual_duration

---
# Feedback
float32 progress_percent    # 合成进度
bool is_synthesizing        # 是否正在合成
bool is_playing             # 是否正在播放
```

### 4.3 服务定义 (srv)

```
# hal_audio_msgs/srv/GetHealthStatus.srv

---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
string audio_device_info
```

```
# hal_audio_msgs/srv/StartAudioRecording.srv
# 开始录音/ASR

uint8 mode                  # 0=连续识别, 1=单次识别, 2=唤醒词监听
float32 max_duration_sec    # 最大录音时长（0=无限制）
string language             # 识别语言
---
# Response
bool success
uint16 error_code
string message
```

```
# hal_audio_msgs/srv/StopAudioRecording.srv
# 停止录音

---
# Response
bool success
uint16 error_code
string message
```

```
# hal_audio_msgs/srv/SetAudioParameters.srv
# 动态调整音频参数

float32 input_gain_db       # 输入增益，999=不改变
float32 output_volume       # 输出音量，999=不改变
uint32 sample_rate_hz       # 采样率，0=不改变
bool enable_aec             # 回声消除，留空=不改变
bool enable_ns              # 降噪，留空=不改变
---
# Response
bool success
uint16 error_code
string message
```

### 4.4 接口汇总表

#### Topics（HAL_Audio 发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/hal_audio/audio_device_state` | `AudioDeviceState` | HAL_Audio → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 音频设备状态 |
| `/hal_audio/vad_event` | `VadEvent` | HAL_Audio → Interaction | Reliable + Volatile + Depth 10 | 事件驱动 | VAD 事件 |
| `/hal_audio/speech_recognition_result` | `SpeechRecognitionResult` | HAL_Audio → Interaction | Reliable + Volatile + Depth 10 | 事件驱动 | ASR 结果 |
| `/hal_audio/heartbeat` | `Heartbeat` | HAL_Audio → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Topics（HAL_Audio 订阅）

| 名称 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `/interaction/tts_request` | `TtsRequest` | Interaction | TTS 合成请求 |

#### Actions

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/hal_audio/speak` | `Speak` | Interaction | TTS 合成并播放 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/hal_audio/get_health_status` | `GetHealthStatus` | EM, HDS | 健康查询 |
| `/hal_audio/start_recording` | `StartAudioRecording` | Interaction | 开始录音/ASR |
| `/hal_audio/stop_recording` | `StopAudioRecording` | Interaction | 停止录音 |
| `/hal_audio/set_audio_parameters` | `SetAudioParameters` | Interaction, Setting | 动态调参 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌─────────────────────────────────────────────────────────────┐
│                     HALAudioNode                             │
│                                                              │
│  ┌─────────────────┐    ┌─────────────────┐    ┌──────────┐ │
│  │   Audio HAL     │    │  Audio Pipeline │    │  TTS     │ │
│  │   Manager       │───►│  (AEC/NS/VAD)   │───►│  Engine  │ │
│  │                 │    │                 │    │          │ │
│  │ - Device open   │    │ - AEC           │    │ - Text   │ │
│  │ - Buffer mgmt   │    │ - Noise Suppress│    │   → Mel  │ │
│  │ - Sample rate   │    │ - VAD detection │    │ - Vocoder│ │
│  └─────────────────┘    └─────────────────┘    └────┬─────┘ │
│          ▲                                          │      │
│          │                                          ▼      │
│  ┌───────┴───────┐                           ┌──────────┐ │
│  │  Microphone   │                           │ Speaker  │ │
│  │  Array        │                           │          │ │
│  └───────────────┘                           └──────────┘ │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  ASR Engine                            │   │
│  │  - Wake Word Detection (端侧 KWS)                      │   │
│  │  - Streaming ASR (端侧或云端 fallback)                  │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 关键决策

1. **ASR 双模策略**：
   - 唤醒词 + 命令词识别在端侧运行（低延迟、离线可用）
   - 长文本/复杂语音识别通过 Gateway 调用云端大模型 ASR
   - 弱网或无网时，端侧 ASR 作为 fallback

2. **TTS 双模策略**：
   - 常用回复语在端侧预合成缓存
   - 非常用文本通过端侧轻量 TTS 模型实时合成
   - 高保真需求通过 Gateway 调用云端 TTS

3. **音频线程模型**：
   - 音频采集和播放使用独立 Real-time 线程（避免 ROS2 executor 调度抖动）
   - ASR/TTS 推理使用独立 CallbackGroup（与音频线程解耦）

### 5.3 关键流程

**ASR 识别流程**：

```
Interaction ──StartAudioRecording──► HAL_Audio
                                    │
                                    ▼
                            [开始音频采集]
                                    │
                                    ▼
                            [AEC + NS + VAD]
                                    │
                                    ▼
                     ┌──────────────┴──────────────┐
                     ▼                              ▼
            [唤醒词检测(端侧)]              [流式ASR推理]
                     │                              │
                     ▼                              ▼
            [VadEvent] ─────────► [SpeechRecognitionResult]
                     │                    │
                     ▼                    ▼
              Interaction ◄──────── Interaction
```

**TTS 播放流程**：

```
Interaction ──Speak Action──► HAL_Audio
                                   │
                                   ▼
                         [文本预处理]
                                   │
                    ┌──────────────┴──────────────┐
                    ▼                              ▼
            [缓存命中?]                    [端侧TTS推理]
                    │                              │
              [是]  ▼                         [否]  │
            [直接播放]◄────────────────────────────┘
                    │
                    ▼
            [音频输出到扬声器]
                    │
                    ▼
            [Action Feedback/Result]
                    │
                    ▼
              Interaction
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口类型 | 接口名 | 说明 |
|------|------|---------|--------|------|
| Interaction | → | Service | `/hal_audio/start_recording` | 请求开始录音 |
| Interaction | → | Service | `/hal_audio/stop_recording` | 请求停止录音 |
| Interaction | → | Action | `/hal_audio/speak` | TTS 合成播放 |
| Interaction | → | Topic | `/interaction/tts_request` | TTS 文本请求 |
| HAL_Audio | → | Topic | `/hal_audio/speech_recognition_result` | ASR 结果 |
| HAL_Audio | → | Topic | `/hal_audio/vad_event` | VAD 事件 |
| Gateway | ↔ | Service | （内部调用） | 云端 ASR/TTS fallback |
| SM | → | Topic | `/sm/robot_state` | 订阅状态（FAULT 时停止音频） |
| EM | → | Service | `/em/module_control` | 生命周期管理 |
| HDS | → | Service | `/hal_audio/get_health_status` | 健康查询 |

### 6.2 关键时序

**唤醒词触发交互时序**：

```
User          HAL_Audio        Interaction        Agent
 │                │                  │               │
 │──[说唤醒词]───►│                  │               │
 │                │──VadEvent───────►│               │
 │                │                  │               │
 │                │──SpeechResult───►│               │
 │                │  (wake_word=true)│               │
 │                │                  │               │
 │                │                  │──Interaction──►│
 │                │                  │   Event        │
 │                │                  │               │
 │                │◄─StartAudioRecording──│◄──────────────│
 │                │                  │  (请求进一步   │
 │                │                  │   语音识别)    │
 │──[说指令]─────►│                  │               │
 │                │──SpeechResult───►│               │
 │                │  (is_final=true) │               │
 │                │                  │               │
 │                │                  │──文本────────►│
 │                │                  │               │
```

---

## 7. 关键参数与配置

```yaml
# hal_audio_params.yaml
hal_audio_node:
  ros__parameters:
    # 音频硬件
    audio_device_name: "hw:0,0"
    sample_rate_hz: 16000
    channels: 4                    # 4 通道麦克风阵列
    frame_size_ms: 20
    input_gain_db: 20.0
    output_volume: 0.8

    # 音频处理
    enable_aec: true
    enable_ns: true
    enable_agc: true               # 自动增益控制
    vad_threshold: 0.7
    vad_min_speech_duration_ms: 250
    vad_max_silence_ms: 1500

    # ASR
    asr_mode: "hybrid"             # "local" / "cloud" / "hybrid"
    wake_word_model_path: "/opt/striding/models/kws.onnx"
    asr_model_path: "/opt/striding/models/asr_streaming.onnx"
    wake_words: ["你好小步", "小步小步"]
    asr_language: "zh"
    asr_max_recognition_sec: 30.0

    # TTS
    tts_model_path: "/opt/striding/models/tts_vits.onnx"
    tts_language: "zh"
    tts_speed: 1.0
    tts_pitch: 1.0
    tts_cache_size: 100            # 预缓存常用回复数
    tts_preload_phrases: ["好的", "我在", "请稍等", "已完成"]

    # 云端 fallback
    cloud_asr_fallback: true
    cloud_tts_fallback: false
    cloud_asr_timeout_ms: 3000

    # 心跳
    heartbeat_rate_hz: 1.0
```

---

## 8. 错误码定义

| 错误码 | 名称 | 说明 | 处理建议 |
|--------|------|------|---------|
| 18000 | AUDIO_OK | 正常 | - |
| 18001 | ERR_DEVICE_OPEN | 音频设备打开失败 | 检查设备节点权限 |
| 18002 | ERR_DEVICE_BUSY | 音频设备被占用 | 等待或强制释放 |
| 18003 | ERR_MIC_DISCONNECTED | 麦克风断开 | 检查硬件连接 |
| 18004 | ERR_SPEAKER_DISCONNECTED | 扬声器断开 | 检查硬件连接 |
| 18005 | ERR_ASR_MODEL_LOAD | ASR 模型加载失败 | 检查模型文件 |
| 18006 | ERR_TTS_MODEL_LOAD | TTS 模型加载失败 | 检查模型文件 |
| 18007 | ERR_ASR_INFERENCE | ASR 推理异常 | 重启 ASR 引擎 |
| 18008 | ERR_TTS_INFERENCE | TTS 推理异常 | 重试或 fallback |
| 18009 | ERR_AUDIO_BUFFER_OVERFLOW | 音频缓冲区溢出 | 降低采样率或增大缓冲区 |
| 18010 | ERR_AUDIO_BUFFER_UNDERRUN | 音频缓冲区欠载 | 检查 CPU 负载 |
| 18011 | ERR_INVALID_LANGUAGE | 不支持的语言 | 使用支持的语言 |
| 18012 | ERR_CLOUD_ASR_TIMEOUT | 云端 ASR 超时 | 切换端侧 ASR |
| 18013 | ERR_CLOUD_TTS_TIMEOUT | 云端 TTS 超时 | 切换端侧 TTS |
| 18014 | ERR_PERMISSION_DENIED | 音频权限不足 | 检查用户权限 |
| 18015 | ERR_UNKNOWN | 未知错误 | 查看日志 |

---

## 9. 安全约束

1. **隐私保护**：
   - 音频原始数据仅在 HAL_Audio 内部处理，不对外发布原始 AudioFrame（除非 DEBUG 模式）
   - ASR 文本结果脱敏后再上报（如过滤密码、身份证号等）
   - 录音开始前必须通过 LED/声音提示用户

2. **音频安全**：
   - 输出音量限制在 max_volume_limit 以下，防止突发大音量损伤听力或扬声器
   - TTS 播放时自动暂停 ASR（避免自说自听导致误触发）

3. **E-Stop 响应**：
   - 订阅 `/sm/robot_state`，在 `ACTIVE_E_STOP` 状态下立即停止 TTS 播放
   - 在 `FAULT` 状态下停止录音和 ASR，降低系统负载

4. **故障隔离**：
   - ASR 推理崩溃不导致音频采集线程退出
   - TTS 推理失败不阻塞 Interaction 的请求（返回错误码）

---

## 10. 包结构

```
hal_audio_msgs/
├── msg/
│   ├── AudioDeviceState.msg
│   ├── AudioFrame.msg
│   ├── VadEvent.msg
│   ├── SpeechRecognitionResult.msg
│   ├── TtsRequest.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── StartAudioRecording.srv
│   ├── StopAudioRecording.srv
│   └── SetAudioParameters.srv
├── action/
│   └── Speak.action
├── CMakeLists.txt
└── package.xml

hal_audio/
├── include/hal_audio/
│   └── hal_audio_node.hpp
├── src/
│   ├── hal_audio_node.cpp
│   ├── audio_hal_manager.cpp      # 音频硬件抽象
│   ├── audio_pipeline.cpp          # AEC/NS/VAD
│   ├── asr_engine.cpp              # ASR 推理
│   └── tts_engine.cpp              # TTS 推理
├── config/
│   └── hal_audio_params.yaml
├── launch/
│   └── hal_audio.launch.py
├── models/                         # 模型文件（部署时软链接）
│   ├── kws.onnx
│   ├── asr_streaming.onnx
│   └── tts_vits.onnx
└── CMakeLists.txt
```

---

## 11. 关键性能指标（KPI）

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 端到端唤醒延迟 | ≤ 200 ms | 从用户说唤醒词到 HAL_Audio 发出 wake_word 事件 |
| ASR 首字延迟 | ≤ 500 ms | 从用户停止说话到首个识别结果 |
| TTS 首包延迟 | ≤ 300 ms | 从收到文本到首个音频帧播放 |
| 音频采集抖动 | ≤ 5 ms | 20ms 帧间隔的 stddev |
| 端侧 ASR 准确率 | ≥ 95% | 命令词场景（安静环境） |
| CPU 占用 | ≤ 15% | 单核（RK3588，同时进行 ASR+TTS） |
| 内存占用 | ≤ 500 MB | 模型+缓冲区+运行时 |
