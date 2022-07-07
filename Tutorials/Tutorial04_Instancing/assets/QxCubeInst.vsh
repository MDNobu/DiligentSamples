cbuffer QxConstants
{
    float4x4 g_ViewProj;
    float4x4 g_Rotation;
};

struct VSInput
{
    // vertex attributes
    float3 Pos : ATTRIB0;
    float2 UV : ATTRIB1;
    
    // instance attributes
    float4 ModelMatRow0 : ATTRIB2;
    float4 ModelMatRow1 : ATTRIB3;
    float4 ModelMatRow2 : ATTRIB4;
    float4 ModelMatRow3 : ATTRIB5;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;
};

void main(in VSInput VSIn, out PSInput VSOut)
{
    float4x4 modelMatInstance = 
        float4x4(VSIn.ModelMatRow0, VSIn.ModelMatRow1, VSIn.ModelMatRow2, VSIn.ModelMatRow3);
    
    float4 transformedPos = mul(float4(VSIn.Pos, 1.), g_Rotation);
    
    transformedPos = mul(transformedPos, modelMatInstance);    
    
    VSOut.Pos = mul(transformedPos, g_ViewProj);
    VSOut.UV = VSIn.UV;
}