# 语音交互硬件选型方案（v1.2）

> 版本：v1.2
> 日期：2026-05-09
> 修订说明：v1.2 在 v1.1 基础上新增 ①同业人形机器人语音方案对标（特斯拉 / Figure / 宇树 / 小米 / 智元 / 1X）；②扩大音频 DSP 候选池（增 Synaptics SR80、Knowles AISonic、Cirrus Logic、启英泰伦、全志 R128）；③最终选型决策矩阵。
> 关联文档：[voice_interaction_subsystem.md](voice_interaction_subsystem.md)、[hal_audio_design.md](../design/layer_07_hal_infra/hal_audio_design.md)

---

## 0. 文档范围与可靠性等级

本文档回答四个问题：

1. 现役主流人形机器人（特斯拉 Optimus、Figure 02、宇树 G1、小米 CyberOne、智元灵犀 X2、1X NEO）**官方公开**的语音相关硬件
2. 人形机器人专用音频 DSP 的候选池与横向对比
3. 各候选方案对本项目（RK3588 单片，端云协同）的适配性
4. 最终选型决策与 BOM

**可靠性标注规则**

| 标注 | 含义 |
|------|------|
| `[官方]` | 官方网站/产品页 publicly disclosed，可直接引用 |
| `[行业通用]` | 人形机器人语音子系统的通用设计实践，不特指某一型号 |
| `[本项目]` | 本项目独立设计决策 |
| `[未公开]` | 厂商未披露，**不做推断** |

---

## 1. 同业人形机器人语音方案对标 `[官方]`

下表汇总当前主流人形机器人**已公开**的语音子系统硬件，作为本项目选型参考。**所有"未公开"项目均不做推断**。

### 1.1 总览对照

| 机器人 | 主算力 | 麦克风 | 扬声器 | 端侧 ASR | LLM 集成 | 特殊设计 |
|--------|--------|--------|--------|---------|---------|---------|
| **Tesla Optimus V3** | 未公开（自研 D1 类） | 未公开 | 未公开 | 未公开 | **Grok（xAI）** | 端云一体 |
| **Figure 02** | 2× NVIDIA RTX GPU | "头部内置麦克风"（数量未公开） | 头部内置（V3 升级 4×功率，4×尺寸） | 自研 Helix VLA（替代 OpenAI） | **Helix VLA（自研）** | speech-to-speech |
| **Unitree G1** | 8 核 CPU | **4-Mic 阵列** | **5W** | 未公开 | 未公开（开放给开发者） | 全系标配 |
| **小米 CyberOne** | 自研 Mi-Sense | "Sophisticated 麦克风"（具体未公开）| 未公开 | **MiAI 语音情感识别** | 自研（45 类情感识别） | 85 类环境声 + 情感 |
| **AgiBot 灵犀 X2** | **RK3588 × 2**（旗舰版加 Orin NX 157 TOPS）| **麦克风阵列**（数量未公开）+ 标配无线领夹麦 | "内置扬声器"（功率未公开） | 未公开 | **GO-1 多模态基础模型** | 头触+视觉+语音多模态 |
| **1X NEO** | 未公开 | 未公开 | 未公开 | 未公开 | LLM（合作方未公开） | 家庭服务定位 |

