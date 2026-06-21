"""LLM 对话生成管道。

拼接系统提示 + 摘要 + 历史消息 + 用户输入，调用 MIMO LLM 生成回复。
"""

from __future__ import annotations

import logging

from openai import OpenAI

from config import settings

logger = logging.getLogger("adpet.llm")

_client: OpenAI | None = None


def _get_client() -> OpenAI:
    global _client
    if _client is None:
        _client = OpenAI(
            api_key=settings.MIMO_API_KEY,
            base_url=settings.MIMO_BASE_URL,
        )
    return _client


async def chat(
    transcript: str,
    history: list[dict[str, str]],
    device_system_prompt: str = "",
    summary: str = "",
) -> str:
    """生成 LLM 对话回复。

    Args:
        transcript: 用户语音转写文本。
        history: 最近的对话历史。
        device_system_prompt: 来自 ESP32 设备的系统提示。
        summary: 对话摘要。

    Returns:
        LLM 生成的回复文本。
    """
    # 构建系统提示
    system_parts = [settings.SERVER_SYSTEM_PROMPT]
    if device_system_prompt:
        system_parts.append(device_system_prompt)
    if summary:
        system_parts.append(f"对话摘要：{summary}")
    system_prompt = "\n".join(system_parts)

    messages = [
        {"role": "system", "content": system_prompt},
        *history,
        {"role": "user", "content": transcript},
    ]

    logger.info("LLM 请求: 消息数 %d, 用户: %s", len(messages), transcript)

    completion = _get_client().chat.completions.create(
        model=settings.LLM_MODEL,
        messages=messages,
        max_tokens=80,
        temperature=0.8,
    )

    reply = completion.choices[0].message.content or "嗯？我刚刚有点走神。"
    logger.info("LLM 回复: %s", reply)
    return reply
