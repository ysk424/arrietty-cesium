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
offset = custom(mat, (ROOT / 'Shaders/WaterlineOffset.hlsl').read_text(),
                dict(World=world, Boat=boat, WakeField=field, FieldOrigin=origin,
                     LocalPatch=local, SurfaceLinear=linear, SurfaceCurve=curve, Wave=zero),
                u.CustomMaterialOutputType.CMOT_FLOAT3)
connect(wave[0], offset, 'Wave', 'Result')
assert mel.connect_material_property(offset, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
mask = custom(mat, '''float2 delta=World.xy-Boat.xy;
float along=dot(delta,BoatDirection.xy)/230;
float lateral=dot(delta,float2(-BoatDirection.y,BoatDirection.x));
float width=42*pow(saturate(1-along*along),.55);
float hull=abs(along)<1 && abs(lateral)<width ? 0:1;
float2 uv=(World.xy-WaterBounds.xy)/WaterSize.xy;
float within=all(uv>=0) && all(uv<=1) ? 1:0;
float area=within>.5 ? Texture2DSampleLevel(WaterMask,WaterMaskSampler,uv,0).r : Ocean;
return area*hull*(LocalPatch>.5 ? step(length(delta),2800.0) : step(2800.0,length(delta)));''',
              dict(World=world, Boat=boat, BoatDirection=direction, LocalPatch=local,
                   WaterMask=mask_texture, WaterBounds=bounds, WaterSize=size, Ocean=ocean),
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
foam = custom(mat, 'return lerp(Color,float3(.65,.79,.74),saturate(Wake.a));',
              dict(Color=zero, Wake=wake), u.CustomMaterialOutputType.CMOT_FLOAT3)
connect(color, foam, 'Color', color_output)
assert mel.connect_material_property(norm, '', u.MaterialProperty.MP_NORMAL)
assert mel.connect_material_property(foam, '', u.MaterialProperty.MP_BASE_COLOR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
mi = eal.duplicate_asset(SOURCE + 'MI_Water_Surface_Gen4', DEST + 'MI_RowWaterline')
mel.set_material_instance_parent(mi, mat)
# The stock instance forces opaque/one-sided, overriding the adapted parent.
# Inherit masking and two-sided rendering for ROW's geographic mesh winding.
mi.set_editor_property('base_property_overrides', u.MaterialInstanceBasePropertyOverrides())
eal.save_loaded_asset(mi)
print('ROW_WATERLINE_CONTENT_READY')
