# Row / Fly integration validation — 2026-09-15

## Row movement magnification and 16-level check

- Root `row.ps1 -mag` / `-Magnification`, Python `--mag` / `--magnification`,
  and UE `-RowMagnification` support 1..10, default 1 and fractional values.
  Physical rowing speed, watts, drive, steering, calibration and water/audio
  inputs are unchanged. World translation, distance and displayed speed scale.
  Scaled paths check water polygon boundaries and holes exactly, plus the
  existing 2.5m clearance. Loaded terrain is swept in <=0.5m pieces following
  curved mean water; the existing hidden-imagery tolerance is retained.
- Existing load logic already accepted 1..16; no level-ten cap was found.
  Every level now passes real FTMS decoder tests in both packet layouts with
  synthetic values. Tests check increasing propulsion from levels 9 through 16,
  maximum speed, stationary-handle gating, zero watts, invalid and stale load.
  Zero received watts remain zero at high load. The user's reported high-load
  behavior has not been reproduced on hardware; no firmware/encoding cause is
  established. `resistance_raw` and `resistance_age_s` provide local diagnostics
  without changing accepted power gain or issuing BLE commands.
- MSVC native suite passed **363 checks**: core 192, water 46, audio 16,
  tracking 42, IMU 54, terrain 13. The new 1x/2.5x/10x comparisons retain identical
  physical speed, heading, yaw rate, power, drive, strokes and elapsed time while
  scaling distance. Pause and tracking loss still stop; invalid multipliers
  never move the boat. Evidence: logs/row/magnification-native-tests.log.
- **14 Python tests** passed. Actual Windows PowerShell 5.1 and PowerShell 7
  launcher transport was exercised with default, `-mag 2.5` and
  `-Magnification 10` under a decimal-comma culture. Python-to-UE forwarding
  was intercepted after explicit Y; invalid values fail before place lookup.
  Existing N/EOF, unknown-lake and water-datum tests passed. Tests use temporary
  settings/log destinations and fake credentials. Evidence:
  logs/row/magnification-python-tests.log.
- UE 5.8.2 rebuilt successfully. **Controls.SetupEnter** and
  **Movement.Magnification** both passed in the actual offline game fixture.
  Checks include pawn displacement, world-speed panel, pause, Home retaining
  the multiplier, a 10cm island between clear water endpoints, radius, blocked
  movement with no distance credit, curved water height and appended CSV fields.
  Evidence: logs/row/magnification-ue-build.log,
  logs/row/magnification-ue-tests.log and setup-controls-20260915-061720.log.
- A real Cesium Moraine Lake render with synthetic straight rowing and 10x
  movement reached geography readiness, calibrated and travelled **141.124m**
  before the shore check paused it at 9.301s active time. A recorded sample had
  physical speed 9.603km/h and world speed 96.028km/h (independently rounded).
  The inspected screenshot shows `WORLD km/h x10`, the 141m distance and
  `Shore / paused`; the process exited normally. Evidence:
  logs/row/magnification-10-preview.log and apps/row/artifacts/magnification-10.png.
  This was a short, sound-disabled, non-VR preview ending at the nearby shore;
  it does not establish long-distance terrain streaming or magnified VR comfort.
- No UE application was running before rebuild. Tests disabled OpenXR and used
  no rower, IMU, HMD, heart-rate or other device workers. Existing live training
  logs and records were not test output destinations. These tests do not
  establish magnified VR comfort or live load-level acceptance. Fly and sibling
  repositories were not modified. The user subsequently requested publication;
  the next live ride and high-load symptom check remain pending.

## Row user confirmation after fixes

On 2026-09-12 the user reported successful operation after the OpenVR path fix
and requested documentation and push. The subsequent normal launch log contains
ROW_OPENVR_LIBRARY available=1, ROW_GEOGRAPHY_READY, calibration progression and
ROW_CONTROL action=start. This confirms resolution of the startup blockers and
initial live setup/start. It is separate from the offline evidence below; no
long-session classification accuracy, slow/short-stroke performance or detailed
steering/audio comfort result was reported. Session data and device identities
remain in ignored local storage.

