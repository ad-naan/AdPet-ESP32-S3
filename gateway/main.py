import json
import os
from typing import Annotated

from fastapi import FastAPI, File, Form, Header, HTTPException, UploadFile
from fastapi.responses import JSONResponse, Response

from providers import create_provider


app = FastAPI(title="AdPet Gateway")
provider = create_provider()
memory: dict[str, list[dict]] = {}


def check_auth(authorization: str | None) -> None:
    expected = os.environ.get("ADPET_GATEWAY_KEY", "")
    if not expected:
        return
    if authorization != f"Bearer {expected}":
        raise HTTPException(status_code=401, detail="invalid gateway key")


@app.get("/health")
async def health():
    return {"ok": True, "provider": os.environ.get("ADPET_PROVIDER", "mock")}


@app.post("/adpet/chat")
async def adpet_chat(
    metadata: Annotated[str, Form()],
    audio: Annotated[UploadFile, File()],
    authorization: Annotated[str | None, Header()] = None,
    x_adpet_device_id: Annotated[str | None, Header()] = None,
):
    check_auth(authorization)
    try:
        meta = json.loads(metadata)
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=400, detail="metadata must be JSON") from exc

    device_id = x_adpet_device_id or meta.get("device_id") or "adpet-001"
    system_prompt = meta.get("system_prompt") or "You are AdPet. Reply briefly and warmly."
    wav = await audio.read()
    if len(wav) < 44:
        raise HTTPException(status_code=400, detail="audio wav is too small")

    history = memory.setdefault(device_id, [])
    result = await provider.voice_chat(
        device_id=device_id,
        system_prompt=system_prompt,
        wav=wav,
        history=history,
    )

    if result.transcript:
        history.append({"role": "user", "content": result.transcript})
    if result.reply:
        history.append({"role": "assistant", "content": result.reply})
    del history[:-8]

    return Response(
        content=result.wav,
        media_type="audio/wav",
        headers={
            "X-AdPet-Transcript": result.transcript[:240],
            "X-AdPet-Reply": result.reply[:240],
            "X-AdPet-Emotion": result.emotion,
        },
    )


@app.post("/adpet/text")
async def adpet_text(payload: dict, authorization: Annotated[str | None, Header()] = None):
    check_auth(authorization)
    device_id = payload.get("device_id") or "adpet-001"
    system_prompt = payload.get("system_prompt") or "You are AdPet."
    text = payload.get("text") or ""
    history = memory.setdefault(device_id, [])
    reply = await provider.text_chat(
        device_id=device_id,
        system_prompt=system_prompt,
        text=text,
        history=history,
    )
    history.append({"role": "user", "content": text})
    history.append({"role": "assistant", "content": reply})
    del history[:-8]
    return JSONResponse({"reply": reply})

