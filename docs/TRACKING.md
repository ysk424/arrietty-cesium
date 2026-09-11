# Brief bar occlusion support

The inherited implementation supports brief occlusion of the configured handle
Tracker. Its effect during actual occlusion and prolonged Cesium VR use remains
unverified in this fork. There is no change to device selection, BLE
telemetry, calibration or the physical steering frame.

`apps/row/Source/RowTracking.h` receives bar and HMD positions projected onto the
calibrated machine axis. While both are tracked, it fits bar versus head
position from up to 64 paired samples, at most 10 Hz, using the last six
seconds. It requires at least 10 samples, bar/head spans of 20/12 cm, positive
correlation of at least 0.8, and a fitted gain between 0.5 and 2.0. Only actual
Tracker measurements teach this relationship. No rider-specific constants
or pose histories are persisted or shipped.

On bar loss, the last real pair anchors a grace interval of **1.25 s** from
detection. A usable fit and bar movement above 0.10 m/s within the previous
0.5 s enable `hmd_assist`. Otherwise `coast` removes drive immediately and
lets the existing boat drag slow the boat. Both keep session time and distance
running and use the actual HMD for lateral steering. The HMD must be fresh in
OpenVR and tracked in the rendering OpenXR runtime. Yaw is never a rowing or
steering input after calibration.

Assisted bar displacement follows incremental fore/aft HMD movement times the
frozen gain. Limits are 2 m/s, 50 cm from the last real bar position, and the
recent measured bar range plus 5 cm. A stationary head adds no displacement;
rate limiting does not leave movement queued for later. The normal 60 ms
bar-velocity filter still smooths drive. More than 75 cm of head displacement
from the anchor or head steps above 2.5 m/s stop rather than masking a recenter.
This is a short game-motion estimate, not a measurement of arm motion or work.

Returning valid poses enter `reacquiring`: coast for **0.15 s of continuous
plausible Tracker readings**, ease the internal bar position toward the real
one, then resume measured derivatives with a fresh anchor. Correction offsets
are never used as rowing velocity. One valid sample cannot reset the grace
deadline. Renewed loss during this confirmation coasts without re-enabling
HMD estimation. A return beyond 1.2 m from the last real bar anchor is rejected.
Successful return clears the fit, which learns again from real measurements.

Only uninterrupted measured motion can add virtual strokes. Assistance and
return clear stroke travel/re-arm state; a real recovery of 12 cm must arm the
next catch. This avoids false stroke counts and catch sounds but can undercount
strokes spanning an occlusion. Pull/hull audio still follows drive/speed; the
accepted stereo left +6 dB balance remains unchanged.

The original 100 ms frame watchdog and real-pose 4.5 m/s discontinuity guard
remain. HMD loss, expired grace or a discontinuity stops; later tracking alone
cannot restart a stopped/paused session. Enter performs the usual new setup.
Pause, stop and new setup cannot be bypassed by automatic return. Calibration
still requires actual valid bar/HMD tracking throughout its original 2 s
settle, 1 s neutral and two strokes.

The panel explicitly shows HMD assistance or coasting. CSV retains its original
power `source` meaning (`bt`, `tracker_estimate`, now also `hmd_estimate`; demos
remain `demo`) and appends `bar_source`, `bar_gap_s`, `tracking_issue`.
Source transitions are written even between one-second session samples.
`bar_gap_s` becomes zero on successful return; source is the last motion source
when stopped. `ROW_TRACKING` logs source, duration, issue, frame duration and
validity flags. `ROW_CALIBRATION_FAILED` logs the exact failing frame's duration,
pose ages and validity, without changing setup behavior. No high-rate pose
recording is enabled and no device identities enter these logs.
Normal `row.ps1` launches overwrite `logs/row/training.log`, with no automatic
per-launch backup for that file. Session CSV history and intentionally archived
diagnostic/test evidence retain their separate files.

Native tests cover failure and recovery cases. `tools/test_tracking.ps1` runs
real UE with an **offline-only** synthetic HMD/bar pair, hiding the bar for
0.65 s every four exercise seconds, starting after four seconds of learning.
`-Capture` is a separate visual run because screenshot readback can itself
trigger the frame watchdog. Neither mode touches the hardware. See
[validation](VALIDATION.md) for evidence and unverified physical behavior.
