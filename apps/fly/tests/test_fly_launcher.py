from pathlib import Path
import importlib.util
import io
import sys
import unittest
from unittest.mock import patch

PATH=Path(__file__).resolve().parents[1]/'tools/launch.py'
spec=importlib.util.spec_from_file_location('fly_launch',PATH)
launch=importlib.util.module_from_spec(spec);spec.loader.exec_module(launch)


class FlightLauncherTests(unittest.TestCase):
    def test_cancel_never_prepares_scene_or_launches(self):
        place=dict(country_ja='日本',region_ja='静岡県',name_ja='富士山')
        for answer in ('N','','yes',EOFError()):
            with patch.object(sys,'argv',['launch.py','Fuji']),patch.object(launch,'resolve',return_value=place),patch('builtins.input',side_effect=answer if isinstance(answer,Exception) else None,return_value=answer),patch.object(launch,'build_scene') as build,patch.object(launch,'launch_scene') as start,patch.object(launch,'cesium_configuration') as config,patch.object(sys,'stdout',io.StringIO()):
                self.assertEqual(launch.main(),0);build.assert_not_called();start.assert_not_called();config.assert_not_called()
