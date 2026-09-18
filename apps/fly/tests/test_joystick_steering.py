import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from arrietty_up.controller_protocol import ControllerSample
from arrietty_up.joystick_steering import JoystickSteering, configured_steering
from arrietty_up.steering import SteeringController
from arrietty_up.serial_controller import ControllerEvent, ControllerEventType
from arrietty_ue.bridge import OfflineDevice, Simulation, ue_pose

WORLD = dict(initial_heading_degrees=0., output_name='fixture', origin_latitude=-8.5, origin_longitude=179.2)


def sample(x=0, y=0, buttons=0):
    return ControllerSample(1, round(x * 32767), round(y * 32767), 0, 0, buttons)


class JoystickSteeringTests(unittest.TestCase):
    def test_analog_deadzone_sign_saturation_and_neutral(self):
        steering = JoystickSteering()
        steering.start()
        steering.receive(sample(1), 1.)
        self.assertFalse(steering.snapshot(1.).tracking)
        steering.receive(sample(), 1.1)
        self.assertTrue(steering.snapshot(1.1).tracking)
        for x, expected in [(0.1, 0), (.575, -7.5), (1, -15), (-1, 15), (0, 0)]:
            steering.receive(sample(x), 1.2)
            self.assertAlmostEqual(steering.snapshot(1.2).effective_angle_degrees, expected, places=3)

    def test_axis_and_inversion(self):
        steering = JoystickSteering('y', True)
        steering.start()
        steering.receive(sample(), 1.)
        steering.receive(sample(1, 0), 1.1)
        self.assertEqual(steering.snapshot(1.1).effective_angle_degrees, 0)
        steering.receive(sample(0, 1), 1.2)
        self.assertEqual(steering.snapshot(1.2).effective_angle_degrees, 15)

    def test_stale_disconnect_recenter_and_stop_require_neutral(self):
        for reset in ('stale', 'gap', 'disconnect', 'recenter', 'stop'):
            with self.subTest(reset=reset):
                steering = JoystickSteering()
                steering.start()
                steering.receive(sample(), 1.)
                steering.receive(sample(1), 1.1)
                self.assertTrue(steering.snapshot(1.1).tracking)
                if reset == 'stale':
                    self.assertFalse(steering.snapshot(1.6).tracking)
                elif reset == 'gap':
                    pass  # No snapshot between packets: gap must still disarm.
                else:
                    getattr(steering, reset)()
                if reset == 'stop':
                    self.assertFalse(steering.snapshot(1.1).tracking)
                    steering.start()
                steering.receive(sample(1), 1.7)
                self.assertFalse(steering.snapshot(1.7).tracking)
                self.assertEqual(steering.snapshot(1.7).effective_angle_degrees, 0)
                steering.receive(sample(), 1.8)
                self.assertTrue(steering.snapshot(1.8).tracking)

    def test_tuning_has_no_rudder_and_exit_requires_neutral(self):
        steering = JoystickSteering()
        steering.start()
        steering.receive(sample(), 1.)
        steering.set_tuning(True)
        steering.receive(sample(1), 1.1)
        self.assertTrue(steering.snapshot(1.1).tracking)
        self.assertEqual(steering.snapshot(1.1).effective_angle_degrees, 0)
        self.assertFalse(steering.snapshot(1.6).tracking)
        steering.set_tuning(False)
        steering.receive(sample(1), 1.7)
        self.assertFalse(steering.snapshot(1.7).tracking)
        steering.receive(sample(), 1.8)
        self.assertTrue(steering.snapshot(1.8).tracking)

    def test_configuration_defaults_and_legacy_opt_in(self):
        with patch.dict(os.environ, {}, clear=True):
            self.assertIsInstance(configured_steering(), JoystickSteering)
            os.environ['ARRIETTY_STEERING_INPUT'] = 'vive'
            self.assertIsInstance(configured_steering(), SteeringController)
            os.environ['ARRIETTY_STEERING_INPUT'] = 'typo'
            with self.assertRaises(ValueError):
                configured_steering()
            os.environ['ARRIETTY_STEERING_INPUT'] = 'joystick1'
            os.environ['ARRIETTY_JOYSTICK1_STEERING_INVERT'] = 'typo'
            with self.assertRaises(ValueError):
                configured_steering()
        with self.assertRaises(ValueError):
            JoystickSteering('z')

    def test_airborne_rudder_adds_to_aileron_without_changing_bank(self):
        from arrietty_up.runtime import RuntimeState
        results = []
        for x in (0, 1, -1):
            state = RuntimeState(ride_active=True, flight_enabled=True)
            state.steering = JoystickSteering()
            state.steering.start()
            state.steering.receive(sample(), 1.)
            state.steering.receive(sample(x), 1.1)
            state.update_steering_state(1.1)
            state.flight.airborne = True
            state.flight.altitude_meters = 100
            state.flight.airspeed_meters_per_second = 24 / 3.6
            state.flight.bank_degrees = -10
            state.digital_controls.roll_right_degrees = 10
            state.advance_flight(.05, 1.1)
            results.append(state)
        bank_only, bank_and_right, bank_and_left = results
        self.assertGreater(ue_pose(bank_and_right)[4], ue_pose(bank_only)[4])
        self.assertLess(ue_pose(bank_and_left)[4], ue_pose(bank_only)[4])
        self.assertEqual(bank_only.flight.bank_degrees, bank_and_right.flight.bank_degrees)
        self.assertEqual(bank_only.flight.pitch_degrees, bank_and_right.flight.pitch_degrees)

    def test_terrain_readiness_precedes_any_hardware(self):
        world = dict(WORLD, terrain_required=True, navigation_radius_m=10000,
                     start_mode='air', start_agl_m=100,
                     water_geojson={'type': 'Polygon', 'coordinates': []})
        with patch('arrietty_ue.bridge.RuntimeState', side_effect=AssertionError('hardware created')):
            sim = Simulation(world, hardware=True)
            sim.step(dict(play=True), .05, 1.)
            self.assertIsNone(sim.state)

    def test_live_bridge_without_vive_and_motion_gates(self):
        # Exercise the hardware branch with all physical services replaced.
        with tempfile.TemporaryDirectory() as td, patch.dict(os.environ, {}, clear=True), \
             patch('arrietty_up.runtime.SerialController.start', side_effect=AssertionError('serial')), \
             patch('arrietty_up.runtime.BluetoothManager.start', side_effect=AssertionError('BLE')), \
             patch('arrietty_up.runtime.FanController.start', side_effect=AssertionError('fan')), \
             patch('arrietty_up.steering.SteeringController.start', side_effect=AssertionError('OpenVR')):
            from arrietty_up.runtime import RuntimeState
            def state_factory():
                state = RuntimeState()
                for name in ('serial', 'bluetooth', 'fan', 'voice'):
                    setattr(state, name, OfflineDevice())
                return state
            with patch('arrietty_ue.bridge.RuntimeState', side_effect=state_factory):
                sim = Simulation(WORLD, hardware=True, log_path=Path(td)/'ride.csv')
                p = dict(play=True, hmd_valid=True, aligned=0, alignment_bearing=180.)
                def frame(now, x=None, buttons=0, event=None):
                    events = [event] if event else ([] if x is None else [ControllerEvent(
                        ControllerEventType.SAMPLE, sample=sample(x, buttons=buttons), received_at=now)])
                    with patch.object(sim.state.serial, 'drain_events', return_value=events):
                        sim.state.ftms_speed_kmh = 24
                        sim.state.cadence_rpm = 70
                        sim.state.last_ftms_sample_seconds = now
                        return sim.step(p, .05, now)
                try:
                    sim.step(dict(p, play=False), .01, 1.)
                    self.assertIsNone(sim.state)
                    sim.step(p, .01, 1.)
                    self.assertIsInstance(sim.state.steering, JoystickSteering)
                    frame(1.01, 0)
                    out = frame(1.02, 0, buttons=1)
                    p['aligned'] = out['align_request']
                    frame(1.03, 0)  # Alignment re-arms centre on next packet.
                    frame(1.04, 0)
                    before = ue_pose(sim.state)[4]
                    frame(1.05, 1)
                    self.assertGreater(ue_pose(sim.state)[4], before)  # Right turn.
                    before = ue_pose(sim.state)[:3]
                    p['hmd_valid'] = False
                    frame(1.06, 1)
                    self.assertEqual(ue_pose(sim.state)[:3], before)
                    p['hmd_valid'] = True
                    frame(1.56)
                    self.assertEqual(ue_pose(sim.state)[:3], before)
                    self.assertFalse(sim.state.steering_tracking)
                    frame(1.57, 1)
                    self.assertEqual(ue_pose(sim.state)[:3], before)
                    frame(1.58, 0)
                    self.assertNotEqual(ue_pose(sim.state)[:3], before)
                    before = ue_pose(sim.state)[:3]
                    frame(1.59, event=ControllerEvent(ControllerEventType.DISCONNECTED))
                    self.assertEqual(ue_pose(sim.state)[:3], before)
                    frame(2.2, event=ControllerEvent(ControllerEventType.SAMPLE, sample=sample(), received_at=1.6))
                    self.assertEqual(ue_pose(sim.state)[:3], before)  # Queued old data.
                    self.assertFalse(sim.state.steering_tracking)
                finally:
                    sim.stop()
                self.assertIsNone(sim.state)
