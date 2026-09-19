// Orthographic depth contains ONLY ROW's rendered Cesium terrain. Empty sea
// stays empty; no bathymetry, acquisition boundary or boat is a seed.
float depth=Texture2DSampleLevel(Source,SourceSampler,UV,0).r;
float2 world=(Center.xy+(UV-.5)*51200.0)*.01;
float mean=dot(SurfaceLinear.xy,world)+SurfaceCurve.x*world.x*world.x+
    SurfaceCurve.y*world.x*world.y+SurfaceCurve.z*world.y*world.y;
bool land=depth>0 && depth<1000000.0-mean*100.0-3.0;
return land ? float3(UV,1) : float3(0,0,0);
