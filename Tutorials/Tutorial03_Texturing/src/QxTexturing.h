
#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{

class QxTexturing final : public SampleBase
{
public:
    void Initialize(const SampleInitInfo& InitInfo) override;

    virtual void Render() override;

    void Update(double CurrTime, double ElapsedTime) override;

private:

    RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IBuffer>        m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>        m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>        m_WVP_VS;
    RefCntAutoPtr<ITextureView>   m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;

    float4x4 m_WorldViewProj;
    
};
} // namespace Diligent