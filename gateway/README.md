# AdPet Gateway

为 ESP32 桌面宠物提供 STT / LLM / TTS 语音对话管道的 Web 服务。

## 功能

- 🎙️ 语音识别 (ASR) — 将 ESP32 录制的 WAV 转为文字
- 🤖 LLM 对话 — 生成可爱的桌面宠物回复
- 🔊 语音合成 (TTS) — 将回复转为 WAV 音频
- 📱 多设备支持 — 按 `device_id` 隔离对话记忆
- 💾 SQLite 持久化 — 设备、消息、配置全部持久存储
- 🎛️ Web 管理界面 — 在浏览器中管理设备和配置

## 快速开始

```bash
cd gateway

# 安装依赖
pip install -r requirements.txt

# 复制配置
copy .env.example .env
# 编辑 .env 填入 MIMO_API_KEY

# 启动
python main.py
```

浏览器打开 `http://localhost:8787` 进入管理界面。

## ESP32 配置

设备端只需配置：

| 字段 | 示例 |
| --- | --- |
| Gateway Base URL | `http://你的电脑IP:8787` |
| Device ID | `adpet-001` |

## API 端点

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| POST | `/adpet/chat` | 语音对话（multipart WAV） |
| POST | `/adpet/text` | 文本测试 |
| GET | `/adpet/health` | 健康检查 |
| GET | `/api/devices` | 设备列表 |
| GET | `/api/devices/{id}/messages` | 对话历史 |
| DELETE | `/api/devices/{id}/messages` | 清空对话 |
| DELETE | `/api/devices/{id}` | 删除设备 |
| GET | `/api/config` | 获取配置 |
| PUT | `/api/config` | 更新配置 |

## 项目结构

```
gateway/
├── main.py              # FastAPI 入口
├── config.py            # 环境变量配置
├── database.py          # SQLite 数据库
├── memory.py            # 对话记忆
├── models.py            # 数据模型
├── auth.py              # API Key 认证
├── pipeline/
│   ├── asr.py           # 语音识别
│   ├── llm.py           # LLM 对话
│   ├── tts.py           # 语音合成
│   └── emotion.py       # 情绪推断
├── static/              # Web 管理界面
├── requirements.txt
└── .env.example
```
