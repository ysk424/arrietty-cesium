from pathlib import Path
import os
import subprocess
import sys
import unittest
from unittest.mock import patch

SHARED=Path(__file__).resolve().parents[3]/'shared/python'
sys.path.insert(0,str(SHARED))
from arrietty_geo import flight_scene
from arrietty_geo.live import live_session


class SharedGeographyTests(unittest.TestCase):
    def test_coast_lake_and_island_hole(self):
        from shapely.geometry import Point,shape
        def geometry(points): return [{'lon':x,'lat':y} for x,y in points]
        coast=geometry([(-.005,-.005),(.005,-.005),(.005,.005),(-.005,.005),(-.005,-.005)])
        outer=geometry([(-.002,-.002),(.002,-.002),(.002,.002),(-.002,.002),(-.002,-.002)])
        inner=geometry([(-.0005,-.0005),(.0005,-.0005),(.0005,.0005),(-.0005,.0005),(-.0005,-.0005)])
        data={'elements':[{'type':'way','tags':{'natural':'coastline'},'geometry':coast},
            {'type':'relation','tags':{'natural':'water'},'members':[{'role':'outer','geometry':outer},{'role':'inner','geometry':inner}]}]}
        with patch.object(flight_scene,'cached',return_value=data):
            water,_=flight_scene.surface_geometry(dict(latitude=0,longitude=0,surface='land'),1000)
        water=shape(water)
        self.assertTrue(water.covers(Point(.02,0)))
        self.assertFalse(water.covers(Point(.004,0)))
        self.assertTrue(water.covers(Point(.001,0)))
        self.assertFalse(water.covers(Point(0,0)))

    @unittest.skipUnless(os.name=='nt','Windows device ownership')
    def test_live_mutex_excludes_other_process_and_releases(self):
        code="from arrietty_geo.live import live_session\nfrom arrietty_geo.common import PlaceError\ntry:\n    with live_session(True): pass\nexcept PlaceError: raise SystemExit(17)\n"
        env=os.environ.copy();env['PYTHONPATH']=str(SHARED)
        def child(): return subprocess.run([sys.executable,'-c',code],env=env,capture_output=True,timeout=10).returncode
        with live_session(True): self.assertEqual(child(),17)
        self.assertEqual(child(),0)
