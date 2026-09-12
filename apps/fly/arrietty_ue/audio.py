"""Read-only audio telemetry; never changes physics or device commands."""
from arrietty_up.constants import SAMPLE_STALE_SECONDS


def audio_state(state, terrain, now, touchdowns):
    fresh = 0 < state.last_ftms_sample_seconds <= now and now-state.last_ftms_sample_seconds <= SAMPLE_STALE_SECONDS
    active = state.ride_active and state.hmd_aligned and state.steering_tracking
    active = active and (terrain is None or (terrain.ready and not terrain.message and not terrain.blocked))
    return dict(active=bool(active), airborne=state.flight.airborne,
                speed=max(0., state.flight.airspeed_meters_per_second*3.6 if state.flight.airborne else state.ground_speed_kmh),
                cadence=max(0., state.cadence_rpm) if fresh else 0.,
                power=max(0., state.power_watts) if fresh else 0., touchdowns=touchdowns)
