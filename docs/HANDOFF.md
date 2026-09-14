# Current handoff — 2026-09-15

Row now supports root `-mag` / `-Magnification` 1..10 (default 1, decimals),
Python `--mag` / `--magnification`, and UE `-RowMagnification`. Only the final
travel distance is multiplied; rowing physics, calibration, HMD lateral yaw
response, water/audio intensity and read-only hardware behavior remain intact.
The panel shows magnified world speed and distance. CSV keeps physical
`speed_kmh`, makes `distance_m` geographic, and appends `movement_magnification`
and `world_speed_kmh`. Stop/Home retains the multiplier. See README and
INTEGRATION for scaled-path polygon checks and short curved-water mesh sweeps.

The user also asked to check load behavior above 10: the existing accepted range
was already 1..16, with no level-ten cutoff. Tests now decode every level through
both FTMS packet layouts and check propulsion through levels 9..16. Fresh zero
machine watts still yield zero thrust; unknown/invalid/stale load stays unity.
No speculative change was made to telemetry decoding or load gain. CSV appends
`resistance_raw` and `resistance_age_s` for diagnosis at the next live ride.
The reported high-load behavior has not been reproduced with hardware.
The user clarified that LOAD does change; the symptom follows stopping and
changing load before rowing again. At the next ride, compare the machine's own
speed/watts display with the received BT watts while resuming strokes. Keep
fresh-zero handling unchanged until the source of the discrepancy is known.

Row native checks: 363; Python tests: 14; UE 5.8.2 rebuild and both Controls.SetupEnter
and Movement.Magnification passed. The new UE test checks actual pawn translation,
panel speed, pause/Home, a narrow island between clear endpoints, radius, blocked
distance rollback, curved water and diagnostic CSV. Test mode disables OpenXR
and opens no device workers. No app was running before rebuild. The user requested
publication on 2026-09-15 and will perform the next live ride later. See VALIDATION
for final render evidence and limits.
The 10x Moraine Lake synthetic render reached 141.124m, then paused at the shore;
one CSV sample showed 9.603km/h physical and 96.028km/h world speed. The screenshot
was inspected and the process exited normally. This short non-VR preview does
not establish long-distance streaming, magnified comfort or live load behavior.

## Previous handoff — 2026-09-12

Fly audio acceptance update: the user confirmed that sound worked through
fly.ps1 and requested push on 2026-09-12. Record this as user listening
acceptance of the current audio implementation, separate from the earlier
offline mixer capture. No detailed per-condition listening results or extended
comfort measurements were supplied. Six masters and all generated audio remain
local; the repository contains playback, import/verification tools and docs.

Row acceptance update: after the OpenVR path fix, the user reported successful
operation and requested documentation and push. The subsequent normal launch
logged OpenVR available=1, geography ready, calibration and rowing start. This
confirms recovery from the two startup blockers and initial live setup/start.
It does not establish long-session IMU accuracy, slow/short-stroke detection or
steering/audio comfort. Native tests: 275; Python tests: 11; UE build and
SetupEnter passed. Private settings, device identities and session data stay local.

Row OpenVR follow-up: the SDK was present with pinned hashes, but the module's
development DLL fallback still traversed six parents from ProjectDir after the
app move. It now traverses four to workspace/ThirdParty/OpenVR. The failed live
launch had available=0 and had not started; it was closed normally before rebuild.
UE now reports ROW_OPENVR_LIBRARY available=1. The initial offline test verified
DLL loading; the later user confirmation is recorded above. SDK reinstallation
was not required.

Row Moraine Lake follow-up: the user's normal launch passed Y but stopped before
calibration with water_below_terrain, showing terrain overhead. The original
40 m shore-clearance probes were 26.90–36.00 m above the established lake water.
Twenty independent most-detailed diagnostic samples showed flat interior terrain
about 35.98 m high relative to that water; merely moving offshore was insufficient.
Row now prefers 150 m lake clearance (40 m fallback for narrow lakes) and applies
a bounded terrain display/collision translation only to a flat elevated lake
sheet: five valid probes, all >3 cm, spread <=25 cm, excess <=50 m. Water remains
at its independently established MSL/EGM96 height and local mean Z=0. Ocean and
inconsistent/unknown terrain gates remain. See INTEGRATION and VALIDATION.
The failed, never-started Row instance was closed normally before rebuilding.
UE offline Moraine preview passed geography readiness and travelled 91.52 m;
the overhead mesh was absent. That preview alone was not live VR/IMU acceptance
or surveyed shoreline accuracy. Normal row.ps1 place launches still require Y.

