#include "QxStructures.hlsl"

Texture2D g_ShadowMap;
SamplerState g_shadow_map_sampler;



void main(
    in ShadowMapVisPSInput PSIn,
    out float4 OutColor : SV_Target0)
{
    OutColor.rgb =
        g_ShadowMap.Sample(g_shadow_map_sampler,
            PSIn.UV).r * float3(1., 1., 1.);
    OutColor.a = 1.f;
}