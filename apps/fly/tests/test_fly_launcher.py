from pathlib import Path
import importlib.util
import io
import tempfile
import sys
import unittest
from unittest.mock import MagicMock, patch

PATH=Path(__file__).resolve().parents[1]/'tools/launch.py'
spec=importlib.util.spec_from_file_location('fly_launch',PATH)
launch=importlib.util.module_from_spec(spec);spec.loader.exec_module(launch)


class FlightLauncherTests(unittest.TestCase):
    def test_live_launch_uses_active_openxr_without_steamvr_process(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            app = root/'apps/fly'
            (app/'unreal/ArriettyUE/Content/Maps').mkdir(parents=True)
            (app/'unreal/ArriettyUE/Content/Maps/CesiumFly.umap').touch()
            editor = root/'engine/Engine/Binaries/Win64/UnrealEditor.exe'
            editor.parent.mkdir(parents=True)
            editor.touch()
            bridge, unreal = MagicMock(), MagicMock()
            bridge.poll.side_effect = [None, 0]
            unreal.wait.return_value = 0
            unreal.poll.return_value = 0
            calls = []
            def start(command, **kwargs):
                calls.append(command)
                if '--hardware' in command:
                    kwargs['stdout'].write('ARRIETTY_UE_BRIDGE_READY hardware=True\n')
                    kwargs['stdout'].flush()
                    return bridge
                (root/'logs/fly/training.log').write_text('fixture')
                return unreal
            with patch.object(launch, 'APP', app), patch.object(launch, 'ROOT', root), \
                 patch.object(launch, 'cesium_configuration', return_value=({'terrain_asset_id': 1, 'imagery_asset_id': 2}, 'fixture')), \
                 patch.object(launch.socket, 'socket'), \
                 patch.object(launch.subprocess, 'check_output', side_effect=AssertionError('SteamVR process check')), \
                 patch.object(launch.subprocess, 'Popen', side_effect=start):
                self.assertEqual(launch.launch_scene(root/'scene.json', {}, engine_root=root/'engine'), 0)
            self.assertIn('--hardware', calls[0])
            self.assertIn('-vr', calls[1])
            self.assertNotIn('-DisablePlugins=OpenXR', calls[1])

    def test_cancel_never_prepares_scene_or_launches(self):
        place=dict(country_ja='日本',region_ja='静岡県',name_ja='富士山')
        for answer in ('N','','yes',EOFError()):
            with patch.object(sys,'argv',['launch.py','Fuji']),patch.object(launch,'resolve',return_value=place),patch('builtins.input',side_effect=answer if isinstance(answer,Exception) else None,return_value=answer),patch.object(launch,'build_scene') as build,patch.object(launch,'launch_scene') as start,patch.object(launch,'cesium_configuration') as config,patch.object(sys,'stdout',io.StringIO()):
                self.assertEqual(launch.main(),0);build.assert_not_called();start.assert_not_called();config.assert_not_called()
