#include "ToneMapping.fxh"
#include "BasicStructures.fxh"

cbuffer cbCameraAttribs
{
    CameraAttribs g_CamearAttribs;
};

cbuffer cbEnvMapRenderAttribs
{
    ToneMappingAttribs g_ToneMappingAttribs;

    float AverageLogLum;
    float MipLevel;
    float Unusued1;
    float Unusued2;
};

struct PSInput
{
    float4 Pos : SV_Position;
    float4 ClipPos : CLIP_POS;
};


TextureCube EnvMap;
SamplerState EnvMap_sampler;



void main(
    in PSInput PSIn,
    out float4 OutColor : SV_Target
    )
{
    // clip to world space
    float4 PosWS = mul(PSIn.ClipPos, g_CamearAttribs.mViewProjInv);
    // #TODO 这里为什么/w
    float3 CameraToSamplePoint =
        PosWS.xyz / PosWS.w - g_CamearAttribs.f4Position.xyz;
    float4 EnvRadiance =
        EnvMap.SampleLevel(EnvMap_sampler, CameraToSamplePoint, MipLevel);

    OutColor =
        float4(ToneMap(EnvRadiance.rgb, g_ToneMappingAttribs, AverageLogLum),
            EnvRadiance.a);

    // OutColor.a = 1.f;
    // OutColor.rgba = EnvRadiance.rgba;
}