# Third-party notices

- Application: MIT, Copyright (c) 2026 ysk424. Derived from Arrietty-row
  revision 316f4b2. The internal ArriettyRow UE module is retained to reuse the
  accepted rowing implementation.
- Unreal Engine: separately licensed by Epic Games; excluded from Git.
- Cesium for Unreal 2.29.1: Apache-2.0, Cesium GS, Inc. and contributors.
  Downloaded locally with its bundled third-party licenses. See
  https://github.com/CesiumGS/cesium-unreal and the local plugin LICENSE/NOTICE.
- Cesium ion terrain and satellite imagery: independently licensed data,
  accessed with the user's existing token. Runtime Cesium attribution is kept.
  https://cesium.com/legal/terms-of-service/
- OpenStreetMap contributors: ODbL. Boundary/coast data are cached locally and
  remain separate from MIT application source. https://www.openstreetmap.org/copyright
- EGM96 15-minute geoid grid: US National Geospatial-Intelligence Agency data
  distributed by OSGeo/PROJ. See
  https://github.com/OSGeo/PROJ-data/blob/master/us_nga/us_nga_README.txt
- pyproj: MIT, with PROJ's own notices. Shapely and NumPy: BSD-3-Clause.
  Python packages and their notices remain in the local virtual environment.
- OpenVR SDK: Valve Corporation, BSD-3-Clause. Pinned files are SHA-256 checked;
  SDK and license are staged locally. https://github.com/ValveSoftware/openvr
- Five Adobe Firefly sound masters were provided by the rider. WAVs and generated
  UE audio stay local and do not inherit the application MIT license.
  See apps/row/sounds/README.md.
- Instrument exposure setup adapts MIT Arrietty-UE58, same copyright.
- Optional inherited device diagnostics use Bleak (MIT) and pyopenvr (MIT).

No old Lake Bled scenery, Epic engine code/binaries, downloaded Cesium plugin,
device identifiers, API credentials or exercise records are committed. The
pinned flight dependency wheels are the explicitly retained binary dependencies.


## Flight integration

Flight application code was copied from Arrietty-UE58 ef86436 (MIT). The retained
BLE/OpenVR/WinRT wheels and their license metadata are in apps/fly/wheels; their
exact hashes are recorded in apps/fly/wheels.lock.json. Additional geography
dependencies are pyproj, Shapely, NumPy, timezonefinder and tzdata, with their
license files installed in the isolated environment. See also
[flight notices](apps/fly/THIRD_PARTY_NOTICES.md).

The deterministic NOAA-based solar position function was adapted from the MIT
Secret-World solar.py module, version 0.1.0. Its Blender-specific editor code and
scenery are not used. Geographic timezones use timezonefinder and IANA tzdata.
Both applications use the Cesium dynamic attribution system; OSM data is credited
to OpenStreetMap contributors under ODbL.
