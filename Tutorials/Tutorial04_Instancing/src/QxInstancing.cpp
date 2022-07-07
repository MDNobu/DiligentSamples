#include "QxInstancing.h"

#include <random>

#include "GraphicsUtilities.h"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "MapHelper.hpp"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{
struct InstanceData
{
    float4x4 Matrix;
    float TextureIndex = 0.f;
};

void QxInstancing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatPipelineState();

    // Load Textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(
        m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV);
    m_CubeIndexBuffer = TexturedCube::CreateIndexBuffer(
        m_pDevice);

    // m_TextureSRV = TexturedCube::LoadTexture(m_pDevice,
    //     "DGLogo.png")->GetDefaultView(
    //         TEXTURE_VIEW_SHADER_RESOURCE);
    //
    // IShaderResourceVariable* texturePSVar =
    //     m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture");
    // texturePSVar->Set(m_TextureSRV);
   // ->Set(m_TextureSRV);
    
    CreatInstanceBuffer();
    LoadTextures();
}

void QxInstancing::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();

    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        MapHelper<float4x4> cbConstance(m_pImmediateContext,
            m_VS_Constants, MAP_WRITE, MAP_FLAG_DISCARD);
        cbConstance[0] = m_ViewProjMatrix.Transpose();
        cbConstance[1] = m_RoationMatrix.Transpose();
    }

    // bind vb/ib/instance buffer
    const Uint64 offsets[] = {0, 0};
    IBuffer* pBuffers[] = {m_CubeVertexBuffer, m_InstancedBuffer};
    m_pImmediateContext->SetVertexBuffers(0,
                                          _countof(pBuffers), pBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION
                                          , SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer,
                                        0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // set pso
    m_pImmediateContext->SetPipelineState(m_pPSO);
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttribs;
    drawAttribs.IndexType  = VT_UINT32;
    drawAttribs.NumIndices = 36;
    drawAttribs.NumInstances =
        m_GridSize * m_GridSize * m_GridSize;
    drawAttribs.Flags      = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(drawAttribs);
}

void QxInstancing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    UpdateUI();

    float4x4 viewMat = float4x4::RotationX(-0.6f)
        *  float4x4::Translation(0.f, 0.f, 4.f);

    //#TODO 这里不太明白，待查看
    auto srfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    float4x4 projMat = GetAdjustedProjectionMatrix(PI_F/4.F, 0.1f, 100.f);
    
    m_ViewProjMatrix = viewMat * srfPreTransform * projMat;

    // global rotation matrix
    m_RoationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) *
        float4x4::RotationX(-static_cast<float>(CurrTime) * 0.25f);
}

