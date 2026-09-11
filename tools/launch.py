"""Interactive launcher: resolve -> explicit Y -> prepare -> UE."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import sys

from places import ROOT, PlaceError, resolve_place, confirmed, build_scene, request_json


def main():
    p=argparse.ArgumentParser()
    p.add_argument('place')
    p.add_argument('--demo',action='store_true')
    p.add_argument('--resolve-only',action='store_true',help='Identify only; no confirmation or terrain preparation')
    p.add_argument('--prepare-only',action='store_true',help='Confirm and prepare scene; do not launch UE')
    p.add_argument('--refresh-place',action='store_true')
    p.add_argument('--volume',type=float,default=.8)
    p.add_argument('--radius-km',type=float,default=3)
    p.add_argument('--water-level',type=float)
    p.add_argument('--model',default=os.environ.get('ARRIETTY_OPENAI_MODEL','gpt-5.4-mini'))
    p.add_argument('--engine-root',default='C:/Program Files/Epic Games/UE_5.8')
    args=p.parse_args()
    if not math.isfinite(args.volume) or not 0<=args.volume<=1:
        raise PlaceError('Volume must be 0..1')
    place=resolve_place(args.place,args.model,args.refresh_place)
    label=f"{place['country_ja']}・{place['region_ja']}の{place['name_ja']}"
    if args.resolve_only:
        print(label)
        return 0
    try:
        answer=input(label+'ですね？ [Y/N]: ')
    except EOFError:
        answer=''
    if not confirmed(answer):
        print('起動を中止しました。場所の指定を変えて再実行してください。')
        return 0
    token=os.environ.get('CESIUM_ION_TOKEN','').strip()
    cfg_path=ROOT/'cesium.local.json'
    config=json.loads(cfg_path.read_text(encoding='utf-8-sig')) if cfg_path.exists() else {}
    token=token or config.get('ion_access_token','')
    if not token:
        raise PlaceError('CESIUM_ION_TOKEN または cesium.local.json を設定してください。')
    # Verify access after confirmation, without exposing token-bearing endpoint data.
    for asset in (config.get('terrain_asset_id',1),config.get('imagery_asset_id',2)):
        if not isinstance(asset,int) or isinstance(asset,bool) or asset<1:
            raise PlaceError('Invalid Cesium asset ID')
        request_json(f'https://api.cesium.com/v1/assets/{asset}/endpoint',headers={'Authorization':'Bearer '+token})
    print('水域と湖面標高を準備しています…',flush=True)
    scene_path,scene=build_scene(place,radius_km=args.radius_km,water_level=args.water_level)
    print(f"水面: 海抜 {scene['water_height_msl_m']:.2f} m / Cesium 高さ {scene['water_height_ellipsoid_m']:.2f} m")
    print('水面標高の出典: '+scene['elevation_source'])
    print('地形を表示する場所: '+str(scene_path))
    if args.prepare_only:
        return 0
    editor=Path(args.engine_root)/'Engine/Binaries/Win64/UnrealEditor.exe'
    project=ROOT/'unreal/ArriettyCesium/ArriettyCesium.uproject'
    content=project.parent/'Content/Row/Maps/CesiumRow.umap'
    if not editor.is_file() or not content.is_file() or not (project.parent/'Binaries/Win64/UnrealEditor-ArriettyRow.dll').is_file():
        raise PlaceError('UE またはプロジェクトの準備が未完了です。tools/prepare.ps1 を実行してください。')
    logs=ROOT/'logs';logs.mkdir(exist_ok=True)
    log=logs/'training.log'
    # FileShare.None matches the original normal-run log protection on Windows.
    import ctypes
    from ctypes import wintypes
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
    kernel.CreateFileW.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    handle=kernel.CreateFileW(str(log),0x40000000,0,None,2,0x80,None)
    if handle==ctypes.c_void_p(-1).value:
        raise PlaceError('training.log が使用中です。実行中のゲームを閉じてください。')
    kernel.CloseHandle(handle)
    command=[str(editor),str(project),'/Game/Row/Maps/CesiumRow','-game',
             '-RowPlace='+str(scene_path),'-RowSettings='+str(ROOT/'settings.local.json'),
             '-RowCesiumConfig='+str(cfg_path),'-RowVolume='+str(args.volume),
             '-abslog='+str(log),'-nosplash','-windowed','-ResX=1600','-ResY=900',
             '-ExecCmds=t.MaxFPS 90,t.IdleWhenNotForeground 0']
    command+=['-RowDemo','-nohmd'] if args.demo else ['-vr']
    # Only the inherited environment carries a token override, never argv or scene JSON.
    env=os.environ.copy();env['CESIUM_ION_TOKEN']=token
    env.pop('OPENAI_API_KEY',None)
    print('Cesium の地形を読み込んでいます。準備が終わったら Enter で開始できます。',flush=True)
    return subprocess.call(command,env=env)


if __name__=='__main__':
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print('\n中止しました。',file=sys.stderr)
        raise SystemExit(130)
    except (PlaceError,OSError,ValueError,KeyError) as exc:
        # PlaceError strings are deliberately sanitized. Other errors must not leak API data.
        message=str(exc) if isinstance(exc,PlaceError) else 'ローカル設定またはデータを読み込めませんでした。'
        print('エラー: '+message,file=sys.stderr)
        raise SystemExit(1)
