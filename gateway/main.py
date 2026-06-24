"""AdPet Gateway — FastAPI 主入口。

为 ESP32 设备提供 STT → LLM → TTS 语音对话管道，
支持多设备、SQLite 持久化和 Web 管理界面。
"""

from __future__ import annotations

import json
import logging
import os
import sys

# 规避 Windows 环境下 httpx 解析 IPv6 [::1/128] 代理排除规则时的 InvalidPort 崩溃 bug
os.environ["NO_PROXY"] = "127.0.0.1,localhost"

from contextlib import asynccontextmanager
from pathlib import Path
from urllib.parse import quote, unquote

from fastapi import Depends, FastAPI, File, Form, Header, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, JSONResponse, Response
from fastapi.staticfiles import StaticFiles

import database as db
import memory
from auth import verify_api_key
from config import settings
from models import ChatMetadata, HealthResponse, TextRequest
from pipeline import emotion, asr, llm, tts

# ─── 日志 ──────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    stream=sys.stdout,
)
logger = logging.getLogger("adpet.gateway")


# ─── 生命周期 ──────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    """启动时初始化数据库，关闭时断开连接。"""
    await db.get_db()
    logger.info("AdPet Gateway 启动 — 端口 %d", settings.PORT)
    yield
    await db.close_db()
    logger.info("AdPet Gateway 已停止")


# ─── 应用 ──────────────────────────────────────────────

