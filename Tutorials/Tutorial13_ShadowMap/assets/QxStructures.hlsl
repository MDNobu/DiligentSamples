struct CubeVSInput
{
    float3 Pos : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV : ATTRIB2;
};

struct CubePSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;
    float NdotL : N_DOT_L;
};

struct PlanePSInput
{
    float4 Pos : SV_POSITION;
    float3 ShadowMapPos : SHADOW_MAP_POS;
    float NdotL : N_DOT_L;
};

struct ShadowMapVisPSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;
};
