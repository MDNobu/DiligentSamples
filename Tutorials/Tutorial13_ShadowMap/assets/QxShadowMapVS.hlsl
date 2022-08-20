#include "QxStructures.hlsl"

void main(
    in uint VerId : SV_VertexID,
    out ShadowMapVisPSInput PSIn
    )
{
    float2 Pos[] = {
        float2(-1.0, -1.0),
        float2(-1.0, +1.0),
        float2(+1.0, -1.0),
        float2(+1.0, +1.0),
    };

    float2 Center = float2(-0.6, - 0.6);
    float2 Size = float2(0.35, 0.35);

    PSIn.Pos =
        float4(Center + Size * Pos[VerId], 0. , 1.);
    PSIn.UV =
        Pos[VerId].xy * F3NDC_XYZ_TO_UVD_SCALE.xy + float2(0.5, 0.5);
}