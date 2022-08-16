#pragma once

#include <array>
#include <random>
#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
class QxResourceUdpate : public SampleBase
{
public:

    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "Qx Resource Update";
    };

private:
    void        CreatePipelineStates();
    void        CreateVertexBuffers();
    void        CreateIndexBuffer();
    void         LoadTexture();

    void DrawCube(
        const float4x4 WVPMatrix,
        IBuffer* pVertexBuffer,
        IShaderResourceBinding* pSRB);

    void UpdateBuffer(Diligent::Uint32 BufferIndex);

    void MapDynamicBuffer(Diligent::Uint32 BufferIndex);

    void UpdateTexture(Uint32 TexIndex);

    void MapTexture(Uint32 TexIndex, bool MapEntireTexture);
private:
    RefCntAutoPtr<IPipelineState> m_pPSO, m_pPSO_NoCull;
    RefCntAutoPtr<IBuffer> m_CubeVertexBuffer[3];
    RefCntAutoPtr<IBuffer> m_CuberIndexBuffer;
    RefCntAutoPtr<IBuffer> m_VSConstants;
    RefCntAutoPtr<IBuffer> m_TextureUpdateBuffer;
    
    
    static constexpr const size_t NumTexture = 4;
    static constexpr const Uint32 MaxUpdateRegionSize = 128;
    static constexpr const Uint32 MaxMapRegionSize = 128;

    std::array<RefCntAutoPtr<ITexture>, NumTexture> m_Textures;
    std::array<RefCntAutoPtr<IShaderResourceBinding>, NumTexture> m_SRBs;

    double m_LastTextureUpdateTime = 0;
    double m_LastBufferUpdateTime = 0;
    double m_LastMapTime = 0;

    std::mt19937 m_gen{0};
    double m_CurrentTime = 0;
};

}
