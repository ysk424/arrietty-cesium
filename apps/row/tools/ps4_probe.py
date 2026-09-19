"""PS4 Bluetooth sensor diagnostic and explicit local device selection.

No HMD, rower, FTMS control point, effects or virtual gamepad drivers are opened.
Only a neutral extended-report request is sent to the selected controller.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
from pathlib import Path
import struct
import time
import zlib

WORKSPACE=Path(__file__).resolve().parents[3]


def enable(path):
    kernel=C.WinDLL('kernel32',use_last_error=True)
    dll=C.WinDLL('hid',use_last_error=True)
    kernel.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,C.c_void_p,W.DWORD,W.DWORD,W.HANDLE]
    kernel.CreateFileW.restype=W.HANDLE
    kernel.CloseHandle.argtypes=[W.HANDLE]
    dll.HidD_GetPreparsedData.argtypes=[W.HANDLE,C.POINTER(C.c_void_p)]
    dll.HidP_GetCaps.argtypes=[C.c_void_p,C.c_void_p]
    dll.HidD_FreePreparsedData.argtypes=[C.c_void_p]
    dll.HidD_SetOutputReport.argtypes=[W.HANDLE,C.c_void_p,W.ULONG]
    dll.HidD_SetOutputReport.restype=C.c_ubyte
    handle=kernel.CreateFileW(path.decode(),0xc0000000,3,None,3,0,None)
    if handle==C.c_void_p(-1).value: raise RuntimeError('Cannot open selected controller')
    preparsed=C.c_void_p()
    try:
        if not dll.HidD_GetPreparsedData(handle,C.byref(preparsed)): raise RuntimeError('Cannot read HID capabilities')
        caps=C.create_string_buffer(64)
        if dll.HidP_GetCaps(preparsed,caps)!=0x110000: raise RuntimeError('Invalid HID capabilities')
        usage,page,_,size,_=struct.unpack_from('<5H',caps.raw)
        if usage!=5 or page!=1 or not 78<=size<=4096: raise RuntimeError('Unsupported controller layout')
        report=bytearray(size);report[:2]=b'\x11\xc4'
        struct.pack_into('<I',report,74,zlib.crc32(b'\xa2'+report[:74]))
        if not dll.HidD_SetOutputReport(handle,C.create_string_buffer(bytes(report)),size):
            raise RuntimeError('Cannot enable sensor reports')
    finally:
        if preparsed.value: dll.HidD_FreePreparsedData(preparsed)
        kernel.CloseHandle(handle)


def main():
    import hid
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--select',action='store_true',help='Save verified controller and select PS4 input in local Row settings')
    parser.add_argument('--seconds',type=int,default=10)
    args=parser.parse_args()
    if not 1<=args.seconds<=300: parser.error('seconds must be 1..300')
    settings=WORKSPACE/'config/row.local.json'
    config=json.loads(settings.read_text(encoding='utf-8-sig')) if settings.exists() else {}
    devices=[d for d in hid.enumerate() if d['vendor_id']==0x054c and d['product_id'] in (0x09cc,0x05c4) and d.get('usage_page')==1 and d.get('usage')==5 and d.get('bus_type')==2]
    if not args.select:
        selected=config.get('ps4_hid_path','').casefold()
        devices=[d for d in devices if d['path'].decode().casefold()==selected]
    if len(devices)!=1: raise RuntimeError('Connect exactly one PS4-compatible Bluetooth controller; use --select for initial setup')
    path=devices[0]['path'];enable(path);h=hid.device();h.open_path(path)
    packets=bad=0;low=[32767]*6;high=[-32768]*6;buttons=set();began=time.monotonic()
    try:
        while time.monotonic()-began<args.seconds:
            b=bytes(h.read(4096,100))
            if len(b)<78 or b[0]!=0x11: continue
            if zlib.crc32(b'\xa1'+b[:74])!=int.from_bytes(b[74:78],'little'):bad+=1;continue
            values=struct.unpack_from('<6h',b,15);packets+=1
            low=[min(a,v) for a,v in zip(low,values)];high=[max(a,v) for a,v in zip(high,values)]
            for name,byte,mask in [('square',7,16),('triangle',7,128),('l1',8,1),('r1',8,2),('l2',8,4),('r2',8,8)]:
                if b[byte]&mask:buttons.add(name)
    finally:h.close()
    result={'packets':packets,'crc_errors':bad,'min_gyro_accel_raw':low,'max_gyro_accel_raw':high,'buttons':sorted(buttons)}
    print(json.dumps(result))
    if packets<10: raise RuntimeError('No usable sensor stream; settings unchanged')
    if args.select:
        if settings.exists():
            backup=WORKSPACE/'logs/row/ps4-settings-before.local.json'
            backup.parent.mkdir(parents=True,exist_ok=True)
            if not backup.exists(): backup.write_bytes(settings.read_bytes())
        config['bar_input']='ps4';config['ps4_hid_path']=path.decode()
        settings.write_text(json.dumps(config,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('Verified controller selected in private Row settings.')


if __name__=='__main__':
    try:main()
    except (OSError,RuntimeError,ValueError) as error:
        raise SystemExit(str(error) if isinstance(error,RuntimeError) else 'Controller diagnostic failed; check the connection and local settings.')
