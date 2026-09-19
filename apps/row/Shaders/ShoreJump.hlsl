// Bounded jump flood: 16+8+4+2+1 pixels covers a 24 m surf band at 1 m/pixel.
float3 best=Texture2DSampleLevel(Source,SourceSampler,UV,0).rgb;
float2 delta=best.xy-UV;
float distance=best.b>.5 ? dot(delta,delta):1e10;
for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
    float2 uv=UV+float2(x,y)*Jump/512.0;
    if(any(uv<0) || any(uv>1)) continue;
    float3 candidate=Texture2DSampleLevel(Source,SourceSampler,uv,0).rgb;
    float2 d=candidate.xy-UV;
    float value=dot(d,d);
    if(candidate.b>.5 && value<distance) { best=candidate; distance=value; }
}
return best;
