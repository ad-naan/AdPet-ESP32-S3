"""情绪推断模块。

根据 LLM 回复内容，推断应该展示的 OLED 表情。
"""

from __future__ import annotations

import re

# 情绪关键词映射表
_EMOTION_KEYWORDS: dict[str, list[str]] = {
    "happy": [
        "哈哈", "嘿嘿", "开心", "高兴", "太好了", "棒", "耶",
        "喜欢", "爱", "幸福", "快乐", "嘻嘻", "好玩",
        "haha", "happy", "yay", "love", "great", "nice",
    ],
    "sad": [
        "难过", "伤心", "呜呜", "哭", "可怜", "遗憾", "抱歉",
        "对不起", "不好意思", "sad", "sorry", "cry",
    ],
    "angry": [
        "生气", "讨厌", "烦", "哼", "可恶", "不行",
        "angry", "hate", "annoying",
    ],
    "thinking": [
        "嗯", "让我想想", "思考", "这个嘛", "或许",
        "hmm", "think", "maybe", "perhaps",
    ],
}


def infer_emotion(reply: str) -> str:
    """根据回复文本推断表情。

    Args:
        reply: LLM 生成的回复文本。

    Returns:
        表情标识：neutral / happy / sad / angry / talking / thinking
    """
    lower = reply.lower()

    # 按优先级匹配
    scores: dict[str, int] = {}
    for emotion, keywords in _EMOTION_KEYWORDS.items():
        count = sum(1 for kw in keywords if kw in lower)
        if count > 0:
            scores[emotion] = count

    if scores:
        return max(scores, key=scores.get)  # type: ignore[arg-type]

    # 默认回复时用 talking
    return "talking"
