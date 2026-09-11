from dataclasses import replace
import math
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from arrietty_ue.terrain import FlightTerrain, HeightPatch, TerrainUnavailable
from arrietty_ue.bridge import Simulation
from arrietty_up.runtime import RuntimeState
from arrietty_up.flight import step_human_powered_flight
from arrietty_geo.flight_scene import local_instant


WORLD=dict(terrain_required=True,water_geojson={'type':'Polygon','coordinates':[]},start_mode='air',start_agl_m=100,navigation_radius_m=10000,initial_heading_degrees=180,origin_longitude=138.7,origin_latitude=35.3,output_name='fixture',timezone='Asia/Tokyo')


def packet(fn=lambda e,n:0):
    return {'terrain':{'ready':True,'origin':[138.7,35.3,1500.],
        'patch':dict(east=-60,north=-60,step=10,count=13,
            heights=[float(fn(e,n)) for n in range(-60,61,10) for e in range(-60,61,10)])}}


class TerrainTests(unittest.TestCase):
    def environment(self,fn=lambda e,n:0,**values):
        terrain=FlightTerrain(dict(WORLD,**values));terrain.receive(packet(fn));return terrain

    def test_magnification_scales_translation_without_speed_rotation_or_spawn_height(self):
        states=[]
        for mag in (1,10):
            env=self.environment(movement_magnification=mag)
            s=RuntimeState(ride_active=True,heading_degrees=180,power_watts=0)
            env.initialize_ride(s)
            self.assertEqual(s.altitude_agl_m,100)
            s.digital_controls.pitch_degrees=-3;s.digital_controls.roll_right_degrees=4
            env.advance(s,.05,1)
            states.append(s)
        a,b=states
        self.assertAlmostEqual(b.position_x_meters,a.position_x_meters*10)
        self.assertAlmostEqual(b.position_y_meters,a.position_y_meters*10)
        self.assertAlmostEqual(b.flight.altitude_meters-100,(a.flight.altitude_meters-100)*10)
        self.assertEqual(b.speed_kmh,a.speed_kmh)
        self.assertEqual(b.heading_degrees,a.heading_degrees)
        self.assertEqual(b.flight.pitch_degrees,a.flight.pitch_degrees)
        self.assertEqual(b.flight.bank_degrees,a.flight.bank_degrees)
        self.assertAlmostEqual(b.distance_meters,a.distance_meters*10)
        self.assertAlmostEqual(b.world_speed_kmh,a.world_speed_kmh*10)
        self.assertAlmostEqual(b.recovery_path_distance_meters,b.distance_meters)

    def test_magnified_ground_roll_follows_real_ground_height(self):
        env=self.environment(lambda e,n:n*.02,start_mode='ground',movement_magnification=10)
        s=RuntimeState(ride_active=True,speed_kmh=18,heading_degrees=180)
        env.initialize_ride(s);env.advance(s,.05,1)
        self.assertAlmostEqual(s.position_y_meters,2.5)
        self.assertAlmostEqual(s.flight.altitude_meters,.05)
        self.assertAlmostEqual(s.world_speed_kmh,180)
        self.assertAlmostEqual(s.altitude_agl_m,0)

    def test_magnified_move_checks_ridge_between_clear_endpoints(self):
        env=self.environment(lambda e,n:150 if n==10 else 0,movement_magnification=10)
        s=RuntimeState(ride_active=True);env.initialize_ride(s)
        def advance(delta,now):s.position_y_meters=2;return 2
        with patch.object(RuntimeState,'advance_flight',side_effect=advance):
            self.assertEqual(env.advance(s,.05,1),0)
        self.assertTrue(env.blocked)
        self.assertEqual(s.position_y_meters,0)
        self.assertEqual(s.distance_meters,0)
        self.assertEqual(s.world_speed_kmh,0)

    def test_magnified_descent_uses_world_impact_speed(self):
        env=self.environment(movement_magnification=10)
        s=RuntimeState(ride_active=True);env.initialize_ride(s);s.flight.altitude_meters=1
        def advance(delta,now):
            s.flight.altitude_meters-=.2;s.flight.vertical_speed_meters_per_second=-1
            return 0
        with patch.object(RuntimeState,'advance_flight',side_effect=advance):env.advance(s,.05,1)
        self.assertTrue(env.blocked)
        self.assertEqual(s.flight.altitude_meters,1)
        self.assertEqual(s.world_vertical_speed_mps,0)

    def test_magnified_missing_terrain_and_radius_do_not_advance(self):
        for radius,blocked in ((10000,False),(30,True)):
            env=self.environment(movement_magnification=10,navigation_radius_m=radius)
            s=RuntimeState(ride_active=True);env.initialize_ride(s)
            def advance(delta,now):s.position_y_meters=10;return 10
            with patch.object(RuntimeState,'advance_flight',side_effect=advance):env.advance(s,.05,1)
            self.assertEqual(env.blocked,blocked)
            self.assertEqual(s.position_y_meters,0)
            self.assertEqual(s.flight.altitude_meters,100)
            self.assertEqual(s.world_speed_kmh,0)

    def test_magnification_rejects_invalid_values(self):
        for value in (0,-1,11,float('nan'),float('inf'),True):
            with self.assertRaises(RuntimeError):self.environment(movement_magnification=value)

    def test_height_patch_rejects_holes_and_invalid_values(self):
        data=packet()['terrain']['patch']
        with self.assertRaises(TerrainUnavailable): HeightPatch(dict(data,heights=[float('nan')]*169))
        with self.assertRaises(TerrainUnavailable): HeightPatch(dict(data,heights=[0]*168))
        with self.assertRaises(TerrainUnavailable): HeightPatch(data).sample(100,0)

    def test_terrain_height_is_not_geoid_corrected_twice(self):
        env=self.environment();s=RuntimeState()
        env.initialize_ride(s)
        self.assertEqual(s.altitude_ellipsoid_m,1600)
        self.assertAlmostEqual(s.altitude_agl_m,100)
        self.assertNotEqual(s.altitude_msl_m,s.altitude_ellipsoid_m)
        self.assertGreater(s.altitude_msl_m,1500)

    def test_flight_below_origin_does_not_land_at_zero(self):
        env=self.environment(lambda e,n:-300)
        s=RuntimeState(ride_active=True,flight_enabled=True,terrain_enabled=True)
        s.flight.airborne=True;s.flight.altitude_meters=-100;s.flight.airspeed_meters_per_second=7
        s.reset_recovery_trail()
        env.advance(s,.05,1)
        self.assertTrue(s.flight.airborne)
        self.assertLess(s.flight.altitude_meters,0)
        self.assertGreater(s.altitude_agl_m,150)

    def test_aircraft_does_not_follow_rising_ground(self):
        env=self.environment(lambda e,n:n*.1)
        s=RuntimeState(ride_active=True,flight_enabled=True,terrain_enabled=True,heading_degrees=180)
        env.initialize_ride(s)
        def advance(delta,now):
            s.position_y_meters+=10
            return 10
        with patch.object(RuntimeState,'advance_flight',side_effect=advance): env.advance(s,.05,1)
        self.assertAlmostEqual(s.flight.altitude_meters,100)
        self.assertAlmostEqual(s.altitude_agl_m,99)

    def test_ground_follows_slope_and_preserves_height_on_mode_change(self):
        env=self.environment(lambda e,n:-100+n*.02,start_mode='ground')
        s=RuntimeState(ride_active=True,speed_kmh=18,heading_degrees=180)
        env.initialize_ride(s);env.advance(s,.05,1)
        self.assertAlmostEqual(s.flight.altitude_meters,-99.995)
        s.toggle_flight()
        self.assertAlmostEqual(s.flight.altitude_meters,-99.995)

    def test_ridge_collision_rolls_back_and_requires_recovery(self):
        env=self.environment(lambda e,n:max(0,n)*2)
        s=RuntimeState(ride_active=True,flight_enabled=True,terrain_enabled=True)
        env.initialize_ride(s);s.flight.altitude_meters=2;s.reset_recovery_trail()
        def advance(delta,now): s.position_y_meters=10;return 10
        with patch.object(RuntimeState,'advance_flight',side_effect=advance): self.assertEqual(env.advance(s,.05,1),0)
        self.assertEqual(s.position_y_meters,0);self.assertEqual(s.flight.altitude_meters,2)
        self.assertTrue(env.blocked)

    def test_missing_ahead_tile_freezes_position_without_changing_height(self):
        env=self.environment();s=RuntimeState(ride_active=True,flight_enabled=True)
        env.initialize_ride(s)
        def advance(delta,now): s.position_y_meters=100;return 100
        with patch.object(RuntimeState,'advance_flight',side_effect=advance): env.advance(s,.05,1)
        self.assertEqual(s.position_y_meters,0);self.assertEqual(s.flight.altitude_meters,100)
        self.assertFalse(env.blocked);self.assertIn('Loading',env.message)

    def test_recovery_restores_three_dimensional_flight_state(self):
        env=self.environment();s=RuntimeState(ride_active=True,flight_enabled=True)
        env.initialize_ride(s)
        for i in range(1,7):
            s.position_y_meters=i;s.flight.altitude_meters=100+i;s.record_recovery_pose(1)
        self.assertTrue(env.recover(s))
        self.assertEqual(s.position_y_meters,4);self.assertEqual(s.flight.altitude_meters,104)
        self.assertTrue(s.flight.airborne)

    def test_water_contact_is_not_a_landing(self):
        water={'type':'Polygon','coordinates':[[[138,35],[139,35],[139,36],[138,36],[138,35]]]}
        env=self.environment(water_geojson=water);s=RuntimeState(ride_active=True,flight_enabled=True)
        env.initialize_ride(s);s.flight.altitude_meters=1
        def advance(delta,now): s.flight.altitude_meters=-1;s.flight.vertical_speed_meters_per_second=-1;return 0
        with patch.object(RuntimeState,'advance_flight',side_effect=advance): env.advance(s,.05,1)
        self.assertTrue(env.blocked);self.assertTrue(s.flight.airborne);self.assertEqual(s.flight.altitude_meters,1)

    def test_gentle_landing_uses_elevated_current_ground(self):
        env=self.environment(lambda e,n:50);s=RuntimeState(ride_active=True,flight_enabled=True)
        env.initialize_ride(s);s.flight.altitude_meters=51
        def advance(delta,now): s.flight.altitude_meters=49;s.flight.vertical_speed_meters_per_second=-1;return 0
        with patch.object(RuntimeState,'advance_flight',side_effect=advance): env.advance(s,.05,1)
        self.assertFalse(s.flight.airborne);self.assertEqual(s.flight.altitude_meters,50)

    def test_geometry_not_ready_cannot_start_hardware(self):
        with tempfile.TemporaryDirectory() as td:
            sim=Simulation(WORLD,hardware=True,log_path=Path(td)/'test.csv')
            with patch.object(RuntimeState,'prepare_devices') as devices:
                sim.step(dict(play=True,hmd_valid=True,terrain={'ready':False}),.02)
                self.assertIsNone(sim.state);devices.assert_not_called()

    def test_local_timezone_and_dst(self):
        self.assertEqual(local_instant('2026-09-12','12:00','Asia/Tokyo').utcoffset().total_seconds(),9*3600)
        self.assertEqual(local_instant('2026-07-12','12:00','Europe/Ljubljana').utcoffset().total_seconds(),2*3600)
        with self.assertRaises(RuntimeError): local_instant('2026-03-08','02:30','America/New_York')
        with self.assertRaises(RuntimeError): local_instant('2026-11-01','01:30','America/New_York')

    def test_takeoff_on_gentle_rising_runway_establishes_clearance(self):
        from arrietty_up.flight import initialize_human_powered_flight
        env=self.environment(lambda e,n:n*.02,start_mode='ground')
        s=RuntimeState(ride_active=True,flight_enabled=True,terrain_enabled=True,heading_degrees=180,power_watts=250,cadence_rpm=70)
        env.initialize_ride(s)
        s.flight=initialize_human_powered_flight(27)
        s.digital_controls.pitch_degrees=3
        maximum=0
        for i in range(250):
            # Shift the complete terrain patch as the simulated rider advances.
            center=round(s.position_y_meters/20)*20
            p=packet(lambda e,n:n*.02)
            p['terrain']['patch']['north']=center-60
            p['terrain']['patch']['heights']=[float(n*.02) for n in range(center-60,center+61,10) for e in range(-60,61,10)]
            env.receive(p);env.advance(s,.05,i*.05)
            maximum=max(maximum,s.altitude_agl_m)
        self.assertFalse(env.blocked,env.message)
        self.assertTrue(s.flight.airborne)
        self.assertGreater(maximum,1)

    def test_recovery_restores_flight_mode_after_landing_and_switching_modes(self):
        env=self.environment();s=RuntimeState(ride_active=True)
        env.initialize_ride(s)
        s.position_y_meters=1;s.record_recovery_pose(1)
        s.position_y_meters=3;s.flight.airborne=False;s.flight_enabled=False;s.flight.altitude_meters=0;s.record_recovery_pose(2)
        self.assertTrue(env.recover(s))
        self.assertTrue(s.flight.airborne);self.assertTrue(s.flight_enabled)
        self.assertEqual(s.flight.altitude_meters,100)

    def test_flight_radius_blocks_without_teleporting(self):
        env=self.environment(navigation_radius_m=5);s=RuntimeState(ride_active=True,flight_enabled=True)
        env.initialize_ride(s)
        def advance(delta,now): s.position_y_meters=10;return 10
        with patch.object(RuntimeState,'advance_flight',side_effect=advance): env.advance(s,.05,1)
        self.assertTrue(env.blocked);self.assertIn('boundary',env.message)
        self.assertEqual(s.position_y_meters,0)
