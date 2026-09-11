"""Use the bundled CPython 3.13 dependencies without changing global Python."""
from pathlib import Path
import json
import os
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root))
sys.path.insert(0,str(root.parents[1]/'shared/python'))
settings_path = root.parents[1] / 'config/fly.local.json'
if settings_path.is_file():
    settings = json.loads(settings_path.read_text(encoding='utf-8-sig'))
    for key, env in [('trainer_address','ARRIETTY_TRAINER_ADDRESS'),('steering_serial','ARRIETTY_STEERING_SERIAL')]:
        if settings.get(key): os.environ.setdefault(env, str(settings[key]))
from arrietty_ue.bridge import main
main()