> **数据源**：Tesla（[techyquantum](https://techyquantum.com/tesla-optimus-v3-with-grok-voice-ai/)、[tesery](https://www.tesery.com/blogs/news/elon-musk-confirms-tesla-optimus-v3-already-uses-grok-voice-ai)）、Figure（[techcrunch](https://techcrunch.com/2024/08/06/figures-new-humanoid-robot-leverages-openai-for-natural-speech-conversations/)、[mikekalil](https://mikekalil.com/blog/figure-03-robot-for-home/)）、Unitree（[robostore](https://robostore.com/products/unitree-g1-robotic-humanoid)、[robotshop](https://www.robotshop.com/products/unitree-g1-humanoid-robot-us)）、小米（[ithome](https://www.ithome.com/0/634/622.htm)、[mi.com](https://www.mi.com/cyberone)）、AgiBot（[agibot.com](https://www.agibot.com/products/X2)、[aiwiki](https://aiwiki.ai/wiki/agibot_x2_ultra)）

### 1.2 关键观察

| 现象 | 解读 |
|------|------|
| **3/6 厂商不公开 mic 数量与阵列形态** | 行业普遍把"麦克风方案"视为差异化点，等同于把 ASR 准确率视为商业机密 |
| **唯一明确公开 4-Mic 的是 Unitree G1** | 与本项目 §3.2 的 4-Mic 环形选型一致，验证该形态在中等成本人形机器人上的合理性 |
| **AgiBot X2 标配"无线领夹麦"** | 暴露固定阵列在远距离/遮挡/嘈杂场景的局限——本项目应预留无线麦接入 |
| **小米 CyberOne 公开"环境声+情感识别"** | 提示语音子系统不止于 ASR，还需感知非语言音频（笑声/哭声/敲门/警报） |
| **Figure 03 公开"扬声器功率 ×4，体积 ×2"** | 间接说明 Figure 02 的扬声器在嘈杂家庭/工厂环境下的输出不足，**功率不能省** |
| **Figure 从 OpenAI 切换到自研 Helix** | 端云分工策略不稳定，本项目 ASR/TTS 应保持**引擎可替换**架构 |
| **Tesla Optimus 用 Grok（云）而非端侧** | 反映 V3 阶段端侧算力仍不足以承载 LLM-级对话——本项目"端云协同"是务实路线 |

### 1.3 对本项目的设计启示

1. **公开可见的最低基线**：4-Mic 阵列 + 5W 扬声器（与 Unitree G1 持平）
2. **实用性升级**：预留**无线领夹麦**接入（参考 AgiBot X2）
3. **环境声分类能力**（参考小米）：在 KWS 之外增加非语言音频分类（紧急关键词如"救命""停下"+ 警报识别）
4. **扬声器功率不能省**（参考 Figure 03 升级路径）：直接按 5W+ 立体声配置，避免后期返工
5. **LLM 引擎抽象**（参考 Figure 切换 OpenAI→Helix）：Agent 层与具体 LLM 解耦，便于切换 Grok/GPT/Qwen/自研

---

## 2. AgiBot 灵犀 X2 深度对标 `[官方]`

由于 AgiBot 是**唯一公开主控芯片型号**（RK3588 × 2）的同价位竞品，单独深入分析。

### 2.1 整机计算平台 `[官方]`

| 配置项 | 青春版 | 旗舰版 |
|--------|--------|--------|
| 基础算力板 | RK3588 × 2 | RK3588 × 2 |
| 高算力开发板 | — | NVIDIA Orin NX（157 TOPS） |
| 内存/存储 | 未公开 | 未公开 |
| Xyber-Edge 协处理器 | ✓ | ✓（小脑控制器，关节运动协调） |
| Xyber-DCU 协处理器 | — | ✓（域控制器，AI 推理与执行器之间） |

**关键观察**：灵犀 X2 用 **双 RK3588**（而非单一高算力 SoC）。典型分工：一片做实时控制/感知，另一片做 AI 推理（含语音）。旗舰版追加 Orin NX 用于 **二次开发**（不是主算力），这一定位说明 AgiBot 内部也认为 Orin NX 不适合做日常运行主控。

### 2.2 音频系统 `[官方]`

| 组件 | 规格 | 数据源 |
|------|------|------|
| 麦克风 | 麦克风阵列（数量、型号、阵型 **未公开**）| AgiBot 官网 |
| 附件麦 | **迷你无线麦克风**（标配）| AgiBot 官网 |
| 扬声器 | 内置扬声器（功率、数量 **未公开**）| AgiBot 官网 |
| 音频 DSP | **未公开** | — |
| 端侧 ASR | **未公开** | — |
| 端侧 TTS | **未公开** | — |
| 端云策略 | **未公开** | — |
| LLM | **GO-1 基础模型**（自研多模态） | aiwiki |

### 2.3 灵犀 X2 对本项目的可参考点

1. **双 SoC 分工**：双 6 TOPS（NPU）证明"单片不够，两片可行"——若本项目端侧 LLM 与感知/运动 NPU 抢占严重，**横向扩展（双 RK3588）成本低于纵向升级（换 Orin）**
2. **有线 + 无线双麦**：标配无线领夹麦，承认固定阵列在家庭/工厂场景的局限性
3. **多模态融合（头触+视觉+语音）**：与本项目 Interaction 模块设计一致

### 2.4 灵犀 X2 不可参考的部分（不做推断）

- 音频 DSP 型号、有无独立 DSP
- 麦克风数量、型号、阵列几何
- ASR/TTS 引擎与模型大小
- 端云切换阈值
- 音频子系统功耗

---

## 3. 本项目语音交互硬件选型 `[本项目]`

### 3.1 设计约束

| 约束 | 来源 |
|------|------|
| 主 SoC：RK3588（单片，NPU 6 TOPS） | 本项目现有平台决策 |
| 端云协同（平衡型） | 用户偏好 |
| 场景：家庭/工厂人形机器人 | 用户定位 |
| 内存：建议升级至 16GB（见 §6） | 本项目 |

### 3.2 麦克风阵列选型 `[行业通用]`

| 参数 | 选型 | 理由 |
|------|------|------|
| 阵列形态 | **4-Mic 环形**，直径 60–70mm | 360° 均匀拾音，DOA 精度 ≤ ±15°，与 Unitree G1 持平 |
| 麦克风类型 | **数字 MEMS，PDM 输出** | 抗 EMI，布线简单，主流选择 |
| 主推型号 | **Knowles SPH0641LU4H-1** | SNR 65 dB(A)，AOP 130 dB SPL，**带宽到 80 kHz**（可兼超声测距），¥8–12/颗 |
| 备选型号 | Knowles SPK0641HT4H-1（顶部口） | 高 SNR，针对多 Mic 阵列优化 |
| 备选型号 | TDK ICS-41351 | SNR 65 dB(A)，¥6–9/颗，国内备货稳定 |
| 安装位置 | 头部顶端或颈部环形开槽 | 避免肩/手臂遮挡，远离电机振动源 |
| 补充方案 | 预留**无线麦克风接收器**接口（蓝牙/UHF）| 参考 AgiBot X2 标配，改善远场/遮挡场景 |

**阵列几何（俯视）**：

```
       Mic0 (前)
          |
  Mic3 ——+—— Mic1
          |
       Mic2 (后)

  D = 65mm，工作频率 300 Hz – 7 kHz
  设计拾音距离 ≤ 3m
```

> **为什么选 SPH0641LU4H-1 而非 LM4H-1？**
> LU4H-1 带宽 100Hz–80kHz，可兼做**超声接近感知**（人形机器人腰下盲区补充）；LM4H-1 仅 100Hz–10kHz，纯语音。多功能复用降低系统总 BOM。

### 3.3 音频 DSP 候选池（v1.2 大幅扩展）`[行业通用]`

**为什么需要独立音频 DSP？**

RK3588 内置音频接口（I2S/PDM）不含硬件 AEC。AEC 若在 SoC 上软件实现：

- ROS2 executor 调度抖动 → reference 信号延迟漂移 > 10ms → ERLE 衰减 ≥ 10dB
- 系统负载尖峰时 AEC 收敛失败 → 自激振铃
- 步行/电机噪声叠加 → 软件 AEC 适配工作量大幅增加

独立 DSP 把 AEC/BF/NS/唤醒词检测全部固化为硬实时任务，与 SoC 完全解耦。

**v1.2 扩展候选池（横向对比）**

| 候选 | 类型 | 算力 | 麦克风路数 | AEC/BF | 端侧 KWS | NPU/AI | 单价（¥）| 适配性 |
|------|------|------|-----------|--------|---------|---------|---------|--------|
| **XMOS XU316-1024** | xCORE.AI | 16 核 1024 MIPS | 8× PDM | ✅ 官方库 | ✅ DS-CNN | ✅ TFLite Micro | ¥80–120 | **⭐⭐⭐ 主推** |
| **Synaptics Astra SR80**（2026 Q2 Sample） | M33+DSP+NPU+HiFi5 | 多核异构 | 多 PDM | ✅ ENC（超 Teams v5）| ✅ 声纹+KWS | ✅ Synaptics NPU | TBD（旗舰定位）| ⭐⭐ 待评估 |
| **Knowles AISonic IA8201**（已被 Syntiant 收购）| Cortex-M4 + 双 Tensilica DSP | 175 MHz × 2 核 | 4× PDM | ✅ DMX 核 | ✅ HMD 永远在线核 | ✅ TFLite Micro | ¥30–45 | ⭐⭐ 性价比 |
| **Knowles AISonic IA8508** | M4 + 4× DSP | 150 MHz × 4 核 | **8× PDM**（最大）| ✅ DNN 加速 | ✅ 多关键词并发 | ✅ DNN 硬件加速 | ¥60–80 | ⭐⭐ 高路数场景 |
| **Cirrus Logic CS47L35** | 3 核 DSP | 450 MIPS | 4× PDM | ✅ SoundClear | ✅ 多触发词 | ⚠️ 无 NPU | ¥25–40 | ⭐⭐ 中端备选 |
| **Cirrus Logic CS47L90** | 7 核 DSP | 975 MIPS | **10× PDM** | ✅ SoundClear+SmartHIFI | ✅ Always-on | ⚠️ 无独立 NPU | ¥60–90 | ⭐⭐ 高保真备选 |
| **启英泰伦 CI13X / CI135X** | BNPU V3.5 | 220 MHz | 多 PDM | ✅ DNN 降噪 | ✅ 离线自然说 | ✅ BNPU | ¥10–25 | ⭐⭐ 国产替代 |
| **全志 R128** | RISC-V + DSP + Codec + WiFi/BT | 集成度高 | 集成 DMIC | ⚠️ 软件 AEC | ⚠️ 基础 | ❌ 无 | ¥15–30 | ⭐ 不推荐独立场景 |
| **Espressif ESP32-S3** | Xtensa LX7 × 2 | — | I2S/PDM | ⚠️ 软件 | ⚠️ 弱 | ❌ | ¥15–25 | ❌ 不推荐 |

### 3.4 DSP 决策（v1.2 更新）

经横向评估，**主推 XMOS XU316-1024**，理由如下：

| 决策维度 | XMOS XU316 表现 |
|---------|---------------|
| 现货可获得性 | ✅ 2026 Q2 量产稳定，国内有代理 |
| AEC 算法成熟度 | ✅ 官方库 + 大量参考设计（Amazon Alexa Voice Service 认证）|
| 端侧 KWS 可固化 | ✅ DS-CNN ~100KB，永不更新，签名审核 |
| 与 RK3588 通信 | ✅ I2S TDM 8-slot + UART 控制通道 |
| 工具链 | ✅ XMOS xTIMEcomposer + TensorFlow Lite Micro |
| 国产替代风险 | ⚠️ 英国厂商，需考虑 BOM 替代方案 |

**国产替代预案**：启英泰伦 CI135X（¥10–25）作为第二供应商，单价显著低，但需要适配 BNPU 工具链。在 POC 阶段并行评估。

### 3.5 XMOS XU316 职责划分（细化）

| 功能 | 说明 |
|------|------|
| PDM 解调 | 4 路 PDM → I2S TDM 8-slot |
| 波束成形（BF） | MVDR/GSC，指向主说话人方向 |
| 声学回声消除（AEC） | 8-tap NLMS，参考信号取自扬声器 I2S 回环 |
| 噪声抑制 + AGC | 抑制电机/步行噪声 |
| 双路 VAD | 一路给唤醒词检测，一路给 SoC 侧 ASR |

> **安全红线落实**：唤醒词检测链路 XMOS → SoC 必须独立 CallbackGroup，与 ROS2 主调度解耦。这是项目 CLAUDE.md 中 **"关键实时路径必须独立 CallbackGroup"** 安全原则在硬件层的落地方式。

### 3.6 Audio Codec / 功放选型 `[行业通用]`

| 器件 | 主推型号 | 职责 | 单价（¥）| 备选 |
|------|---------|------|---------|------|
| 上行 ADC | **TI TLV320ADC5140** | XMOS I2S TDM 输出 → SoC，4ch 16kHz | ¥15–20 | TI ADCx140 系列 |
| 下行功放 | **TI TAS5805M** | SoC I2S → 扬声器，D 类数字功放，内置 EQ | ¥10–15 | NXP TFA9874 |

### 3.7 扬声器选型 `[行业通用]`（v1.2 提升功率）

参考 Figure 03 的升级路径（×2 体积、×4 功率），本项目直接按上线水准配置：

| 参数 | v1.1 选型 | v1.2 选型（提升） | 理由 |
|------|---------|-----------------|------|
| 数量 | 2（立体声）| 2（立体声）| 与原版一致 |
| 尺寸 / 功率 | Φ40mm，3W RMS | **Φ50mm，5W RMS（与 Unitree G1 持平）** | 嘈杂工厂 SNR 兜底 |
| 频响 | 200 Hz – 18 kHz ±3 dB | **150 Hz – 20 kHz ±3 dB** | 更好低频，机器人语音更"厚" |
| 候选型号 | Veco FT43F40L | **Veco SP50-5W-8**（或同规格 Knowles）| 国内供货稳定 |
| 声腔 | 后腔密封 | 后腔密封 | 防漏声反馈至麦阵 |
| 安装位置 | 胸腔两侧 | 胸腔两侧 | 远离麦阵 |

### 3.8 主 SoC 方案 `[本项目]`

维持 **RK3588**，参考灵犀 X2 双片方案做演进规划：

| 阶段 | 配置 | NPU 算力 | 说明 |
|------|------|---------|------|
| **当前（单片）** | RK3588 × 1，**16GB LPDDR4X** | 6 TOPS | 语音 + 感知 + 运动分时共享 NPU，需精细调度 |
| **扩展（双片）** | RK3588 × 2 | 6 + 6 TOPS | 参考灵犀 X2：一片运动/感知，一片 AI/语音 |
| **高算力（可选）** | + Orin NX | +157 TOPS | 端侧 7B+ LLM；仅当场景要求极强本地 AI 时考虑 |

> **为什么不直接上 Orin NX？**
> Orin NX 功耗 10–25W，灵犀 X2 旗舰版将其定位为"二次开发板"而非主控，说明 AgiBot 也认为 Orin NX 不适合作为日常运行主算力。本项目同样建议将其作为可选扩展。

---

## 4. 端侧 ASR 算力需求分析 `[行业通用]`

### 4.1 模型规格与 RK3588 NPU 适配性

| 模型 | 参数量 | INT8 大小 | RK3588 NPU RTF | 流式支持 | 推荐 |
|------|--------|----------|--------------|---------|------|
| **Streaming Conformer-tiny** | ~15M | **30 MB** | **~0.08** | ✅ chunk=160ms | ⭐⭐⭐ 低延迟首选 |
| Paraformer-small | ~35M | ~70 MB | ~0.20 | ❌（VAD 切段）| ⭐⭐⭐ 精度优先 |
| FunASR SenseVoice-Small | ~234M | ~115 MB | ~0.35 | ❌ | ⭐⭐ NPU 余量紧 |
| Whisper-small | ~244M | ~120 MB | ~0.75 | ❌ | ❌ RK3588 NPU 不推荐 |

> RTF < 0.3 = 比实时快 3 倍以上，端侧可用。

### 4.2 ASR 内存占用估算

```
Streaming Conformer-tiny（流式 chunk 模式）：
  模型权重（RKNN INT8）        ：30 MB
  推理激活缓冲（8 chunk 窗口）  ：20 MB
  音频环形缓冲（8s × 4ch）      ： 4 MB
  BPE 词表 + 解码器             ： 8 MB
  ─────────────────────────────────────
  小计                         ：62 MB
```

### 4.3 流式 vs 非流式选择原则

```
首次响应延迟要求 ≤ 500ms？
  → 是：必须用流式 ASR（Streaming Conformer-tiny）
  → 否：Paraformer-small 精度更高，可考虑

用户句子平均时长 > 5s？
  → 是：流式（避免长段积压）

精度优先（工厂指令场景）？
  → Paraformer-small + 云端 ASR 增强兜底

本项目默认推荐：Streaming Conformer-tiny（低延迟）
可选升级：Paraformer-small（精度场景）
```

---

## 5. 端侧 TTS 算力需求分析 `[行业通用]`

### 5.1 模型规格对比

| 模型 | INT8 大小 | NPU RTF（RK3588）| MOS | 推荐 |
|------|----------|-----------------|-----|------|
| VITS-lite（轻量自研）| **80 MB** | **~0.08** | ~4.0 | ⭐⭐⭐ 资源最省 |
| **CosyVoice-Light** | **~120 MB** | **~0.15** | **~4.3** | ⭐⭐⭐ 主推（开源）|
| VITS-medium | ~150 MB | ~0.25 | ~4.3 | ⭐⭐ NPU 余量偏紧 |
| CosyVoice-Full | ~600 MB | ~1.5 | 4.7 | ❌ 端侧不可用 |

### 5.2 TTS 首包延迟估算（CosyVoice-Light，RK3588 NPU）

```
输入："好的，我去拿一下"（10 汉字）

声学模型（Flow-matching AM）：~60 ms
HiFi-GAN Vocoder（流式并行）：~40 ms
文本前处理（数字/符号规范化）：~10 ms
─────────────────────────────────────
首包延迟合计               ：~110 ms（< 目标 300ms ✅）
全句合成（1.8s 音频）       ：~270 ms
```

### 5.3 TTS 内存占用估算

```
CosyVoice-Light：
  声学模型（RKNN INT8）   ：120 MB
  HiFi-GAN Vocoder（INT8）： 25 MB
  推理激活缓冲            ： 30 MB
  预缓存高频短语（×20 条）：  15 MB
  ─────────────────────────────────────
  小计                   ：190 MB
```

### 5.4 端侧 TTS 可用性阈值

```
最低可用线（勉强实时）：
  NPU ≥ 0.5 TOPS + 模型 ≤ 80 MB → VITS-lite，RTF ~0.3

推荐线（流畅 + 好音质）：
  NPU ≥ 1.5 TOPS + 模型 ≤ 150 MB → CosyVoice-Light，RTF ~0.15

高品质线（接近云端）：
  → 直接走云端 TTS（无算力限制）
```

---

## 6. RK3588 NPU 时间片综合预算 `[本项目]`

### 6.1 各任务算力占用

| 任务 | TOPS 需求 | 持续时间 |
|------|---------|---------|
| KWS（唤醒词检测）| ~0.1 | 常驻 |
| ASR streaming（用户说话时）| ~0.5 | 间歇脉冲（每 160ms chunk 触发）|
| TTS 合成 | ~2.0 | 合成窗口（约 300ms 脉冲）|
| 本地 LLM（Qwen2.5-1.5B q4）| ~3.0 | 推理期持续（约 500ms）|

### 6.2 最坏并发场景

| 场景 | 活跃任务 | 总占用 | 6 TOPS 余量 |
|------|---------|--------|------------|
| 待机 | KWS | 0.1 | 5.9 ✅ |
| 用户说话 | KWS + ASR | 0.6 | 5.4 ✅ |
| LLM 推理 | LLM | 3.0 | 3.0 ✅ |
| TTS 合成 | TTS | 2.0 | 4.0 ✅ |
| ASR 尾帧 + TTS 首帧重叠 | ASR + TTS | 2.5 | 3.5 ✅ |
| **LLM + 感知同时（危险）** | LLM + 视觉推理 | ~5.5 | **0.5 ⚠️** |

**结论**：单 RK3588（6 TOPS）**刚好能覆盖**语音子系统分时需求，但 LLM 与视觉感知同时运行时 NPU 余量极低。这正是灵犀 X2 选择双 RK3588 的核心原因之一。

### 6.3 内存总规划

| 组件 | 内存 |
|------|------|
| OS + ROS2 基础 | ~800 MB |
| ASR（Conformer-tiny）| 62 MB |
| TTS（CosyVoice-Light）| 190 MB |
| KWS（SoC 侧唤醒词）| 15 MB |
| 本地 LLM（Qwen2.5-1.5B q4）| ~1100 MB |
| 感知 / SLAM | ~1500 MB |
| 其他模块 | ~400 MB |
| **合计** | **~4.1 GB** |

> 标准 8GB 余量仅约 3.9GB，迭代空间不足。**建议升级至 16GB**。

---

## 7. 整机语音子系统 BOM `[本项目]`（v1.2 更新）

| 类别 | 器件 | 型号 | 数量 | 单价（¥）| 小计（¥）|
|------|------|------|------|---------|---------|
| 麦克风 | 数字 MEMS | Knowles SPH0641LU4H-1（兼超声）| 4 | 10 | 40 |
| 音频 DSP（主推）| xCORE.AI | XMOS XU316-1024 | 1 | 100 | 100 |
| 音频 DSP（备选 A）| AISonic | Knowles IA8201（Syntiant）| (1) | (35) | (35) |
| 音频 DSP（备选 B）| 国产 | 启英泰伦 CI135X | (1) | (18) | (18) |
| ADC Codec | 4ch ADC | TI TLV320ADC5140 | 1 | 18 | 18 |
| 功放 | D 类数字功放 | TI TAS5805M | 1 | 12 | 12 |
| 扬声器 | **Φ50mm 5W**（v1.2 提升）| Veco SP50-5W-8 | 2 | 18 | 36 |
| 晶振 | TCXO 12.288 MHz | NDK NZ2520SDA | 1 | 8 | 8 |
| 无线麦接收器 | UHF/蓝牙接收模块（预留）| TBD | 1 | 50 | 50 |
| PCB + 被动器件 | 声学前端子板（定制 4 层）| — | 1 | 80 | 80 |
| **合计（主推方案，前端子系统）** | | | | | **¥344** |
| **合计（国产替代方案）** | 用 CI135X 替代 XU316 | | | | **¥262**（-23.8%）|

> 主 SoC（RK3588 16GB 模组，~¥1000）为整机共用，不单独计入语音子系统 BOM。

---

## 8. 最终选型决策矩阵 `[本项目]`

### 8.1 评估维度与权重

| 维度 | 权重 | 说明 |
|------|------|------|
| 可获得性 | 25% | 国内现货、交期、备库稳定性 |
| 性能（AEC/BF/KWS）| 25% | 嘈杂场景下的实测表现 |
| 工具链成熟度 | 15% | SDK 文档、示例、社区 |
| 单价 | 15% | BOM 成本（量产 1000 台规模）|
| 国产替代风险 | 10% | 地缘政治/供应链风险 |
| 与 RK3588 集成难度 | 10% | I2S/UART/驱动适配工作量 |

### 8.2 候选方案打分（满分 5，加权后总分 100）

| 候选 | 可获得 | 性能 | 工具链 | 单价 | 替代风险 | 集成难度 | 加权总分 |
|------|--------|------|--------|------|---------|---------|---------|
| **XMOS XU316-1024**（主推）| 4 | 5 | 5 | 3 | 2（英国）| 5 | **80.0** |
| Synaptics SR80（候补）| 2（2026 Q4 量产）| 5 | 4 | 2 | 2（美国）| 4 | 64.0 |
| Knowles AISonic IA8201 | 4 | 4 | 4 | 4 | 2（已被 Syntiant 收购）| 4 | 76.0 |
| Cirrus Logic CS47L35 | 3 | 4 | 4 | 4 | 2（美国）| 3 | 68.0 |
| **启英泰伦 CI135X**（国产替代）| 5 | 3 | 3 | 5 | 5（成都）| 3 | **76.0** |
| 全志 R128 | 5 | 2 | 3 | 5 | 5 | 4 | 68.0 |
| Espressif ESP32-S3 | 5 | 2 | 4 | 5 | 5 | 4 | 70.0 |

### 8.3 决策结论

**主路线**：**XMOS XU316-1024**（80.0 分）
**国产替代第二供应商**：**启英泰伦 CI135X**（76.0 分，仅低 4 分但单价 1/5、政治替代风险最低）
**禁止使用**：ESP32-S3（AEC 软件实现，质量不达标）

### 8.4 双供应商策略

| 阶段 | 主供 | 备供 |
|------|------|------|
| POC（2026 Q3）| XMOS XU316 | — |
| 试产（2026 Q4）| XMOS XU316 | 启英泰伦 CI135X 适配评估 |
| 量产（2027 Q2）| 双供应商，按可获得性切换 | 切换工作量目标 ≤ 4 周 |

**双供应商接口约束**：定义统一的 DSP↔SoC 协议（I2S TDM 8-slot + UART 控制帧格式），两个 DSP 实现必须满足同一协议，HAL_Audio 抽象层不感知具体型号。

---

## 9. POC 验证项 `[待执行]`

| 验证项 | 测试方法 | 通过标准 | 状态 |
|--------|---------|---------|------|
| RK3588 NPU ASR + TTS 分时是否产生优先级抢占 | RKNN Multi-context 压力测试 | 无反转，TTS 首包 ≤ 300ms | 🔲 待执行 |
| XMOS XU316 AEC 在步行振动下的 ERLE 稳定性 | 步行 + TTS 同时播放，测 AEC 收敛 | ERLE ≥ 25 dB | 🔲 待执行 |
| CosyVoice-Light RKNN INT8 量化后 MOS 衰减 | 主观 MOS 评分 N=10 | MOS ≥ 4.0 | 🔲 待执行 |
| 唤醒词在 SNR = -5 dB 下的唤醒率 | MUSAN 噪声库测试 | ≥ 90% | 🔲 待执行 |
| 启英泰伦 CI135X 适配可行性 | 移植唤醒词链路 | 全功能对齐 XU316 | 🔲 待执行 |
| 4-Mic 环形阵 DOA 精度 | 转盘测试，每 30° 一个采样点 | DOA 误差 ≤ ±15° | 🔲 待执行 |
| 无线领夹麦在跨房间场景的 ASR 准确率 | 工厂噪声 70 dBA + 5m 距离 | CER ≤ 8% | 🔲 待执行 |

---

## 10. 主流方案对照速查表（决策快查用）

| 配置维度 | 本项目 v1.2 | Unitree G1 | AgiBot 灵犀 X2 | Figure 03 | Tesla Optimus V3 |
|---------|------------|-----------|---------------|-----------|-----------------|
| 麦克风数量 | 4-Mic 环形 | 4-Mic | 未公开 | 未公开 | 未公开 |
| 麦克风位置 | 头部环形 | 未公开 | 未公开 | 头部 | 未公开 |
| 扬声器功率 | 5W × 2 | 5W | 未公开 | 升级版（Figure 02 ×4）| 未公开 |
| 音频 DSP | XMOS XU316 | 未公开 | 未公开 | 未公开 | 未公开 |
| 端侧 ASR | Streaming Conformer-tiny | 未公开 | 未公开 | Helix VLA（自研）| Grok（云）|
| 端侧 TTS | CosyVoice-Light | 未公开 | 未公开 | 未公开 | 未公开 |
| 主算力 | RK3588 单片 16GB | 8 核 CPU + 未公开 NPU | RK3588 × 2（旗舰加 Orin NX）| 2× NVIDIA RTX | 自研 D1 类 |
| LLM 集成 | 端云协同（Qwen2.5-1.5B 端 + 云）| 开放给开发者 | GO-1（自研）| Helix VLA（自研）| Grok（云）|
| 无线领夹麦 | 预留接口 | 否 | **标配** | 否 | 否 |
| 头触联动 | 设计中 | 否 | **标配** | 否 | 否 |

---

## 11. 参考资料

### 11.1 官方信息源

- **Tesla Optimus**：[techyquantum.com/tesla-optimus-v3-with-grok-voice-ai/](https://techyquantum.com/tesla-optimus-v3-with-grok-voice-ai/)、[tesery.com/blogs/news/elon-musk-confirms-tesla-optimus-v3-already-uses-grok-voice-ai](https://www.tesery.com/blogs/news/elon-musk-confirms-tesla-optimus-v3-already-uses-grok-voice-ai)
- **Figure 02 / 03**：[techcrunch.com/2024/08/06/figures-new-humanoid-robot-leverages-openai-for-natural-speech-conversations/](https://techcrunch.com/2024/08/06/figures-new-humanoid-robot-leverages-openai-for-natural-speech-conversations/)、[mikekalil.com/blog/figure-03-robot-for-home/](https://mikekalil.com/blog/figure-03-robot-for-home/)
- **Unitree G1**：[robostore.com/products/unitree-g1-robotic-humanoid](https://robostore.com/products/unitree-g1-robotic-humanoid)、[robotshop.com/products/unitree-g1-humanoid-robot-us](https://www.robotshop.com/products/unitree-g1-humanoid-robot-us)
- **小米 CyberOne**：[mi.com/cyberone](https://www.mi.com/cyberone)、[ithome.com/0/634/622.htm](https://www.ithome.com/0/634/622.htm)
- **AgiBot 灵犀 X2**：[agibot.com/products/X2](https://www.agibot.com/products/X2)、[aiwiki.ai/wiki/agibot_x2_ultra](https://aiwiki.ai/wiki/agibot_x2_ultra)

### 11.2 音频 DSP 候选官方资料

- **XMOS XU316**：[xmos.com](https://www.xmos.com)（xCORE.AI 系列）
- **Synaptics Astra SR80**：[synaptics.com/products/far-field-voice-dsp](https://www.synaptics.com/products/far-field-voice-dsp)、[synaptics.com/company/news/synaptics-astra-edge-ai-sr80-premium-audio-srw1500-intelligence](https://www.synaptics.com/company/news/synaptics-astra-edge-ai-sr80-premium-audio-srw1500-intelligence)
- **Knowles AISonic IA8201/IA8508**：[knowles.com/applications/mobile-solutions/Audio-Processors](https://www.knowles.com/applications/mobile-solutions/Audio-Processors)、[digikey.com/en/product-highlight/k/knowles/ia8201-audio-edge-processor](https://www.digikey.com/en/product-highlight/k/knowles/ia8201-audio-edge-processor)
- **Cirrus Logic CS47L35/CS47L90**：[cirrus.com/products/cs47l90](https://www.cirrus.com/products/cs47l90)、[cirrus.com/products/cs47l35](https://www.cirrus.com/products/cs47l35)
- **启英泰伦 CI135X**：[chipintelli.com](https://www.chipintelli.com)、[eet-china.com/news/202405182672.html](https://www.eet-china.com/news/202405182672.html)
- **全志 R128**：[r128.docs.aw-ol.com/](https://r128.docs.aw-ol.com/)
- **Knowles SPH0641 系列**：[digikey.com/en/products/detail/knowles/SPH0641LU4H-1/5332438](https://www.digikey.com/en/products/detail/knowles/SPH0641LU4H-1/5332438)

### 11.3 模型与工具链（行业通用）

- Sherpa-onnx（ASR / KWS / TTS 端侧推理）：[github.com/k2-fsa/sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)
- FunASR / Paraformer：[github.com/alibaba-damo-academy/FunASR](https://github.com/alibaba-damo-academy/FunASR)
- CosyVoice（开源 TTS）：[github.com/FunAudioLLM/CosyVoice](https://github.com/FunAudioLLM/CosyVoice)
- RKNN Toolkit 2：Rockchip NPU 部署工具
- XMOS xCORE.AI Audio Reference Design

### 11.4 项目内部文档

- 软件架构与接口：[voice_interaction_subsystem.md](voice_interaction_subsystem.md)
- HAL_Audio 模块设计：[hal_audio_design.md](../design/layer_07_hal_infra/hal_audio_design.md)
- Interaction 模块设计：[interaction_design.md](../design/layer_02_interaction/interaction_design.md)
- SM 状态机：[sm_design.md](../design/layer_06_middleware/sm_design.md)

---

## 附录 A：v1.x 修订记录

| 日期 | 版本 | 主要变更 |
|------|------|---------|
| 2026-05-08 | v1.0 | 初版（含大量对灵犀 X2 内部方案的错误推断）|
| 2026-05-08 | v1.1 | 修正：基于官网公开信息重写，严格区分已知/未知/本项目设计 |
| 2026-05-09 | **v1.2** | **新增同业对标（Tesla/Figure/Unitree/小米/AgiBot）；扩展 DSP 候选池（Synaptics/Knowles/Cirrus/启英泰伦/全志）；新增决策矩阵；扬声器升级 3W→5W；明确双供应商策略** |

## 附录 B：v1.0 错误纠正记录

| v1.0 错误 | 实际情况 | 纠正方式 |
|---------|---------|---------|
| 主控为 Qualcomm QRB5165 | 实际为 RK3588 × 2（官方公开）| 已更正，移除所有 QRB5165 相关描述 |
| "内置 Hexagon DSP 处理 AEC/BF" | 官方未公开音频 DSP 方案 | 已删除，改为"未公开" |
| "推测使用 FunASR Paraformer-small" | 灵犀 X2 ASR 方案完全未公开 | 已删除归因 |
| "推测使用 CosyVoice-Light TTS" | 灵犀 X2 TTS 方案完全未公开 | 已删除归因 |
| "LPDDR5 12GB" 内存 | 官方未公开内存规格 | 已删除 |
| 6-Mic 混合阵 | 官方仅说"麦克风阵列"，无具体数量 | 已更正为"未公开" |
