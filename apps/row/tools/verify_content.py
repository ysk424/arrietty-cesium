"""Verify generated assets; Cesium actors and tokens exist only at runtime."""
import json
from pathlib import Path
import unreal as u
root=Path(__file__).resolve().parents[1]
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Row/Maps/CesiumRow')
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property('default_game_mode').get_name()=='RowGameMode'
for cls in (u.SkyAtmosphere,u.SkyLight,u.DirectionalLight,u.ExponentialHeightFog):
    assert any(isinstance(a,cls) for a in actors),cls
assert not any(isinstance(a,u.Cesium3DTileset) for a in actors)
assert not any(isinstance(a,u.CesiumGeoreference) for a in actors)
start=next(a for a in actors if isinstance(a,u.PlayerStart))
assert start.get_actor_location().is_nearly_zero()
material=u.load_asset('/Game/Row/Materials/M_RowWater')
assert material.get_editor_property('blend_mode')==u.BlendMode.BLEND_MASKED
mel=u.MaterialEditingLibrary
assert 'WaterMask' in [str(x) for x in mel.get_texture_parameter_names(material)]
assert {'WaterBounds','WaterSize','SurfaceLinear','SurfaceCurve'}.issubset({str(x) for x in mel.get_vector_parameter_names(material)})
for name in ('M_Hull','M_Wood','M_Instruments','MI_RowTerrain'):
    assert u.load_asset('/Game/Row/Materials/'+name)
report=dict(map='CesiumRow',native_sky=True,serialized_cesium_tokens=False,water_mask=True,curved_mean_water=True)
(root/'logs/content-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('ROW_CONTENT_VERIFIED '+json.dumps(report))
