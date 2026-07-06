import asyncio
import os
import sys

from google import genai
from google.genai import types

from audio_utils import wav_to_pcm16


async def chunks(data: bytes):
    for i in range(0, len(data), 1024):
        yield data[i:i + 1024]


async def main() -> None:
    wav_path = os.environ.get("ADPET_TEST_WAV", "gateway/test_speech.wav")
    with open(wav_path, "rb") as f:
        wav = f.read()
    pcm, sample_rate = wav_to_pcm16(wav)
    client = genai.Client(
        api_key=os.environ["GEMINI_API_KEY"],
        http_options={"api_version": os.environ.get("GEMINI_API_VERSION", "v1beta")},
    )
    config = types.LiveConnectConfig(
        response_modalities=["AUDIO"],
        temperature=0.8,
        thinking_config=types.ThinkingConfig(include_thoughts=False),
        speech_config=types.SpeechConfig(
            voice_config=types.VoiceConfig(
                prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name="Aoede")
            )
        ),
        system_instruction=types.Content(parts=[types.Part(text="Reply briefly in Chinese.")]),
    )
    async with client.aio.live.connect(
        model=os.environ.get("GEMINI_LIVE_MODEL", "gemini-2.5-flash-native-audio-latest"),
        config=config,
    ) as session:
        chunk_size = 1024
        bytes_per_second = sample_rate * 2
        for i in range(0, len(pcm), chunk_size):
            chunk = pcm[i:i + chunk_size]
            await session.send_realtime_input(
                audio=types.Blob(data=chunk, mime_type=f"audio/pcm;rate={sample_rate}")
            )
            await asyncio.sleep(len(chunk) / bytes_per_second)

        silence = b"\x00\x00" * int(sample_rate * 0.8)
        for i in range(0, len(silence), chunk_size):
            chunk = silence[i:i + chunk_size]
            await session.send_realtime_input(
                audio=types.Blob(data=chunk, mime_type=f"audio/pcm;rate={sample_rate}")
            )
            await asyncio.sleep(len(chunk) / bytes_per_second)
        await session.send_realtime_input(audio_stream_end=True)

        index = 0
        async for message in session.receive():
            index += 1
            print("MESSAGE", index, type(message))
            print(message)
            if index >= 5:
                break


if __name__ == "__main__":
    asyncio.run(main())