Row follow-up: WT9011DCL replaces the occluded handle Tracker in this PC's
ignored local settings (`bar_input=wt9011dcl`). Native BLE notifications only;
use the observed random address type on reconnect. Sensor wake button was needed.
Raw data and identities stay under logs/row. See [ROW_IMU.ja.md](ROW_IMU.ja.md).
The IMU measures acceleration/attitude, not room position. Its setup preserves
2 s settle / 1 s quiet / 2 cycles, but explicitly uses forward-facing HMD yaw
during the quiet second for the fixed steering frame. Start with the handle
extended and pull first. Tracker-mode machine-line calibration is unchanged.
HMD lateral steering and Enter/numpad-0 remain. IMU gaps >=.25 s stop immediately;
there is no HMD handle fallback or automatic restart. Sensor and rower commands
remain read-only apart from notification subscription.
Native reception and a user-reported 10-cycle recording passed; C++ replay
counts 10, and online calibration on its first two cycles leaves 8 exercise
pulls. This is development/replay evidence, not independent accuracy or VR
acceptance; the later initial live setup/start is recorded above. Longer sessions
and detailed rowing feel still need evaluation. The capture/probe diagnostics
did not run a place launcher.

Follow-up: a live Fuji air-start ride confirmed movement and descent; AGL
increased over falling terrain. Detailed session observations remain local in
logs/fly/handoff.local.md. This does not establish collision acceptance.
The user requested `-mag` for more visible
travel. Fly now supports translation magnification 1..10 (default 1), including
vertical deltas, with unchanged initial AGL, physics/turning and device commands.
WORLD/DIST show geographic motion; AIR remains the flight-model speed. Terrain
lookahead and mesh sweeps cover faster movement. See README and INTEGRATION.

User accepted the implementation and requested documentation, persistent handoff
and a push to the existing origin/main on 2026-09-12. Initial integration tests
used no hardware; later live motion was checked, but magnified VR comfort and
contact handling still require live acceptance. Root row.ps1 replaces
run.ps1 with the same options. Root fly.ps1 starts the copied UE58 flight program
in Cesium terrain. apps/row and apps/fly have separate UE projects, environments,
settings and logs. Shared geography is under shared/. Read README.md and
INTEGRATION.md for the current paths and height/terrain contracts.

Source snapshots: row 3b9fb96; flight ef86436 (runtime b3b0cc1). No sibling files
were changed. Private row/fly device settings were copied/moved into config/.
Original row session records and local caches were retained and their scene paths
updated. The application no longer needs Blender or Secret-World to build/run.

Row's sea/lake restrictions, telemetry-only rower, Tracker calibration, water and audio
are preserved; the optional IMU calibration is described above. Fly retains T2 controls, fan/PTT, handle steering, view-forward
Button 1 and R alignment. Do not apply rowing device restrictions to the T2.

Fly uses sampled ground ellipsoid height as origin. Altitude is h-h0 internally,
including negative heights; MSL and AGL are separate. Takeoff permits wheel contact
on a gently rising launch surface while lift builds; after clearance >0.5m this
special handling ends. Terrain availability and path collision gate motion.
Recovery restores a verified 3D pose. See VALIDATION.md for actual evidence and
remaining live checks. Initial global radius is capped at 10km. No globe rebasing.

## Human-powered glider audio: implemented and user confirmed

The user supplied six PCM16/48 kHz stereo masters in root wav_fly/, including
an additional pedal-powered propeller loop. Five loops are 15 s and touchdown
is 1 s. The importer accepts the delivered double extensions and embedded space;
never rename or overwrite these originals or their FireFlyOriginal directory.
Canonical files in apps/fly/sounds/ take precedence. All sound masters and
generated assets are ignored by Git. Source hashes are recorded locally in
apps/fly/artifacts/audio/preparation.local.json. See [FLY_AUDIO.ja.md](FLY_AUDIO.ja.md).

