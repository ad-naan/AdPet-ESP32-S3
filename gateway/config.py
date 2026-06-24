"""Gateway 配置管理

从环境变量或 .env 文件加载所有配置项。
"""

from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    """Gateway 全局配置。"""

    # ---- MIMO API ----
    MIMO_API_KEY: str
    MIMO_BASE_URL: str = "https://token-plan-cn.xiaomimimo.com/v1"

    # ---- 网关认证 ----
    GATEWAY_API_KEY: str = ""

    # ---- 模型 ----
    ASR_MODEL: str = "mimo-v2.5-asr"
    LLM_MODEL: str = "mimo-v2.5"
    TTS_MODEL: str = "mimo-v2.5-tts"
    TTS_VOICE: str = "Chloe"

    # ---- 对话记忆 ----
    MAX_HISTORY_TURNS: int = 8

    # ---- 音频限制 ----
    MAX_WAV_SIZE: int = 225280  # ~220 KB

    # ---- 系统提示 ----
    SERVER_SYSTEM_PROMPT: str = (
        "你是 AdPet，一只可爱的桌面电子宠物。"
        "用简短、温暖、可爱的语气回复。回复控制在两句话以内。"
    )

    # ---- 服务器 ----
    HOST: str = "0.0.0.0"
    PORT: int = 8788

    model_config = {
        "env_file": ".env",
        "env_file_encoding": "utf-8",
    }


settings = Settings()
