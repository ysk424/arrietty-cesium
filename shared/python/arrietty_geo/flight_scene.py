"""Flight destinations, OSM water exclusions and launch candidates."""
import hashlib
import json
import math
import os
import time
import urllib.parse
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
from . import WORKSPACE
from .common import PlaceError, request_json, safe_text, number, geoid_offset


def cached(namespace, key, fetch, refresh=False):
    path=WORKSPACE/'cache/places'/namespace/(hashlib.sha256(key.encode()).hexdigest()+'.json')
    if path.exists() and not refresh and time.time()-path.stat().st_mtime<30*86400:
        return json.loads(path.read_text(encoding='utf-8'))
    result=fetch()
    path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_suffix('.tmp')
    temp.write_text(json.dumps(result,ensure_ascii=False),encoding='utf-8');temp.replace(path)
    return result


def resolve(query, model, refresh=False):
    query=safe_text(query,'place')
    def fetch():
        key=os.environ.get('OPENAI_API_KEY','').strip()
        if not key: raise PlaceError('OPENAI_API_KEY を設定してください。')
        properties={k:{'type':'string'} for k in ('name','name_ja','country_ja','region_ja')}
        properties.update(status={'type':'string','enum':['found','not_found']},
                          surface={'type':'string','enum':['land','sea','lake']},
                          latitude={'type':'number'},longitude={'type':'number'})
        response=request_json('https://api.openai.com/v1/responses',headers={'Authorization':'Bearer '+key},body={
            'model':model,'tools':[{'type':'web_search'}],
            'instructions':'Identify the user destination for a Cesium flight simulator. Search the web to disambiguate it. Mountains, plains, cities, islands, lakes and seas are supported. Return the actual destination coordinates, NOT an invented runway. Prefer the best known match, use country/province to disambiguate, and set not_found if uncertain. Islands mean land. Classify the point as land, sea or lake. Return Japanese *_ja labels, province in region_ja. Never supply terrain elevations; the renderer samples them.',
            'input':query,'text':{'format':{'type':'json_schema','name':'flight_place','strict':True,
              'schema':{'type':'object','properties':properties,'required':list(properties),'additionalProperties':False}}},
            'max_output_tokens':3000})
        if response.get('status')!='completed': raise PlaceError('場所の検索が完了しませんでした。')
        output=''.join(c.get('text','') for o in response.get('output',[]) if o.get('type')=='message' for c in o.get('content',[]) if c.get('type')=='output_text')
        try: return validate_place(json.loads(output))
        except (ValueError,TypeError): raise PlaceError('有効な場所情報を取得できませんでした。') from None
    return validate_place(cached('fly',model+'\n'+query,fetch,refresh))


def validate_place(place):
    if not isinstance(place,dict) or place.get('status')!='found': raise PlaceError('場所を特定できませんでした。国や地域を加えてください。')
    for key in ('name','name_ja','country_ja','region_ja'): place[key]=safe_text(place.get(key),key)
    number(place.get('latitude'),-85,85,'latitude');number(place.get('longitude'),-180,180,'longitude')
    if place.get('surface') not in ('land','sea','lake'): raise PlaceError('Invalid surface type')
    return place


def local_instant(date, clock, zone):
    tz=ZoneInfo(zone)
    date=date or datetime.now(tz).date().isoformat()
    try: naive=datetime.fromisoformat(date+'T'+clock)
    except ValueError: raise PlaceError('日時は YYYY-MM-DD / HH:MM で指定してください。') from None
    if naive.tzinfo is not None or not 1901<=naive.year<=2099: raise PlaceError('Invalid local date/time')
    instant=naive.replace(tzinfo=tz)
    # Reject skipped DST times and ambiguous clock times rather than picking silently.
    from datetime import timezone
    if instant.astimezone(timezone.utc).astimezone(tz).replace(tzinfo=None)!=naive:
        raise PlaceError('この現地時刻は夏時間の切替で存在しません。')
    if instant.utcoffset()!=naive.replace(tzinfo=tz,fold=1).utcoffset():
        raise PlaceError('夏時間の切替で重複する時刻です。別の時刻を指定してください。')
    return instant


