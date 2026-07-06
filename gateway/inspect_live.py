import asyncio
import inspect
import os

from google import genai
from google.genai import types


async def main() -> None:
    client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])
    config = types.LiveConnectConfig(response_modalities=["AUDIO"])
    async with client.aio.live.connect(
        model=os.environ.get("GEMINI_LIVE_MODEL", "gemini-2.5-flash-native-audio-latest"),
        config=config,
    ) as session:
        print([name for name in dir(session) if not name.startswith("_")])
        print("send", inspect.signature(session.send))
        print("start_stream", inspect.signature(session.start_stream))
        print("receive", inspect.signature(session.receive))


if __name__ == "__main__":
    asyncio.run(main())
