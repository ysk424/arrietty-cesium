"""UE Python commandlet: import derived PCM into /Game/Fly/Audio."""
from pathlib import Path
import sys
import unreal as u
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audio_sources import SOUNDS, prepare_all


def build(required=True):
    output = prepare_all(required=required)
    if output is None:
        print('FLY_AUDIO_NOT_SUPPLIED see=docs/FLY_AUDIO.ja.md')
        return
    assets = u.AssetToolsHelpers.get_asset_tools()
    folder = '/Game/Fly/Audio'
    for _, _, name, loop, channels in SOUNDS:
        task = u.AssetImportTask()
        for key, value in dict(filename=str(output/(name+'.wav')), destination_path=folder,
                               destination_name=name, automated=True, replace_existing=True, save=False).items():
            task.set_editor_property(key, value)
        assets.import_asset_tasks([task])
        sound = u.load_asset(folder+'/'+name)
        assert isinstance(sound, u.SoundWave), name
        for key, value in dict(looping=loop, volume=1., pitch=1.,
                               sound_asset_compression_type=u.SoundAssetCompressionType.PCM,
                               loading_behavior=u.SoundWaveLoadingBehavior.FORCE_INLINE,
                               virtualization_mode=u.VirtualizationMode.PLAY_WHEN_SILENT).items():
            sound.set_editor_property(key, value)
        assert sound.get_editor_property('num_channels') == channels, name
        assert u.EditorAssetLibrary.save_loaded_asset(sound), name
    print('FLY_AUDIO_CONTENT_READY assets=6 compression=PCM')


if __name__ == '__main__': build()
