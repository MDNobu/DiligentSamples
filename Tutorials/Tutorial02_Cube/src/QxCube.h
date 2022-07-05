#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
class QxCube final : public SampleBase
{
public:
    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "Qx Cube Tutorial";
    }

private:
    void         CreatePipelineState();
    void         CreateVertexBuffer();
    void         CreateIndexBuffer();
private:
    RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    RefCntAutoPtr<IBuffer> m_CubeVertxBuffer;
    RefCntAutoPtr<IBuffer> m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer> m_VSContants;

    float4x4 m_WorldViewProjMatrix;
    
};
}