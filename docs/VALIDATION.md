# Validation — 2026-09-11

These results apply to the new Cesium fork. Inherited automated coverage is
summarized in UPSTREAM_VALIDATION.md; personal hardware/session reports are private.

## Completed

- After the Cesium spelling/path correction, `ArriettyCesiumEditor` rebuilt,
  content/audio regeneration and saved-map verification passed, and all 10
  Python tests passed again. The renamed root launcher confirmed Koh Hong and
  cancelled on N. An actual offscreen Lake Chuzenji demo loaded terrain/water,
  reached 28.49 m of synthetic travel and exited normally. Evidence:
  logs/rename-prepare.log, logs/rename-content-check.log,
  logs/rename-place-tests.log, logs/rename-launch-n.log,
  logs/cesium-renamed.log and artifacts/cesium-renamed.png.
- UE 5.8.2 / MSVC: ArriettyCesiumEditor Win64 Development built successfully,
  including Cesium for Unreal 2.29.1. The final build also contains the invalid
  scene / failed initialization guards.
- `tools/prepare.ps1`: pinned SDK/plugin/geoid checks, UE water/boat material and
  map generation, and all five local sound imports completed. Repeated preparation
  was verified; the generated map and terrain wrapper are rebuilt from source.
- `tools/verify_content.py`: saved CesiumRow map, native sky, origin PlayerStart,
  water mask and curved-water parameters verified. No Cesium token/actor is
  serialized into the map. Evidence: logs/content-validation.json.
- Native checks: 104 core, 46 water, 16 audio and 42 tracking checks passed
  (208 total). Evidence: logs/native-tests.log.
- Python: 10 tests passed, covering explicit Y only; N/EOF without terrain or UE;
  Y preparation; invalid input; unknown lake elevation; source evidence; EGM96
  correction sign; elevated lake conversion; Nominatim JSON v2; coastline
  orientation, island holes, and the generated lake mask. Evidence: logs/place-tests.log.
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
  Evidence: logs/setup-controls-20260911-124830.log.
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

Local images: artifacts/koh-hong-shore.png, artifacts/bled-final.png,
artifacts/chuzenji-final.png. Corresponding logs have the same stem under logs/.
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
