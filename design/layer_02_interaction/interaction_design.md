# Interaction 模块详细设计文档

## 1. 模块概述与定位

**模块全称**：Human-Robot Interaction（人机交互总控）

**模块缩写**：Interaction（或简写为 IA）

**定位**：Interaction 是端侧人机交互的唯一入口与总控模块，负责整合多模态输入（语音意图、交互视觉、触觉语义、APP 消息），进行意图理解与仲裁，输出标准化的交互事件给 Agent。管理交互状态（对话轮次、注意力焦点、情感状态、交互优先级），并向下游协调反馈输出（语音 TTS、LED、动作、表情等）。

**核心价值**：
- 将分散的多模态输入（语音、视觉、触觉、消息）统一为结构化交互事件
- 管理交互上下文与对话状态，避免 Agent 被底层模态细节污染
- 实现多模态冲突仲裁（如用户说话时同时做手势，以何者优先）

**所处层次**：交互层（位于 AI 层与应用层之间，是新引入的独立分层）

## 2. 职责边界

| 边界 | Interaction 负责 | 对方负责 |
|------|-----------------|---------|
| Interaction ↔ HAL_Audio | 接收 ASR 文本/VAD 事件，下发 TTS 请求 | 音频采集、ASR 推理、TTS 合成 |
| Interaction ↔ Perception | 接收人体/人脸/手势检测结果 | 环境感知（障碍物、物体检测） |
| Interaction ↔ HAL_EtherCAT | 接收触觉传感器数据 | 触觉硬件采集、电机驱动 |
| Interaction ↔ Gateway | 接收 APP/云端消息，发送交互响应 | 通信协议、云端路由 |
| Interaction ↔ Agent | 上报标准化交互事件，接收 Agent 反馈 | 自然语言理解、推理规划、任务生成 |
| Interaction ↔ TE | 交互触发的任务请求（如"跳个舞"） | 任务调度与执行 |
| Interaction ↔ MP | 交互反馈动作（如点头、挥手） | 动作播放执行 |
| Interaction ↔ SM | 查询机器人状态（是否允许交互） | 全局状态机管理 |
| Interaction ↔ HDS | 上报交互异常（如长时间无响应） | 故障定级 |

**明确不负责的领域**：
- 不负责音频底层处理（ASR/TTS 推理）—— 由 HAL_Audio 负责
- 不负责环境感知（障碍物检测、SLAM）—— 由 Perception 负责
- 不负责高层推理与任务规划 —— 由 Agent 负责
- 不直接控制机器人运动 —— 由 MC/MS/MP 负责
- 不直接对接云端 API —— 由 Gateway 负责

## 3. 状态机设计

```
                         ┌──────────────┐
         ┌───────────────►│              │◄──────────────┐
         │                │    IDLE      │               │
         │                │              │               │
         │                └──────┬───────┘               │
         │                       │ activate()            │
    timeout /                    ▼                       │
    no_interaction          ┌──────────────┐            │
         │                  │   STANDBY    │            │
         │                  │  (等待唤醒)   │            │
         │                  └──────┬───────┘            │
         │                         │ wake_word /        │
         │                         │ person_detected    │
         │                         ▼                    │
         │                  ┌──────────────┐           │
         │                  │  LISTENING   │           │
         │                  │  (采集输入)   │           │
         │                  └──────┬───────┘           │
         │                         │ input_complete     │
         │                         ▼                    │
         │                  ┌──────────────┐           │
         │                  │  PROCESSING  │           │
         │                  │  (意图理解)   │           │
         │                  └──────┬───────┘           │
         │                         │ intent_resolved    │
         │            ┌────────────┼────────────┐      │
         │            ▼            ▼            ▼      │
         │     ┌──────────┐ ┌──────────┐ ┌──────────┐ │
         │     │RESPONDING│ │DELEGATING│ │AWAITING  │ │
         │     │(语音/动作│ │(提交任务│ │CONFIRM   │ │
         │     │ 反馈)    │ │ 给Agent) │ │(等待确认)│ │
         │     └────┬─────┘ └────┬─────┘ └────┬─────┘ │
         │          │            │            │       │
         │          └────────────┴────────────┘       │
         │                       │                      │
         │                       ▼                      │
         │                  ┌──────────────┐           │
         └──────────────────│   STANDBY    │◄──────────┘
                            │              │
                            └──────────────┘

                            ┌──────────────┐
                            │    FAULT     │
                            │              │
                            └──────────────┘
```

