"""Flight entry point: destination -> Y -> scene -> isolated UE/bridge session."""
import argparse
import json
import math
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import time

APP=Path(__file__).resolve().parents[1]
ROOT=APP.parents[1]
sys.path.insert(0,str(ROOT/'shared/python'))
from arrietty_geo.common import PlaceError, confirmed, cesium_configuration, request_json
from arrietty_geo.flight_scene import resolve, build_scene


def launch_scene(path, scene, *, engine_root, offline=False, smoke=False, headless=False, prepare_only=False, volume=.8):
    if not math.isfinite(volume) or not 0<=volume<=1: raise PlaceError('Volume must be between 0 and 1')
    if (smoke or headless) and not (offline or prepare_only): raise PlaceError('Headless/Smoke requires Offline')
    hardware=not (offline or prepare_only)
    editor=Path(engine_root)/'Engine/Binaries/Win64/UnrealEditor.exe'
    project=APP/'unreal/ArriettyUE/ArriettyUE.uproject'
    if not editor.is_file() or not (project.parent/'Content/Maps/CesiumFly.umap').is_file():
        raise PlaceError('tools/prepare.ps1 -App Fly を実行してください。')
    cfg,token=cesium_configuration()
    # UE owns the active OpenXR runtime (Meta Link, SteamVR, etc.). The wired
    # panel and trainer do not require a SteamVR process.
    # Keep the accepted exclusive bridge port; never connect to someone else's session.
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as probe:
        probe.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1)
        try: probe.bind(('127.0.0.1',19858))
        except OSError: raise PlaceError('飛行ブリッジが起動中です。先に終了してください。') from None
    runtime=APP/'.runtime/ue';runtime.mkdir(parents=True,exist_ok=True)
    logs=ROOT/'logs/fly';logs.mkdir(parents=True,exist_ok=True)
    prefix='smoke' if smoke else 'prepare' if prepare_only else 'offline' if offline else 'training'
    session=runtime/'session.json'
    session.write_text(json.dumps(dict(token=secrets.token_hex(32),port=19858,hardware=hardware)),encoding='utf-8')
    config=runtime/'cesium.json'
    # Token remains in inherited environment only. Scene/session logs never contain it.
    config.write_text(json.dumps({k:cfg[k] for k in ('terrain_asset_id','imagery_asset_id')}),encoding='utf-8')
    env=os.environ.copy()
    env.update(ARRIETTY_UE_SESSION=str(session),ARRIETTY_UE_SOLAR=str(path),ARRIETTY_CESIUM_CONFIG=str(config),CESIUM_ION_TOKEN=token)
    env.pop('OPENAI_API_KEY',None)
    bridge_env=env.copy();bridge_env.pop('CESIUM_ION_TOKEN',None)
    log=logs/(prefix+'.log')
    bridge_args=[sys.executable,'-u',str(APP/'tools/ue_bridge.py'),'--world',str(path),'--session',str(session),'--log-path',str(logs/('latest-ue-flight.csv' if hardware else 'latest-ue-offline.csv'))]
    if hardware: bridge_args.append('--hardware')
    command=[str(editor),str(project),'/Game/Maps/CesiumFly','-game','-nosplash','-nop4','-windowed','-ResX=1600','-ResY=900','-abslog='+str(log),'-ExecCmds=t.MaxFPS 90,t.IdleWhenNotForeground 0']
    # OpenXR PreInit creates a runtime instance even with -nohmd on Windows.
    command+=['-vr'] if hardware else ['-nohmd','-DisablePlugins=OpenXR']
    command+=['-FlyVolume='+str(volume),'-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0']
    if smoke: command+=['-ArriettySmoke','-unattended']
    if headless: command+=['-RenderOffscreen','-nosound','-ForceRes']
    if prepare_only: command+=['-FlyPrepareOnly','-unattended']
    bridge=None;app=None
    try:
        with (logs/(prefix+'-bridge.log')).open('w',encoding='utf-8') as out, (logs/(prefix+'-bridge.err.log')).open('w',encoding='utf-8') as err:
            bridge=subprocess.Popen(bridge_args,cwd=APP,env=bridge_env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
            deadline=time.monotonic()+15
            while time.monotonic()<deadline:
                if bridge.poll() is not None: raise PlaceError('飛行ブリッジを起動できませんでした。logs/fly を確認してください。')
                if 'ARRIETTY_UE_BRIDGE_READY' in (logs/(prefix+'-bridge.log')).read_text(encoding='utf-8'): break
                time.sleep(.1)
            else: raise PlaceError('飛行ブリッジの起動がタイムアウトしました。')
            print('Cesium の標高と周辺地形を読み込んでいます。機器準備は自動で進みます。正面を見て Button 1 で開始します。',flush=True)
            app=subprocess.Popen(command,cwd=APP,env=env)
            try: result=app.wait(timeout=300 if smoke or prepare_only else None)
            except subprocess.TimeoutExpired: raise PlaceError('地形の準備またはテストがタイムアウトしました。') from None
            if result: raise PlaceError('Unreal の実行に失敗しました。logs/fly を確認してください。')
            output=log.read_text(encoding='utf-8',errors='replace')
            if smoke and 'ARRIETTY_UE_SMOKE_DONE' not in output: raise PlaceError('飛行テストが完了しませんでした。logs/fly/smoke.log を確認してください。')
            if prepare_only and 'FLY_GEOGRAPHY_READY' not in output: raise PlaceError('開始地点の地形を確認できませんでした。山岳では -StartMode Air を指定してください。')
    finally:
        if app is not None and app.poll() is None:
            app.terminate();app.wait(timeout=15)
        if bridge is not None and bridge.poll() is None:
            try: bridge.wait(timeout=35)
            except subprocess.TimeoutExpired: bridge.terminate();bridge.wait(timeout=5)
    return 0


def main():
    p=argparse.ArgumentParser()
    p.add_argument('place');p.add_argument('--offline',action='store_true');p.add_argument('--smoke',action='store_true');p.add_argument('--headless',action='store_true')
    p.add_argument('--resolve-only',action='store_true');p.add_argument('--prepare-only',action='store_true');p.add_argument('--refresh-place',action='store_true')
    p.add_argument('--start-mode',choices=['ground','air'],default='ground');p.add_argument('--start-agl',type=float,default=100);p.add_argument('--radius-km',type=float,default=10)
    p.add_argument('--magnification',type=float,default=1)
    p.add_argument('--volume',type=float,default=.8)
    p.add_argument('--date',default='');p.add_argument('--time',default='12:00');p.add_argument('--model',default=os.environ.get('ARRIETTY_OPENAI_MODEL','gpt-5.4-mini'))
    p.add_argument('--engine-root',default='C:/Program Files/Epic Games/UE_5.8')
    args=p.parse_args()
    place=resolve(args.place,args.model,args.refresh_place)
    label=f"{place['country_ja']}・{place['region_ja']}の{place['name_ja']}"
    if args.resolve_only: print(label);return 0
    try: answer=input(label+'ですね？ [Y/N]: ')
    except EOFError: answer=''
    if not confirmed(answer): print('起動を中止しました。');return 0
    cfg,token=cesium_configuration()
    for asset in (cfg['terrain_asset_id'],cfg['imagery_asset_id']):
        request_json(f'https://api.cesium.com/v1/assets/{asset}/endpoint',headers={'Authorization':'Bearer '+token})
    print('水域境界と発進地点を準備しています…',flush=True)
    path,scene=build_scene(place,radius_km=args.radius_km,start_mode=args.start_mode,start_agl=args.start_agl,local_date=args.date,local_time=args.time,magnification=args.magnification)
    print(f"{scene['timezone']} / {scene['local_time']} / 半径 {args.radius_km:g} km",flush=True)
    print(f'移動倍率: {args.magnification:g}倍（旋回・飛行計算・初期対地高度は等倍）',flush=True)
    print('地表の高さは Cesium で確認します。'+('対地高度 '+str(args.start_agl)+' m から発進します。' if args.start_mode=='air' else '周辺の平坦な陸地を探します。'),flush=True)
    from arrietty_geo.live import live_session
    with live_session(not (args.offline or args.prepare_only)):
        return launch_scene(path,scene,engine_root=args.engine_root,offline=args.offline,smoke=args.smoke,headless=args.headless,prepare_only=args.prepare_only,volume=args.volume)


if __name__=='__main__':
    try: raise SystemExit(main())
    except KeyboardInterrupt: print('\n中止しました。',file=sys.stderr);raise SystemExit(130)
    except (PlaceError,OSError,ValueError,KeyError) as exc:
        print('エラー: '+(str(exc) if isinstance(exc,PlaceError) else '設定または地理データを読み込めませんでした。'),file=sys.stderr)
        raise SystemExit(1)
