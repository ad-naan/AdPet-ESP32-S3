import base64
import asyncio
import os
import json
import time
import urllib.error
import urllib.request
from collections.abc import AsyncIterator
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
        self.client = genai.Client(
            api_key=os.environ["GEMINI_API_KEY"],
            http_options={"api_version": os.environ.get("GEMINI_API_VERSION", "v1beta")},
        )
        self.model = os.environ.get("GEMINI_LIVE_MODEL", "gemini-2.5-flash-native-audio-latest")

    async def voice_chat(self, *, device_id: str, system_prompt: str, wav: bytes, history: list[dict]) -> GatewayResult:
        try:
            pcm, sample_rate = wav_to_pcm16(wav)
            voice_name = os.environ.get("GEMINI_LIVE_VOICE", "Aoede")
            config = self.types.LiveConnectConfig(
                response_modalities=["AUDIO"],
                temperature=0.8,
                thinking_config=self.types.ThinkingConfig(include_thoughts=False),
                speech_config=self.types.SpeechConfig(
                    voice_config=self.types.VoiceConfig(
                        prebuilt_voice_config=self.types.PrebuiltVoiceConfig(
                            voice_name=voice_name,
                        ),
                    ),
                ),
                system_instruction=self.types.Content(
                    parts=[self.types.Part(text=system_prompt)]
                ),
            )

            audio_chunks: list[bytes] = []
            transcript = ""
            reply_text = ""

            async with self.client.aio.live.connect(model=self.model, config=config) as session:
                await self._send_pcm_realtime(session, pcm, sample_rate)

                deadline = time.monotonic() + float(os.environ.get("GEMINI_LIVE_TIMEOUT_SEC", "18"))
                while time.monotonic() < deadline:
                    try:
                        response = await asyncio.wait_for(session._receive(), timeout=1.0)
                    except TimeoutError:
                        continue

                    if getattr(response, "data", None):
                        audio_chunks.append(response.data)
                    if getattr(response, "text", None):
                        reply_text += response.text
                    server_content = getattr(response, "server_content", None)
                    if server_content and getattr(server_content, "model_turn", None):
                        for part in server_content.model_turn.parts:
                            inline_data = getattr(part, "inline_data", None)
                            if inline_data and getattr(inline_data, "data", None):
                                audio_chunks.append(inline_data.data)
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
        except Exception as exc:
            return GatewayResult(
                transcript="",
                reply=f"Gemini Live error: {exc}",
                emotion="confused",
                wav=make_tone_wav(330, 0.12) + make_tone_wav(220, 0.18),
            )

    async def _send_pcm_realtime(self, session, pcm: bytes, sample_rate: int) -> None:
        chunk_size = int(os.environ.get("GEMINI_LIVE_CHUNK_BYTES", "1024"))
        bytes_per_second = sample_rate * 2
        mime_type = f"audio/pcm;rate={sample_rate}"

        for i in range(0, len(pcm), chunk_size):
            chunk = pcm[i:i + chunk_size]
            if hasattr(session, "send_realtime_input"):
                await session.send_realtime_input(
                    audio=self.types.Blob(data=chunk, mime_type=mime_type)
                )
            else:
                await session.send(input={"data": chunk, "mimeType": mime_type})
            await asyncio.sleep(len(chunk) / bytes_per_second)

        silence_ms = int(os.environ.get("GEMINI_LIVE_END_SILENCE_MS", "900"))
        silence = b"\x00\x00" * int(sample_rate * silence_ms / 1000)
        for i in range(0, len(silence), chunk_size):
            chunk = silence[i:i + chunk_size]
            if hasattr(session, "send_realtime_input"):
                await session.send_realtime_input(
                    audio=self.types.Blob(data=chunk, mime_type=mime_type)
                )
            else:
                await session.send(input={"data": chunk, "mimeType": mime_type})
            await asyncio.sleep(len(chunk) / bytes_per_second)

        if hasattr(session, "send_realtime_input"):
            await session.send_realtime_input(audio_stream_end=True)

    async def text_chat(self, *, device_id: str, system_prompt: str, text: str, history: list[dict]) -> str:
        try:
            response = self.client.models.generate_content(
                model=os.environ.get("GEMINI_TEXT_MODEL", "gemini-2.5-flash"),
                contents=f"{system_prompt}\n\nUser: {text}\nReply briefly as AdPet.",
            )
            return getattr(response, "text", "") or "I heard you."
        except Exception as exc:
            return f"Gemini text error: {exc}"


