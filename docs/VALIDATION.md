# Row / Fly integration validation — 2026-09-12

## Movement magnification follow-up

`fly.ps1 -mag 1..10` was added after the user's first live Fuji ride. It scales
east/north/up displacement while retaining the flight model, rotation rate,
initial AGL and device commands. The geographic route, contact checks and distance
use the scaled position. WORLD/DIST/V/S expose the resulting world motion.

- Fly rebuilt successfully; 107 Python tests and all four UE tests passed.
  New checks compare 1x/10x translation using the actual flight model while
  asserting equal airspeed and attitude; they cover magnified ground motion,
  a ridge between clear endpoints, impact descent, missing terrain and radius.
  The native sweep test checks an obstacle beyond the old two-metre reach and
  verifies bounded terrain lookahead. Evidence: logs/fly/ue-build.log,
  logs/fly/python-tests.log, logs/fly/ue-automation.log.
- Actual Fuji air-start renders completed at both 1x and 10x, using offline
  synthetic input. Over about 41s of travel, distance was approximately 396m
  and 3.96km respectively. Initial AGL remained 100m in both cases. After the
  first second neither run recorded a stopped sample. This verifies streaming
  along this route, not every network condition or all terrain.
- Both final panels were visually checked: AIR stayed about 36km/h while WORLD
  showed about 35km/h at 1x and 354km/h at 10x. Evidence:
  apps/fly/artifacts/magnification-1.png and magnification-10.png,
  logs/fly/magnification-comparison.json and magnification-*-smoke.log.
- The live application was observed closed before rebuilding. Test launches
  used -Offline/-nohmd with OpenXR disabled. The user's latest live training
  log and CSV were hash-checked as unchanged. Magnified live VR comfort and
  control feel have not yet been tested.

Source snapshots: arrietty-cesium 3b9fb96 and Arrietty-UE58 ef86436 (accepted
flight runtime b3b0cc1). Both applications were prepared locally with UE 5.8.2
and Cesium for Unreal 2.29.1. The sibling repositories were not modified.

## Build and automated checks

- Both UE targets built successfully; both maps/materials were regenerated.
  Row sound imports and saved content checks passed after the directory move.
  Evidence: logs/row/ue-build.log, logs/row/ue-content.log,
  logs/fly/ue-build.log and logs/fly/ue-content.log.
- Row: 10 Python tests, 208 native checks (104 core, 46 water, 16 audio,
  42 tracking), and UE ArriettyRow.Controls.SetupEnter passed after relocation.
  The final no-OpenXR row test also passed. Evidence: logs/row/native-tests.log,
  logs/fly/final-tests-console.log and logs/row/setup-controls-*.log.
- Fly: 101 Python tests passed, including the inherited device/control tests,
  negative valley heights, MSL/geoid conversion, no automatic AGL following,
  elevated landing, takeoff on gently rising ground, ridge/water/radius stops,
  missing terrain, and verified recovery of altitude, attitude and flight mode.
  Evidence: logs/fly/python-tests.log.
- Four UE automation tests passed: Arrietty.Coordinates.Attitude, Cesium,
  HmdAlignment, and Arrietty.Terrain.Sweep. HmdAlignment exercises the real
  DefaultXRCamera with simulated tracking; Sweep uses an actual physics scene.
  These are not live HMD tests. Evidence: logs/fly/ue-automation.log.
- Actual PowerShell 5.1 and 7 launchers were exercised with intercepted Python
  argv, including place/date/decimal-compatible argument transport. Explicit
  N and EOF cannot prepare or launch a scene. Both root launchers also exited
  successfully on N with an actual cached destination.
- The Windows live-device mutex was checked across two independent processes.
  Terrain readiness blocks device-worker startup. Tests use mocked devices;
  the render sessions explicitly report hardware=False.

## Actual Cesium rendering

The following captures use real Cesium terrain/imagery and synthetic input.
Screenshots were visually inspected. Fly's noon exposure was adjusted after
checking both coastal and mountain scenes.

| Application / place | Start | Observed result |
|---|---|---|
| Row / Lake Bled | Existing elevated water datum | Geography ready, 103.15m synthetic travel |
| Row / Koh Hong | Existing sea datum | Geography ready, 80.63m travel, shore contact paused |
| Fly / Funafuti airport | Ground | 410.22m travel, takeoff and 35.71m final AGL |
| Fly / Mount Fuji | Air, terrain + 100m | 397.22m travel, 201.14m final AGL over descending terrain |

