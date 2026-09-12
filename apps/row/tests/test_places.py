import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import places
import launch


class PlaceTests(unittest.TestCase):
    def test_only_explicit_y_confirms(self):
        for answer in ('Y','y',' Y ','\ty\n'):
            self.assertTrue(places.confirmed(answer))
        for answer in ('N','n','','yes','はい','1',None,'Y\nN'):
            self.assertFalse(places.confirmed(answer))

    def test_n_or_eof_never_loads_terrain_or_launches(self):
        place={'country_ja':'タイ','region_ja':'クラビ県','name_ja':'ホン島'}
        for response in ('N','','YES',EOFError()):
            with patch.object(sys,'argv',['launch.py','Koh Hong']), patch.object(launch,'resolve_place',return_value=place), \
                 patch('builtins.input',side_effect=response if isinstance(response,Exception) else None,return_value=response), \
                 patch.object(launch,'request_json') as net, patch.object(launch,'build_scene') as build, \
                 patch.object(launch.subprocess,'call') as ue, patch.object(sys,'stdout',io.StringIO()):
                self.assertEqual(launch.main(),0)
                net.assert_not_called();build.assert_not_called();ue.assert_not_called()

    def test_y_prepares_once_without_launching_when_requested(self):
        place={'country_ja':'タイ','region_ja':'クラビ県','name_ja':'ホン島'}
        scene={'water_height_msl_m':0,'water_height_ellipsoid_m':-24.6,'elevation_source':'test'}
        with tempfile.TemporaryDirectory() as folder, patch.object(sys,'argv',['launch.py','Koh Hong','--prepare-only']), \
             patch.object(launch,'ROOT',Path(folder)), patch.object(launch,'resolve_place',return_value=place), \
             patch.dict(launch.os.environ,{'CESIUM_ION_TOKEN':'test-only'}), patch('builtins.input',return_value='Y'), \
             patch.object(launch,'request_json') as net, patch.object(launch,'build_scene',return_value=(Path(folder)/'scene.json',scene)) as build, \
             patch.object(launch.subprocess,'call') as ue, patch.object(sys,'stdout',io.StringIO()):
            self.assertEqual(launch.main(),0)
            self.assertEqual(net.call_count,2);build.assert_called_once();ue.assert_not_called()

    def test_nominatim_jsonv2_category_is_recognized(self):
        from shapely.geometry import mapping,Polygon
        data=[{'category':'water','type':'lake','geojson':mapping(Polygon([(0,0),(1,0),(1,1),(0,1)])),
               'osm_type':'relation','osm_id':646}]
        with patch.object(places,'cache_json',return_value=data):
            geometry,source=places.lake_geometry({'osm_query':'lake','name':'lake','country':'test','latitude':.5,'longitude':.5})
        self.assertEqual(geometry.area,1)
        self.assertEqual(source,'https://www.openstreetmap.org/relation/646')

    def test_unknown_lake_never_becomes_sea_level(self):
        lake={'water_type':'lake','water_level_msl_m':None,'elevation_source':None}
        with self.assertRaises(places.PlaceError):
            places.water_height(lake)
        self.assertEqual(places.water_height(lake,1042)[0],1042)
        self.assertEqual(places.water_height({'water_type':'sea'})[0],0)

    def test_lake_source_must_be_in_search_evidence(self):
        lake={'water_type':'lake','water_level_msl_m':475,'elevation_source':'https://example.com/elevation','search_sources':[]}
        with self.assertRaises(places.PlaceError):
            places.water_height(lake)
        lake['search_sources']=[lake['elevation_source']]
        self.assertEqual(places.water_height(lake)[0],475)

    def test_invalid_numbers_and_terminal_controls(self):
        for value in (True,float('nan'),float('inf'),-451,6501,'475'):
            with self.assertRaises(places.PlaceError):
                places.number(value,-450,6500,'height')
        for value in ('Koh\x1b[2J Hong','a\nb','',None):
            with self.assertRaises(places.PlaceError):
                places.safe_text(value,'query')

    def test_geoid_sign_and_high_lake_datum(self):
        transform=places.geoid_transformer()
        koh=transform.transform(98.6905,8.0705,0,errcheck=True)[2]
        bled=transform.transform(14.09,46.36,475,errcheck=True)[2]
        high=transform.transform(14.09,46.36,1475,errcheck=True)[2]
        self.assertAlmostEqual(koh,-24.626015,places=4)
        self.assertAlmostEqual(bled,522.470032,places=4)
        self.assertAlmostEqual(high-bled,1000,places=8)

    def test_coastline_orientation_and_island_hole(self):
        from shapely.geometry import Point
        data={'elements':[{'geometry':[{'lon':x,'lat':y} for x,y in [(-100,-100),(100,-100),(100,100),(-100,100),(-100,-100)]]}]}
        with patch.object(places,'cache_json',return_value=data):
            water,_=places.ocean_geometry({'latitude':0,'longitude':0},lambda x,y:(x,y),1000)
        self.assertFalse(water.covers(Point(0,0)))
        self.assertTrue(water.covers(Point(500,0)))
        self.assertAlmostEqual(water.area,4000000-40000)

    def test_scene_keeps_lake_hole_and_mean_height(self):
        from shapely.geometry import Polygon
        lake=Polygon([(14.08,46.355),(14.10,46.355),(14.10,46.365),(14.08,46.365)],
                     [[(14.089,46.359),(14.091,46.359),(14.091,46.361),(14.089,46.361)]])
        place={'water_type':'lake','latitude':46.36,'longitude':14.09,'launch_latitude':46.36,'launch_longitude':14.095,
               'name':'Fixture lake','country':'Fixture','water_level_msl_m':1475,'elevation_source':'https://example.com/lake',
               'search_sources':['https://example.com/lake']}
        with tempfile.TemporaryDirectory() as folder, patch.object(places,'CACHE',Path(folder)), \
             patch.object(places,'lake_geometry',return_value=(lake,'fixture')):
            path,scene=places.build_scene(place)
            self.assertTrue(path.is_file())
            self.assertEqual(scene['water_height_msl_m'],1475)
            self.assertGreater(scene['water_height_ellipsoid_m'],1500)
            self.assertTrue(any(len(p)>1 for p in scene['water_polygons_m']))
            self.assertEqual(Path(scene['water_mask_path']).stat().st_size,2048**2)
            self.assertNotIn('token',json.dumps(scene).lower())
            self.assertLess(scene['surface_coefficients'][2],0)
            self.assertLess(scene['surface_coefficients'][4],0)

    def test_lake_spawn_avoids_shore_slope_and_retains_narrow_lakes(self):
        from shapely.geometry import Polygon,Point
        from shapely.ops import transform
        from pyproj import Transformer
        for half_width,minimum_clearance in ((.004,149),(.0008,39)):
            lake=Polygon([(-half_width,-.008),(half_width,-.008),(half_width,.008),(-half_width,.008)])
            place={'water_type':'lake','latitude':0,'longitude':0,'launch_latitude':0,'launch_longitude':half_width,
                   'name':'Fixture lake','country':'Fixture','water_level_msl_m':1884,'elevation_source':'https://example.com/lake',
                   'search_sources':['https://example.com/lake']}
            with tempfile.TemporaryDirectory() as folder, patch.object(places,'CACHE',Path(folder)), \
                 patch.object(places,'lake_geometry',return_value=(lake,'fixture')):
                _,scene=places.build_scene(place)
                project=Transformer.from_crs('EPSG:4326','+proj=aeqd +lat_0=0 +lon_0=0 +datum=WGS84 +units=m',always_xy=True)
                shore=transform(project.transform,lake).boundary
                origin=Point(project.transform(scene['origin_longitude'],scene['origin_latitude']))
                self.assertGreaterEqual(origin.distance(shore),minimum_clearance)
                self.assertEqual(scene['water_height_msl_m'],1884)
                for lon,lat,_ in scene['height_probes']:
                    self.assertTrue(lake.covers(Point(lon,lat)))


if __name__=='__main__':
    unittest.main()
