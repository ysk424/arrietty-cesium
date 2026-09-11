# Inherited implementation coverage

Arrietty-cesium derives its rowing implementation from Arrietty-row `316f4b2`.
The fork retains engine-independent regression tests for:

- Rowing calibration, stroke detection, drive/coast and telemetry freshness.
- HMD lateral steering, the 8 cm straight zone and calibration-axis handling.
- Brief bar-tracking loss, bounded assistance and stable reacquisition.
- Water waves, the Kelvin wake, bow whitewater and audio state gating.

The retained native suite contains 104 core, 46 water, 16 audio and 42 tracking
checks (208 total). The UE control test exercises Enter, calibration progress,
pause, stop/retry and panel attachment with synthetic input.

Current-fork results and their limits are in [VALIDATION.md](VALIDATION.md).
Original personal workout measurements, device captures and user feedback are
not distributed with this repository. Earlier hardware testing does not validate
Cesium VR performance, comfort or terrain behavior.
