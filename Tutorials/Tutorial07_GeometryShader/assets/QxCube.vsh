#include "QxStructures.fxh"

cbuffer VSConstants
{
    Constants g_Constants;
};

struct VSInput
{
    float3 Pos : ATTRIB0;
    float2 UV : ATTRIB1;
};


void main(in VSInput VSIn, out VSOutput VSOut)
{
    VSOut.Pos = mul(float4(VSIn.Pos, 1.0),
        g_Constants.WorldViewProj);
    VSOut.UV = VSIn.UV;
}