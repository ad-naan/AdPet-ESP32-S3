"""API Key 认证依赖。

当 GATEWAY_API_KEY 为空时跳过认证。
"""

from fastapi import Header, HTTPException

from config import settings


async def verify_api_key(
    authorization: str | None = Header(default=None),
) -> None:
    """FastAPI 依赖：验证 Bearer token。"""
    expected = settings.GATEWAY_API_KEY
    if not expected:
        # 未设置密钥，跳过认证
        return

    if not authorization:
        raise HTTPException(status_code=401, detail="Missing Authorization header")

    # 支持 "Bearer xxx" 和裸 token
    token = authorization.removeprefix("Bearer ").strip()
    if token != expected:
        raise HTTPException(status_code=403, detail="Invalid API key")
