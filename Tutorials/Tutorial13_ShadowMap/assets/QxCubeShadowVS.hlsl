#include "QxStructures.hlsl"

cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
};

void main(
    in CubeVSInput VSIn,
    out PSInput PSIn
    )
{
    PSIn.Pos =
        mul(float4(VSIn.Pos, 1.), g_WorldViewProj);
}