Fly origin ellipsoid heights were 40.058m at Funafuti and 3793.713m at Fuji;
Funafuti's origin MSL was 7.425m. This checks that sea-adjacent ground is not
mistaken for ellipsoid height zero. Fuji AGL grows as the ground falls away,
without tying aircraft altitude to that ground.

Local evidence: apps/row/artifacts/migration-bled.png,
apps/row/artifacts/migration-koh-hong.png, apps/fly/artifacts/funafuti-final.png,
apps/fly/artifacts/fuji-final.png. Logs: logs/row/migration-*.log,
logs/fly/funafuti-smoke.log, logs/fly/fuji-smoke.log and corresponding
*-latest-ue-offline.csv. Fly smoke completion requires >100m movement and
>3m clearance after geography readiness, not merely an airborne state flag.

## No-hardware startup and remaining acceptance

During an earlier -nohmd render, Windows OpenXR PreInit briefly started the
SteamVR background runtime. It exited after UE. Offline/demo, preparation and
test entry points now explicitly disable the OpenXR plugin; XRBase is declared
separately for the camera tests. Live launchers retain OpenXR. Final verification
records process monitoring in logs/fly/no-vr-verification.json: preparation,
the four UE tests and a 410.48m coastal smoke flight all exited successfully
with zero observed SteamVR processes.

No live VR ride, BLE/serial device session, fan or trainer-load acceptance was
performed. The user will power the hardware after delivery. Check row calibration
and tracking first, then fly P -> Button 1 alignment, handle steering, takeoff,
MSL/AGL, R, recovery, and Esc/window-close cleanup. VR frame time, HMD readability
and credit placement still need live assessment.

Terrain and OSM resolution limit ground contact accuracy. The 10m height grid
and loaded-mesh sweep do not establish accuracy for small buildings, trees or
roads. Fly AGL is relative to the terrain dataset, not surveyed water level;
water contact is excluded using OSM. Mountain ground starts can fail the slope
check; use Air explicitly. The supported radius remains at most 10km.

UE startup still emits some upstream engine validation diagnostics, including
the existing Cesium semantic-enum initialization diagnostic. Passing builds and
named test results above do not imply that every engine log line is error-free.
Live VR performance and stability remain unverified.

Private device settings were preserved and checked against the source without
printing their values. Config, tokens, generated assets, SDK/plugin downloads,
caches, logs and exercise records remain excluded from Git. Fly now uses the
same HTTP/Cesium log suppression as row because upstream token-refresh logs can
include authenticated URLs. Values found in local test logs were redacted.
Application geography readiness/failure codes remain logged. Bundled hardware
wheels are verified against the copied SHA256 manifest. No publication was made.

The following retained results describe the earlier rowing-only fork.

# Validation — 2026-09-11

These results apply to the new Cesium fork. Inherited automated coverage is
summarized in UPSTREAM_VALIDATION.md; personal hardware/session reports are private.

## Completed

- After the Cesium spelling/path correction, `ArriettyCesiumEditor` rebuilt,
  content/audio regeneration and saved-map verification passed, and all 10
  Python tests passed again. The renamed root launcher confirmed Koh Hong and
  cancelled on N. An actual offscreen Lake Chuzenji demo loaded terrain/water,
  reached 28.49 m of synthetic travel and exited normally. Evidence:
  logs/row/rename-prepare.log, logs/row/rename-content-check.log,
  logs/row/rename-place-tests.log, logs/row/rename-launch-n.log,
  logs/row/cesium-renamed.log and apps/row/artifacts/cesium-renamed.png.
- UE 5.8.2 / MSVC: ArriettyCesiumEditor Win64 Development built successfully,
  including Cesium for Unreal 2.29.1. The final build also contains the invalid
  scene / failed initialization guards.
- `tools/prepare.ps1`: pinned SDK/plugin/geoid checks, UE water/boat material and
  map generation, and all five local sound imports completed. Repeated preparation
  was verified; the generated map and terrain wrapper are rebuilt from source.
- `tools/verify_content.py`: saved CesiumRow map, native sky, origin PlayerStart,
  water mask and curved-water parameters verified. No Cesium token/actor is
  serialized into the map. Evidence: logs/row/content-validation.json.
