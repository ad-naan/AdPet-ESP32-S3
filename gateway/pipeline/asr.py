"""语音识别 (ASR) 管道。

将 ESP32 录制的 WAV 音频转为文字。
"""

from __future__ import annotations

import base64
import logging

from openai import OpenAI

from config import settings

logger = logging.getLogger("adpet.asr")

_client: OpenAI | None = None


def _get_client() -> OpenAI:
    global _client
    if _client is None:
        _client = OpenAI(
            api_key=settings.MIMO_API_KEY,
            base_url=settings.MIMO_BASE_URL,
        )
    return _client


async def transcribe(wav_bytes: bytes) -> str:
    """将 WAV 音频转写为文本。

    Args:
        wav_bytes: 16 kHz mono PCM WAV 原始字节。

    Returns:
        识别出的文本。
    """
    audio_b64 = base64.b64encode(wav_bytes).decode("utf-8")

    logger.info("ASR 请求: 音频大小 %d 字节", len(wav_bytes))

    completion = _get_client().chat.completions.create(
        model=settings.ASR_MODEL,
        messages=[
            {
                "role": "user",
                "content": [
                    {
                        "type": "input_audio",
                        "input_audio": {
                            "data": f"data:audio/wav;base64,{audio_b64}"
                        },
                    }
                ],
            }
        ],
        extra_body={"asr_options": {"language": "auto"}},
    )

    transcript = completion.choices[0].message.content or ""
    logger.info("ASR 结果: %s", transcript)
    return transcript
