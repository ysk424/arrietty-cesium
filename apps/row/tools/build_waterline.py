"""Adapt a locally purchased Waterline Gen 4 material; never distribute its assets."""
from pathlib import Path
import unreal as u

mel = u.MaterialEditingLibrary
eal = u.EditorAssetLibrary
SOURCE = '/Game/Waterline/3_Ocean_Sim/2_Ocean_Materials/1_Water_Surface/'
DEST = '/Game/Row/Waterline/'
ROOT = Path(__file__).resolve().parents[1]


def connect(source, target, pin, output=''):
    assert mel.connect_material_expressions(source, output, target, pin), (pin, output)


def node(mat, cls, **properties):
    result = mel.create_material_expression(mat, cls)
    for name, value in properties.items():
        result.set_editor_property(name, value)
    return result


def scalar(mat, name, value):
    return node(mat, u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)


def vector(mat, name, value):
    return node(mat, u.MaterialExpressionVectorParameter, parameter_name=name, default_value=u.LinearColor(*value))


def custom(mat, code, inputs, output):
    result = node(mat, u.MaterialExpressionCustom, code=code, output_type=output)
    slots = []
    for name in inputs:
        slot = u.CustomInput()
        slot.set_editor_property('input_name', name)
        slots.append(slot)
    result.set_editor_property('inputs', slots)
    for name, source in inputs.items():
        connect(source, result, name)
    return result


# Duplicate fresh so repeated preparation cannot stack adapter expressions.
for name in ('MI_RowWaterline', 'M_RowWaterline'):
    if eal.does_asset_exist(DEST + name):
        eal.delete_asset(DEST + name)