Fly now has six independent audio voices: soft/fast wind, pedal drivetrain,
propeller, wheels and touchdown. The native mixer smooths and bounds volume and
pitch. Gliding wind follows physical AIR speed after pedalling stops; cadence
drives pedal/propeller sound, which fades on zero cadence or stale FTMS input.
Propeller decay is slower than drivetrain decay. Ground roll is grounded only.
Touchdown is counted by accepted flight-to-ground transitions, avoiding repeats
from successive packets, setup and recovery. Setup, tracking/terrain gates,
Esc and the bridge watchdog mute the mix. -Volume is 0..1, default .8.
Magnification, physics and device/fan commands remain independent of audio.
Wind stays stereo; mechanical sounds are downmixed to mono; no row ear correction.

prepare_audio.ps1 regenerates derived WAV/UE imports. Full Fly preparation also
imports available masters, allowing new builds without any supplied audio.
test_audio.ps1 runs an empty-map fixture with OpenXR disabled and no bridge,
devices, microphone or Cesium requests. The actual 30 s UE output passed asset,
mix, glide/fast-wind, single-touchdown and silence checks. The later user
listening confirmation is recorded above; per-condition and extended comfort
results were not reported. Optional stall audio, air rings, routes and new
ambient beds remain unimplemented.

Latest automated evidence before audio: fly 107 Python tests and 4 UE tests; row 10 Python,
208 native checks and its UE setup test. Two actual Fuji renders at 1x/10x kept
initial AGL=100m and travelled about 396m/3.96km over the same 41s with no stopped
samples after the first second. See VALIDATION.md for scope and limitations.

The following retained notes describe the original rowing integration:

# Handoff

2026-09-11: Independent arrietty-cesium fork from Arrietty-row 316f4b2, version 0.1.0.
User requested root row.ps1 with a place name, OpenAI identification, simple
country/place Y/N confirmation, Cesium terrain and the existing UE water.
Scope: oceans and lakes, not rivers. N exits; user refines the place name.

Implemented and built locally. Run `./row.ps1 "Koh Hong"`, confirm with Y.
`-Demo` uses synthetic input and no HMD. All normal launch modes retain Y/N.
OpenAI uses Responses API, gpt-5.4-mini and web search with structured output.
Place and water-boundary caches are local; `-RefreshPlace` refreshes AI lookup.

Credentials, device settings, sound masters and exercise records are private
local files. Use the example JSON files for a new setup; never commit local
values or generated assets. The old Lake Bled world assets are not required.

Internal ArriettyRow module remains to reuse its accepted rowing implementation;
project and target names are ArriettyCesium. UE 5.8.2, Cesium 2.29.1. Scripts
bootstrap local dependencies and regenerate materials/map; plugin and EGM96
checksums are pinned. Keep all SQLite caches out of Git as well as Saved/Content.

Water datum: launch-origin mean water is Z=0; away from origin, boat and shaders
share a quadratic local approximation of the geoid/ellipsoid water surface.
Published MSL levels are converted with the EGM96 grid. Never use terrain heights
as lake levels. Mapped low terrain pixels are clipped at mean water + 3 cm so
baked imagery does not occlude UE wave troughs. Islands remain dry. Navigation
checks OSM polygons, local radius and exposed terrain. Startup waits for terrain
and checks five water launch samples. Unknown heights/network failures stop safely.

Tests: 208 native checks, 10 Python tests, UE SetupEnter automation and generated
asset checks passed. Actual UE renders for Koh Hong, Lake Bled and Lake Chuzenji
were inspected. See VALIDATION.md for exact evidence and limitations, including
the upstream Cesium enum initialization diagnostic. Live HMD fitness testing and
VR performance/attribution placement remain unverified; do not inherit earlier
rowing hardware acceptance as evidence for this new geography integration.

Repository: https://github.com/ysk424/arrietty-cesium (MIT application code).
The project, UE targets and launch paths use the Cesium spelling. Public history
uses the GitHub noreply identity and excludes personal hardware/session reports.
Continue in this directory, not siblings. Run the staged-tree and history checks
in tools/check_public_tree.py before publishing further changes.
