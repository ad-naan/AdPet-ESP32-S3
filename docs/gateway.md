# AdPet Gateway Implementation Guide

The ESP32 should stay small: record audio, show OLED emotions, play returned WAV, and call one gateway endpoint.

The gateway owns:

1. STT / ASR
2. LLM chat
3. TTS
4. Long conversation memory
5. Prompt and expression strategy
6. Provider-specific API formats

## ESP32 Configuration

The built-in config page now needs only:

| Field | Example |
| --- | --- |
| Gateway Base URL | `http://192.168.1.100:8787` |
| Gateway API Key | optional shared secret |
| Device ID | `adpet-001` |
| System Prompt | persona prompt |
| Record ms | `2000` |
| Trigger RMS / Peak | microphone trigger threshold |

The ESP32 calls:

```text
POST {Gateway Base URL}/adpet/chat
POST {Gateway Base URL}/adpet/text
```

## Voice Endpoint

### Request

```http
POST /adpet/chat
Authorization: Bearer <gateway-api-key>
X-AdPet-Device-Id: adpet-001
Content-Type: multipart/form-data; boundary=...
```

Multipart fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `metadata` | JSON | Device ID and system prompt from ESP32 config. |
| `audio` | `audio/wav` file | 16 kHz mono PCM WAV recorded by ESP32. |

Example `metadata`:

```json
{
  "device_id": "adpet-001",
  "system_prompt": "You are AdPet, a cute tiny desktop pet."
}
```

### Response

Return binary WAV audio directly:

```http
HTTP/1.1 200 OK
Content-Type: audio/wav
X-AdPet-Transcript: user text
X-AdPet-Reply: assistant reply
X-AdPet-Emotion: talking

<wav bytes>
```

Rules:

1. Return 16-bit PCM WAV.
2. Keep audio short. Under `220 KB` is safest for the current ESP32 code.
3. Put transcript and reply into headers for debugging and UI display.
4. If TTS fails, you may return `204 No Content`, but the current ESP32 works best with `200 audio/wav`.

## Text Test Endpoint

Used by the config page text box.

### Request

```http
POST /adpet/text
Authorization: Bearer <gateway-api-key>
Content-Type: application/json
```

```json
{
  "device_id": "adpet-001",
  "system_prompt": "You are AdPet...",
  "text": "Hello"
}
```

### Response

```json
{
  "reply": "Hi! I am AdPet."
}
```

## Suggested Gateway Flow

```text
receive ESP32 WAV
  -> save or stream audio temporarily
  -> call OpenAI-compatible ASR
  -> append transcript to device conversation memory
  -> call OpenAI-compatible LLM
  -> choose emotion
  -> call OpenAI-compatible TTS
  -> return WAV bytes to ESP32
```

## Memory Strategy

Keep memory by `device_id`.

Recommended stored state:

```json
{
  "device_id": "adpet-001",
  "summary": "The user likes concise cute replies.",
  "recent_messages": [
    { "role": "user", "content": "..." },
    { "role": "assistant", "content": "..." }
  ]
}
```

Use:

1. System prompt from ESP32 config.
2. Server-side permanent persona prompt.
3. Conversation summary.
4. Last 4-8 turns.

Do not send unlimited history to the LLM.

## MIMO-Compatible Provider Example

ASR format from your example:

```python
completion = client.chat.completions.create(
    model="mimo-v2.5-asr",
    messages=[{
        "role": "user",
        "content": [{
            "type": "input_audio",
            "input_audio": {
                "data": "data:audio/wav;base64,..."
            }
        }]
    }],
    extra_body={
        "asr_options": {
            "language": "auto"
        }
    }
)
```

TTS format from your example:

```python
completion = client.chat.completions.create(
    model="mimo-v2.5-tts",
    messages=[
        {"role": "user", "content": "voice style"},
        {"role": "assistant", "content": "reply text"}
    ],
    audio={
        "format": "wav",
        "voice": "Chloe"
    }
)
```

Gateway decodes:

```python
audio_bytes = base64.b64decode(completion.choices[0].message.audio.data)
```

Then returns `audio_bytes` directly to ESP32.

## Minimal FastAPI Sketch

```python
import base64
import os
from fastapi import FastAPI, File, Form, Header, UploadFile
from fastapi.responses import Response, JSONResponse
from openai import OpenAI

app = FastAPI()

client = OpenAI(
    api_key=os.environ["MIMO_API_KEY"],
    base_url="https://api.xiaomimimo.com/v1",
)

memory = {}

@app.post("/adpet/chat")
async def adpet_chat(
    metadata: str = Form(...),
    audio: UploadFile = File(...),
    authorization: str | None = Header(default=None),
    x_adpet_device_id: str | None = Header(default=None),
):
    wav = await audio.read()
    audio_base64 = base64.b64encode(wav).decode("utf-8")

    asr = client.chat.completions.create(
        model="mimo-v2.5-asr",
        messages=[{
            "role": "user",
            "content": [{
                "type": "input_audio",
                "input_audio": {
                    "data": f"data:audio/wav;base64,{audio_base64}"
                }
            }]
        }],
        extra_body={"asr_options": {"language": "auto"}},
    )
    transcript = asr.choices[0].message.content or ""

    device_id = x_adpet_device_id or "adpet-001"
    history = memory.setdefault(device_id, [])
    messages = [
        {"role": "system", "content": "You are AdPet. Reply briefly and cutely."},
        *history[-8:],
        {"role": "user", "content": transcript},
    ]

    llm = client.chat.completions.create(
        model="mimo-v2.5",
        messages=messages,
        max_tokens=80,
        temperature=0.8,
    )
    reply = llm.choices[0].message.content or "嗯？我刚刚有点走神。"

    history.append({"role": "user", "content": transcript})
    history.append({"role": "assistant", "content": reply})

    tts = client.chat.completions.create(
        model="mimo-v2.5-tts",
        messages=[
            {"role": "user", "content": "Cute, bright, warm desktop-pet voice. Speak briefly."},
            {"role": "assistant", "content": reply},
        ],
        audio={"format": "wav", "voice": "Chloe"},
    )
    wav_reply = base64.b64decode(tts.choices[0].message.audio.data)

    return Response(
        content=wav_reply,
        media_type="audio/wav",
        headers={
            "X-AdPet-Transcript": transcript,
            "X-AdPet-Reply": reply,
            "X-AdPet-Emotion": "talking",
        },
    )

@app.post("/adpet/text")
async def adpet_text(payload: dict):
    text = payload.get("text", "")
    llm = client.chat.completions.create(
        model="mimo-v2.5",
        messages=[
            {"role": "system", "content": payload.get("system_prompt", "You are AdPet.")},
            {"role": "user", "content": text},
        ],
        max_tokens=80,
    )
    return JSONResponse({"reply": llm.choices[0].message.content or ""})
```

Run:

```bash
pip install fastapi uvicorn python-multipart openai
set MIMO_API_KEY=your_key
uvicorn main:app --host 0.0.0.0 --port 8787
```

Set ESP32 Gateway Base URL to:

```text
http://YOUR_PC_LAN_IP:8787
```