**状态说明**：

| 状态 | 说明 |
|------|------|
| IDLE | 初始状态，交互系统未激活 |
| STANDBY | 待机状态，监听唤醒词或人体靠近 |
| LISTENING | 正在采集多模态输入（语音+视觉+触觉） |
| PROCESSING | 整合输入、进行意图理解与仲裁 |
| RESPONDING | 直接反馈（TTS 回复、点头、LED 表情） |
| DELEGATING | 将任务委托给 Agent/TE（如"去厨房拿水"） |
| AWAITING_CONFIRM | 等待用户确认（如"您要冷水还是热水？"） |
| FAULT | 交互系统故障 |

**状态转换条件**：
- `activate()`：系统启动后进入待机
- `wake_word` / `person_detected`：检测到唤醒词或用户靠近
- `input_complete`：语音输入结束（VAD 尾端检测）或手势完成
- `intent_resolved`：意图理解完成
- `direct_response`：意图可直接本地响应（如"你好"→TTS"您好"）
- `delegate_to_agent`：意图需要 Agent 推理（如"帮我规划路线"）
- `need_confirm`：意图存在歧义，需要用户确认
- `confirm_received`：用户确认
- `timeout`：交互超时时返回待机
- `error`：意图理解失败或模块异常

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# interaction_msgs/msg/InteractionState.msg
# 交互状态广播

uint8 state                 # 当前状态
uint8 prev_state
builtin_interfaces/Time state_changed_at
string active_user_id       # 当前交互用户ID（人脸识别）
float32 attention_level     # 注意力等级（0.0-1.0）
uint32 dialogue_turn        # 当前对话轮次
float32 session_duration_sec # 当前会话时长
```

```
# interaction_msgs/msg/InteractionEvent.msg
# 标准化交互事件（Interaction → Agent/TE/MP）

builtin_interfaces/Time stamp
string event_id             # 事件唯一ID
uint8 modality              # 输入模态
uint8 MODALITY_VOICE   = 0
uint8 MODALITY_VISION  = 1
uint8 MODALITY_TOUCH   = 2
uint8 MODALITY_MESSAGE = 3
uint8 MODALITY_FUSION  = 4  # 多模态融合

string intent_category      # 意图类别（"greeting", "command", "query", "confirm", "cancel", "unknown"）
string intent_action        # 意图动作（"navigate", "pick", "wave", "dance", "stop"）
string text                 # 文本内容（ASR 结果或消息文本）
float32 confidence          # 意图置信度（0.0-1.0）
string target_object        # 目标对象（如"杯子"）
string target_location      # 目标位置（如"厨房"）
string json_payload         # 模态特定数据（JSON）
string source_user_id       # 来源用户ID
```

```
# interaction_msgs/msg/VoiceIntent.msg
# 语音意图（由 Interaction 从 ASR 结果生成）

builtin_interfaces/Time stamp
string text
string language
float32 confidence
bool is_wake_word
bool is_command
bool is_question
string detected_entities    # JSON 数组（NER 结果）
```

```
# interaction_msgs/msg/VisualIntent.msg
# 视觉意图（由 Interaction 从 Perception 人体检测结果生成）

builtin_interfaces/Time stamp
string user_id
uint8 gesture_type
uint8 GESTURE_NONE     = 0
uint8 GESTURE_WAVE     = 1
uint8 GESTURE_POINT    = 2
uint8 GESTURE_STOP     = 3
uint8 GESTURE_THUMBS_UP = 4
float32 gesture_confidence
float32[3] gaze_direction # 注视方向向量
float32 face_emotion_score # 情绪评分（-1.0=负向, 1.0=正向）
bool is_looking_at_robot
```

```
# interaction_msgs/msg/TouchIntent.msg
# 触觉意图（由 Interaction 从 HAL_EtherCAT 触觉数据生成）

