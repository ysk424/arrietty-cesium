# Arrietty Cesium integration contract

Row baseline: arrietty-cesium 3b9fb96. Fly baseline: Arrietty-UE58 ef86436
(accepted runtime b3b0cc1; later documentation changes). Sibling repositories
are read-only sources and are not required at runtime or rebuild time.

## Separation

`row.ps1` preserves the former `run.ps1` options. Row implementation, shaders,
sounds and UE project live in apps/row. Fly keeps its own Python simulation and
UE project in apps/fly. UE target/module names are deliberately retained.
Each app has its own .venv, UE Content/Binaries/Intermediate/Saved and logs.
The root config directory contains independent private device configurations.
The live-session Windows mutex prevents row and fly taking the HMD/devices
at the same time. Demo/offline launches do not acquire hardware.

Shared code supplies sanitized HTTP, explicit-Y confirmation, numerical input
validation, EGM96 datum conversion, Cesium terrain/imagery creation, and bearing
conversion. Water-area navigation, rowing physics and flight physics remain
application specific. Plugins are pinned by shared/cesium.lock.json; each UE
project extracts its own plugin build tree from the same verified archive.

## Height and coordinate contract

- Row: the launch mean water surface is local Z=0; its existing curved water
  model and lake MSL evidence requirements remain unchanged.
  Lake launch selection prefers 150 m shoreline clearance, falling back to the
  original 40 m only when the lake has no 150 m interior. Five successful open-water
  terrain probes still gate readiness. If all exceed known water by >3 cm, their
  spread is <=25 cm and the largest excess is <=50 m, Row translates its terrain
  tileset (rendering and collision together) down by that excess. This bounded
  display alignment handles flat elevated lake sheets; it does not establish a
  measured lake elevation or alter MSL, EGM96, georeference, curved water or HMD.
  It is logged as ROW_LAKE_TERRAIN_ALIGNMENT. Terrain above water outside those
  bounds, failed samples and unknown lake levels still block. Oceans receive no
  such alignment. This Row-only display offset is not a Fly terrain/AGL datum.
- Fly: launch ground height h0 is sampled from Cesium in WGS84 ellipsoid metres.
  The chosen ground point becomes the fixed CesiumGeoreference origin.
- Flight altitude_meters and the transport pose Z are h - h0. Negative values
  are valid in a valley. Aircraft height is integrated independently of terrain.
- At east e/north n, use the launch ECEF tangent basis to find longitude/latitude,
  then set the ellipsoid height to h0 + altitude. Cesium converts to UE including
  curvature. Python uses the same tangent basis for geographic telemetry.
- Transport X is north cm and Y is east cm, retained for the tested simulation.
  Its yaw is geographic bearing. The UE adapter maps to Cesium east/south/up
  (yaw=bearing-90) and the local curved tangent orientation.
- Display MSL = ellipsoid height minus EGM96 offset at the current coordinate.
  AGL = aircraft ellipsoid height minus sampled/interpolated terrain height.
  HMD floor-relative eye height remains a child transform, never part of h0.
- Existing CSV altitude_m remains launch-relative. Additional columns explicitly
  name MSL, AGL, ellipsoid height, coordinates and origin reference.

Ground start selects a candidate within roughly 1km, checks OSM water exclusions
and nine terrain probes out to 50m, and accepts <=6% centre-to-probe grade.
Air start uses terrain + requested clearance, 24km/h and a 3-degree initial pitch.
No terrain is flattened. In ground mode the vehicle follows terrain. In flight
it does not follow terrain automatically. Contact resolves only against current
terrain, never an absolute altitude=0 plane.

`-Magnification` / `-mag` (1..10, default 1) scales each translation delta after
the normal physics step: east, north and airborne vertical displacement. It
does not scale initial AGL, absolute altitude, attitude, time, aerodynamic speed,
rider watts or hardware commands. Ground motion follows real terrain at its new
horizontal position. Turning rate is retained, so world turn radius increases.
Collision uses the scaled path and world descent speed; a high-impact approach
is not treated as a gentle landing. Recovery remains about 2 world metres.
AIR/speed_kmh retain physical model speed; WORLD/world_speed_kmh and DIST/distance_m
describe geographic horizontal motion. V/S/world_vertical_speed_mps describe
the accepted world vertical motion, including zero when movement is blocked.

## Terrain availability and motion

UE queries a 13x13 grid at 10m spacing using SampleHeightMostDetailed, with a new
patch after moving 20m. The async query does not block the render thread. The
multiplied mode uses a 25x25 grid at the same 10m spacing, a bounded half-second
lookahead (up to 50m), and a minimum query interval of 0.2s. The current/recovery
positions remain inside that patch. The loaded-mesh sweep follows world motion
and expands to at least 0.1s of travel. Unknown patches still block movement.
The initial scene waits for verified heights and settled visible terrain. Later
motion needs complete patch coverage; missing data freezes motion until covered.
Height-field crossings are checked at <=1m intervals along each proposed step.
A visibility-channel sphere sweep ahead of the rider also detects loaded mesh
obstacles between samples. Frustum culling is disabled on the fly tileset to keep
nearby collision geometry when the rider turns their head.

A slope above 12%, water contact, or obstacle blocks movement. Gentle landings
require descent <=2.5m/s and bank <12 degrees. A blocked step restores the previous
simulation pose and distance. Recovery searches the accepted route about 2m back,
restores altitude/attitude/flight state, and revalidates that point against terrain.
No valid recovery point requires Esc to restart. The local flight radius is
1-10km, default 10km. There is no global free-roaming/rebasing mode.

World Terrain/OSM resolution and 10m interpolation limit ground accuracy; fine
roads, buildings, trees and bathymetry are not guaranteed. Water is excluded from
landing using mapped OSM polygons/coastlines. AGL describes the terrain dataset,
not a surveyed water level. Fly uses Cesium water imagery, without row's custom
wave shader. Row's independent high-lake water system is retained.

## Controls and time

Row optionally selects `bar_input=wt9011dcl` in its private settings. A separate
native GATT worker reads WIT FFE5/FFE4 (vendor UUID suffix 5f9a34fb), with the
advertised or previously verified address type. It subscribes only; no control
or sensor-configuration commands are sent. Acceleration is rotated by sensor
attitude, gravity is measured during setup, and two initial cycles fit the
horizontal pull axis. This AHRS frame is independent of SteamVR. Consequently
IMU setup explicitly uses averaged forward-facing HMD yaw for the fixed room
steering frame; the original Tracker trajectory fit remains in Tracker mode.
The bounded velocity estimate gates the existing rowing model, never creates
a fictitious absolute bar pose, and is recorded as `imu_velocity`. HMD lateral
steering, controls, water and audio stay intact. Stale IMU >=.25 s stops the ride
without HMD handle assistance or auto-restart. See ROW_IMU.ja.md for limitations.

Python still owns flight dynamics and hardware workers. UE still owns OpenXR,
camera alignment and UI. P enables device preparation only after geography is
ready. Button 1 latches the actual view's horizontal forward; R realigns without
moving or resetting elapsed time. HMD/handle invalidity stops motion and airflow.
The source watchdog and Esc/window-close cleanup remain in place.

Local time uses timezonefinder and IANA tzdata, including summer time. Ambiguous
or nonexistent DST clock times are rejected. NOAA solar mathematics was copied
from the MIT Secret-World solar module; Blender/editor code was omitted. No
Secret-World import or sibling path is used. Time is frozen after Apply.
The accepted aerodynamic constants, including fixed air density, are unchanged.