class GeminiRestProvider(BaseProvider):
    def __init__(self) -> None:
        self.api_key = os.environ["GEMINI_API_KEY"]
        self.text_model = os.environ.get("GEMINI_TEXT_MODEL", "gemini-2.5-flash")
        self.tts_model = os.environ.get("GEMINI_TTS_MODEL", "gemini-3.1-flash-tts-preview")
        self.tts_voice = os.environ.get("GEMINI_TTS_VOICE", "Kore")

    async def voice_chat(self, *, device_id: str, system_prompt: str, wav: bytes, history: list[dict]) -> GatewayResult:
        try:
            parsed = await asyncio.to_thread(self._audio_to_reply, system_prompt, wav, history)
            reply = parsed.get("reply") or "我听到了。"
            tts_wav = await asyncio.to_thread(self._tts_wav, reply)
            return GatewayResult(
                transcript=parsed.get("transcript", ""),
                reply=reply,
                emotion=parsed.get("emotion", "talking"),
                wav=tts_wav,
            )
        except Exception as exc:
            return GatewayResult(
                transcript="",
                reply=f"Gemini REST error: {exc}",
                emotion="confused",
                wav=make_tone_wav(330, 0.12) + make_tone_wav(220, 0.18),
            )

    async def text_chat(self, *, device_id: str, system_prompt: str, text: str, history: list[dict]) -> str:
        try:
            prompt = f"{system_prompt}\n\nUser: {text}\nReply briefly in Chinese as AdPet."
            body = {
                "contents": [{"parts": [{"text": prompt}]}],
                "generationConfig": {"thinkingConfig": {"thinkingBudget": 0}},
            }
            response = self._post_json(
                f"https://generativelanguage.googleapis.com/v1beta/models/{self.text_model}:generateContent?key={self.api_key}",
                body,
            )
            return self._extract_text(response) or "我听到了。"
        except Exception as exc:
            return f"Gemini REST text error: {exc}"

    def _audio_to_reply(self, system_prompt: str, wav: bytes, history: list[dict]) -> dict:
        history_text = "\n".join(f"{item.get('role')}: {item.get('content')}" for item in history[-8:])
        prompt = (
            f"{system_prompt}\n\n"
            f"Recent history:\n{history_text}\n\n"
            "你会收到一段用户语音。请识别用户说了什么，并以小桌宠身份用中文简短回答。"
            "只输出 JSON，字段为 transcript、reply、emotion。"
            "emotion 只能选 idle、happy、sad、angry、sleepy、confused、talking。"
        )
        body = {
            "contents": [{
                "parts": [
                    {"text": prompt},
                    {
                        "inline_data": {
                            "mime_type": "audio/wav",
                            "data": base64.b64encode(wav).decode("utf-8"),
                        }
                    },
                ]
            }],
            "generationConfig": {
                "responseMimeType": "application/json",
                "thinkingConfig": {"thinkingBudget": 0},
            },
        }
        response = self._post_json(
            f"https://generativelanguage.googleapis.com/v1beta/models/{self.text_model}:generateContent?key={self.api_key}",
            body,
        )
        text = self._extract_text(response)
        try:
            parsed = json.loads(text)
        except json.JSONDecodeError:
            parsed = {"transcript": "", "reply": text, "emotion": "talking"}
        return parsed if isinstance(parsed, dict) else {"transcript": "", "reply": str(parsed), "emotion": "talking"}

    def _tts_wav(self, text: str) -> bytes:
        body = {
            "model": self.tts_model,
            "input": text[:300],
            "response_format": {"type": "audio"},
            "generation_config": {
                "speech_config": [{"voice": self.tts_voice}]
            },
        }
        response = self._post_json(
            "https://generativelanguage.googleapis.com/v1beta/interactions",
            body,
            headers={"x-goog-api-key": self.api_key},
        )
        for step in response.get("steps", []):
            for content in step.get("content", []):
                if content.get("mime_type") == "audio/l16" and content.get("data"):
                    pcm = base64.b64decode(content["data"])
                    return pcm16_to_wav(pcm, sample_rate=24000)
        raise ValueError("Gemini TTS did not return audio/l16 data")

    def _post_json(self, url: str, body: dict, headers: dict | None = None) -> dict:
        request_headers = {"Content-Type": "application/json"}
        if headers:
            request_headers.update(headers)
        req = urllib.request.Request(
            url,
            data=json.dumps(body).encode("utf-8"),
            headers=request_headers,
        )
        try:
            with urllib.request.urlopen(req, timeout=45) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"HTTP {exc.code}: {detail[:500]}") from exc

    def _extract_text(self, response: dict) -> str:
        candidates = response.get("candidates") or []
        if not candidates:
            return ""
        parts = candidates[0].get("content", {}).get("parts", [])
        return "".join(part.get("text", "") for part in parts)


def create_provider() -> BaseProvider:
    provider = os.environ.get("ADPET_PROVIDER", "mock").lower()
    if provider == "mimo":
        return MimoProvider()
    if provider in ("gemini_rest", "gemini_http"):
        return GeminiRestProvider()
    if provider in ("gemini", "gemini_live"):
        return GeminiLiveProvider()
    return MockProvider()
