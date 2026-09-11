"""Sanitized network access, input validation and vertical datum conversion."""
import json
import math
import urllib.error
import urllib.parse
import urllib.request
from functools import lru_cache
from . import WORKSPACE


class PlaceError(RuntimeError):
    pass


def request_json(url, *, body=None, headers=None, timeout=60):
    headers = {'User-Agent': 'ArriettyCesium/0.2 (personal fitness simulator)', **(headers or {})}
    if isinstance(body, dict):
        body = json.dumps(body).encode('utf-8')
        headers['Content-Type'] = 'application/json'
    try:
        with urllib.request.urlopen(urllib.request.Request(url, data=body, headers=headers), timeout=timeout) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        raise PlaceError(f'{urllib.parse.urlsplit(url).hostname}: HTTP {exc.code}') from None
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
        raise PlaceError(f'{urllib.parse.urlsplit(url).hostname}: connection or response failed') from None


def number(value, lo, hi, label):
    if type(value) not in (int, float) or not math.isfinite(value) or not lo <= value <= hi:
        raise PlaceError(f'Invalid {label}')
    return float(value)


def safe_text(value, label):
    if not isinstance(value, str) or not value.strip() or len(value) > 300 or any(ord(c)<32 or ord(c)==127 for c in value):
        raise PlaceError(f'Invalid {label}')
    return value.strip()


def confirmed(answer):
    return isinstance(answer, str) and answer.strip().casefold() == 'y'


@lru_cache(maxsize=1)
def geoid_transformer():
    from pyproj import Transformer, datadir
    grid = WORKSPACE/'ThirdParty/Geoid/us_nga_egm96_15.tif'
    if not grid.is_file():
        raise PlaceError('標高補正データがありません。tools/prepare.ps1 を実行してください。')
    datadir.append_data_dir(str(grid.parent))
    return Transformer.from_crs('EPSG:9707', 'EPSG:4979', always_xy=True, allow_ballpark=False, only_best=True)


def geoid_offset(lon, lat):
    return geoid_transformer().transform(lon, lat, 0, errcheck=True)[2]


def cesium_configuration():
    import os
    path = WORKSPACE/'config/cesium.local.json'
    cfg = json.loads(path.read_text(encoding='utf-8-sig')) if path.exists() else {}
    token = os.environ.get('CESIUM_ION_TOKEN', '').strip() or cfg.get('ion_access_token', '')
    if not token:
        raise PlaceError('config/cesium.local.json または CESIUM_ION_TOKEN を設定してください。')
    for key, default in [('terrain_asset_id', 1), ('imagery_asset_id', 2)]:
        value = cfg.get(key, default)
        if type(value) is not int or value < 1:
            raise PlaceError('Invalid Cesium asset ID')
        cfg[key] = value
    return cfg, token
