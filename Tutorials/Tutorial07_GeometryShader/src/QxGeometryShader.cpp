#include "QxGeometryShader.h"
#include "../../Common/src/TexturedCube.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "imgui.h"
#include <cassert>

#include "GraphicsUtilities.h"

namespace Diligent
{
namespace
{
    struct Constants
    {
        float4x4 WorldViewProj;
        float4 ViewportSize;
        float LineWidth;
    };
}


void QxGeometryShader::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);
    Attribs.EngineCI.Features.GeometryShaders = DEVICE_FEATURE_STATE_ENABLED;
}

void QxGeometryShader::CreatePipelineState()
{
        GraphicsPipelineStateCreateInfo psoCreateInfo;

    psoCreateInfo.PSODesc.Name = "Cube PSO";
    psoCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCreateInfo.GraphicsPipeline.NumViewports = 1;
    psoCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    psoCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    psoCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE::CULL_MODE_BACK;
    psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;

    CreateUniformBuffer(
        m_pDevice,
        sizeof(Constants),
        "Shader constants CB",
        &m_ShaderConstants
        );

    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderResourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderResourceFactory);
    shaderCI.pShaderSourceStreamFactory = pShaderResourceFactory;
    
    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Cube VS";
        shaderCI.FilePath = "QxCube.vsh";
        m_pDevice->CreateShader(shaderCI, &pVS);
        
        assert(pVS.RawPtr());
    }

    RefCntAutoPtr<IShader> pGS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Cube GS";
        shaderCI.FilePath = "QxCube.gsh";
        m_pDevice->CreateShader(shaderCI, &pGS);
        assert(pGS.RawPtr());
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Cube PS";
        shaderCI.FilePath = "QxCube.psh";
        m_pDevice->CreateShader(shaderCI, &pPS);
        assert(pPS.RawPtr());
    }

    LayoutElement layoutElements[] =
    {
        LayoutElement{0, 0, 3, VT_FLOAT32, false},
        LayoutElement{1, 0, 2, VT_FLOAT32, false}
    };

    psoCreateInfo.pVS = pVS;
    psoCreateInfo.pPS = pPS;
    psoCreateInfo.pGS = pGS;

    psoCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = layoutElements;
    psoCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(layoutElements);

    psoCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc vars[] =
    {
        ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };

    psoCreateInfo.PSODesc.ResourceLayout.Variables = vars;
    psoCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(vars);

    SamplerDesc samLinearClampDes
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc samplerDescs[] =
    {
        ImmutableSamplerDesc{SHADER_TYPE_PIXEL, "g_Texture", samLinearClampDes}
    };
    psoCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers =
        samplerDescs;
    psoCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
        _countof(samplerDescs);

    m_pDevice->CreateGraphicsPipelineState(psoCreateInfo, &m_pPSO);

    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")->Set(m_ShaderConstants);
    IShaderResourceVariable* gsConstants = m_pPSO->GetStaticVariableByName(SHADER_TYPE_GEOMETRY, "GSConstants");
    gsConstants->Set(m_ShaderConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")->Set(m_ShaderConstants);

    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}



void QxGeometryShader::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    CreatePipelineState();

    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV);
    m_CubeIndexBuffer = TexturedCube::CreateIndexBuffer(m_pDevice);
    m_TextureSRV = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")
        ->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}

void QxGeometryShader::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // clear the back buffer
    const float clearColor[] = {0.35f, 0.35f, 0.35f, 1.f};
    m_pImmediateContext->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        MapHelper<Constants> consts(m_pImmediateContext, m_ShaderConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        consts->WorldViewProj = m_WVP.Transpose();

        const auto& scDesc = m_pSwapChain->GetDesc();
        consts->ViewportSize = float4(
            static_cast<float>(scDesc.Width)
            , static_cast<float>(scDesc.Height)
            ,  1.f /static_cast<float>(scDesc.Width)
            ,  1.f / static_cast<float>(scDesc.Height));

        consts->LineWidth = m_LineWidth;
    }

    IBuffer* pBuffers[] = {m_CubeVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffers, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_pImmediateContext->SetPipelineState(m_pPSO);
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    
    DrawIndexedAttribs drawAttrs;
    drawAttrs.IndexType = VT_UINT32;
    drawAttrs.NumIndices = 36;
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(drawAttrs);
}

void QxGeometryShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    UpdateUI();
    // Apply rotation
    float4x4 CubeModelTransform = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-PI_F * 0.1f);

    // Camera is at (0, 0, -5) looking along the Z axis
    float4x4 View = float4x4::Translation(0.f, 0.0f, 5.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute world-view-projection matrix
    m_WVP = CubeModelTransform * View * SrfPreTransform * Proj;
}

void QxGeometryShader::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SliderFloat("Line Width", &m_LineWidth, 1.f, 10.f);
    }
    ImGui::End();
}
}