void QxInstancing::CreatPipelineState()
{
    LayoutElement layoutElements[] =
    {
        LayoutElement{0, 0, 3, VT_FLOAT32, false },
        LayoutElement{1, 0, 2, VT_FLOAT32, false},

        // per intanced data - second buffer slot
        LayoutElement{2, 1, 4, VT_FLOAT32, false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        LayoutElement{3, 1, 4, VT_FLOAT32, false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        LayoutElement{4, 1, 4, VT_FLOAT32, false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        LayoutElement{5, 1, 4, VT_FLOAT32, false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        LayoutElement{6, 1, 1, VT_FLOAT32, false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
    };

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderResourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderResourceFactory);

    TexturedCube::CreatePSOInfo cubePsoCi;
    cubePsoCi.pDevice = m_pDevice;
    cubePsoCi.RTVFormat = m_pSwapChain->GetDesc().ColorBufferFormat;
    cubePsoCi.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    cubePsoCi.pShaderSourceFactory = pShaderResourceFactory;
    cubePsoCi.VSFilePath = "QxCubeInst.vsh";
    cubePsoCi.PSFilePath = "QxCubeInst.psh";
    cubePsoCi.ExtraLayoutElements = layoutElements;
    cubePsoCi.NumExtraLayoutElements = _countof(layoutElements);

    m_pPSO = TexturedCube::CreatePipelineState(cubePsoCi);

    // 创建存储 vp rotation矩阵的uniform buffer
    CreateUniformBuffer(
        m_pDevice,
        sizeof(float4x4) * 2,
        "VS constants CB",
        &m_VS_Constants
        );

    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX,
        "QxConstants")->Set(m_VS_Constants);

    
    m_pPSO->CreateShaderResourceBinding(
        &m_SRB, true);
}

void QxInstancing::CreatInstanceBuffer()
{
    BufferDesc instanceBufferDesc;
    instanceBufferDesc.Name = "Instance data buffer";
    // 只有 instance 数量变化才需要重新上传，所以这里用default
    instanceBufferDesc.Usage = USAGE_DEFAULT;
    instanceBufferDesc.BindFlags = BIND_VERTEX_BUFFER;
    instanceBufferDesc.Size = sizeof(float4x4) * MaxInstances;
    m_pDevice->CreateBuffer(instanceBufferDesc,
        nullptr, &m_InstancedBuffer);
    PopulateInstanceBuffer();
}

void QxInstancing::PopulateInstanceBuffer()
{
    const size_t          zGridSize = static_cast<size_t>(m_GridSize);
    // std::vector<float4x4> instanceData(
    //     zGridSize * zGridSize * zGridSize);
    std::vector<InstanceData> instanceDatas(zGridSize * zGridSize * zGridSize);
    

    float fGridSize = static_cast<float>(m_GridSize);

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
    // to generate consistent distribution.

    std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
    std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
    std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);
    std::uniform_int_distribution<Int32>  tex_distr(0, NumTextures - 1);

    float baseScale = 0.6f / fGridSize;
    int instId = 0;
    for (int x = 0; x < m_GridSize; ++x)
    {
        for (int y = 0; y < m_GridSize; ++y)
        {
            for (int z = 0; z < m_GridSize; ++z)
            {
                // Add random offset from central position in the grid
                float xOffset = 2.f * (x + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float yOffset = 2.f * (y + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float zOffset = 2.f * (z + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                // Random scale
                float scale = baseScale * scale_distr(gen);
                // Random rotation
                float4x4 rotation = float4x4::RotationX(rot_distr(gen)) * float4x4::RotationY(rot_distr(gen)) * float4x4::RotationZ(rot_distr(gen));
                // Combine rotation, scale and translation
                float4x4 matrix        = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(xOffset, yOffset, zOffset);
                InstanceData& curInst = instanceDatas[instId++];
                curInst.Matrix = matrix;
                curInst.TextureIndex = static_cast<float>(tex_distr(gen));
                // instanceData[instId++] = matrix;
            }
        }
    }

    // Update instance data buffer
    Uint32 dataSize = static_cast<Uint32>(
        sizeof(instanceDatas[0]) * instanceDatas.size());
    m_pImmediateContext->UpdateBuffer(
        m_InstancedBuffer, 0, dataSize,
        instanceDatas.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void QxInstancing::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt("Grid Size", &m_GridSize, 1, 32))
        {
            PopulateInstanceBuffer();
        }
    }
    
    ImGui::End();
}

void QxInstancing::LoadTextures()
{
    RefCntAutoPtr<ITexture> pTextureArray;
    for (int texIndex = 0; texIndex < NumTextures; ++texIndex)
    {
        std::stringstream filenameSS;
        filenameSS << "DGLogo" << texIndex << ".png";
        const auto fileName= filenameSS.str();
        RefCntAutoPtr<ITexture> srcTex = TexturedCube::LoadTexture(
            m_pDevice, fileName.c_str());
        const auto& texDesc = srcTex->GetDesc();
        if (pTextureArray == nullptr)
        {
            auto texArrayDesc = texDesc;
            texArrayDesc.ArraySize = NumTextures;
            texArrayDesc.Type = RESOURCE_DIM_TEX_2D_ARRAY;
            texArrayDesc.Usage = USAGE_DEFAULT;
            texArrayDesc.BindFlags = BIND_SHADER_RESOURCE;
            m_pDevice->CreateTexture(texArrayDesc, nullptr, &pTextureArray);
        }

        // copy current texture into the texture array
        for (int mip = 0; mip < texDesc.MipLevels; ++mip)
        {
            CopyTextureAttribs copyAttib(
                srcTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                pTextureArray, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            copyAttib.DstMipLevel = mip;
            copyAttib.SrcMipLevel = mip;
            copyAttib.DstSlice = texIndex;
            m_pImmediateContext->CopyTexture(copyAttib);
        }
    }

    m_TextureSRV = pTextureArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
    
 }
}