Before publication, the entire staged tree passed the public-tree audit. The
audit now excludes only the known `bar_input` enum values from private-value
matching; addresses, serials, tokens and unknown values remain protected. Direct
checks confirmed that every collected private value is still detected, and both
commit identities use GitHub noreply addresses. Unrelated Fly audio changes were
left in the working tree for their separate publication.

## Row OpenVR runtime path follow-up

- The user's live Row launch logged ROW_OPENVR_LIBRARY available=0 and never
  started calibration or rowing. The SDK bootstrap verified every pinned hash;
  the SDK existed, but the module used six parent traversals from its project
  directory instead of four to workspace/ThirdParty/OpenVR.
- Corrected that development DLL fallback; retained executable/project binary
  lookup and deferred OpenVR shutdown. Closed only the failed pre-start Row
  instance normally and rebuilt successfully with UE 5.8.2.
- The actual UE process then logged ROW_OPENVR_LIBRARY available=1, passed the
  existing SetupEnter test and returned exit code 0. The automation runner used
  its test-complete exit path; it did not log module shutdown. No HMD or BLE
  workers were initialized in this offline test. This establishes DLL loading
  and control regression coverage; the later user confirmation is recorded above.
- Ignored evidence: logs/row/openvr-before-fix.local.log,
  openvr-path-ue-build.log, and setup-controls-20260912-134616.log.

## Row Moraine Lake terrain follow-up

- The user's confirmed normal place launch failed before calibration/start.
  Logs gave terrain minus known water = 26.896–36.004 m over five probes at the
  original launch, with the rendered mesh above the boat. A diagnostic using
  CesiumJS 1.133.0 sampleTerrainMostDetailed sampled 20 locations at 40/80/120/160 m
  shore clearance. Interior samples were nearly flat at about +35.98 m; this
  establishes a dataset/water-level discrepancy, not its geodetic cause. Known
  lake MSL remained 1884 m; no terrain-derived water elevation was substituted.
- The revised scene has approximately 149.85 m shoreline clearance after local
  polygon conversion/simplification. Its five UE probes range from +35.975 to
  +35.979 m. Row translated only its terrain actor by -35.979 m, leaving water,
  georeference and HMD coordinates unchanged. The original scene is retained.
- MSVC native suite passed 275 checks, including 13 added terrain checks for
  submerged terrain, the existing 3 cm tolerance, bounded flat-lake alignment,
  ocean rejection, observed steep shoreline, excessive correction, non-flat and
  water-crossing samples, NaN and infinity. Eleven Python tests passed, including
  wide/narrow lake launch clearance, water-probe containment, unchanged lake
  elevation, explicit Y/N/EOF behavior, unknown lakes and island holes.
- UE 5.8.2 rebuild passed. Real UE offline rendering reached
  ROW_GEOGRAPHY_READY, completed synthetic calibration and travelled 91.52 m
  before the screenshot at 55 s. Visual inspection found no overhead mesh and
  the rowing dashboard was active. The screenshot capture itself caused a later
  frame-gap pause; this diagnostic used synthetic tracking and no VR/hardware.
  It exited normally. The user's failed pre-start instance was closed normally
  before rebuild; no live ride was stopped.
- Evidence is ignored: logs/row/moraine-before-fix.local.log,
  moraine-native-tests.log, moraine-ue-build.log, moraine-terrain-aligned.log,
  cache/row/terrain-diagnostic, and apps/row/artifacts/moraine-terrain-aligned.png.
  The alignment is a bounded Row display compromise, not surveyed terrain
  correction; distant terrain/shoreline accuracy remains unverified. The later
  initial live start is recorded above. Fly terrain/altitude were not modified.

## Row WT9011DCL follow-up

- The BLE 5.0 sensor, mounted near-vertically at the handle centre, supplied
  acceleration, angular velocity and Euler angles at about 9.9 Hz. Pairing alone
  did not establish a data connection. After waking it, using its advertised
  random address type resolved connection failures. That type is saved locally.
- The native C++ IMU-only probe received 211 valid packets over its 30-second
  connection/listening run, with zero rejected packets and zero connection
  errors. It initialized no VR client, rower or heart-rate subscription, and
  exited normally. Only the WIT notification CCCD was written.