app = FastAPI(
    title="AdPet Gateway",
    description="为 ESP32 桌面宠物提供 STT/LLM/TTS 语音对话管道",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# 异常处理
import openai
from fastapi import Request

@app.exception_handler(openai.AuthenticationError)
async def openai_auth_exception_handler(request: Request, exc: openai.AuthenticationError):
    logger.error("MIMO API Key 验证失败: %s", exc)
    return JSONResponse(
        status_code=401,
        content={"detail": "MIMO API Key is invalid or expired. Check your .env file."}
    )

@app.exception_handler(openai.APIError)
async def openai_api_exception_handler(request: Request, exc: openai.APIError):
    logger.error("MIMO API 错误: %s", exc)
    return JSONResponse(
        status_code=502,
        content={"detail": f"MIMO API error: {exc.message}"}
    )

# 静态文件
static_dir = Path(__file__).parent / "static"
if static_dir.exists():
    app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")


# ═══════════════════════════════════════════════════════
#  ESP32 设备端点
# ═══════════════════════════════════════════════════════

@app.post("/adpet/chat", dependencies=[Depends(verify_api_key)])
async def adpet_chat(
    metadata: str = Form(...),
    audio: UploadFile = File(...),
    x_adpet_device_id: str | None = Header(default=None),
):
    """语音对话端点。接收 WAV → ASR → LLM → TTS → 返回 WAV。"""
    # 解析 metadata
    try:
        meta = ChatMetadata(**json.loads(metadata))
    except Exception:
        meta = ChatMetadata()

    device_id = x_adpet_device_id or meta.device_id
    logger.info("── 语音请求 [%s] ──", device_id)

    try:
        # 1. ASR
        wav_bytes = await audio.read()
        transcript = await asr.transcribe(wav_bytes)

        if not transcript.strip():
            logger.warning("ASR 未识别到有效文本")
            return Response(status_code=204)

        # 2. 获取历史
        history = await memory.get_recent(device_id)

        # 3. LLM
        reply = await llm.chat(
            transcript=transcript,
            history=history,
            device_system_prompt=meta.system_prompt,
        )

        # 4. 记录对话
        await memory.append(device_id, "user", transcript)
        await memory.append(device_id, "assistant", reply)

        # 5. 情绪推断
        emo = emotion.infer_emotion(reply)

        # 6. TTS
        try:
            wav_reply = await tts.synthesize(reply)
        except Exception as e:
            logger.error("TTS 失败: %s", e)
            return Response(status_code=204)

        logger.info("── 完成 [%s] emotion=%s ──", device_id, emo)

        return Response(
            content=wav_reply,
            media_type="audio/wav",
            headers={
                "X-AdPet-Transcript": quote(transcript),
                "X-AdPet-Reply": quote(reply),
                "X-AdPet-Emotion": emo,
            },
        )
    except Exception as e:
        type_str = type(e).__name__
        if "AuthenticationError" in type_str or "auth" in str(e).lower():
            logger.error("MIMO API Key 验证失败: %s", e)
            return JSONResponse(
                status_code=401,
                content={"detail": "MIMO API Key is invalid or expired. Check your .env file."}
            )
        elif "APIError" in type_str:
            logger.error("MIMO API 错误: %s", e)
            return JSONResponse(
                status_code=502,
                content={"detail": f"MIMO API error: {str(e)}"}
            )
        else:
            logger.error("未预期错误: %s", e, exc_info=True)
            return JSONResponse(
                status_code=500,
                content={"detail": str(e)}
            )


@app.post("/adpet/text", dependencies=[Depends(verify_api_key)])
async def adpet_text(payload: TextRequest):
    """文本测试端点，供配置页测试对话。"""
    device_id = payload.device_id
    logger.info("── 文本请求 [%s]: %s ──", device_id, payload.text)

    try:
        history = await memory.get_recent(device_id)

        reply = await llm.chat(
            transcript=payload.text,
            history=history,
            device_system_prompt=payload.system_prompt,
        )

        await memory.append(device_id, "user", payload.text)
        await memory.append(device_id, "assistant", reply)

        return JSONResponse({"reply": reply})
    except Exception as e:
        type_str = type(e).__name__
        if "AuthenticationError" in type_str or "auth" in str(e).lower():
            logger.error("MIMO API Key 验证失败: %s", e)
            return JSONResponse(
                status_code=401,
                content={"detail": "MIMO API Key is invalid or expired. Check your .env file."}
            )
        elif "APIError" in type_str:
            logger.error("MIMO API 错误: %s", e)
            return JSONResponse(
                status_code=502,
                content={"detail": f"MIMO API error: {str(e)}"}
            )
        else:
            logger.error("未预期错误: %s", e, exc_info=True)
            return JSONResponse(
                status_code=500,
                content={"detail": str(e)}
            )


# ═══════════════════════════════════════════════════════
#  管理 API
# ═══════════════════════════════════════════════════════

@app.get("/adpet/health")
async def health():
    """健康检查。"""
    count = await memory.device_count()
    return HealthResponse(status="ok", device_count=count)


@app.get("/api/devices")
async def api_list_devices():
    """列出所有设备。"""
    devices = await memory.list_devices()
    return JSONResponse(devices)


@app.get("/api/devices/{device_id}/messages")
async def api_device_messages(device_id: str):
    """获取设备的对话历史。"""
    messages = await db.get_device_messages(device_id, limit=100)
    return JSONResponse(messages)


@app.delete("/api/devices/{device_id}/messages")
async def api_clear_messages(device_id: str):
    """清空设备的对话历史。"""
    await db.clear_device_messages(device_id)
    return JSONResponse({"status": "ok"})


@app.delete("/api/devices/{device_id}")
async def api_delete_device(device_id: str):
    """删除设备及其所有数据。"""
    await db.delete_device(device_id)
    return JSONResponse({"status": "ok"})


@app.put("/api/devices/{device_id}/alias")
async def api_update_alias(device_id: str, payload: dict):
    """更新设备别名。"""
    alias = payload.get("alias", "")
    await db.update_device_alias(device_id, alias)
    return JSONResponse({"status": "ok"})


@app.get("/api/config")
async def api_get_config():
    """获取网关配置。"""
    cfg = await db.get_all_config()
    return JSONResponse(cfg)


@app.put("/api/config")
async def api_set_config(payload: dict):
    """更新网关配置。"""
    await db.set_many_config(payload)
    return JSONResponse({"status": "ok"})


# ═══════════════════════════════════════════════════════
#  管理界面入口
# ═══════════════════════════════════════════════════════

@app.get("/", response_class=HTMLResponse)
async def admin_page():
    """管理界面首页。"""
    index_file = static_dir / "index.html"
    if index_file.exists():
        return HTMLResponse(index_file.read_text(encoding="utf-8"))
    return HTMLResponse("<h1>AdPet Gateway</h1><p>static/index.html 不存在</p>")


# ─── 启动 ──────────────────────────────────────────────

if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "main:app",
        host=settings.HOST,
        port=settings.PORT,
        reload=False,
    )
