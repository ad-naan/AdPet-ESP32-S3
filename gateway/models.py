"""Pydantic 数据模型。"""

from pydantic import BaseModel, Field


class ChatMetadata(BaseModel):
    """Voice chat 请求中的 metadata JSON 字段。"""

    device_id: str = "adpet-001"
    system_prompt: str = ""


class TextRequest(BaseModel):
    """Text endpoint 请求体。"""

    device_id: str = "adpet-001"
    system_prompt: str = ""
    text: str


class TextResponse(BaseModel):
    """Text endpoint 响应体。"""

    reply: str


class DeviceInfo(BaseModel):
    """单个设备的状态信息。"""

    device_id: str
    message_count: int = 0
    summary: str = ""
    last_active: str = ""


class HealthResponse(BaseModel):
    """健康检查响应。"""

    status: str = "ok"
    device_count: int = 0
