"""Local-only hardware diagnostics; the simulator runtime is entirely C++.

Writes identities and raw health data only under ignored logs/.
No fitness-machine control point writes. CCCD notification subscriptions only.
"""
import argparse
import asyncio
import json
from pathlib import Path
import time

ROOT = Path(__file__).resolve().parents[1]
RUN = ROOT / 'logs' / ('diagnostic-' + time.strftime('%Y%m%dT%H%M%S') + '-' + str(time.time_ns() % 1000000000))


def tracker(seconds):
    import openvr
    cfg = json.loads((ROOT / 'settings.local.json').read_text(encoding='utf-8-sig'))
    serial = cfg['tracker_serial']
    vr = openvr.init(openvr.VRApplication_Background)
    samples = []
    connected = False
    try:
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            poses = vr.getDeviceToAbsoluteTrackingPose(openvr.TrackingUniverseStanding, 0,
                                                      openvr.k_unMaxTrackedDeviceCount)
            for i, p in enumerate(poses):
                if not p.bDeviceIsConnected:
                    continue
                if vr.getStringTrackedDeviceProperty(i, openvr.Prop_SerialNumber_String) != serial:
                    continue
                connected = True
                if p.bPoseIsValid:
                    m = p.mDeviceToAbsoluteTracking
                    samples.append([time.monotonic(), *[float(m[j][3]) for j in range(3)]])
            time.sleep(.01)
    finally:
        openvr.shutdown()
    RUN.mkdir(exist_ok=True, parents=True)
    (RUN / 'tracker.local.json').write_text(json.dumps(samples), encoding='utf-8')
    span = [round(max(s[j] for s in samples)-min(s[j] for s in samples), 4)
            for j in (1, 2, 3)] if samples else []
    print(json.dumps(dict(configured_tracker_connected=connected, valid_samples=len(samples),
                         room_xyz_span_m=span)))


async def bluetooth(seconds):
    from bleak import BleakClient, BleakScanner
    cfgpath = ROOT / 'settings.local.json'
    cfg = json.loads(cfgpath.read_text(encoding='utf-8-sig'))
    records = []
    devices = await BleakScanner.discover(timeout=12, return_adv=True)
    rows, hearts = [], []
    for dev, adv in devices.values():
        uuids = [s.lower() for s in adv.service_uuids]
        records.append(dict(name=dev.name, address=dev.address, services=uuids))
        if cfg.get('rower_address'):
            if dev.address.lower() == cfg['rower_address'].lower(): rows.append(dev)
        elif any(s.startswith('00001826') for s in uuids) and any(
                n in (dev.name or '').upper() for n in ('MERACH', 'MRK', 'Q1', 'ROW')):
            rows.append(dev)
        if cfg.get('heart_rate_address'):
            if dev.address.lower() == cfg['heart_rate_address'].lower(): hearts.append(dev)
        elif any(s.startswith('0000180d') for s in uuids): hearts.append(dev)
    logdir = RUN
    logdir.mkdir(exist_ok=True, parents=True)
    (logdir/'ble-discovery.local.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    print(json.dumps(dict(discovered=len(devices), rower_candidates=len(rows), heart_candidates=len(hearts))), flush=True)
    async def receive(dev, role, uuid):
        packets = []
        try:
            async with BleakClient(dev, timeout=20) as client:
                services = [{'uuid':s.uuid, 'characteristics':[
                    {'uuid':c.uuid, 'properties':c.properties} for c in s.characteristics]} for s in client.services]
                (logdir/f'{role}-gatt.local.json').write_text(json.dumps(services, indent=2))
                char = client.services.get_characteristic(uuid)
                if char is None:
                    print(role + ': standard characteristic missing', flush=True)
                    return
                cfg[role+'_address'] = dev.address
                cfgpath.write_text(json.dumps(cfg, indent=2)+'\n', encoding='utf-8')
                def received(_, payload):
                    packets.append(dict(t=time.time(), hex=bytes(payload).hex()))
                    if len(packets) == 1: print(role + ': first notification received', flush=True)
                await client.start_notify(char, received)
                print(role + ': subscribed; ready to row', flush=True)
                await asyncio.sleep(seconds)
                await client.stop_notify(char)
        except Exception as e:
            # Exception strings can contain hardware identities; keep them local.
            (logdir/f'{role}-error.local.txt').write_text(str(e), encoding='utf-8')
            print(role + ': connection failed; see local log', flush=True)
        finally:
            (logdir/f'{role}-packets.local.json').write_text(json.dumps(packets), encoding='utf-8')
            print(json.dumps(dict(role=role, packets=len(packets))), flush=True)
    jobs=[]
    if len(rows)==1: jobs.append(receive(rows[0], 'rower', '00002ad1-0000-1000-8000-00805f9b34fb'))
    if len(hearts)==1: jobs.append(receive(hearts[0], 'heart_rate', '00002a37-0000-1000-8000-00805f9b34fb'))
    await asyncio.gather(*jobs)


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('kind', choices=['tracker', 'ble'])
    parser.add_argument('--seconds', type=int, default=60)
    args=parser.parse_args()
    if args.kind=='tracker': tracker(args.seconds)
    else: asyncio.run(bluetooth(args.seconds))