builtin_interfaces/Time stamp
string body_part            # 被触摸的身体部位（"hand", "shoulder", "head"）
uint8 touch_type
uint8 TOUCH_TYPE_TAP     = 0
uint8 TOUCH_TYPE_HOLD    = 1
uint8 TOUCH_TYPE_SWIPE   = 2
uint8 TOUCH_TYPE_PAT     = 3
float32 pressure          # 压力值（N 或归一化）
float32 duration_sec
```

```
# interaction_msgs/msg/MessageIntent.msg
# 消息意图（由 Interaction 从 Gateway 消息生成）

builtin_interfaces/Time stamp
string message_id
string sender_id            # 发送者
string channel              # 通道（"app", "cloud", "local_ui"）
string text
string message_type         # "text", "voice", "command", "notification"
```

```
# interaction_msgs/msg/EmotionState.msg
# 机器人情感/表情状态（Interaction → MP/LED）

builtin_interfaces/Time stamp
uint8 emotion
uint8 EMOTION_NEUTRAL  = 0
uint8 EMOTION_HAPPY    = 1
uint8 EMOTION_SAD      = 2
uint8 EMOTION_SURPRISED = 3
uint8 EMOTION_CONFUSED = 4
uint8 EMOTION_FOCUS    = 5
float32 intensity         # 强度（0.0-1.0）
string led_pattern        # LED 灯效模式
string motion_hint        # 动作提示（如"nod", "shake_head", "wave"）
```

```
# interaction_msgs/msg/Heartbeat.msg
# Interaction 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
uint32 active_sessions
float32 avg_intent_latency_ms
```

### 4.2 服务定义 (srv)

```
# interaction_msgs/srv/GetHealthStatus.srv

---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 total_interactions_today
```

```
# interaction_msgs/srv/QueryInteractionHistory.srv
# 查询交互历史

builtin_interfaces/Time start_time
builtin_interfaces/Time end_time
string user_id_filter       # 空=全部
uint8 modality_filter       # 255=全部
---
# Response
bool success
InteractionEvent[] events
uint32 count
```

```
# interaction_msgs/srv/SetInteractionMode.srv
# 设置交互模式

uint8 mode
uint8 MODE_NORMAL      = 0
uint8 MODE_SILENT      = 1   # 静音模式（禁用TTS，仅LED反馈）
uint8 MODE_CHILD       = 2   # 儿童模式（简化语言、高安全）
uint8 MODE_DEMO        = 3   # 演示模式（自动循环展示）
uint8 MODE_PRIVACY     = 4   # 隐私模式（禁用录音、本地处理）
---
# Response
bool success
uint16 error_code
string message
```

```
# interaction_msgs/srv/SendMessage.srv
# 向用户发送消息（通过 Gateway 转发到 APP）

string text
string message_type         # "text", "voice", "image", "notification"
string target_user_id
bool require_ack            # 是否需要用户确认
---
# Response
bool success
uint16 error_code
string message_id
```

### 4.3 接口汇总表

#### Topics（Interaction 发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/interaction/interaction_state` | `InteractionState` | IA → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 交互状态 |
| `/interaction/interaction_event` | `InteractionEvent` | IA → Agent/TE | Reliable + Volatile + Depth 100 | 事件驱动 | 标准化交互事件 |
| `/interaction/emotion_state` | `EmotionState` | IA → MP/LED | Reliable + Volatile + Depth 1 | 事件驱动 | 情感/表情状态 |
| `/interaction/voice_intent` | `VoiceIntent` | IA → Agent | Reliable + Volatile + Depth 10 | 事件驱动 | 语音意图（细粒度） |
| `/interaction/visual_intent` | `VisualIntent` | IA → Agent | Reliable + Volatile + Depth 10 | 事件驱动 | 视觉意图 |
| `/interaction/touch_intent` | `TouchIntent` | IA → Agent | Reliable + Volatile + Depth 10 | 事件驱动 | 触觉意图 |
| `/interaction/heartbeat` | `Heartbeat` | IA → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Topics（Interaction 订阅）

