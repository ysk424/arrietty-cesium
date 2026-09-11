"""Run inside UE's Python commandlet. Changes only /Game/Row/Audio."""
from pathlib import Path
import sys
import unreal as u

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audio_sources import SOUNDS, prepare_all

output, report = prepare_all()
assets = u.AssetToolsHelpers.get_asset_tools()
folder = '/Game/Row/Audio'
for name, asset, loop, _ in SOUNDS:
    task = u.AssetImportTask()
    task.set_editor_property('filename', str(output/(asset+'.wav')))
    task.set_editor_property('destination_path', folder)
    task.set_editor_property('destination_name', asset)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    assets.import_asset_tasks([task])
    sound = u.load_asset(folder+'/'+asset)
    assert isinstance(sound, u.SoundWave), asset
    sound.set_editor_property('looping', loop)
    sound.set_editor_property('volume', 1.0)
    sound.set_editor_property('pitch', 1.0)
    sound.set_editor_property('sound_asset_compression_type', u.SoundAssetCompressionType.PCM)
    sound.set_editor_property('loading_behavior', u.SoundWaveLoadingBehavior.FORCE_INLINE)
    sound.set_editor_property('virtualization_mode', u.VirtualizationMode.PLAY_WHEN_SILENT)
    assert sound.get_editor_property('num_channels') == 2, asset
    assert u.EditorAssetLibrary.save_loaded_asset(sound), asset
print('ROW_AUDIO_CONTENT_READY assets=5 stereo_left_db=6 compression=PCM')