mat = eal.duplicate_asset(SOURCE + 'M_Water_Surface_Gen4', DEST + 'M_RowWaterline')
assert mat
expressions = mel.get_material_expressions(mat)
world = node(mat, u.MaterialExpressionWorldPosition,
             world_position_shader_offset=u.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
zero = node(mat, u.MaterialExpressionConstant3Vector, constant=u.LinearColor(0, 0, 0, 0))
one = node(mat, u.MaterialExpressionConstant, r=1.0)
# The vendor projects a grid around the view. ROW supplies its own geographic
# mesh. Replace only the projection outputs; retain the vendor wave functions.
projection = [x for x in expressions if isinstance(x, u.MaterialExpressionMaterialFunctionCall)
              and x.get_editor_property('material_function')
              and x.get_editor_property('material_function').get_name() == 'MF_InfinitePlane_V2']
assert len(projection) == 1, 'Unrecognized Waterline material: inspect this version first'
replacement = {'Box Mask Fade': one, 'Final WPO Offset': zero,
               'Grid Origin': zero, 'Box Mask Range': one, 'WS UVs': world}
for expr in expressions:
    pins = mel.get_material_expression_input_names(expr)
    inputs = mel.get_inputs_for_material_expression(mat, expr)
    for pin, source in zip(pins, inputs):
        if source == projection[0]:
            output = mel.get_input_node_output_name_for_material_expression(expr, source)
            assert output in replacement, str(output)
            connect(replacement[output], expr, pin)

wave = [x for x in expressions if isinstance(x, u.MaterialExpressionMaterialFunctionCall)
        and x.get_editor_property('material_function')
        and x.get_editor_property('material_function').get_name() == 'MF_Ocean_Displacement_Gen4']
assert len(wave) == 1
connect(world, wave[0], 'World Position (Excluding MO)')
connect(world, wave[0], 'Absolute World Position')
black = u.load_asset('/Engine/EngineResources/Black')
assert black
field = node(mat, u.MaterialExpressionTextureObjectParameter, parameter_name='WakeField',
             texture=black, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
mask_texture = node(mat, u.MaterialExpressionTextureObjectParameter, parameter_name='WaterMask',
                    texture=black, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
boat = vector(mat, 'Boat', (0, 0, 0, 0))
direction = vector(mat, 'BoatDirection', (1, 0, 0, 0))
origin = vector(mat, 'FieldOrigin', (-3200, -3200, 6400, 0))
local = scalar(mat, 'LocalPatch', 0)
linear = vector(mat, 'SurfaceLinear', (0, 0, 0, 0))
curve = vector(mat, 'SurfaceCurve', (0, 0, 0, 0))
bounds = vector(mat, 'WaterBounds', (-300000, -300000, 600000, 600000))
size = vector(mat, 'WaterSize', (600000, 600000, 0, 0))
ocean = scalar(mat, 'Ocean', 0)
shore_enabled = scalar(mat, 'RowShoreEnabled', 0)
shore = [x for x in expressions if isinstance(x, u.MaterialExpressionMaterialFunctionCall)
         and x.get_editor_property('material_function')
         and x.get_editor_property('material_function').get_name() == 'MF_Shore_Gen3']
assert len(shore) == 1, 'Waterline shoreline function missing'
connect(world, shore[0], 'World Position')
# Retain the purchased rolling-wave, normal and foam graph. A runtime gate
# makes the same material safe for lakes, fixtures and old scenes without data.
for expr in expressions:
    for pin, source in zip(mel.get_material_expression_input_names(expr),
                           mel.get_inputs_for_material_expression(mat, expr)):
        if source != shore[0]:
            continue
        output = mel.get_input_node_output_name_for_material_expression(expr, source)
        scalar_output = output == 'Flatten Mask'
        neutral = '0.0' if scalar_output else ('float3(0,0,1)' if output == 'Normals' else 'float3(0,0,0)')
        gate = custom(mat, f'return lerp({neutral},Value,Enabled);',
                      dict(Value=zero, Enabled=shore_enabled),
                      u.CustomMaterialOutputType.CMOT_FLOAT1 if scalar_output else u.CustomMaterialOutputType.CMOT_FLOAT3)
        connect(shore[0], gate, 'Value', output)
        connect(gate, expr, pin)
shore_offset = custom(mat, '''float fade=1-smoothstep(20000.0,24000.0,length(World.xy-Boat.xy));
return float3(0,0,clamp(Value.z,-15.0,15.0)*Enabled*fade);''',
                      dict(Value=zero, Enabled=shore_enabled, World=world, Boat=boat),
                      u.CustomMaterialOutputType.CMOT_FLOAT3)
connect(shore[0], shore_offset, 'Value', 'Displacement')
combined_wave = node(mat, u.MaterialExpressionAdd)
connect(wave[0], combined_wave, 'A', 'Result')
connect(shore_offset, combined_wave, 'B')
offset = custom(mat, (ROOT / 'Shaders/WaterlineOffset.hlsl').read_text(),
                dict(World=world, Boat=boat, WakeField=field, FieldOrigin=origin,
                     LocalPatch=local, SurfaceLinear=linear, SurfaceCurve=curve, Wave=zero),
                u.CustomMaterialOutputType.CMOT_FLOAT3)
connect(combined_wave, offset, 'Wave')
assert mel.connect_material_property(offset, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
mask = custom(mat, '''float2 delta=World.xy-Boat.xy;
float along=dot(delta,BoatDirection.xy)/230;
float lateral=dot(delta,float2(-BoatDirection.y,BoatDirection.x));
float width=42*pow(saturate(1-along*along),.55);
float hull=abs(along)<1 && abs(lateral)<width ? 0:1;
float2 uv=(World.xy-WaterBounds.xy)/WaterSize.xy;
float within=all(uv>=0) && all(uv<=1) ? 1:0;
float area=within>.5 ? Texture2DSampleLevel(WaterMask,WaterMaskSampler,uv,0).r : Ocean;
float distance=length(delta);
float patch=LocalPatch>1.5 ? step(2800.0,distance)*step(distance,24000.0) :
    (LocalPatch>.5 ? step(distance,2800.0) : step(Enabled>.5 ? 24000.0:2800.0,distance));
return area*hull*patch;''',
              dict(World=world, Boat=boat, BoatDirection=direction, LocalPatch=local,
                   WaterMask=mask_texture, WaterBounds=bounds, WaterSize=size, Ocean=ocean,
                   Enabled=shore_enabled),
              u.CustomMaterialOutputType.CMOT_FLOAT1)
assert mel.connect_material_property(mask, '', u.MaterialProperty.MP_OPACITY_MASK)
mat.set_editor_property('blend_mode', u.BlendMode.BLEND_MASKED)
mat.set_editor_property('two_sided', True)
# Preserve ROW's speed/drive-based whitewater and wake slopes over vendor waves.
normal = mel.get_material_property_input_node(mat, u.MaterialProperty.MP_NORMAL)
normal_output = mel.get_material_property_input_node_output_name(mat, u.MaterialProperty.MP_NORMAL)
color = mel.get_material_property_input_node(mat, u.MaterialProperty.MP_BASE_COLOR)
color_output = mel.get_material_property_input_node_output_name(mat, u.MaterialProperty.MP_BASE_COLOR)
wake = custom(mat, '''float2 uv=(World.xy-FieldOrigin.xy)/FieldOrigin.z+.5/256.0;
float4 v=Texture2DSampleLevel(WakeField,WakeFieldSampler,uv,0);
float fade=1-smoothstep(2200.0,2800.0,length(World.xy-Boat.xy));
return v*fade;''', dict(World=world, FieldOrigin=origin, WakeField=field, Boat=boat),
              u.CustomMaterialOutputType.CMOT_FLOAT4)
norm = custom(mat, 'return normalize(float3(N.xy-Wake.gb,N.z));', dict(N=zero, Wake=wake),
              u.CustomMaterialOutputType.CMOT_FLOAT3)
connect(normal, norm, 'N', normal_output)
foam = custom(mat, '''float surf=saturate(max(Shore.r,max(Shore.g,Shore.b)))*Enabled;
return lerp(Color,float3(.65,.79,.74),max(saturate(Wake.a),surf));''',
              dict(Color=zero, Wake=wake, Shore=zero, Enabled=shore_enabled), u.CustomMaterialOutputType.CMOT_FLOAT3)
connect(color, foam, 'Color', color_output)
connect(shore[0], foam, 'Shore', 'Shore Churn Foam')
assert mel.connect_material_property(norm, '', u.MaterialProperty.MP_NORMAL)
assert mel.connect_material_property(foam, '', u.MaterialProperty.MP_BASE_COLOR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
mi = eal.duplicate_asset(SOURCE + 'MI_Water_Surface_Gen4', DEST + 'MI_RowWaterline')
mel.set_material_instance_parent(mi, mat)
# The stock instance forces opaque/one-sided, overriding the adapted parent.
# Inherit masking and two-sided rendering for ROW's geographic mesh winding.
mi.set_editor_property('base_property_overrides', u.MaterialInstanceBasePropertyOverrides())
# UE 5.8's setter returns false even after success; verify by reading back.
mel.set_material_instance_static_switch_parameter_value(mi, 'Use Shore Manager Data', True)
assert mel.get_material_instance_static_switch_parameter_value(mi, 'Use Shore Manager Data')
# Gentle visual surf. Capture-dependent gravity surges are disabled: ROW has
# coastlines, not bathymetry. All distances/heights here are vendor cm units.
for name, value in {'Shore Wave Panning Speed':-.1, 'Shore Wave Animation Speed':.12,
                    'Shore Foam Power':2., 'Shore Foam Blend':2.,
                    'Shore Wave Normal Power':2., 'Shore Shallows Range':0.,
                    'Shore Shallows Blend':1.5, 'Grav Wave Climb':0.,
                    'Grav Flow':0., 'Grav Foam':0., 'Breeze Power':0.,
                    'Shore Texture Scale':1.}.items():
    mel.set_material_instance_scalar_parameter_value(mi, name, value)
mel.set_material_instance_vector_parameter_value(mi, 'Shore Wave Displacement', u.LinearColor(0,0,12,0))
mel.set_material_instance_vector_parameter_value(mi, 'Shore Texture Location', u.LinearColor(0,0,0,0))
mel.set_material_instance_texture_parameter_value(mi, 'Shore Data Texture', black)
mel.set_material_instance_texture_parameter_value(mi, 'Shore Capture Texture', black)
for name, asset in {'Shore Noise Texture':'T_Noise_Masks',
                    'Shore Wave Texture':'T_PL_Wave_1_Disp',
                    'Shore Wave Normal Texture':'T_PL_Wave_1_Nrml',
                    'Shore Wave Foam Texture':'T_PL_Wave_F_2'}.items():
    texture = u.load_asset('/Game/Waterline/3_Ocean_Sim/1_BP_Actors/3_Shore_Manager/Shore_Textures/' + asset)
    assert texture
    mel.set_material_instance_texture_parameter_value(mi, name, texture)
mel.update_material_instance(mi)
eal.save_loaded_asset(mi)

# Independent terrain-to-shore adapter. The vendor animation above is retained;
# these small GPU passes locate the actual rendered coast without reading back
# terrain, adding bathymetry or importing the vendor's camera/physics managers.
for suffix in ('Seeds', 'Jump', 'Field'):
    path = DEST + 'M_RowShore' + suffix
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    capture_mat = u.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_RowShore' + suffix, DEST.rstrip('/'), u.Material, u.MaterialFactoryNew())
    capture_mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    capture_mat.set_editor_property('allow_negative_emissive_color', True)
    inputs = dict(
        UV=node(capture_mat, u.MaterialExpressionTextureCoordinate),
        Source=node(capture_mat, u.MaterialExpressionTextureObjectParameter, parameter_name='Source',
                    texture=black, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR))
    if suffix == 'Seeds':
        inputs.update(Center=vector(capture_mat, 'Center', (0,0,0,0)),
                      SurfaceLinear=vector(capture_mat, 'SurfaceLinear', (0,0,0,0)),
                      SurfaceCurve=vector(capture_mat, 'SurfaceCurve', (0,0,0,0)))
    if suffix == 'Jump':
        inputs['Jump'] = scalar(capture_mat, 'Jump', 1)
    output = custom(capture_mat, (ROOT / f'Shaders/Shore{suffix}.hlsl').read_text(),
                    inputs, u.CustomMaterialOutputType.CMOT_FLOAT3)
    assert mel.connect_material_property(output, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(capture_mat)
    eal.save_loaded_asset(capture_mat)

# Local-only probe for the optional GPU test: exercise the purchased foam
# animation with a known coastline independently of terrain and exposure.
for name in ('MI_RowShoreProbe', 'M_RowShoreProbe'):
    if eal.does_asset_exist(DEST + name):
        eal.delete_asset(DEST + name)
probe = u.AssetToolsHelpers.get_asset_tools().create_asset(
    'M_RowShoreProbe', DEST.rstrip('/'), u.Material, u.MaterialFactoryNew())
probe.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
coords = node(probe, u.MaterialExpressionTextureCoordinate)
position = custom(probe, 'return float3((UV-.5)*51200.0,0);', dict(UV=coords),
                  u.CustomMaterialOutputType.CMOT_FLOAT3)
function = node(probe, u.MaterialExpressionMaterialFunctionCall,
                material_function=shore[0].get_editor_property('material_function'))
connect(position, function, 'World Position')
connect(scalar(probe, 'ProbeTime', 0), function, 'Time Overrid')
probe_data = node(probe, u.MaterialExpressionTextureObjectParameter, parameter_name='Shore Data Texture',
                  texture=black, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
probe_output = custom(probe, 'return float3(Foam.r,abs(Wave.z),Texture2DSampleLevel(Data,DataSampler,UV,0).r);',
                      dict(Foam=node(probe,u.MaterialExpressionConstant3Vector),
                           Wave=node(probe,u.MaterialExpressionConstant3Vector),Data=probe_data,UV=coords),
                      u.CustomMaterialOutputType.CMOT_FLOAT3)
connect(function, probe_output, 'Foam', 'Shore Churn Foam')
connect(function, probe_output, 'Wave', 'Displacement')
assert mel.connect_material_property(probe_output, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(probe)
eal.save_loaded_asset(probe)
probe_mi = eal.duplicate_asset(DEST + 'MI_RowWaterline', DEST + 'MI_RowShoreProbe')
mel.set_material_instance_parent(probe_mi, probe)
mel.update_material_instance(probe_mi)
eal.save_loaded_asset(probe_mi)
print('ROW_WATERLINE_CONTENT_READY')
