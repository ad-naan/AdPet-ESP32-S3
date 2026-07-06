import base64
import os
from dataclasses import dataclass

from openai import OpenAI

from audio_utils import make_tone_wav, pcm16_to_wav, wav_to_pcm16


@dataclass
class GatewayResult:
    transcript: str
    reply: str
    emotion: str
    wav: bytes


class BaseProvider:
    async def voice_chat(self, *, device_id: str, system_prompt: str, wav: bytes, history: list[dict]) -> GatewayResult:
        raise NotImplementedError

    async def text_chat(self, *, device_id: str, system_prompt: str, text: str, history: list[dict]) -> str:
        raise NotImplementedError


class MockProvider(BaseProvider):
    async def voice_chat(self, *, device_id: str, system_prompt: str, wav: bytes, history: list[dict]) -> GatewayResult:
        reply = "Gateway is working. I heard you."
        return GatewayResult(
            transcript="mock transcript",
            reply=reply,
            emotion="talking",
            wav=make_tone_wav(988, 0.18) + make_tone_wav(1175, 0.18),
        )

    async def text_chat(self, *, device_id: str, system_prompt: str, text: str, history: list[dict]) -> str:
        return f"Mock reply: {text[:80]}"


class MimoProvider(BaseProvider):
    def __init__(self) -> None:
        self.client = OpenAI(
            api_key=os.environ["MIMO_API_KEY"],
            base_url=os.environ.get("MIMO_BASE_URL", "https://api.xiaomimimo.com/v1"),
        )
        self.asr_model = os.environ.get("MIMO_ASR_MODEL", "mimo-v2.5-asr")
        self.llm_model = os.environ.get("MIMO_LLM_MODEL", "mimo-v2.5")
        self.tts_model = os.environ.get("MIMO_TTS_MODEL", "mimo-v2.5-tts")
        self.tts_voice = os.environ.get("MIMO_TTS_VOICE", "Chloe")

    async def voice_chat(self, *, device_id: str, system_prompt: str, wav: bytes, history: list[dict]) -> GatewayResult:
        audio_base64 = base64.b64encode(wav).decode("utf-8")
        asr = self.client.chat.completions.create(
            model=self.asr_model,
            messages=[{
                "role": "user",
                "content": [{
                    "type": "input_audio",
                    "input_audio": {"data": f"data:audio/wav;base64,{audio_base64}"},
                }],
            }],
            extra_body={"asr_options": {"language": "auto"}},
        )
        transcript = asr.choices[0].message.content or ""
        reply = await self.text_chat(device_id=device_id, system_prompt=system_prompt, text=transcript, history=history)

        tts = self.client.chat.completions.create(
            model=self.tts_model,
            messages=[
                {"role": "user", "content": "Cute, bright, warm desktop-pet voice. Speak briefly."},
                {"role": "assistant", "content": reply},
            ],
            audio={"format": "wav", "voice": self.tts_voice},
        )
        wav_reply = base64.b64decode(tts.choices[0].message.audio.data)
        return GatewayResult(transcript=transcript, reply=reply, emotion="talking", wav=wav_reply)

    async def text_chat(self, *, device_id: str, system_prompt: str, text: str, history: list[dict]) -> str:
        messages = [
            {"role": "system", "content": system_prompt},
            *history[-8:],
            {"role": "user", "content": text},
        ]
        llm = self.client.chat.completions.create(
            model=self.llm_model,
            messages=messages,
            max_tokens=80,
            temperature=0.8,
        )
        return llm.choices[0].message.content or "I heard you."


class GeminiLiveProvider(BaseProvider):
    def __init__(self) -> None:
        from google import genai
        from google.genai import types

        self.genai = genai
        self.types = types
        self.client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])
        self.model = os.environ.get("GEMINI_LIVE_MODEL", "gemini-2.0-flash-live-001")

    async def voice_chat(self, *, device_id: str, system_prompt: str, wav: bytes, history: list[dict]) -> GatewayResult:
        pcm, sample_rate = wav_to_pcm16(wav)
        config = self.types.LiveConnectConfig(
            response_modalities=["AUDIO"],
            system_instruction=system_prompt,
        )

        audio_chunks: list[bytes] = []
        transcript = ""
        reply_text = ""

        async with self.client.aio.live.connect(model=self.model, config=config) as session:
            await session.send_realtime_input(
                audio=self.types.Blob(data=pcm, mime_type=f"audio/pcm;rate={sample_rate}")
            )
            await session.send_client_content(turns=[], turn_complete=True)

            async for response in session.receive():
                if getattr(response, "data", None):
                    audio_chunks.append(response.data)
                server_content = getattr(response, "server_content", None)
                if server_content and getattr(server_content, "model_turn", None):
                    for part in server_content.model_turn.parts:
                        if getattr(part, "text", None):
                            reply_text += part.text
                if server_content and getattr(server_content, "turn_complete", False):
                    break

        if not audio_chunks:
            return GatewayResult(
                transcript=transcript,
                reply=reply_text or "Gemini did not return audio.",
                emotion="confused",
                wav=make_tone_wav(440, 0.22),
            )

        return GatewayResult(
            transcript=transcript,
            reply=reply_text or "",
            emotion="talking",
            wav=pcm16_to_wav(b"".join(audio_chunks), sample_rate=24000),
        )

    async def text_chat(self, *, device_id: str, system_prompt: str, text: str, history: list[dict]) -> str:
        response = self.client.models.generate_content(
            model=os.environ.get("GEMINI_TEXT_MODEL", "gemini-2.0-flash"),
            contents=f"{system_prompt}\n\nUser: {text}\nReply briefly as AdPet.",
        )
        return getattr(response, "text", "") or "I heard you."


def create_provider() -> BaseProvider:
    provider = os.environ.get("ADPET_PROVIDER", "mock").lower()
    if provider == "mimo":
        return MimoProvider()
    if provider in ("gemini", "gemini_live"):
        return GeminiLiveProvider()
    return MockProvider()

