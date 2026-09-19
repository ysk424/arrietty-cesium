// Centimetres in UE; geographic water coefficients are evaluated in metres.
float2 p=World.xy*.01;
float mean=dot(SurfaceLinear.xy,p)+SurfaceCurve.x*p.x*p.x+
           SurfaceCurve.y*p.x*p.y+SurfaceCurve.z*p.y*p.y;
float2 uv=(World.xy-FieldOrigin.xy)/FieldOrigin.z+.5/256.0;
float wake=Texture2DSampleLevel(WakeField,WakeFieldSampler,uv,0).r;
float fade=1-smoothstep(2200.0,2800.0,length(World.xy-Boat.xy));
// Vertical displacement leaves shoreline/hull masks aligned with geography.
return float3(0,0,mean*100+Wave.z+wake*100*fade*LocalPatch);
