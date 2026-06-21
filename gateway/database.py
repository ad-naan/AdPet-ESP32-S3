"""SQLite 数据库管理。

管理设备信息、对话历史、网关配置的持久化存储。
"""

from __future__ import annotations

import json
import logging
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import aiosqlite

logger = logging.getLogger("adpet.database")

DB_PATH = Path(__file__).parent / "adpet_gateway.db"

_db: aiosqlite.Connection | None = None


async def get_db() -> aiosqlite.Connection:
    """获取数据库连接（单例）。"""
    global _db
    if _db is None:
        _db = await aiosqlite.connect(str(DB_PATH))
        _db.row_factory = aiosqlite.Row
        await _db.execute("PRAGMA journal_mode=WAL")
        await _db.execute("PRAGMA foreign_keys=ON")
        await _init_tables(_db)
    return _db


async def close_db() -> None:
    """关闭数据库连接。"""
    global _db
    if _db is not None:
        await _db.close()
        _db = None


async def _init_tables(db: aiosqlite.Connection) -> None:
    """初始化数据库表结构。"""
    await db.executescript(
        """
        CREATE TABLE IF NOT EXISTS devices (
            device_id   TEXT PRIMARY KEY,
            alias       TEXT DEFAULT '',
            summary     TEXT DEFAULT '',
            last_active TEXT DEFAULT '',
            created_at  TEXT DEFAULT ''
        );

        CREATE TABLE IF NOT EXISTS messages (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id   TEXT NOT NULL,
            role        TEXT NOT NULL,
            content     TEXT NOT NULL,
            created_at  TEXT NOT NULL,
            FOREIGN KEY (device_id) REFERENCES devices(device_id)
        );

        CREATE INDEX IF NOT EXISTS idx_messages_device
            ON messages(device_id, created_at);

        CREATE TABLE IF NOT EXISTS gateway_config (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        """
    )
    await db.commit()
    logger.info("数据库表初始化完成: %s", DB_PATH)


# ─── 设备操作 ───────────────────────────────────────────

async def ensure_device(device_id: str) -> None:
    """确保设备存在，不存在则自动创建。"""
    db = await get_db()
    now = datetime.now(timezone.utc).isoformat()
    await db.execute(
        """
        INSERT INTO devices (device_id, created_at, last_active)
        VALUES (?, ?, ?)
        ON CONFLICT(device_id) DO UPDATE SET last_active = ?
        """,
        (device_id, now, now, now),
    )
    await db.commit()


async def list_devices() -> list[dict[str, Any]]:
    """列出所有设备及消息数。"""
    db = await get_db()
    cursor = await db.execute(
        """
        SELECT d.device_id, d.alias, d.summary, d.last_active, d.created_at,
               COUNT(m.id) AS message_count
        FROM devices d
        LEFT JOIN messages m ON d.device_id = m.device_id
        GROUP BY d.device_id
        ORDER BY d.last_active DESC
        """
    )
    rows = await cursor.fetchall()
    return [dict(r) for r in rows]


async def device_count() -> int:
    db = await get_db()
    cursor = await db.execute("SELECT COUNT(*) FROM devices")
    row = await cursor.fetchone()
    return row[0] if row else 0


async def update_device_alias(device_id: str, alias: str) -> None:
    db = await get_db()
    await db.execute(
        "UPDATE devices SET alias = ? WHERE device_id = ?",
        (alias, device_id),
    )
    await db.commit()


async def delete_device(device_id: str) -> None:
    """删除设备及其所有消息。"""
    db = await get_db()
    await db.execute("DELETE FROM messages WHERE device_id = ?", (device_id,))
    await db.execute("DELETE FROM devices WHERE device_id = ?", (device_id,))
    await db.commit()


# ─── 消息操作 ───────────────────────────────────────────

async def append_message(device_id: str, role: str, content: str) -> None:
    """追加一条对话消息。"""
    db = await get_db()
    now = datetime.now(timezone.utc).isoformat()
    await ensure_device(device_id)
    await db.execute(
        "INSERT INTO messages (device_id, role, content, created_at) VALUES (?, ?, ?, ?)",
        (device_id, role, content, now),
    )
    await db.execute(
        "UPDATE devices SET last_active = ? WHERE device_id = ?",
        (now, device_id),
    )
    await db.commit()


async def get_recent_messages(
    device_id: str, turns: int = 8
) -> list[dict[str, str]]:
    """获取设备最近 N 轮（2N 条）消息。"""
    db = await get_db()
    limit = turns * 2
    cursor = await db.execute(
        """
        SELECT role, content FROM (
            SELECT role, content, created_at
            FROM messages
            WHERE device_id = ?
            ORDER BY created_at DESC
            LIMIT ?
        ) sub ORDER BY created_at ASC
        """,
        (device_id, limit),
    )
    rows = await cursor.fetchall()
    return [{"role": r["role"], "content": r["content"]} for r in rows]


async def get_device_messages(
    device_id: str, limit: int = 50
) -> list[dict[str, Any]]:
    """获取设备消息（含时间戳，给管理界面用）。"""
    db = await get_db()
    cursor = await db.execute(
        """
        SELECT id, role, content, created_at
        FROM messages
        WHERE device_id = ?
        ORDER BY created_at DESC
        LIMIT ?
        """,
        (device_id, limit),
    )
    rows = await cursor.fetchall()
    return [dict(r) for r in reversed(rows)]


async def clear_device_messages(device_id: str) -> None:
    """清空设备的所有消息。"""
    db = await get_db()
    await db.execute("DELETE FROM messages WHERE device_id = ?", (device_id,))
    await db.commit()


# ─── 配置操作 ───────────────────────────────────────────

async def get_config(key: str, default: str = "") -> str:
    """读取配置值。"""
    db = await get_db()
    cursor = await db.execute(
        "SELECT value FROM gateway_config WHERE key = ?", (key,)
    )
    row = await cursor.fetchone()
    return row["value"] if row else default


async def set_config(key: str, value: str) -> None:
    """写入配置值。"""
    db = await get_db()
    await db.execute(
        """
        INSERT INTO gateway_config (key, value) VALUES (?, ?)
        ON CONFLICT(key) DO UPDATE SET value = ?
        """,
        (key, value, value),
    )
    await db.commit()


async def get_all_config() -> dict[str, str]:
    """获取所有配置。"""
    db = await get_db()
    cursor = await db.execute("SELECT key, value FROM gateway_config")
    rows = await cursor.fetchall()
    return {r["key"]: r["value"] for r in rows}


async def set_many_config(items: dict[str, str]) -> None:
    """批量写入配置。"""
    db = await get_db()
    for key, value in items.items():
        await db.execute(
            """
            INSERT INTO gateway_config (key, value) VALUES (?, ?)
            ON CONFLICT(key) DO UPDATE SET value = ?
            """,
            (key, value, value),
        )
    await db.commit()
