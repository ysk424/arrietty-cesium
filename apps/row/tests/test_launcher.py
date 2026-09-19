"""Row magnification through both launcher layers, without network or hardware."""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import launch


class RowLauncherTests(unittest.TestCase):
    def test_invalid_magnification_rejected_before_place_lookup(self):
        for value in ('0','.9','10.1','nan','inf','-inf'):
            with self.subTest(value=value), patch.object(sys,'argv',['launch.py','Fixture','--mag='+value]), \
                 patch.object(launch,'resolve_place') as resolve:
                with self.assertRaises(launch.PlaceError): launch.main()
                resolve.assert_not_called()

    def test_magnification_reaches_ue_after_explicit_y(self):
        place=dict(country_ja='Fixture',region_ja='Fixture',name_ja='Fixture')
        scene=dict(water_height_msl_m=0,water_height_ellipsoid_m=0,elevation_source='fixture')
        for option in ([],['--mag','2.5'],['--magnification','10'],['--legacy-water']):
            with self.subTest(option=option), tempfile.TemporaryDirectory() as td:
                root=Path(td);app=root/'apps/row';project=app/'unreal/ArriettyCesium'
                for file in (root/'engine/Engine/Binaries/Win64/UnrealEditor.exe',
                             project/'Content/Row/Maps/CesiumRow.umap',project/'Binaries/Win64/UnrealEditor-ArriettyRow.dll'):
                    file.parent.mkdir(parents=True,exist_ok=True);file.touch()
                (root/'logs').mkdir()
                with patch.object(sys,'argv',['launch.py','Fixture','--demo','--engine-root',str(root/'engine'),*option]), \
                     patch.object(launch,'ROOT',app),patch.object(launch,'WORKSPACE',root), \
                     patch.dict(os.environ,{'CESIUM_ION_TOKEN':'test-only','OPENAI_API_KEY':'test-only'}), \
                     patch.object(launch,'resolve_place',return_value=place),patch('builtins.input',return_value='Y'), \
                     patch.object(launch,'request_json'),patch.object(launch,'build_scene',return_value=(root/'scene.json',scene)), \
                     patch('arrietty_geo.live.live_session',return_value=contextlib.nullcontext()) as session, \
                     patch.object(launch.subprocess,'call',return_value=0) as start,patch.object(sys,'stdout',io.StringIO()):
                    self.assertEqual(launch.main(),0)
                    command=start.call_args.args[0]
                    mag=float(option[-1]) if option and option[0]!='--legacy-water' else 1.
                    self.assertEqual(float(next(arg.split('=',1)[1] for arg in command if arg.startswith('-RowMagnification='))),mag)
                    self.assertEqual('-RowLegacyWater' in command,'--legacy-water' in option)
                    self.assertIn('-DisablePlugins=OpenXR',command)
                    self.assertNotIn('test-only',' '.join(command))
                    self.assertNotIn('OPENAI_API_KEY',start.call_args.kwargs['env'])
                    session.assert_called_once_with(False)

    @unittest.skipUnless(os.name=='nt','Windows PowerShell integration')
    def test_powershell_alias_and_decimal_transport(self):
        workspace=Path(__file__).resolve().parents[3]
        for shell in ('powershell.exe','pwsh.exe'):
            if not shutil.which(shell): continue
            for option in ([],['-mag','2.5'],['-Magnification','10'],['-LegacyWater']):
                with self.subTest(shell=shell,option=option),tempfile.TemporaryDirectory() as td:
                    root=Path(td);directory=root/'apps/row/tools';directory.mkdir(parents=True)
                    script=(workspace/'row.ps1').read_text(encoding='utf-8-sig')
                    script=script.replace("$python=Join-Path $PSScriptRoot 'apps/row/.venv/Scripts/python.exe'", "$python='"+sys.executable.replace("'","''")+"'")
                    (root/'row.ps1').write_text(script,encoding='utf-8-sig')
                    (directory/'launch.py').write_text('import sys,json;print("ARGV="+json.dumps(sys.argv[1:]))',encoding='utf-8')
                    result=subprocess.run([shell,'-NoProfile','-ExecutionPolicy','Bypass','-Command',
                        "[cultureinfo]::CurrentCulture='de-DE'; & '"+str(root/'row.ps1').replace("'","''")+"' 'Lake Bled' -Demo "+' '.join(option)],
                        capture_output=True,text=True,errors='replace',timeout=30)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                    args=json.loads(next(line[5:] for line in result.stdout.splitlines() if line.startswith('ARGV=')))
                    self.assertEqual(args[0],'Lake Bled')
                    self.assertEqual(args[args.index('--magnification')+1],option[-1] if option and option[0]!='-LegacyWater' else '1')
                    self.assertEqual('--legacy-water' in args,'-LegacyWater' in option)
                    self.assertIn('--demo',args)
