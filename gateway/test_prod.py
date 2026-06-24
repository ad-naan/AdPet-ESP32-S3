import requests
import io
import struct
import json

BASE = "https://pet.adnaan.site"
DEVICE_ID = "test-device-001"

def make_silent_wav(duration_ms=500, sample_rate=16000):
    num_samples = int(sample_rate * duration_ms / 1000)
    pcm_bytes = num_samples * 2
    buf = io.BytesIO()
    buf.write(b"RIFF")
    buf.write(struct.pack("<I", 36 + pcm_bytes))
    buf.write(b"WAVEfmt ")
    buf.write(struct.pack("<IHHIIHH", 16, 1, 1, sample_rate, sample_rate * 2, 2, 16))
    buf.write(b"data")
    buf.write(struct.pack("<I", pcm_bytes))
    buf.write(b"\x00" * pcm_bytes)
    return buf.getvalue()

def run_test():
    wav = make_silent_wav(500)
    metadata = json.dumps({
        "device_id": DEVICE_ID,
        "system_prompt": "你是 AdPet。不管用户发送什么，用中文回复'你好啊，我是小宠物'。",
    })

    print(f"Sending request to {BASE}/adpet/chat ...")
    r = requests.post(
        f"{BASE}/adpet/chat",
        files={"audio": ("test.wav", wav, "audio/wav")},
        data={"metadata": metadata},
        headers={"X-AdPet-Device-Id": DEVICE_ID},
    )

    print(f"HTTP Status: {r.status_code}")
    print(f"Headers: {dict(r.headers)}")
    print(f"Content Length: {len(r.content)} bytes")
    if r.status_code == 200:
        with open("response_prod.wav", "wb") as f:
            f.write(r.content)
        print("Response WAV saved to response_prod.wav")
    else:
        print(f"Response text: {r.text}")

if __name__ == "__main__":
    run_test()
