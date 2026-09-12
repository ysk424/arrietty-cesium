"""Read-only WT9011DCL BLE capture. Identities/raw samples stay under logs/row.

Only subscribes to notifications; never sends sensor or fitness control commands.
Use --select to remember the sole discovered sensor after valid data arrives.
"""
import argparse
import asyncio
import json
from pathlib import Path
import struct
import time
import re
import logging

WORKSPACE = Path(__file__).resolve().parents[3]
SERVICE = '0000ffe5-0000-1000-8000-00805f9a34fb'
NOTIFY = '0000ffe4-0000-1000-8000-00805f9a34fb'


async def capture(seconds, select=False, paired_file=None):
    from bleak import BleakClient, BleakScanner
    from bleak.backends.device import BLEDevice
    run = WORKSPACE / 'logs/row' / ('wit-' + time.strftime('%Y%m%dT%H%M%S'))
    run.mkdir(parents=True, exist_ok=True)
    logging.basicConfig(filename=run/'ble-debug.local.log', level=logging.DEBUG, encoding='utf-8')
    cfg_path = WORKSPACE / 'config/row.local.json'
    cfg = json.loads(cfg_path.read_text(encoding='utf-8-sig')) if cfg_path.exists() else {}
    packets, samples = [], []
    try:
        devices = await BleakScanner.discover(timeout=10, return_adv=True)
        (run/'discovery.local.json').write_text(json.dumps([
            dict(name=d.name, local_name=a.local_name, address=d.address, services=a.service_uuids)
            for d,a in devices.values()], indent=2), encoding='utf-8')
        candidates = []
        for device, adv in devices.values():
            if cfg.get('imu_address'):
                matches = device.address.lower() == cfg['imu_address'].lower()
            else:
                matches = (device.name or adv.local_name or '').upper().startswith('WT901')
            if matches:
                candidates.append(device)
        if not candidates and cfg.get('imu_address'):
            candidates.append(BLEDevice(cfg['imu_address'], 'Configured WIT IMU', None))
        if not candidates and paired_file:
            paired = json.loads(Path(paired_file).read_text(encoding='utf-8-sig'))
            for entry in paired if isinstance(paired, list) else [paired]:
                match = re.search(r'DEV_([0-9A-F]{12})', entry['InstanceId'], re.I)
                if match and entry['FriendlyName'].upper().startswith('WT901'):
                    value = match[1]
                    candidates.append(BLEDevice(':'.join(value[i:i+2] for i in range(0,12,2)), entry['FriendlyName'], None))
        print(json.dumps(dict(discovered=len(devices), imu_candidates=len(candidates))), flush=True)
        if len(candidates) != 1:
            return 1
        device = candidates[0]
        (run / 'device.local.json').write_text(json.dumps(dict(name=device.name, address=device.address)), encoding='utf-8')
        options = dict(use_cached_services=False)
        if cfg.get('imu_address_type') in ('public', 'random'):
            options['address_type'] = cfg['imu_address_type']
        if device.details:
            advertisement = device.details.adv or device.details.scan
            from winrt.windows.devices.bluetooth import BluetoothAddressType
            options['address_type'] = 'random' if advertisement.bluetooth_address_type == BluetoothAddressType.RANDOM else 'public'
        print('Connecting using '+options.get('address_type', 'registered')+' BLE address type', flush=True)
        async with BleakClient(device, timeout=40, winrt=options) as client:
            services = [dict(uuid=s.uuid, characteristics=[dict(uuid=c.uuid, properties=c.properties)
                        for c in s.characteristics]) for s in client.services]
            (run / 'gatt.local.json').write_text(json.dumps(services, indent=2), encoding='utf-8')
            characteristic = client.services.get_characteristic(NOTIFY)
            if characteristic is None or 'notify' not in characteristic.properties:
                print('Expected WIT BLE notification characteristic unavailable', flush=True)
                return 1
            began = time.monotonic()
            def received(_, payload):
                data = bytes(payload)
                now = time.monotonic() - began
                packets.append(dict(t=now, hex=data.hex()))
                if len(data) == 20 and data[:2] == b'\x55\x61':
                    values = struct.unpack('<9h', data[2:])
                    samples.append([now, *[v/32768*16 for v in values[:3]],
                                    *[v/32768*2000 for v in values[3:6]],
                                    *[v/32768*180 for v in values[6:]]])
                    if len(samples) == 1:
                        print('IMU receiving acceleration, angular velocity and angles', flush=True)
            await client.start_notify(characteristic, received)
            print('IMU capture started', flush=True)
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline and not (run/'stop.local').exists():
                await asyncio.sleep(.1)
            await client.stop_notify(characteristic)
        if select and samples:
            cfg['imu_address'] = device.address
            if options.get('address_type'):
                cfg['imu_address_type'] = options['address_type']
            cfg_path.write_text(json.dumps(cfg, indent=2)+'\n', encoding='utf-8')
        duration = samples[-1][0]-samples[0][0] if len(samples)>1 else 0
        summary = dict(packets=len(packets), samples=len(samples),
                       hz=round((len(samples)-1)/duration, 1) if duration else 0,
                       acceleration_g_mean=[round(sum(s[i] for s in samples)/len(samples),4) for i in (1,2,3)] if samples else [],
                       acceleration_g_span=[round(max(s[i] for s in samples)-min(s[i] for s in samples),4) for i in (1,2,3)] if samples else [])
        (run/'summary.local.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
        print(json.dumps(summary), flush=True)
        return 0 if samples else 1
    except Exception as exc:
        (run/'error.local.txt').write_text(type(exc).__name__+': '+str(exc), encoding='utf-8')
        print('IMU connection failed; details saved in local log', flush=True)
        return 1
    finally:
        (run/'packets.local.json').write_text(json.dumps(packets), encoding='utf-8')
        (run/'samples.local.json').write_text(json.dumps(samples), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--seconds', type=int, default=20)
    parser.add_argument('--select', action='store_true')
    parser.add_argument('--paired-file', type=Path, help='Private Windows PnP inventory for an already paired device')
    args = parser.parse_args()
    if not 1 <= args.seconds <= 600:
        parser.error('--seconds must be 1..600')
    raise SystemExit(asyncio.run(capture(args.seconds, args.select, args.paired_file)))
