"""Wired panel steering, independent of the HMD and OpenVR."""
from __future__ import annotations

import math
import os
import time

from . import constants as c
from .steering import SteeringController, SteeringSnapshot, TRACKING_STALE_SECONDS

DEADZONE = 0.15


class JoystickSteering:
    def __init__(self, axis: str = "x", invert: bool = False):
        if axis not in ("x", "y"):
            raise ValueError("joystick1_steering_axis must be x or y")
        self.axis = axis
        self.invert = invert
        self.running = False
        self._last_sample = None
        self._samples = 0
        self._armed = False
        self._tuning = False
        self._value = 0.0

    def start(self):
        self.running = True
        self._tuning = False
        self._samples = 0
        self.disconnect()
        return True

    def stop(self):
        self.running = False
        self.disconnect()
        return True

    def recenter(self):
        # Never calibrate a deflected stick as the new centre.
        self._armed = False
        self._value = 0.0

    def disconnect(self):
        self._last_sample = None
        self.recenter()

    def set_tuning(self, active: bool):
        if active != self._tuning:
            self.recenter()
        self._tuning = active

    def receive(self, sample, received_at: float):
        if self._last_sample is None or received_at - self._last_sample >= TRACKING_STALE_SECONDS:
            self.recenter()
        value = sample.joystick1[0 if self.axis == "x" else 1]
        if not math.isfinite(value) or not math.isfinite(received_at):
            self.disconnect()
            return
        self._last_sample = received_at
        self._samples += 1
        self._value = max(-1.0, min(1.0, value))
        if abs(self._value) <= DEADZONE:
            self._armed = True

    def snapshot(self, now_seconds=None):
        now = time.monotonic() if now_seconds is None else now_seconds
        fresh = self.running and self._last_sample is not None and 0 <= now - self._last_sample < TRACKING_STALE_SECONDS
        if not fresh:
            self.recenter()
        ready = fresh and (self._armed or self._tuning)
        angle = 0.0
        if ready and not self._tuning:
            value = self._value * (-1 if self.invert else 1)
            amount = max(0.0, abs(value) - DEADZONE) / (1.0 - DEADZONE)
            # Positive panel X is right; positive simulation yaw is left.
            angle = -math.copysign(amount * c.MAX_EFFECTIVE_STEERING_DEGREES, value)
        status = "J1 LOST" if not fresh else "J1 TUNE" if self._tuning else "J1 READY" if ready else "J1 CENTER"
        return SteeringSnapshot(
            status=status, message=status, tracking=ready, serial="",
            model="Wired Joystick 1", raw_angle_degrees=angle,
            effective_angle_degrees=angle, sample_count=self._samples,
            last_pose_seconds=self._last_sample or 0.0,
        )


def configured_steering():
    source = os.environ.get("ARRIETTY_STEERING_INPUT", "joystick1")
    if source == "vive":
        return SteeringController()
    if source != "joystick1":
        raise ValueError("steering_input must be joystick1 or vive")
    invert = os.environ.get("ARRIETTY_JOYSTICK1_STEERING_INVERT", "false").lower()
    if invert not in ("true", "false"):
        raise ValueError("joystick1_steering_invert must be true or false")
    return JoystickSteering(os.environ.get("ARRIETTY_JOYSTICK1_STEERING_AXIS", "x"), invert == "true")
