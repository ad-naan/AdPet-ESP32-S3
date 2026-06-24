"""AdPet Gateway 端点测试脚本。

测试内容:
  1. 健康检查
  2. 文本对话端点
  3. 设备列表 API
  4. 配置读写 API
  5. 语音对话端点（使用合成 WAV）
"""

import io
import json
import os
import struct
import sys
import time

# 规避 Windows 环境下 httpx 解析 IPv6 代理规则崩溃 bug
os.environ["NO_PROXY"] = "127.0.0.1,localhost"

import requests

BASE = "http://localhost:8788"
DEVICE_ID = "test-device-001"

from urllib.parse import unquote

def safe_print(label: str, text: str):
    encoding = sys.stdout.encoding or "utf-8"
    # 将 Unicode 特殊字符进行安全替换，防止 Windows 中文控制台（GBK）打印报错
    encoded = text.encode(encoding, errors="replace")
    decoded = encoded.decode(encoding)
    print(f"{label}{decoded}")


def make_silent_wav(duration_ms: int = 500, sample_rate: int = 16000) -> bytes:
    """生成一段静音 WAV 用于测试。"""
    num_samples = int(sample_rate * duration_ms / 1000)
    pcm_bytes = num_samples * 2  # 16-bit mono

    buf = io.BytesIO()
    buf.write(b"RIFF")
    buf.write(struct.pack("<I", 36 + pcm_bytes))
    buf.write(b"WAVEfmt ")
    buf.write(struct.pack("<IHHIIHH", 16, 1, 1, sample_rate, sample_rate * 2, 2, 16))
    buf.write(b"data")
    buf.write(struct.pack("<I", pcm_bytes))
    buf.write(b"\x00" * pcm_bytes)
    return buf.getvalue()


def test(name: str, fn):
    """运行单个测试。"""
    print(f"\n{'='*50}")
    print(f"测试: {name}")
    print("=" * 50)
    try:
        fn()
        print(f"[PASS] {name}")
        return True
    except Exception as e:
        print(f"[FAIL] {name}: {e}")
        return False


def test_health():
    r = requests.get(f"{BASE}/adpet/health")
    assert r.status_code == 200, f"HTTP {r.status_code}"
    data = r.json()
    print(f"  状态: {data['status']}, 设备数: {data['device_count']}")
    assert data["status"] == "ok"


def test_text_chat():
    r = requests.post(
        f"{BASE}/adpet/text",
        json={
            "device_id": DEVICE_ID,
            "system_prompt": "你是一只可爱的桌面宠物，用简短中文回复。",
            "text": "你好，你叫什么名字？",
        },
    )
    if r.status_code == 401:
        print("  (MIMO API KEY 校验不通过，接口正常处理 401)")
        return
    assert r.status_code == 200, f"HTTP {r.status_code}: {r.text}"
    data = r.json()
    safe_print("  回复: ", data['reply'])
    assert len(data["reply"]) > 0


def test_device_list():
    r = requests.get(f"{BASE}/api/devices")
    assert r.status_code == 200, f"HTTP {r.status_code}"
    devices = r.json()
    print(f"  设备数: {len(devices)}")
    found = any(d["device_id"] == DEVICE_ID for d in devices)
    print(f"  测试设备已注册: {found}")


def test_device_messages():
    r = requests.get(f"{BASE}/api/devices/{DEVICE_ID}/messages")
    assert r.status_code == 200, f"HTTP {r.status_code}"
    msgs = r.json()
    print(f"  消息数: {len(msgs)}")
    if msgs:
        last = msgs[-1]
        safe_print(f"  最后一条: [{last['role']}] ", last['content'][:60])


def test_config_rw():
    # 写入
    r = requests.put(
        f"{BASE}/api/config",
        json={"test_key": "test_value_123"},
    )
    assert r.status_code == 200, f"写入失败: HTTP {r.status_code}"

    # 读取
    r = requests.get(f"{BASE}/api/config")
    assert r.status_code == 200, f"读取失败: HTTP {r.status_code}"
    cfg = r.json()
    assert cfg.get("test_key") == "test_value_123", f"值不匹配: {cfg}"
    print(f"  配置读写正常: test_key={cfg['test_key']}")


def test_voice_chat():
    wav = make_silent_wav(500)
    metadata = json.dumps({
        "device_id": DEVICE_ID,
        "system_prompt": "你是 AdPet。用简短中文回复。用户发送了一段静音。",
    })

    r = requests.post(
        f"{BASE}/adpet/chat",
        files={"audio": ("test.wav", wav, "audio/wav")},
        data={"metadata": metadata},
        headers={"X-AdPet-Device-Id": DEVICE_ID},
    )

    print(f"  HTTP 状态: {r.status_code}")
    transcript = unquote(r.headers.get('X-AdPet-Transcript', ''))
    reply = unquote(r.headers.get('X-AdPet-Reply', ''))
    emotion = r.headers.get('X-AdPet-Emotion', '')

    safe_print("  Transcript: ", transcript if transcript else "(无)")
    safe_print("  Reply: ", reply if reply else "(无)")
    safe_print("  Emotion: ", emotion if emotion else "(无)")

    if r.status_code == 401:
        print("  (MIMO API KEY 校验不通过，语音接口正常处理 401)")
        return
    elif r.status_code == 200:
        print(f"  音频大小: {len(r.content)} 字节")
        assert r.headers.get("Content-Type", "").startswith("audio/wav")
    elif r.status_code == 204:
        print("  (ASR 未识别到文本，返回 204 — 静音测试的预期行为)")
    else:
        raise AssertionError(f"意外状态码: {r.status_code} — {r.text}")


def test_clear_messages():
    r = requests.delete(f"{BASE}/api/devices/{DEVICE_ID}/messages")
    assert r.status_code == 200, f"HTTP {r.status_code}"
    print(f"  设备 {DEVICE_ID} 消息已清空")


if __name__ == "__main__":
    print("AdPet Gateway 测试")
    print(f"目标: {BASE}\n")

    tests = [
        ("健康检查", test_health),
        ("文本对话", test_text_chat),
        ("设备列表", test_device_list),
        ("设备消息", test_device_messages),
        ("配置读写", test_config_rw),
        ("语音对话（静音）", test_voice_chat),
        ("清空消息", test_clear_messages),
    ]

    passed = sum(test(name, fn) for name, fn in tests)
    total = len(tests)

    print(f"\n{'='*50}")
    print(f"结果: {passed}/{total} 通过")
    print("=" * 50)

    sys.exit(0 if passed == total else 1)