- Native checks: 104 core, 46 water, 16 audio and 42 tracking checks passed
  (208 total). Evidence: logs/row/native-tests.log.
- Python: 10 tests passed, covering explicit Y only; N/EOF without terrain or UE;
  Y preparation; invalid input; unknown lake elevation; source evidence; EGM96
  correction sign; elevated lake conversion; Nominatim JSON v2; coastline
  orientation, island holes, and the generated lake mask. Evidence: logs/row/place-tests.log.
- Actual root PowerShell launcher was run with redirected stdin: `N` for Koh Hong
  exited successfully without preparing/launching. `Y` with `-PrepareOnly` for
  Lake Chuzenji prepared the scene successfully.
- Real OpenAI Responses API / web search resolved Koh Hong (Thailand/Krabi),
  Lake Bled (Slovenia) and Lake Chuzenji (Japan/Tochigi). No mocked network data
  were used for the following rendering checks.
- Cesium ion assets 1 (terrain) and 2 (imagery) were accessed successfully.
  Credentials remain outside Git.
- UE automation `ArriettyRow.Controls.SetupEnter` passed: repeated Enter preserves
  calibration, stop/retry works, and instrument attachment is correct. This test
  uses the explicit offline `RowTestFixture`, without geodata/hardware.
  Evidence: logs/row/setup-controls-20260911-124830.log.
- Public-source check passed: settings, token, device IDs, generated content,
  plugins, imagery/terrain SQLite cache and exercise data excluded. The token
  value was also checked for absence from the project .log files.

## Actual UE renders

All use offline synthetic rowing input, the real terrain service and generated
UE materials. Screenshots were inspected after fixing water/terrain overlap.
The final captures show UE water and the boat with the real terrain backdrop.

| Place | Lake/sea MSL m | Cesium origin ellipsoid m | Terrain minus reference water at 5 launch probes | Result |
|---|---:|---:|---:|---|
| Koh Hong | 0 | -24.687 | -0.003 to -0.002 m | Ready, demo rowing, shore/water inspected |
| Lake Bled | 475 | 522.486 | -0.295 to -0.294 m | Ready, demo rowing |
| Lake Chuzenji | 1269 | 1311.837 | -0.009 to -0.005 m | Ready, demo rowing |

Local images: apps/row/artifacts/koh-hong-shore.png, apps/row/artifacts/bled-final.png,
apps/row/artifacts/chuzenji-final.png. Corresponding logs have the same stem under logs/row/.
The final frames recorded 28.49 m, 11.55 m and 28.49 m of synthetic boat travel.

The original water shader failed because a material vector's default RGB output
was used as RGBA. It now uses separate XY bounds and size parameters. Subsequent
renders compiled the water and terrain materials successfully.
A generated Cesium material wrapper hides only mapped water-side terrain pixels
at/below mean water + 3 cm, retaining native imagery/material layers. This avoids
baked water imagery competing with troughs without artificially raising lakes.
The same threshold is used when rejecting hidden water-surface collision hits.

## Limits

- No live HMD/rower/Tracker/heart-rate trial was performed for this fork. VR
  performance, comfort, HMD attribution placement, physical steering and prolonged
  sessions require a rider trial. Existing rowing code tests are not such a trial.
- Screen capture can cause a frame stall and the existing tracking watchdog may
  stop a synthetic demo after the captured frame. These short captures do not
  establish long-session timing. Preview is capped at 60 FPS; normal launch at 90.
- Cesium 2.29.1 emits an upstream startup diagnostic about
  `FCesiumMetadataPropertyStatisticValue::Semantic` initialization. It did not
  prevent successful compilation, terrain loading, rendering or the explicit
  UE automation test. The downloaded plugin was not patched.
- Global terrain/imagery have limited resolution. Cliff textures, individual
  trees and buildings are not equivalent to the separately authored old Bled scene.
- Source elevations are representative published levels, not current gauges.
  A lake whose terrain appears above the stated water level fails the launch
  check and needs a verified `-WaterLevelM` override. A terrain sample is never
  silently substituted for lake surface elevation.
- The local approximation and mapped boundary bound navigation (default 3 km,
  configurable 1–10 km). No river flow, tide changes, global circumnavigation,
  bathymetry or guaranteed offline terrain availability is implemented.
