#include "QxInstancing.h"

#include "GraphicsUtilities.h"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "MapHelper.hpp"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{
void QxInstancing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatPipelineState();

    // Load Textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(
        m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV);
    m_CubeIndexBuffer = TexturedCube::CreateIndexBuffer(
        m_pDevice);

    m_TextureSRV = TexturedCube::LoadTexture(m_pDevice,
        "DGLogo.png")->GetDefaultView(
            TEXTURE_VIEW_SHADER_RESOURCE);

    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
    
    CreatInstanceBuffer();
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
        LayoutElement{5, 1, 4, VT_FLOAT32, false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
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
}


