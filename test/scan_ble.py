import asyncio
from bleak import BleakScanner

async def main():
    print("Scanning for BLE devices...")
    devices = await BleakScanner.discover(timeout=8)
    for d in devices:
        print(f"Name: {d.name}, Address: {d.address}")

if __name__ == "__main__":
    asyncio.run(main())