#include "QxStructures.hlsl"

Texture2D g_ShadowMap;
SamplerComparisonState g_ShadowMap_sampler;

struct PlanePSOutput
{
    float4 Color : SV_Target;
};

void main(
    in PlanePSInput PSIn,
    out PlanePSOutput PSOut
    )
{
    float LightAmount =
        g_ShadowMap.SampleCmp(
            g_ShadowMap_sampler,
            PSIn.ShadowMapPos.xy,
            max(PSIn.ShadowMapPos.z, 1e-7));
    PSOut.Color.rgb =
        float3(1., 1., 1.) *
            (PSIn.NdotL * LightAmount * 0.8 + 0.2);
    PSOut.Color.a = 1.0;
}
