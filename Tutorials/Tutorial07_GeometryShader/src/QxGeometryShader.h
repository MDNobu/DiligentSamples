
#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
class QxGeometryShader final : public SampleBase
{
public:
    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "Qx Geomety Shader";
    };

    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override;
private:
    void         CreatePipelineState();
    void         UpdateUI();

private:
    RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IBuffer> m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer> m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer> m_ShaderConstants;
    RefCntAutoPtr<ITextureView> m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;

    float4x4 m_WVP;
    float m_LineWidth = 3.f;
    
};
}
