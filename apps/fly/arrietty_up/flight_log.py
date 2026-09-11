"""One latest-session CSV, sampled without disk I/O on the game thread."""

from __future__ import annotations

import csv
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from queue import Empty, Full, Queue
from threading import Event, Thread


FIELDS = (
    "recorded_utc", "session_seconds", "ride_seconds", "east_m", "north_m",
    "altitude_m", "bearing_deg", "speed_kmh", "ride_active", "flight_enabled",
    "airborne", "distance_m", "world_file", "world_local_time",
    "origin_latitude", "origin_longitude",
    "pitch_deg", "roll_deg", "altitude_msl_m", "altitude_agl_m", "altitude_ellipsoid_m",
    "latitude", "longitude", "origin_ellipsoid_m", "height_reference",
    "movement_magnification", "world_speed_kmh", "world_vertical_speed_mps",
)
METADATA_FIELDS = ("world_file", "world_local_time", "origin_latitude", "origin_longitude", "origin_ellipsoid_m", "height_reference")
SAMPLE_INTERVAL_SECONDS = 0.1


class FlightLog:
    def __init__(self, path: Path, metadata: dict | None = None):
        self.path = Path(path)
        self.metadata = metadata or {}
        self.error = ""
        self.dropped_samples = 0
        self._queue = Queue(maxsize=120)
        self._stop = Event()
        self._next_sample = float("-inf")
        self._thread = Thread(target=self._write, name="ArriettyFlightLog", daemon=True)
        self._thread.start()

    @classmethod
    def for_session(cls):
        root = Path(os.environ.get("ARRIETTY_PROJECT_ROOT") or Path(__file__).resolve().parents[1])
        try:
            metadata = json.loads(os.environ.get("ARRIETTY_FLIGHT_METADATA", "{}"))
            if not isinstance(metadata, dict):
                metadata = {}
        except ValueError:
            metadata = {}
        return cls(root / "logs" / "latest-flight.csv", metadata)

    def sample(self, state, now: float, *, force: bool = False) -> None:
        if self.error or self._stop.is_set() or (not force and now < self._next_sample):
            return
        self._next_sample = now + SAMPLE_INTERVAL_SECONDS
        row = {
            "recorded_utc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
            "session_seconds": round(max(0.0, now - state.started_at), 3),
            "ride_seconds": round(state.ride_elapsed_seconds, 3),
            "east_m": round(state.position_x_meters, 3),
            "north_m": round(state.position_y_meters, 3),
            "altitude_m": round(state.flight.altitude_meters, 3),
            "altitude_msl_m": round(state.altitude_msl_m,3) if state.terrain_enabled else "",
            "altitude_agl_m": round(state.altitude_agl_m,3) if state.terrain_enabled else "",
            "altitude_ellipsoid_m": round(state.altitude_ellipsoid_m,3) if state.terrain_enabled else "",
            "latitude": state.latitude if state.terrain_enabled else "",
            "longitude": state.longitude if state.terrain_enabled else "",
            "bearing_deg": round(state.navigation_heading_degrees, 3),
            # Match the applied vehicle pose: the runtime levels it on ground.
            # Its internal bank is left-positive; CSV roll is right-wing-down.
            "pitch_deg": round(state.flight.pitch_degrees, 3) if state.flight.airborne else 0.0,
            "roll_deg": round(-state.flight.bank_degrees, 3) if state.flight.airborne else 0.0,
            "speed_kmh": round(state.speed_kmh, 3),
            "ride_active": int(state.ride_active),
            "flight_enabled": int(state.flight_enabled),
            "airborne": int(state.flight.airborne),
            "distance_m": round(state.distance_meters, 3),
            "movement_magnification": state.movement_magnification,
            "world_speed_kmh": round(state.world_speed_kmh,3),
            "world_vertical_speed_mps": round(state.world_vertical_speed_mps,3),
        }
        for key in METADATA_FIELDS:
            row[key] = self.metadata.get(key, "")
        try:
            self._queue.put_nowait(row)
        except Full:
            self.dropped_samples += 1

    def close(self) -> bool:
        self._stop.set()
        self._thread.join(timeout=2.0)
        return not self._thread.is_alive()

    def _write(self):
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            # Each game session replaces the previous route. No dated files.
            with self.path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=FIELDS)
                writer.writeheader()
                stream.flush()
                print(f"ARRIETTY_FLIGHT_LOG {self.path}", flush=True)
                while not self._stop.is_set() or not self._queue.empty():
                    try:
                        row = self._queue.get(timeout=0.1)
                    except Empty:
                        continue
                    writer.writerow(row)
                    stream.flush()
        except (OSError, ValueError, csv.Error) as error:
            self.error = str(error)
            print(f"ARRIETTY_FLIGHT_LOG_ERROR {error}", flush=True)
