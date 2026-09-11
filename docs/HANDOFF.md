# Current handoff — 2026-09-12

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

Row's sea/lake restrictions, telemetry-only rower, calibration, water and audio
are unchanged. Fly retains T2 controls, fan/PTT, handle steering, view-forward
Button 1 and R alignment. Do not apply rowing device restrictions to the T2.

Fly uses sampled ground ellipsoid height as origin. Altitude is h-h0 internally,
including negative heights; MSL and AGL are separate. Takeoff permits wheel contact
on a gently rising launch surface while lift builds; after clearance >0.5m this
special handling ends. Terrain availability and path collision gate motion.
Recovery restores a verified 3D pose. See VALIDATION.md for actual evidence and
remaining live checks. Initial global radius is capped at 10km. No globe rebasing.

## Next work: human-powered glider audio

The user will create sound masters later. The production brief is
[FLY_AUDIO.ja.md](FLY_AUDIO.ja.md); local delivery directory is apps/fly/sounds/.
Prepare five core files: wind-soft.wav, wind-fast.wav, pedal-drive.wav,
ground-roll.wav, touchdown.wav. Optional sixth: stall-buffet.wav. The first four
are loops; touchdown is a short one-shot. The brief includes durations, channels
and export settings. Audio import/playback for fly is not implemented yet.

Preserve the quiet human-powered glider character: no combustion/jet engine.
Stopping pedalling should fade drive sound while gliding wind continues. Use
speed-dependent wind layers with bounded gains; -mag must not multiply volume,
pitch or physical fan output by its numeric value. Keep hardware rules intact.
Original masters remain local and unchanged; prepare derived UE assets separately.
Air rings, route markers and new ambient beds were suggestions, not implemented
or approved additions. The next concrete task after assets arrive is audio
integration plus offline capture and live listening checks.

Latest automated evidence: fly 107 Python tests and 4 UE tests; row 10 Python,
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
