// Compact FXAA-style directional edge filter, in display-referred LDR.
// Independent implementation of local-contrast/directional resampling from
// Timothy Lottes' FXAA paper (NVIDIA, 2011). Not the FXAA 3.11 quality preset.
// Preserve alpha and flat pixels; feather into the untouched source.
Texture2D<float4> image : register(t0);
SamplerState linearClamp : register(s0);
RWTexture2D<float4> outputImage : register(u0);
cbuffer Params : register(b0) {
    uint2 dimensions; uint2 origin;
    float2 center; float innerHalf; float outerHalf;
    uint2 extent; float2 padding;
};
float Luma(float3 c) { return dot(c,float3(.299,.587,.114)); }
float3 Read(float2 uv) { return image.SampleLevel(linearClamp,uv,0).rgb; }
[numthreads(8,8,1)]
void main(uint3 thread : SV_DispatchThreadID) {
    if(any(thread.xy>=extent)) return;
    uint2 p=origin+thread.xy;
    if(any(p>=dimensions)) return;
    float4 original=image.Load(int3(p,0));
    float distance=max(abs(float(p.x)+.5-center.x),abs(float(p.y)+.5-center.y));
    float mask=1-smoothstep(innerHalf,outerHalf,distance);
    if(mask<=0) { outputImage[p]=original; return; }
    float2 inverseSize=1.0/float2(dimensions);
    float2 uv=(float2(p)+.5)*inverseSize;
    float nw=Luma(Read(uv+float2(-1,-1)*inverseSize));
    float ne=Luma(Read(uv+float2( 1,-1)*inverseSize));
    float sw=Luma(Read(uv+float2(-1, 1)*inverseSize));
    float se=Luma(Read(uv+float2( 1, 1)*inverseSize));
    float mid=Luma(original.rgb);
    float lo=min(mid,min(min(nw,ne),min(sw,se)));
    float hi=max(mid,max(max(nw,ne),max(sw,se)));
    if(hi-lo<max(.0312,hi*.125)) { outputImage[p]=original; return; }
    float2 direction=float2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));
    float reduce=max((nw+ne+sw+se)*(.25*.125),1.0/128.0);
    float reciprocal=1.0/(min(abs(direction.x),abs(direction.y))+reduce);
    direction=clamp(direction*reciprocal,-8.0,8.0)*inverseSize;
    float3 a=.5*(Read(uv+direction*(-1.0/6.0))+Read(uv+direction*(1.0/6.0)));
    float3 b=.5*a+.25*(Read(uv-direction*.5)+Read(uv+direction*.5));
    float lb=Luma(b);
    outputImage[p]=float4(lerp(original.rgb,(lb<lo || lb>hi)?a:b,mask),original.a);
}
