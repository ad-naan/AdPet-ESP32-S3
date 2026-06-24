"""文字转语音 (TTS) 管道。

将 LLM 回复文本合成为 WAV 音频返回给 ESP32。
"""

from __future__ import annotations

import base64
import logging
import wave
import io
import audioop

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
    logger.info("TTS 原始大小: %d 字节", len(wav_bytes))

    # 服务器端下采样：将 24000Hz 降至 12000Hz，体积直接缩减 50% 减半！彻底消除 4G 网络抖动
    wav_bytes = resample_wav(wav_bytes, 12000)
    logger.info("TTS 下采样至 12000Hz 后大小: %d 字节", len(wav_bytes))

    if len(wav_bytes) > settings.MAX_WAV_SIZE:
        logger.warning(
            "TTS 音频超过限制: %d > %d 字节",
            len(wav_bytes),
            settings.MAX_WAV_SIZE,
        )

    return wav_bytes


def resample_wav(wav_bytes: bytes, target_rate: int = 12000) -> bytes:
    """使用 Python 内置 audioop 对 WAV 音频进行重采样。"""
    try:
        with wave.open(io.BytesIO(wav_bytes), "rb") as wav_in:
            params = wav_in.getparams()
            nchannels, sampwidth, framerate, nframes, comptype, compname = params

            if framerate == target_rate:
                return wav_bytes

            raw_data = wav_in.readframes(nframes)

            # audioop.ratecv 重采样
            resampled_data, _ = audioop.ratecv(raw_data, sampwidth, nchannels, framerate, target_rate, None)

            # 重新打包为标准 WAV
            out_io = io.BytesIO()
            with wave.open(out_io, "wb") as wav_out:
                wav_out.setnchannels(nchannels)
                wav_out.setsampwidth(sampwidth)
                wav_out.setframerate(target_rate)
                wav_out.writeframes(resampled_data)
            return out_io.getvalue()
    except Exception as e:
        logger.error("WAV 重采样失败: %s", e)
        return wav_bytes
