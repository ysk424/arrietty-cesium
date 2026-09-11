# Handoff

2026-09-11: Independent arrietty-cesium fork from Arrietty-row 316f4b2, version 0.1.0.
User requested root run.ps1 with a place name, OpenAI identification, simple
country/place Y/N confirmation, Cesium terrain and the existing UE water.
Scope: oceans and lakes, not rivers. N exits; user refines the place name.

Implemented and built locally. Run `./run.ps1 "Koh Hong"`, confirm with Y.
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
