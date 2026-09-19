// Waterline shoreline data: R influence, GB signed direction toward shore.
float3 seed=Texture2DSampleLevel(Source,SourceSampler,UV,0).rgb;
float2 delta=(seed.xy-UV)*512.0;
float distance=length(delta);
if(seed.b<.5 || distance<.5 || distance>=24.0) return float3(0,0,0);
return float3(1-distance/24.0,delta/max(distance,.001));
