import asyncio
import os

from audio_utils import make_tone_wav
from providers import create_provider


async def main() -> None:
    provider = create_provider()
    test_wav_path = os.environ.get("ADPET_TEST_WAV")
    if test_wav_path:
        with open(test_wav_path, "rb") as f:
            wav = f.read()
    else:
        wav = make_tone_wav(660, 0.5)

    result = await provider.voice_chat(
        device_id="test",
        system_prompt="You are a tiny cute desktop pet. Reply in short Chinese.",
        wav=wav,
        history=[],
    )
    print("TRANSCRIPT:", result.transcript)
    print("REPLY:", result.reply)
    print("EMOTION:", result.emotion)
    print("WAV_BYTES:", len(result.wav))


if __name__ == "__main__":
    asyncio.run(main())
