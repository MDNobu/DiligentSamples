cbuffer Constants
{
    float4x4 g_ViewProj;
    float4x4 g_Rotation;
};

cbuffer InstanceData
{
    float4x4 g_InstanceMatr;
};

struct VSInput
{
    float3 Pos : ATTRIB0;
    float2 UV : ATTRIB1;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;  
};

void main(in VSInput VSIn, 
    out PSInput PSIn)
{
    float4 transformedPos = mul(float4(VSIn.Pos, 1.0), g_Rotation);
    
    transformedPos = mul(transformedPos, g_InstanceMatr);

    PSIn.Pos = mul(transformedPos, g_ViewProj);
    PSIn.UV = VSIn.UV;
}