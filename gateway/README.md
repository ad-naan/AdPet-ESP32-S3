# AdPet Gateway

This service keeps ESP32 memory usage small. The ESP32 uploads one short WAV file and receives one WAV reply. The gateway handles provider-specific STT/LLM/TTS or Gemini Live logic.

## Install

```bash
cd gateway
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

## Run Mock Mode First

Mock mode does not call any AI provider. Use it to test ESP32 networking and speaker playback.

```bash
set ADPET_PROVIDER=mock
uvicorn main:app --host 0.0.0.0 --port 8787
```

Set the ESP32 config page Gateway Base URL to:

```text
http://YOUR_PC_LAN_IP:8787
```

## Run MIMO Mode

```bash
set ADPET_PROVIDER=mimo
set MIMO_API_KEY=your_key
uvicorn main:app --host 0.0.0.0 --port 8787
```

## Run Gemini Live Mode

```bash
set ADPET_PROVIDER=gemini_live
set GEMINI_API_KEY=your_key
set GEMINI_LIVE_MODEL=gemini-2.0-flash-live-001
uvicorn main:app --host 0.0.0.0 --port 8787
```

Gemini Live SDK behavior has changed across versions. If this adapter needs adjustment, keep the ESP32-facing endpoints unchanged and edit only `providers.py`.

## ESP32 Contract

`POST /adpet/chat` accepts multipart form data:

- `metadata`: JSON string with `device_id` and `system_prompt`
- `audio`: 16 kHz mono PCM WAV from ESP32

It returns:

- body: `audio/wav`
- `X-AdPet-Transcript`: optional transcript
- `X-AdPet-Reply`: assistant text
- `X-AdPet-Emotion`: suggested expression