- A complete user-reported ten-cycle capture contained 1,012 samples. Its C++
  replay counted ten pulls and ended with zero drive. A second pass started
  the ordinary settle/quiet/two-cycle calibration during its initial stationary
  interval; calibration completed on two cycles and the model counted the
  remaining eight pulls. HMD poses and watts were synthetic in these replays.
  The recording was also used during development; this is not an independent
  estimate of classification accuracy. Raw data and identity are ignored logs.
- MSVC native suite passed 262 checks: existing core 104, water 46, audio 16,
  tracking 42, and IMU 54. IMU checks cover signed decoding, gravity compensation
  for near-vertical mounting and pure rotation, quiet/bias gating, duplicate and
  stale samples, ten-cycle 10 Hz input with 100 Hz rendering, two-cycle setup,
  timeout, loss/reconnection, HMD yaw versus lateral steering, and pause.
  Velocity damping was tuned to avoid counting braking at the end of a smooth
  synthetic stroke train as an extra pull; the real recording still counts ten.
- UE 5.8.2 Row rebuild passed. Existing UE SetupEnter automation passed with
  repeated Enter, stop/retry and instrument attachment. Ten Row Python tests
  passed, including Y/N/EOF launch behavior and unknown-lake rejection.
- Evidence: logs/row/imu-native-tests.log, imu-replay-result.local.log,
  imu-ue-build.log, and setup-controls-20260912-131733.log. Diagnostics did not
  use the normal training log. No place launcher or live VR ride was started.

Local Row input is now selected as `wt9011dcl`; the old Tracker serial remains
available if explicitly switching back. Tracker-mode calibration is preserved.
IMU-mode setup explicitly latches averaged forward-facing HMD yaw because AHRS
coordinates do not identify SteamVR room orientation. Initial live setup/start
was subsequently confirmed above. Steering comfort, propulsion/audio timing,
slow/short pulls, mounting movement and long-session drift still need evaluation.
See [ROW_IMU.ja.md](ROW_IMU.ja.md).

## Human-powered glider audio follow-up

- On 2026-09-12 the user confirmed that the sound worked through fly.ps1 and
  requested push. This is user listening acceptance of the current implementation,
  separate from the automated evidence below. No per-condition listening report
  or extended comfort result was supplied.
- Six user-supplied masters were inspected: PCM16, 48 kHz, stereo; five 15 s
  loops and one 1 s touchdown. Original hashes match the local preparation
  manifest. Derived wind retains stereo, mechanical/contact sounds are mono,
  loop joins use 0.2 s crossfades and all asset peaks are limited to 0.5.
- UE 5.8.2 fly build and six PCM/ForceInline imports passed. Python: 111 tests
  passed, including no-hardware bridge behavior, stale drive samples, terrain/
  tracking/alignment gates, touchdown count, magnification-independent sound
  telemetry and Volume forwarding through Windows PowerShell and pwsh.
- The normal UE suite passed five editor tests (existing coordinates, HMD
  alignment and terrain sweep, plus Audio.Mix). The empty-map audio fixture
  passed two client tests, Audio.Mix and Audio.Assets. These share the mix test;
  they are six distinct UE tests overall.
- A real UE output capture completed with all six assets and exactly one
  touchdown. Its 30 s sequence covered setup, pedalled ground travel, powered
  flight, unpowered glide, fast glide, landing/roll and stop. Glide remained
  audible; fast wind RMS exceeded slow glide; initial and final silence passed
  the PCM quantization threshold; peak remained below 0.95. WAV evidence and
  numerical verification are local under apps/fly/artifacts/audio/ue-mix-*.
- Evidence logs: logs/fly/audio-build.log, audio-prepare.log, python-tests.log,
  ue-automation.log and audio-runtime-*.log. Tests disabled OpenXR; the audio
  fixture uses no bridge, device worker, microphone or terrain network request.
  Live training logs/CSV were not test output destinations. Row code was untouched.

The automated checks verify program behavior and exported samples; the user's
separate listening confirmation is recorded above. Detailed HMD balance,
pedal-propeller feel and individual landing-sound results were not reported.
The optional stall sound is absent.

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