| 名称 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `/hal_audio/speech_recognition_result` | `SpeechRecognitionResult` | HAL_Audio | ASR 结果 |
| `/hal_audio/vad_event` | `VadEvent` | HAL_Audio | VAD 事件 |
| `/perception/human_detection` | `HumanDetection` | Perception | 人体/人脸/手势检测 |
| `/hal_ethercat/touch_sensor` | `TouchSensorData` | HAL_EtherCAT | 触觉传感器数据 |
| `/gateway/incoming_message` | `AppMessage` | Gateway | APP/云端消息 |
| `/agent/agent_response` | `AgentResponse` | Agent | Agent 响应（需要转达给用户） |
| `/sm/robot_state` | `RobotState` | SM | 机器人状态 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/interaction/get_health_status` | `GetHealthStatus` | EM, HDS | 健康查询 |
| `/interaction/query_interaction_history` | `QueryInteractionHistory` | Gateway, Agent | 查询交互历史 |
| `/interaction/set_interaction_mode` | `SetInteractionMode` | Gateway, Setting | 设置交互模式 |
| `/interaction/send_message` | `SendMessage` | Agent, TE | 向用户发消息 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      InteractionNode                             │
│                                                                  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │  Input Fusion   │  │ Intent Engine   │  │   Response      │  │
│  │   (输入融合)     │──►│   (意图引擎)     │──►│   Manager       │  │
│  │                 │  │                 │  │   (响应管理)     │  │
│  │ - VoiceIntent   │  │ - NER/LU        │  │ - TTS dispatch  │  │
│  │ - VisualIntent  │  │ - Context mgmt  │  │ - LED dispatch  │  │
│  │ - TouchIntent   │  │ - Arbitration   │  │ - Motion dispatch│ │
│  │ - MessageIntent │  │ - Priority queue│  │ - Msg dispatch  │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
│           ▲                                              │       │
│           │                                              ▼       │
│  ┌────────┴────────┐                            ┌──────────────┐│
│  │ Context Memory  │                            │ Output Queue ││
│  │                 │                            │              ││
│  │ - Session stack │                            │ (优先级排序)  ││
│  │ - User profiles │                            │              ││
│  │ - Dialogue hist │                            └──────────────┘│
│  └─────────────────┘                                            │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 关键决策

1. **多模态仲裁策略**：
   - 语音意图优先级最高（人类与机器人交互的主要模态）
   - 手势作为语音的补充（如指向+"拿那个"）
   - 触觉作为打断信号（如拍肩膀→暂停当前动作）
   - 消息作为远程交互输入
   - 当多模态同时触发时，采用时间窗口（500ms）内融合策略

2. **上下文管理**：
   - 短期上下文：当前会话（最近 10 轮对话）
   - 中期上下文：当前用户 profile（偏好、历史指令模式）
   - 长期上下文：全局交互统计（高频指令、常见问题）

3. **隐私模式**：
   - 支持一键切换隐私模式（禁用录音、仅本地处理、不上传云端）
   - 隐私模式下 ASR 完全在端侧运行，不使用云端 fallback

### 5.3 关键流程

**交互事件处理流程**：

```
User ──[语音/手势/触摸/消息]──► HAL_Audio/Perception/EtherCAT/Gateway
                                      │
                                      ▼
                              [Interaction 输入融合]
                                      │
                    ┌─────────────────┼─────────────────┐
                    ▼                 ▼                 ▼
              [VoiceIntent]    [VisualIntent]     [TouchIntent]
                    │                 │                 │
                    └─────────────────┼─────────────────┘
                                      ▼
                              [意图理解与仲裁]
                                      │
                    ┌─────────────────┼─────────────────┐
                    ▼                 ▼                 ▼
              [直接响应]         [委托Agent]         [需要确认]
                    │                 │                 │
                    ▼                 ▼                 ▼
            [TTS/LED/动作]     [InteractionEvent]  [TTS询问]
                    │           上报给 Agent        等待用户
                    │                 │                 │
                    │                 ▼                 │
                    │           [Agent推理]◄────────────┘
                    │                 │
                    │                 ▼
                    │           [任务提交给 TE]
                    │                 │
                    └─────────────────┘
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口类型 | 接口名 | 说明 |
|------|------|---------|--------|------|
| HAL_Audio | → | Topic | `/hal_audio/speech_recognition_result` | 接收 ASR 结果 |
| HAL_Audio | → | Topic | `/hal_audio/vad_event` | 接收 VAD 事件 |
| HAL_Audio | → | Action | `/hal_audio/speak` | TTS 播放 |
| Perception | → | Topic | `/perception/human_detection` | 接收人体/手势检测 |
| HAL_EtherCAT | → | Topic | `/hal_ethercat/touch_sensor` | 接收触觉数据 |
| Gateway | → | Topic | `/gateway/incoming_message` | 接收消息 |
| Gateway | → | Service | `/interaction/send_message` | 发送消息给用户 |
| Agent | → | Topic | `/agent/agent_response` | 接收 Agent 响应 |
| Agent | ← | Topic | `/interaction/interaction_event` | 上报交互事件 |
| TE | ← | Topic | `/interaction/interaction_event` | 直接任务委托 |
| MP | ← | Topic | `/interaction/emotion_state` | 表情/动作反馈 |
| SM | → | Topic | `/sm/robot_state` | 订阅状态 |
| Setting | → | Service | `/interaction/set_interaction_mode` | 模式切换 |

