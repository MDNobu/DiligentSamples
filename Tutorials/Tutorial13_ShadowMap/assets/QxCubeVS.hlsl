#include "QxStructures.hlsl"

cbuffer Constants
{
    float4x4 g_WorldViewProj;
    float4x4 g_NormalTransform;
    float4 g_LightDirection;
};

void main(
    in CubeVSInput VSIn,
    out CubePSInput PSIn
    )
{
    PSIn.Pos =
        mul(float4(VSIn.Pos, 1.f), g_WorldViewProj);
    float3 Normal =
        mul(float4(VSIn.Normal, 0.f), g_NormalTransform).xyz;
    PSIn.NdotL =
        saturate(dot(Normal, -g_LightDirection));
    PSIn.UV = VSIn.UV;
}