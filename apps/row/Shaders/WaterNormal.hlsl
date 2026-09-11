// All positions are UE centimeters; the wave spectrum uses world-space meters.
float2 p = World.xy * 0.01;
float footprint=max(length(ddx(p)),length(ddy(p)));
p += .07*float2(sin(p.y*.83+Time*.21),sin(p.x*.61-Time*.13));
float2 uv = (World.xy - FieldOrigin.xy) / FieldOrigin.z + 0.5 / 256.0;
float4 wake = Texture2DSample(WakeField, WakeFieldSampler, uv);
float distanceToBoat = length(World.xy - Boat.xy);
float localWeight = 1.0 - smoothstep(2200.0, 2800.0, distanceToBoat);
float2 slope = float2(0, 0);
// Low lake chop and short wind ripples: separate wavelengths, travel directions,
// frequencies. Pixel normals remain detailed on distant low-density geometry.
float2 dirs[5] = {float2(.96,.28),float2(-.4,.9165),float2(.6,.8),float2(.89,-.456),float2(-.78,.626)};
float k[5] = {1.4,2.6,7.1,16.3,31.2};
float a[5] = {.014,.009,.0035,.0016,.00065};
for(int i=0;i<5;i++) {
    float phase=dot(p,dirs[i])*k[i]-Time*sqrt(9.81*k[i]);
    float aa=1-smoothstep(.5,2.0,k[i]*footprint);
    slope += dirs[i]*(a[i]*k[i]*cos(phase))*aa;
}
slope += wake.gb * localWeight;
float3 normal = normalize(float3(-slope,1));
// Smooth stochastic breakup avoids the grid/zigzag pattern of multiplied sines.
float2 cell=floor(p*5),f=frac(p*5); f=f*f*(3-2*f);
float4 hash=frac(sin(float4(dot(cell,float2(127.1,311.7)),
    dot(cell+float2(1,0),float2(127.1,311.7)),dot(cell+float2(0,1),float2(127.1,311.7)),
    dot(cell+1,float2(127.1,311.7))))*43758.5453);
float noise=lerp(lerp(hash.x,hash.y,f.x),lerp(hash.z,hash.w,f.x),f.y);
float foam=saturate(wake.a*localWeight)*(.25+.75*smoothstep(.15,.85,noise));
return float4(normal,foam);
