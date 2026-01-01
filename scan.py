#!/usr/bin/env -S uv run --script

# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "bleak",
# ]
# ///

import asyncio
from bleak import BleakScanner, BleakClient
import logging

logger = logging.getLogger(__name__)

async def main():
    logging.basicConfig(
        level=logging.INFO,
        #format="%(asctime)-15s %(name)-8s %(levelname)s: %(message)s",
        format="%(message)s",
    )

    device = await BleakScanner.find_device_by_name(
        "Cosori Gooseneck Kettle", cb={"use_bdaddr": True}
    )
    if device is None:
        logger.error("could not find device")
        return

    print(f"Found: {device.name} - {device.address}")

    logger.info("disconnected")

asyncio.run(main())
