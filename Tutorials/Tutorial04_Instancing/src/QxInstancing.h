#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
class QxInstancing final : public SampleBase
{
public:

    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    
    void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "QxInstancing";
    };
private:
    void CreatPipelineState();
    void CreatInstanceBuffer();
    void  PopulateInstanceBuffer();
    void UpdateUI();
private:

    RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IBuffer> m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer> m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer> m_VS_Constants;
    RefCntAutoPtr<IBuffer> m_InstancedBuffer;

    RefCntAutoPtr<ITextureView> m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;

    float4x4 m_ViewProjMatrix;
    float4x4 m_RoationMatrix;
    int m_GridSize = 5;

    static constexpr int MaxGridSize = 5;
    static constexpr int MaxInstances = MaxGridSize * MaxGridSize * MaxGridSize; 
};


}
