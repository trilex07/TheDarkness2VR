// SMAA 1x HIGH, no temporal history, jitter, depth or motion-vector guesses.
// Edge/color inputs and output use display-referred UNORM, as the NIS bridge.
cbuffer Metrics : register(b0) { float4 rtMetrics; };
#define SMAA_RT_METRICS rtMetrics
#define SMAA_HLSL_4
#define SMAA_PRESET_HIGH
#include "../third_party/smaa/SMAA.hlsl"
Texture2D Input0 : register(t0);
Texture2D Input1 : register(t1);
Texture2D Input2 : register(t2);
struct EdgeVertex { float4 position:SV_Position; float2 uv:TEXCOORD0; float4 offset[3]:TEXCOORD1; };
struct WeightVertex { float4 position:SV_Position; float2 uv:TEXCOORD0; float2 pixel:TEXCOORD1; float4 offset[3]:TEXCOORD2; };
struct BlendVertex { float4 position:SV_Position; float2 uv:TEXCOORD0; float4 offset:TEXCOORD1; };
void Triangle(uint id,out float4 position,out float2 uv) {
    uv=float2((id<<1)&2,id&2);
    position=float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
EdgeVertex EdgeVS(uint id:SV_VertexID) {
    EdgeVertex o;Triangle(id,o.position,o.uv);SMAAEdgeDetectionVS(o.uv,o.offset);return o;
}
WeightVertex WeightVS(uint id:SV_VertexID) {
    WeightVertex o;Triangle(id,o.position,o.uv);SMAABlendingWeightCalculationVS(o.uv,o.pixel,o.offset);return o;
}
BlendVertex BlendVS(uint id:SV_VertexID) {
    BlendVertex o;Triangle(id,o.position,o.uv);SMAANeighborhoodBlendingVS(o.uv,o.offset);return o;
}
float4 EdgePS(EdgeVertex i):SV_Target { return float4(SMAALumaEdgeDetectionPS(i.uv,i.offset,Input0),0,0); }
float4 WeightPS(WeightVertex i):SV_Target { return SMAABlendingWeightCalculationPS(i.uv,i.pixel,i.offset,Input0,Input1,Input2,0); }
float4 BlendPS(BlendVertex i):SV_Target { return SMAANeighborhoodBlendingPS(i.uv,i.offset,Input0,Input1); }
