#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
class QxShadowMap final : public SampleBase
{
public:
    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    void        Update(double CurrTime, double ElapsedTime) override;
    void        ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override;
    const Char* GetSampleName() const override
    {
        return "Qx Shadow Map";
    };

private:
    void CreateCubePSO();
    void CreatePlanePSO();
    void CreateShadowMapVisPSO();
    void UpdateUI();
    void CreateShadowMap();
    void RenderShadowMap();
    void RenderCube(const float4x4&
        CameraViewProj,
        bool IsShadowPass);
    void RenderPlane();
    void RenderShadowMapVis();

    RefCntAutoPtr<IPipelineState> m_pCubePSO;
    RefCntAutoPtr<IPipelineState> m_pPlanePSO;
    RefCntAutoPtr<IPipelineState> m_pCubeShadowPSO;
    RefCntAutoPtr<IPipelineState> m_pShadowMapVisPSO;

    RefCntAutoPtr<IBuffer> m_CuberVertexBuffer;
    RefCntAutoPtr<IBuffer> m_CubeIndexBufffer;
    RefCntAutoPtr<IBuffer> m_VSConstants;
    RefCntAutoPtr<ITextureView> m_TexutureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_CubeSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_CubeShadowSRB;

    RefCntAutoPtr<IShaderResourceBinding> m_PlaneSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_ShadowMapVisSRB;
    RefCntAutoPtr<ITextureView> m_ShadowMapDSV;
    RefCntAutoPtr<ITextureView> m_ShadowMapSRV;

    float4x4 m_CubeWorldMatrix;
    float4x4 m_CameraVieweProjMatrix;
    float4x4 m_WorldToShadowMapUVDepthMatr;
    float3 m_LightDirection =
        normalize(float3(-0.49f, -0.60f, 0.64f));
    Uint32 m_ShadowMapSize = 512;
    TEXTURE_FORMAT m_ShadowMapFormat =
        TEX_FORMAT_D16_UNORM;
};
}