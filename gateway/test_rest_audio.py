import base64
import json
import os
import urllib.error
import urllib.request


def main() -> None:
    wav_path = os.environ.get("ADPET_TEST_WAV", "gateway/test_speech.wav")
    model = os.environ.get("GEMINI_TEXT_MODEL", "gemini-2.5-flash")
    with open(wav_path, "rb") as f:
        wav = f.read()

    body = {
        "contents": [{
            "parts": [
                {
                    "text": (
                        "你是一个小桌宠。请先识别音频内容，再用中文简短回答。"
                        "只输出 JSON，字段为 transcript、reply、emotion。"
                    )
                },
                {
                    "inline_data": {
                        "mime_type": "audio/wav",
                        "data": base64.b64encode(wav).decode("utf-8"),
                    }
                },
            ]
        }],
        "generationConfig": {
            "responseMimeType": "application/json",
            "thinkingConfig": {"thinkingBudget": 0},
        },
    }
    url = (
        f"https://generativelanguage.googleapis.com/v1beta/models/{model}:"
        f"generateContent?key={os.environ['GEMINI_API_KEY']}"
    )
    req = urllib.request.Request(
        url,
        data=json.dumps(body).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    try:
        print(urllib.request.urlopen(req, timeout=30).read().decode("utf-8")[:2000])
    except urllib.error.HTTPError as exc:
        print(exc.code)
        print(exc.read().decode("utf-8")[:2000])


if __name__ == "__main__":
    main()
