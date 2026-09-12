# Arrietty Cesium

Read README.md and docs/HANDOFF.md before changes. docs/INTEGRATION.md defines
coordinates, height, terrain and process boundaries. Do not modify siblings.

- apps/row and apps/fly are independent UE applications; root row.ps1/fly.ps1
  are the user launchers. Share geography helpers, not their control models.
- Row preserves accepted rowing, calibration, HMD lateral steering, water and
  audio. Mean launch water is Z=0; away from origin it curves. Sea/lake only.
  Never silently substitute sea level or terrain bottom for an unknown lake.
  BLE rower telemetry is read-only. Enter and numpad 0 semantics stay unchanged.
- Fly preserves UE58 Button 1 view-forward alignment, R recenter, handle steering,
  flight controls and device-worker shutdown. Its T2 load control is inherited
  from UE58 and is separate from the rower telemetry-only restriction.
- Fly ground origin is sampled WGS84 ellipsoid height. Flight height, MSL and
  AGL are distinct; allow negative launch-relative height. Unknown terrain must
  not become zero. Keep the geography readiness gate before hardware/motion.
- All normal place launchers require explicit Y; N/EOF exits before scene/UE.
- Settings, IDs, secrets, health/session logs, generated assets, downloaded
  plugins/SDKs and caches stay out of Git. Never print tokens or pass them in argv.
- Record actual tests and limitations. No demo/native test establishes live VR
  acceptance. Check running processes before rebuilding; do not stop a live ride.
- Fly is a human-powered glider. Six user masters, including a pedal-powered
  propeller, are imported locally from wav_fly/ or apps/fly/sounds/. Preserve
  masters; docs/FLY_AUDIO.ja.md defines the mix and offline capture workflow.
  Keep glide wind after pedalling stops. No engine audio or automatic changes
  to hardware loads/fan response when adding movement/audio effects.
- Check the staged public tree before publication. Do not publish unless asked.