### 6.2 关键时序

**多模态交互时序（语音+手势）**：

```
User          HAL_Audio       Perception      Interaction       Agent
 │                │               │                │              │
 │──[指向左边]─────►│               │                │              │
 │                │               │──VisualIntent─►│              │
 │                │               │  (gesture=point│              │
 │                │               │   direction=left)             │
 │                │               │                │              │
 │──[拿那个]──────►│               │                │              │
 │                │──SpeechResult─►│                │              │
 │                │  ("拿那个")    │                │              │
 │                │               │                │              │
 │                │               │                │[融合: 指向+语音]
 │                │               │                │              │
 │                │               │                │──Interaction──►│
 │                │               │                │   Event        │
 │                │               │                │  intent="pick" │
 │                │               │                │  target=left   │
 │                │               │                │              │
 │                │◄─Speak Action─│◄───────────────│◄─────────────│
 │                │  "好的，拿左边的杯子"             │              │
 │                │               │                │              │
 │──[TTS播放]─────│               │                │              │
 │                │               │                │              │
```

---

## 7. 关键参数与配置

```yaml
# interaction_params.yaml
interaction_node:
  ros__parameters:
    # 交互模式
    default_mode: 0                # NORMAL
    enable_wake_word: true
    enable_gesture: true
    enable_touch: true
    enable_remote_message: true

    # 唤醒与监听
    wake_word_timeout_sec: 10.0    # 唤醒后监听超时
    max_session_duration_sec: 300.0 # 单会话最大时长
    input_fusion_window_ms: 500    # 多模态融合时间窗口

    # 意图仲裁
    voice_priority: 100
    gesture_priority: 80
    touch_priority: 90
    message_priority: 70
    min_confidence_threshold: 0.6

    # 上下文
    max_dialogue_turns: 10
    context_ttl_sec: 60.0          # 上下文存活时间
    max_user_profiles: 20

    # 响应
    default_tts_speed: 1.0
    enable_led_feedback: true
    enable_motion_feedback: true
    response_timeout_sec: 5.0      # Agent 响应超时

    # 隐私
    privacy_mode_local_only: true  # 隐私模式下禁止云端
    auto_privacy_timeout_sec: 0    # 自动进入隐私模式（0=禁用）

    # 心跳
    heartbeat_rate_hz: 1.0
```

---

## 8. 错误码定义

