#include "QxStructures.hlsl"

Texture2D g_Texture;
SamplerState g_texture_sampler;

struct PSOutput
{
    float4 Color : SV_Target;
};

void main(
    in CubePSInput PSIn,
    out PSOutput PSOut
    )
{
    PSOut.Color =
        g_Texture.Sample(
            g_texture_sampler, PSIn.UV)
            * (PSIn.NdotL * 0.8 + 0.2);
}
