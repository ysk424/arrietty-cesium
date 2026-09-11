# Arrietty-cesium
Read README.md and docs/HANDOFF.md before changes.
Independent fork of Arrietty-row 316f4b2. Do not modify sibling projects.
Reuse the accepted rowing, VR calibration, HMD lateral steering, water and audio.
Mean water at the launch origin is Z=0 cm; away from origin it curves. Cesium origin height is water surface WGS84
ellipsoid height (metres); never confuse lake bottom, MSL or ellipsoid heights.
Never silently use sea level for an unknown lake. Ocean and lake only.
Location confirmation accepts only Y; N or EOF exits without launching UE.
Keep secrets, device IDs, health/session data, generated UE assets, downloaded
plugins and SDKs out of Git. Never print API tokens or put them on command lines.
BLE is telemetry only; never write rower resistance/control points.
Enter setup semantics and numpad 0 stop/home remain unchanged. Missing HR is --.
Record actual tests and their limits honestly; do not inherit upstream acceptance
as evidence of Cesium visual/VR correctness.