| 错误码 | 名称 | 说明 | 处理建议 |
|--------|------|------|---------|
| 19000 | INTERACTION_OK | 正常 | - |
| 19001 | ERR_INTENT_RECOGNITION | 意图识别失败 | 请求用户重复 |
| 19002 | ERR_LOW_CONFIDENCE | 意图置信度低于阈值 | 请求用户澄清 |
| 19003 | ERR_MULTIMODAL_CONFLICT | 多模态输入冲突 | 以语音优先，提示用户 |
| 19004 | ERR_CONTEXT_LOST | 上下文丢失 | 重新开始对话 |
| 19005 | ERR_AGENT_TIMEOUT | Agent 响应超时 | 提示用户网络问题 |
| 19006 | ERR_TTS_UNAVAILABLE | TTS 不可用 | LED 反馈替代 |
| 19007 | ERR_INVALID_MODE | 无效交互模式 | 使用支持的模式 |
| 19008 | ERR_USER_NOT_RECOGNIZED | 用户未识别 | 请求用户注册 |
| 19009 | ERR_PRIVACY_BLOCKED | 隐私模式阻止操作 | 提示用户切换模式 |
| 19010 | ERR_MESSAGE_SEND_FAILED | 消息发送失败 | 检查 Gateway 连接 |
| 19011 | ERR_HISTORY_FULL | 交互历史满 | 清理历史记录 |
| 19012 | ERR_SENSOR_UNAVAILABLE | 传感器不可用 | 检查 HAL 模块 |
| 19013 | ERR_TOO_MANY_SESSIONS | 并发会话过多 | 关闭旧会话 |
| 19014 | ERR_INVALID_INTENT | 无效意图 | 提示用户重新表达 |
| 19015 | ERR_UNKNOWN | 未知错误 | 查看日志 |

---

## 9. 安全约束

1. **交互安全**：
   - 危险指令（如"撞墙"、"自毁"）必须经 Agent 二次确认，Interaction 不直接过滤
   - 儿童模式下禁用危险动作指令（通过 SM 状态控制）
   - 交互事件上报 Agent 前，必须经过 SM 状态校验（`FAULT` / `ACTIVE_E_STOP` 下静默处理）

2. **隐私保护**：
   - 所有音频/视觉原始数据不存储在 Interaction 中，仅保留意图结果
   - 交互历史默认保留 7 天，支持一键清除
   - 人脸识别数据本地加密存储，不上传

3. **E-Stop 响应**：
   - `ACTIVE_E_STOP` 状态下：停止所有交互响应（TTS/LED/动作），仅保留触觉紧急停止
   - `FAULT` 状态下：进入静默模式，等待恢复

4. **故障隔离**：
   - 单模态输入故障（如麦克风坏）不导致整个交互系统崩溃
   - 视觉/触觉可作为语音的 fallback 输入方式

---

## 10. 包结构

```
interaction_msgs/
├── msg/
│   ├── InteractionState.msg
│   ├── InteractionEvent.msg
│   ├── VoiceIntent.msg
│   ├── VisualIntent.msg
│   ├── TouchIntent.msg
│   ├── MessageIntent.msg
│   ├── EmotionState.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── QueryInteractionHistory.srv
│   ├── SetInteractionMode.srv
│   └── SendMessage.srv
├── CMakeLists.txt
└── package.xml

interaction/
├── include/interaction/
│   └── interaction_node.hpp
├── src/
│   ├── interaction_node.cpp
│   ├── input_fusion.cpp         # 多模态输入融合
│   ├── intent_engine.cpp         # 意图理解与仲裁
│   ├── context_manager.cpp       # 上下文管理
│   └── response_manager.cpp      # 响应管理
├── config/
│   └── interaction_params.yaml
├── launch/
│   └── interaction.launch.py
└── CMakeLists.txt
```

---

## 11. 关键性能指标（KPI）

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 意图理解延迟 | ≤ 300 ms | 从输入完成到 InteractionEvent 发出 |
| 多模态融合延迟 | ≤ 100 ms | 在融合窗口内完成模态对齐 |
| TTS 反馈延迟 | ≤ 800 ms | 从意图理解到 TTS 播放开始 |
| 会话上下文保持 | ≥ 10 轮 | 对话轮次不丢失 |
| 用户识别准确率 | ≥ 98% | 人脸识别（注册后） |
| 手势识别准确率 | ≥ 90% | 预定义手势集 |
| CPU 占用 | ≤ 10% | 单核（RK3588，空闲时） |
| 内存占用 | ≤ 200 MB | 上下文+用户 profile+运行时 |
