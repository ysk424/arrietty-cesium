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

Optional locally purchased Waterline Gen 4 content supplies GPU waves/materials
through RowWater. ROW retains the geographic mesh, curved mean datum, water/hull
masks and speed/drive wakes. Vertical-only vendor displacement is added to the
curved mean. Vendor buoyancy, drag, underwater and shallow-water simulation are
disabled; no vendor camera/input owns the ride. One vendor actor produces shared
wave textures; per-eye rendering remains UE's responsibility. OpenXR stereo
acceptance is still pending. Root `-LegacyWater` / Python `--legacy-water` /
UE `-RowLegacyWater` select original water. Test fixtures use original water so
control regression tests never require proprietary content. See ROW_WATERLINE.ja.md.

Row supports `-Magnification` / `-mag` (1..10, default 1, decimals allowed),
forwarded as `--magnification` (`--mag` also accepted by Python) and
`-RowMagnification` to UE. The rowing model scales only the final horizontal
travel after its existing physics and steering step. Speed, power, drive,
strokes, elapsed time, calibration and yaw rate are unchanged. Water/audio
receive the original physical speed and drive; curved mean water determines
the new position's Z. Stop/Home preserves the launch option. Model distance,
the speed panel and `world_speed_kmh` describe magnified geographic motion;
CSV `speed_kmh` retains the physical model speed and `movement_magnification`
records the option. Turn radius grows with magnification. Water polygon edges
and holes are checked against the whole proposed path plus the existing 2.5m
clearance; mesh sweeps cover the scaled step in <=0.5m pieces following curved
water. A blocked step rolls back its distance and pauses. Radius is unchanged.

Row's load gain accepts fresh integer dial values 1..16, independently of
movement magnification. Fresh zero machine watts remain zero, including at
maximum load. Unknown, invalid or >=3s stale load yields unknown LOAD and unity
power gain. CSV appends `resistance_raw` and `resistance_age_s` for diagnosis;
they never authorize a device write or revive invalid/stale data.

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

Row's optional `bar_input=ps4` selects an explicit private `ps4_hid_path`.
Windows HID reads CRC-validated Bluetooth 0x11 reports, with a neutral
HidD_SetOutputReport request enabling sensors (no effects). A worker integrates
gyro and complementary gravity correction for the arbitrary mounted angle;
PS4-only bias washout/velocity decay feeds the existing IMU rowing model.
Square is Enter, triangle is keypad 0; repeats are suppressed and stop wins.
L1/R1 select +/-0.5 steer, L2/R2 +/-1; same-side strong wins, opposing sides
cancel, release removes yaw immediately. HMD lean/gaze do not steer PS4 mode.
Quest and VIVE use these same controls. PS4/WIT HMD poses use UE/OpenXR raw
tracking space; only legacy Tracker mode starts the OpenVR pose worker.
The earlier HMD-lean and manual-restart contract remains for WIT/Tracker mode.

The user explicitly requested automatic PS4 reconnect recovery. PS4 staleness
>=0.25 s pauses motion/audio. A previously running ride resumes when fresh
controller/IMU and HMD tracking return, retaining position, time, distance and
calibration and resetting estimated velocity. Square pause and triangle Home
cancel automatic recovery; geography remains the outer gate. HMD-only loss,
frame faults, terrain pauses and interrupted calibration do not auto-resume.
Do not integrate a disconnected interval or replay queued start events.
Sensor yaw during a gap is not observable; retaining the frame and releveling
gravity is an estimate requiring mounted live evaluation. See ROW_PS4.ja.md.

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
camera alignment and UI. Initial device preparation starts automatically only
after geography is ready, a fresh authenticated bridge reply is received and
any edited date/time is applied. No P key is required for either Quest or VIVE.
This starts device preparation, not movement: Button 1 still latches the actual
view's horizontal forward; R realigns without
moving or resetting elapsed time. Fly's default steering source is wired panel
Joystick 1, independent of UE's active OpenXR runtime. The serial worker timestamps
samples at reception; samples queued for >=0.5 s are rejected. Missing, disconnected
or >=0.5 s stale input stops motion and airflow. Centre within 15% is required after
start, alignment, disconnect/staleness and tuning exit. Axis x (default positive
right) or y and inversion are private settings. Deflection beyond the deadzone
maps continuously to the inherited +/-15 degree steering input; the same input
drives ground steering and airborne rudder, retaining the bank-plus-rudder model.
Joystick 2 and Button 3/4 controls are unchanged. Joystick 1 SW retains tuning;
while tuning, fresh panel input is still required but rudder is zero. Exiting
tuning requires neutral again. No automatic BLE load or fan-response changes.
The optional `steering_input=vive` retains the old handle tracking/recenter worker;
only that source initializes OpenVR. HMD or selected steering-source invalidity
stops motion and airflow. The Fly launcher no longer requires vrserver.exe;
Quest Link uses Meta OpenXR, VIVE uses SteamVR OpenXR. Row is unchanged.
The source watchdog and Esc/window-close cleanup remain in place.
Esc or the source watchdog consumes the one-time automatic start so stopped
devices cannot silently restart. The setup UI retains explicit resume after
such a stop. PrepareOnly never auto-starts devices; Smoke retains its isolated
offline schedule. HMD selection never changes this UI or selects VIVE steering.

Local time uses timezonefinder and IANA tzdata, including summer time. Ambiguous
or nonexistent DST clock times are rejected. NOAA solar mathematics was copied
from the MIT Secret-World solar module; Blender/editor code was omitted. No
Secret-World import or sibling path is used. Time is frozen after Apply.
The accepted aerodynamic constants, including fixed air density, are unchanged.

## Flight audio

Python sends read-only physical air/ground speed, fresh cadence/power, airborne
and readiness flags plus a monotonically increasing accepted-touchdown count.
Touchdowns are detected only across actual advance steps, after terrain accepts
motion, never across recovery or setup. UE consumes authenticated telemetry and
locally gates it by play, geography, HMD tracking and the one-second watchdog.
The native mixer smooths six bounded voices; magnification is absent from its
inputs. Pedal/propeller fade after cadence stops, while gliding wind continues.
No audio code writes device commands or changes physics. The propeller has a
0.4 s decay time constant; other active loops use 0.18 s. Global stops fade faster.
Master volume is 0..1 (default .8). Voices are non-spatialized and may not overlap
multiple instances. Wind is stereo, mechanics mono. Original audio is local;
derived loop joins, headroom, imports and verification are described in FLY_AUDIO.ja.md.
