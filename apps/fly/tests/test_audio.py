from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from arrietty_ue.audio import audio_state
from arrietty_ue.bridge import Simulation
from arrietty_up.runtime import RuntimeState


class AudioTests(unittest.TestCase):
    def test_gliding_wind_independent_of_cadence_and_magnification(self):
        s=RuntimeState(ride_active=True,hmd_aligned=True,steering_tracking=True)
        s.flight.airborne=True; s.flight.airspeed_meters_per_second=24/3.6
        s.last_ftms_sample_seconds=10; s.cadence_rpm=70; s.power_watts=200
        a=audio_state(s,None,10,0)
        s.movement_magnification=10; s.world_speed_kmh=240
        self.assertEqual(a,audio_state(s,None,10,0))
        s.cadence_rpm=0; s.power_watts=0
        a=audio_state(s,None,10,0)
        self.assertTrue(a['active']); self.assertEqual(a['speed'],24)
        self.assertEqual(a['cadence'],0)

    def test_stale_sensor_stops_drive_but_keeps_gliding_wind(self):
        s=RuntimeState(ride_active=True,hmd_aligned=True,steering_tracking=True)
        s.flight.airborne=True; s.flight.airspeed_meters_per_second=7
        s.last_ftms_sample_seconds=10; s.cadence_rpm=80; s.power_watts=250
        a=audio_state(s,None,12,0)
        self.assertTrue(a['active']); self.assertGreater(a['speed'],20)
        self.assertEqual((a['cadence'],a['power']),(0,0))

    def test_readiness_alignment_and_terrain_gates(self):
        s=RuntimeState(ride_active=True,hmd_aligned=True,steering_tracking=True)
        terrain=SimpleNamespace(ready=True,message='',blocked=False)
        self.assertTrue(audio_state(s,terrain,10,0)['active'])
        for name in ('ride_active','hmd_aligned','steering_tracking'):
            setattr(s,name,False)
            self.assertFalse(audio_state(s,terrain,10,0)['active'])
            setattr(s,name,True)
        for kw in (dict(ready=False),dict(message='Loading terrain'),dict(blocked=True)):
            gate=SimpleNamespace(ready=True,message='',blocked=False)
            for k,v in kw.items(): setattr(gate,k,v)
            self.assertFalse(audio_state(s,gate,10,0)['active'])

    def test_bridge_audio_waits_for_button_and_alignment(self):
        world=dict(initial_heading_degrees=0.,output_name='audio-fixture',origin_latitude=0.,origin_longitude=0.)
        with tempfile.TemporaryDirectory() as td:
            sim=Simulation(world,log_path=Path(td)/'offline.csv')
            p=dict(play=True,buttons=0,aligned=0,hmd_valid=True,speed=24,power=200)
            try:
                self.assertFalse(sim.step(p,.02)['audio']['active'])
                out=sim.step(dict(p,buttons=1),.02)
                self.assertFalse(out['audio']['active'])
                p['aligned']=out['align_request']
                a=sim.step(p,.02)['audio']
                self.assertTrue(a['active']); self.assertEqual(a['cadence'],70)
                self.assertFalse(sim.step(dict(p,hmd_valid=False),.02)['audio']['active'])
                self.assertFalse(sim.step(dict(p,play=False),.02)['playing'])
            finally: sim.stop()


if __name__=='__main__': unittest.main()
