"""文字转语音 (TTS) 管道。

将 LLM 回复文本合成为 WAV 音频返回给 ESP32。
"""

from __future__ import annotations

import base64
import logging

from openai import OpenAI

from config import settings

logger = logging.getLogger("adpet.tts")

_client: OpenAI | None = None


def _get_client() -> OpenAI:
    global _client
    if _client is None:
        _client = OpenAI(
            api_key=settings.MIMO_API_KEY,
            base_url=settings.MIMO_BASE_URL,
        )
    return _client


async def synthesize(reply_text: str) -> bytes:
    """将文本合成为 WAV 音频。

    Args:
        reply_text: 要合成的回复文本。

    Returns:
        16-bit PCM WAV 字节。

    Raises:
        ValueError: 音频大小超过限制。
    """
    logger.info("TTS 请求: %s", reply_text)

    completion = _get_client().chat.completions.create(
        model=settings.TTS_MODEL,
        messages=[
            {
                "role": "user",
                "content": "Cute, bright, warm desktop-pet voice. Speak briefly.",
            },
            {"role": "assistant", "content": reply_text},
        ],
        audio={"format": "wav", "voice": settings.TTS_VOICE},
    )

    wav_bytes = base64.b64decode(completion.choices[0].message.audio.data)
    logger.info("TTS 结果: 音频大小 %d 字节", len(wav_bytes))

    if len(wav_bytes) > settings.MAX_WAV_SIZE:
        logger.warning(
            "TTS 音频超过限制: %d > %d 字节",
            len(wav_bytes),
            settings.MAX_WAV_SIZE,
        )

    return wav_bytes
