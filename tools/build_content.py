"""UE Editor asset preparation only. All gameplay/devices/waves run in C++."""
from pathlib import Path
import unreal as u

ROOT=Path(__file__).resolve().parents[1]
assets=u.AssetToolsHelpers.get_asset_tools()
mel=u.MaterialEditingLibrary
folder='/Game/Row/Materials'

def make(name):
    mat=u.load_asset(folder+'/'+name)
    if not mat: mat=assets.create_asset(name,folder,u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    return mat

def scalar(mat,name,value):
    n=mel.create_material_expression(mat,u.MaterialExpressionScalarParameter)
    n.set_editor_property('parameter_name',name); n.set_editor_property('default_value',value)
    return n

def vector(mat,name,value):
    n=mel.create_material_expression(mat,u.MaterialExpressionVectorParameter)
    n.set_editor_property('parameter_name',name); n.set_editor_property('default_value',u.LinearColor(*value))
    return n

def prop(mat,node,target,output=''):
    assert mel.connect_material_property(node,output,target)

def link(source,target,input_name,output=''):
    names=mel.get_material_expression_input_names(target)
    if input_name=='Input' and len(names)==1: input_name=names[0]
    assert mel.connect_material_expressions(source,output,target,input_name), (target.get_class().get_name(), input_name, names)

def custom(mat,code,inputs,output=u.CustomMaterialOutputType.CMOT_FLOAT4):
    node=mel.create_material_expression(mat,u.MaterialExpressionCustom)
    node.set_editor_property('code',code); node.set_editor_property('output_type',output)
    slots=[]
    for k in inputs:
        slot=u.CustomInput(); slot.set_editor_property('input_name',k); slots.append(slot)
    node.set_editor_property('inputs',slots)
    for name,source in inputs.items(): link(source,node,name)
    return node

water=make('M_RowWater')
water.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
water.set_editor_property('two_sided',True)
water.set_editor_property('tangent_space_normal',False)
water.set_editor_property('opacity_mask_clip_value',.5)
world=mel.create_material_expression(water,u.MaterialExpressionWorldPosition)
time=mel.create_material_expression(water,u.MaterialExpressionTime)
field=mel.create_material_expression(water,u.MaterialExpressionTextureObjectParameter)
field.set_editor_property('parameter_name','WakeField')
black=u.load_asset('/Engine/EngineResources/Black')
if not black:
    # Neutral zero data, generated locally; not an art asset or backdrop.
    import struct
    neutral=ROOT/'artifacts/neutral.tga'; neutral.parent.mkdir(exist_ok=True,parents=True)
    neutral.write_bytes(struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,1,1,32,0x28)+bytes(4))
    task=u.AssetImportTask(); task.filename=str(neutral); task.destination_path=folder
    task.destination_name='T_Neutral'; task.automated=True; task.replace_existing=True; task.save=True
    assets.import_asset_tasks([task]); black=u.load_asset(folder+'/T_Neutral')
    black.set_editor_property('srgb',False); u.EditorAssetLibrary.save_loaded_asset(black)
field.set_editor_property('texture',black)
field.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
boat=vector(water,'Boat',(30000,60000,0,0))
direction=vector(water,'BoatDirection',(1,0,0,0))
origin=vector(water,'FieldOrigin',(26800,56800,6400,0))
local=scalar(water,'LocalPatch',0)
linear=vector(water,'SurfaceLinear',(0,0,0,0))
curve=vector(water,'SurfaceCurve',(0,0,0,0))
water_bounds=vector(water,'WaterBounds',(-300000,-300000,600000,600000))
water_size=vector(water,'WaterSize',(600000,600000,0,0))
ocean=scalar(water,'Ocean',0)
water_mask=mel.create_material_expression(water,u.MaterialExpressionTextureObjectParameter)
water_mask.set_editor_property('parameter_name','WaterMask')
water_mask.set_editor_property('texture',black)
water_mask.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
inputs=dict(World=world,Time=time,WakeField=field,Boat=boat,FieldOrigin=origin,LocalPatch=local)
offset_inputs=dict(inputs,SurfaceLinear=linear,SurfaceCurve=curve)
norm=custom(water,(ROOT/'Shaders/WaterNormal.hlsl').read_text(),inputs)
offset=custom(water,(ROOT/'Shaders/WaterOffset.hlsl').read_text(),offset_inputs,u.CustomMaterialOutputType.CMOT_FLOAT3)
mask=custom(water,'''float2 delta=World.xy-Boat.xy; float d=length(delta);
float along=dot(delta,BoatDirection.xy)/230;
float lateral=dot(delta,float2(-BoatDirection.y,BoatDirection.x));
float width=42*pow(saturate(1-along*along),.55);
float hull=abs(along)<1 && abs(lateral)<width ? 0:1;
float2 uv=(World.xy-WaterBounds.xy)/WaterSize.xy;
float within=all(uv>=0) && all(uv<=1) ? 1:0;
float waterArea=within>.5 ? Texture2DSampleLevel(WaterMask,WaterMaskSampler,uv,0).r : Ocean;
return waterArea*hull*(LocalPatch>.5 ? step(d,2800.0) : step(2800.0,d));''',
            dict(World=world,Boat=boat,BoatDirection=direction,LocalPatch=local,WaterMask=water_mask,WaterBounds=water_bounds,WaterSize=water_size,Ocean=ocean),u.CustomMaterialOutputType.CMOT_FLOAT1)
