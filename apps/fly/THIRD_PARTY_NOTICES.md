# Third-party notices

The flight application retains these Arrietty-UE58 Python wheels for Windows BLE and
SteamVR controller-pose support. Each wheel retains its package metadata,
license file, and original license terms.

- openvr 2.12.1401 — BSD License — https://github.com/cmbruns/pyopenvr
- Bleak 3.0.2 — MIT License — https://github.com/hbldh/bleak
- PyWinRT runtime and Windows projections 3.2.1 — MIT License — https://github.com/pywinrt/pywinrt
- typing_extensions 4.16.0 — PSF-2.0 License — https://github.com/python/typing_extensions

These components are independent libraries and are not relicensed under the
Arrietty-UE58 MIT License.

The `arrietty_up` package and its regression tests originate from
[Arrietty-UP](https://github.com/ysk424/Arrietty-UP), commit
`b1a82dfc3624ec3dbb71bec088289b6fcec1a03c`, under the MIT License,
copyright (c) 2026 ysk424. Its license is retained as this repository's LICENSE.

Unreal Engine and its engine content are installed separately and are not
redistributed in this repository. The generated materials reference local
engine assets and remain excluded from Git.

The former Secret World exported scenery is not part of this application.
The current scene uses Cesium terrain/imagery and OpenStreetMap water boundaries.
Runtime Cesium credits and OSM attribution are retained. The shared solar helper
adapts MIT Secret-World solar mathematics only; Blender code/assets and the former
Allen Coral Atlas derivative are not used. See the repository's
[current third-party notices](../../THIRD_PARTY_NOTICES.md).