def surface_geometry(place, radius):
    from pyproj import Transformer, CRS
    from shapely.geometry import Polygon, LineString, box, mapping
    from shapely.ops import unary_union, polygonize, transform
    lat,lon=place['latitude'],place['longitude']
    projection=CRS.from_proj4(f'+proj=aeqd +lat_0={lat} +lon_0={lon} +datum=WGS84 +units=m')
    fwd=Transformer.from_crs(4326,projection,always_xy=True).transform
    inv=Transformer.from_crs(projection,4326,always_xy=True).transform
    extent=radius+2000
    dy=extent/110000;dx=dy/max(.08,math.cos(math.radians(lat)))
    if lon-dx < -180 or lon+dx > 180: raise PlaceError('日付変更線をまたぐ飛行範囲にはまだ対応していません。')
    bbox=f'{lat-dy},{lon-dx},{lat+dy},{lon+dx}'
    query=f'[out:json][timeout:45];(way["natural"="coastline"]({bbox});nwr["natural"="water"]({bbox});nwr["waterway"="riverbank"]({bbox});way["aeroway"="runway"]({bbox}););out geom;'
    def fetch():
        result=request_json('https://overpass-api.de/api/interpreter',body=urllib.parse.urlencode({'data':query}).encode(),headers={'Content-Type':'application/x-www-form-urlencoded'},timeout=60)
        if 'remark' in result: raise PlaceError('水域境界の取得が完了しませんでした。再実行してください。')
        return result
    data=cached('fly-surfaces',query,fetch)
    water=[];coasts=[];candidates=[]
    def coordinates(item): return [(p['lon'],p['lat']) for p in item.get('geometry',[]) if 'lon' in p and 'lat' in p]
    for item in data.get('elements',[]):
        tags=item.get('tags',{});coords=coordinates(item)
        if tags.get('natural')=='coastline' and len(coords)>1:
            coasts.append(transform(fwd,LineString(coords)));continue
        if tags.get('aeroway')=='runway' and len(coords)>1:
            line=transform(fwd,LineString(coords))
            for fraction in (.25,.5,.75):
                p=line.interpolate(fraction,normalized=True)
                if p.distance(__import__('shapely').geometry.Point(0,0))<=1000: candidates.append([p.x,p.y])
            continue
        if item.get('type')=='relation':
            outer=[];inner=[]
            for member in item.get('members',[]):
                points=coordinates(member)
                if len(points)>1: (inner if member.get('role')=='inner' else outer).append(LineString(points))
            if outer:
                shape=unary_union(list(polygonize(unary_union(outer))))
                if inner: shape=shape.difference(unary_union(list(polygonize(unary_union(inner)))))
                if not shape.is_empty: water.append(transform(fwd,shape))
        elif len(coords)>3 and coords[0]==coords[-1]:
            shape=Polygon(coords)
            if shape.is_valid: water.append(transform(fwd,shape))
    domain=box(-extent,-extent,extent,extent)
    if coasts:
        parts=polygonize(unary_union([domain.boundary]+[c.intersection(domain) for c in coasts]))
        for part in parts:
            p=part.representative_point()
            if not domain.covers(p): continue
            line=min(coasts,key=lambda c:c.distance(p));d=line.project(p)
            a=line.interpolate(max(0,d-.1));b=line.interpolate(min(line.length,d+.1))
            if (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x)<0: water.append(part)
    elif place['surface']=='sea': water.append(domain)
    if place['surface']=='lake' and not water:
        raise PlaceError('湖の水域を確認できませんでした。別の場所または再試行を指定してください。')
    water_shape=unary_union(water).intersection(domain) if water else Polygon()
    return mapping(transform(inv,water_shape)),candidates


def build_scene(place, *, radius_km=10, start_mode='ground', start_agl=100, local_date='', local_time='12:00', magnification=1):
    from timezonefinder import TimezoneFinder
    from .solar import position
    number(radius_km,1,10,'radius');number(start_agl,10,3000,'start AGL')
    number(magnification,1,10,'magnification')
    if start_mode not in ('ground','air'): raise PlaceError('Invalid start mode')
    zone=TimezoneFinder().timezone_at(lng=place['longitude'],lat=place['latitude'])
    if zone is None: raise PlaceError('現地のタイムゾーンを特定できませんでした。')
    instant=local_instant(local_date,local_time,zone)
    solar=position(instant,place['latitude'],place['longitude'])
    water,candidates=surface_geometry(place,radius_km*1000)
    candidates=[[0.,0.]]+candidates+[[float(e),float(n)] for d in (100,300,600) for e in (-d,0,d) for n in (-d,0,d) if e or n]
    # Reject launch footprints crossing mapped water before sampling their grade.
    from shapely.geometry import shape, Point
    from pyproj import Transformer, CRS
    water_shape=shape(water)
    local=CRS.from_proj4(f'+proj=aeqd +lat_0={place["latitude"]} +lon_0={place["longitude"]} +datum=WGS84 +units=m')
    inv=Transformer.from_crs(local,4326,always_xy=True)
    checks=((0,0),(50,0),(-50,0),(0,50),(0,-50),(25,25),(25,-25),(-25,25),(-25,-25))
    candidates=[p for p in candidates if not any(water_shape.covers(Point(*inv.transform(p[0]+e,p[1]+n))) for e,n in checks)]
    scene=dict(schema_version=2,kind='fly',name=place['name'],name_ja=place['name_ja'],
        origin_longitude=place['longitude'],origin_latitude=place['latitude'],
        geoid_offset_m=geoid_offset(place['longitude'],place['latitude']),
        navigation_radius_m=radius_km*1000,start_mode=start_mode,start_agl_m=start_agl,movement_magnification=magnification,
        launch_candidates=candidates,water_geojson=water,timezone=zone,local_time=solar['local'],
        sun_azimuth=solar['azimuth_degrees'],sun_elevation=solar['elevation_degrees'],
        initial_heading_degrees=180.,output_name='Cesium World Terrain',terrain_required=True)
    folder=WORKSPACE/'cache/fly/scenes'/hashlib.sha256(json.dumps(scene,sort_keys=True).encode()).hexdigest()[:20]
    folder.mkdir(parents=True,exist_ok=True)
    path=folder/'scene.json';path.write_text(json.dumps(scene,ensure_ascii=False,indent=2),encoding='utf-8')
    return path,scene