xyz=mel.create_material_expression(water,u.MaterialExpressionComponentMask)
for k in ('r','g','b'): xyz.set_editor_property(k,True)
link(norm,xyz,'Input'); prop(water,xyz,u.MaterialProperty.MP_NORMAL)
foam=mel.create_material_expression(water,u.MaterialExpressionComponentMask); foam.set_editor_property('a',True)
foam.set_editor_property('r',False); foam.set_editor_property('g',False); link(norm,foam,'Input')
color=mel.create_material_expression(water,u.MaterialExpressionLinearInterpolate)
link(vector(water,'DeepWater',(.018,.075,.060,1)),color,'A')
link(vector(water,'FoamTint',(.65,.79,.74,1)),color,'B'); link(foam,color,'Alpha')
prop(water,color,u.MaterialProperty.MP_BASE_COLOR); prop(water,offset,u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
prop(water,mask,u.MaterialProperty.MP_OPACITY_MASK)
prop(water,scalar(water,'Roughness',.25),u.MaterialProperty.MP_ROUGHNESS)
prop(water,scalar(water,'Specular',.32),u.MaterialProperty.MP_SPECULAR)
prop(water,scalar(water,'Metallic',0),u.MaterialProperty.MP_METALLIC)
mel.recompile_material(water); u.EditorAssetLibrary.save_loaded_asset(water)

# Preserve Cesium's glTF/imagery material layers, but discard its baked water
# pixels where our mapped UE water replaces them. This also preserves islands.
terrain_path=folder+'/M_RowTerrain'
terrain=u.load_asset(terrain_path)
if terrain:
    # Generated local asset: rebuild the wrapper from the pinned Cesium master.
    assert u.EditorAssetLibrary.delete_asset(terrain_path)
    terrain=None
if not terrain:
    terrain=u.EditorAssetLibrary.duplicate_asset('/CesiumForUnreal/Materials/M_CesiumBaseMaterial',terrain_path)
    original=mel.get_material_property_input_node(terrain,u.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    original_output=mel.get_material_property_input_node_output_name(terrain,u.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    assert original
    tworld=mel.create_material_expression(terrain,u.MaterialExpressionWorldPosition)
    tmask=mel.create_material_expression(terrain,u.MaterialExpressionTextureObjectParameter)
    tmask.set_editor_property('parameter_name','WaterMask');tmask.set_editor_property('texture',black)
    tmask.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    clip=custom(terrain,'''float2 uv=(World.xy-WaterBounds.xy)/WaterSize.xy;
float inside=all(uv>=0) && all(uv<=1) ? 1:0;
float area=inside>.5 ? Texture2DSampleLevel(WaterMask,WaterMaskSampler,uv,0).r : Ocean;
float2 p=World.xy*.01;
float mean=100*(dot(SurfaceLinear.xy,p)+SurfaceCurve.x*p.x*p.x+SurfaceCurve.y*p.x*p.y+SurfaceCurve.z*p.y*p.y);
return step(.5,area)*step(World.z,mean+3);''',
        dict(World=tworld,WaterMask=tmask,WaterBounds=vector(terrain,'WaterBounds',(-300000,-300000,0,0)),
             WaterSize=vector(terrain,'WaterSize',(600000,600000,0,0)),Ocean=scalar(terrain,'Ocean',0),
             SurfaceLinear=vector(terrain,'SurfaceLinear',(0,0,0,0)),SurfaceCurve=vector(terrain,'SurfaceCurve',(0,0,0,0))),
        u.CustomMaterialOutputType.CMOT_FLOAT1)
    invisible=mel.create_material_expression(terrain,u.MaterialExpressionMakeMaterialAttributes)
    link(scalar(terrain,'HiddenWaterOpacity',0),invisible,'OpacityMask')
    blend=mel.create_material_expression(terrain,u.MaterialExpressionBlendMaterialAttributes)
    link(original,blend,'A',original_output);link(invisible,blend,'B');link(clip,blend,'Alpha')
    prop(terrain,blend,u.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
terrain.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
mel.recompile_material(terrain);u.EditorAssetLibrary.save_loaded_asset(terrain)
terrain_instance=u.load_asset(folder+'/MI_RowTerrain')
if not terrain_instance:
    terrain_instance=u.EditorAssetLibrary.duplicate_asset('/CesiumForUnreal/Materials/Instances/MI_CesiumThreeOverlaysAndClipping',folder+'/MI_RowTerrain')
mel.set_material_instance_parent(terrain_instance,terrain)
u.EditorAssetLibrary.save_loaded_asset(terrain_instance)

for name,color,roughness in [('M_Hull',(.018,.16,.13,1),.28),('M_Wood',(.29,.13,.040,1),.38)]:
    mat=make(name); mat.set_editor_property('two_sided',True)
    prop(mat,vector(mat,'Tint',color),u.MaterialProperty.MP_BASE_COLOR)
    prop(mat,scalar(mat,'Roughness',roughness),u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat); u.EditorAssetLibrary.save_loaded_asset(mat)

# MIT Arrietty-UE58 instrument exposure approach, adapted for rowing.
panel=u.load_asset(folder+'/M_Instruments')
if not panel: panel=u.EditorAssetLibrary.duplicate_asset('/Engine/EngineMaterials/Widget3DPassThrough',folder+'/M_Instruments')
panel.set_editor_property('blend_mode',u.BlendMode.BLEND_OPAQUE)
panel.set_editor_property('two_sided',True)
source=mel.get_material_property_input_node(panel,u.MaterialProperty.MP_EMISSIVE_COLOR)
if not isinstance(source,u.MaterialExpressionEyeAdaptationInverse):
    out=mel.get_material_property_input_node_output_name(panel,u.MaterialProperty.MP_EMISSIVE_COLOR)
    inverse=mel.create_material_expression(panel,u.MaterialExpressionEyeAdaptationInverse)
    link(source,inverse,mel.get_material_expression_input_names(inverse)[0],out)
    prop(panel,inverse,u.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(panel); u.EditorAssetLibrary.save_loaded_asset(panel)

level=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert u.EditorLoadingAndSavingUtils.new_blank_map(False)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',u.load_class(None,'/Script/ArriettyRow.RowGameMode'))
actors.spawn_actor_from_class(u.PlayerStart,u.Vector(0,0,0))
sun=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,1000),u.Rotator(-38,-35,0))
sun.set_actor_label('Sun')
sun.light_component.set_editor_property('mobility',u.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('intensity',50000)
sun.light_component.set_editor_property('atmosphere_sun_light',True)
sky=actors.spawn_actor_from_class(u.SkyAtmosphere,u.Vector(0,0,0))
sky.set_actor_label('Atmosphere')
light=actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,1000))
light.light_component.set_editor_property('mobility',u.ComponentMobility.MOVABLE)
light.light_component.set_editor_property('real_time_capture',True)
light.light_component.set_editor_property('intensity',1)
fog=actors.spawn_actor_from_class(u.ExponentialHeightFog,u.Vector(0,0,0))
fog.component.set_editor_property('fog_density',.00025)
post=actors.spawn_actor_from_class(u.PostProcessVolume,u.Vector(0,0,0))
post.set_editor_property('unbound',True)
settings=post.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_method',True)
settings.set_editor_property('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_bias',True)
settings.set_editor_property('auto_exposure_bias',0)
settings.set_editor_property('override_camera_iso',True)
settings.set_editor_property('camera_iso',100)
settings.set_editor_property('override_camera_shutter_speed',True)
settings.set_editor_property('camera_shutter_speed',125)
settings.set_editor_property('override_depth_of_field_fstop',True)
settings.set_editor_property('depth_of_field_fstop',8)
post.set_editor_property('settings',settings)
assert u.EditorLoadingAndSavingUtils.save_map(world,'/Game/Row/Maps/CesiumRow')
u.EditorAssetLibrary.save_directory(folder)
print('ROW_CONTENT_READY cesium_runtime_scene=1')
