"""多设备对话记忆管理。

基于 SQLite 持久化，按 device_id 隔离每个设备的对话历史。
"""

from __future__ import annotations

import logging
from typing import Any

import database as db
from config import settings

logger = logging.getLogger("adpet.memory")


async def append(device_id: str, role: str, content: str) -> None:
    """向指定设备追加消息。"""
    await db.append_message(device_id, role, content)


async def get_recent(device_id: str) -> list[dict[str, str]]:
    """获取最近 N 轮对话。"""
    return await db.get_recent_messages(device_id, settings.MAX_HISTORY_TURNS)


async def list_devices() -> list[dict[str, Any]]:
    """列出所有设备状态。"""
    return await db.list_devices()


async def device_count() -> int:
    return await db.device_count()
