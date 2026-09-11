"""Exercise both real PowerShell entry points without starting UE/hardware."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]


@unittest.skipUnless(os.name=='nt','Windows PowerShell integration')
class LauncherArgumentTests(unittest.TestCase):
    def test_launchers_preserve_native_arguments(self):
        for shell in ('powershell.exe','pwsh.exe'):
            executable=shutil.which(shell)
            if not executable: continue
            for app in ('fly','row'):
                for date in ('','2026-09-12'):
                    with self.subTest(shell=shell,app=app,date=date),tempfile.TemporaryDirectory() as td:
                        root=Path(td);directory=root/'apps'/app
                        (directory/'tools').mkdir(parents=True)
                        (directory/'.venv/Scripts').mkdir(parents=True)
                        # Copy the base interpreter with its DLL, stdlib and .pth setup
                        # by referencing the tested interpreter via a PowerShell shim.
                        (directory/'.venv/Scripts/python.exe').touch()
                        script=(ROOT/(app+'.ps1')).read_text(encoding='utf-8-sig')
                        script=script.replace("$python=Join-Path $PSScriptRoot 'apps/"+app+"/.venv/Scripts/python.exe'", "$python='"+sys.executable.replace("'","''")+"'")
                        (root/(app+'.ps1')).write_text(script,encoding='utf-8-sig')
                        (directory/'tools/launch.py').write_text('import sys,json;print("ARGV="+json.dumps(sys.argv[1:]))',encoding='utf-8')
                        args=[executable,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(root/(app+'.ps1')),'Lake Bled']
                        if app=='fly':
                            args+=['-Offline','-StartMode','Air','-StartAglM','125.5','-mag','2.5']
                            if date: args+=['-LocalDate',date]
                        else: args+=['-Demo','-WaterLevelM','475.5']
                        result=subprocess.run(args,capture_output=True,text=True,errors='replace',timeout=30)
                        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                        values=json.loads(next(line[5:] for line in result.stdout.splitlines() if line.startswith('ARGV=')))
                        self.assertEqual(values[0],'Lake Bled')
                        if app=='fly':
                            self.assertIn('--offline',values)
                            self.assertEqual(values[values.index('--start-agl')+1],'125.5')
                            self.assertEqual(values[values.index('--magnification')+1],'2.5')
                            if date: self.assertEqual(values[values.index('--date')+1],date)
                            else: self.assertNotIn('--date',values)
                        else:
                            self.assertIn('--demo',values)
                            self.assertEqual(values[values.index('--water-level')+1],'475.5')
