import io
import math
import wave


def make_tone_wav(frequency_hz: int = 880, seconds: float = 0.25, sample_rate: int = 16000) -> bytes:
    frames = int(seconds * sample_rate)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        for i in range(frames):
            sample = int(math.sin(2 * math.pi * frequency_hz * i / sample_rate) * 9000)
            wav.writeframesraw(sample.to_bytes(2, "little", signed=True))
    return buf.getvalue()


def wav_to_pcm16(wav_bytes: bytes) -> tuple[bytes, int]:
    with wave.open(io.BytesIO(wav_bytes), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        sample_rate = wav.getframerate()
        pcm = wav.readframes(wav.getnframes())

    if channels != 1 or sample_width != 2:
        raise ValueError("Expected mono 16-bit PCM WAV")

    return pcm, sample_rate


def pcm16_to_wav(pcm: bytes, sample_rate: int = 24000) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(pcm)
    return buf.getvalue()

