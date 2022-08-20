#include "QxShadowMap.h"

#include "GraphicsUtilities.h"
#include "../../../../DiligentCore/ThirdParty/glslang/glslang/MachineIndependent/glslang_tab.cpp.h"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{
void QxShadowMap::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    std::vector<StateTransitionDesc> Barriers;

    CreateUniformBuffer(
        m_pDevice,
        sizeof(float4x4) * 2 + sizeof(float4),
        "VS Constants CB",
        &m_VSConstants
        );
    Barriers.emplace_back(
        m_VSConstants,
        RESOURCE_STATE_UNKNOWN,
        RESOURCE_STATE_CONSTANT_BUFFER,
        STATE_TRANSITION_FLAG_UPDATE_STATE
        );

    CreateCubePSO();
    CreatePlanePSO();
    CreateShadowMapVisPSO();

    // Load Cube
    m_CuberVertexBuffer =
        TexturedCube::CreateVertexBuffer(
            m_pDevice,
            TexturedCube::VERTEX_COMPONENT_FLAG_POS_NORM_UV);
    m_CubeIndexBufffer =
        TexturedCube::CreateIndexBuffer(
            m_pDevice);

    Barriers.emplace_back(
        m_CuberVertexBuffer,
        RESOURCE_STATE_UNKNOWN,
        RESOURCE_STATE_VERTEX_BUFFER,
        STATE_TRANSITION_FLAG_UPDATE_STATE);
    Barriers.emplace_back(
        m_CubeIndexBufffer,
        RESOURCE_STATE_UNKNOWN,
        RESOURCE_STATE_INDEX_BUFFER,
        STATE_TRANSITION_FLAG_UPDATE_STATE);

    auto CubeTexture =
        TexturedCube::LoadTexture(
            m_pDevice, "DGLogo.png");

    m_CubeSRB->GetVariableByName(
        SHADER_TYPE_PIXEL,
        "g_Texture")->Set(
            CubeTexture->GetDefaultView(
                TEXTURE_VIEW_SHADER_RESOURCE));
    Barriers.emplace_back(
        CubeTexture,
        RESOURCE_STATE_UNKNOWN,
        RESOURCE_STATE_SHADER_RESOURCE,
        STATE_TRANSITION_FLAG_UPDATE_STATE);

    CreateShadowMap();
    m_pImmediateContext->TransitionResourceStates(
        static_cast<Uint32>(
            Barriers.size()),
            Barriers.data()
            ));
    
}

void QxShadowMap::Render()
{
    
}

void QxShadowMap::Update(double CurrTime, double ElapsedTime)
{
    
}

void QxShadowMap::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.DepthClamp =
        DEVICE_FEATURE_STATE_OPTIONAL;
}

void QxShadowMap::CreateCubePSO()
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(
        nullptr, &pShaderSourceFactory);

    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice =
        m_pDevice;
    CubePsoCI.RTVFormat =
        m_pSwapChain->GetDesc().ColorBufferFormat;
    CubePsoCI.DSVFormat =
        m_pSwapChain->GetDesc().DepthBufferFormat;
    CubePsoCI.pShaderSourceFactory =
        pShaderSourceFactory;
    CubePsoCI.VSFilePath = "QxCubeVS.hlsl";
    CubePsoCI.PSFilePath = "QxCubePS.hlsl";
    CubePsoCI.Components =
        TexturedCube::VERTEX_COMPONENT_FLAG_POS_NORM_UV;

    m_pCubePSO =
        TexturedCube::CreatePipelineState(CubePsoCI);

    m_pCubePSO->GetStaticVariableByName(
        SHADER_TYPE_PIXEL, "Constants")->Set(m_VSConstants);

    m_pCubePSO->CreateShaderResourceBinding(
        &m_CubeSRB,
        true);

    // Create Shadow pass PSO
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "Cube ShadowPSO";

    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 0;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_UNKNOWN;
    PSOCreateInfo.GraphicsPipeline.DSVFormat =
        m_ShadowMapFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
        PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pShadowVS;
    {
        ShaderCI.Desc.ShaderType= SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Cube Shadow VS";
        ShaderCI.FilePath = "QxCubeShadowVS.hlsl";
        m_pDevice->CreateShader(ShaderCI, &pShadowVS);
    }
    PSOCreateInfo.pVS = pShadowVS;

    PSOCreateInfo.pPS = nullptr;

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - normal
        LayoutElement{2, 0, 3, VT_FLOAT32, False},
        // Attribute 2 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False}
    };

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements =
        _countof(LayoutElems);

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    if (m_pDevice->GetDeviceInfo().Features.DepthClamp)
    {
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = false;
    }

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCubeShadowPSO);
    m_pCubeShadowPSO->GetStaticVariableByName(
        SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pCubeShadowPSO->CreateShaderResourceBinding(
        &m_CubeShadowSRB, true);
    
}

void QxShadowMap::CreatePlanePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Plane PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial renders to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // No cull
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    ShaderCI.UseCombinedTextureSamplers = true;
    
    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create plane vertex shader
    RefCntAutoPtr<IShader> pPlaneVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane VS";
        // ShaderCI.FilePath        = "plane.vsh";
        ShaderCI.FilePath = "QxPlaneVS.hlsl";
        m_pDevice->CreateShader(ShaderCI, &pPlaneVS);
    }

    // Create plane pixel shader
    RefCntAutoPtr<IShader> pPlanePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane PS";
        // ShaderCI.FilePath        = "plane.psh";
        ShaderCI.FilePath = "QxPlanePS.hlsl";
        m_pDevice->CreateShader(ShaderCI, &pPlanePS);
    }

    PSOCreateInfo.pVS = pPlaneVS;
    PSOCreateInfo.pPS = pPlanePS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] =
    {
        {
            SHADER_TYPE_PIXEL, "g_ShadowMap",
            SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE
        }
    };

    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    SamplerDesc ComparsionSampler;
    ComparsionSampler.MinFilter = FILTER_TYPE_COMPARISON_LINEAR;
    ComparsionSampler.MagFilter = FILTER_TYPE_COMPARISON_LINEAR;
    ComparsionSampler.MipFilter = FILTER_TYPE_COMPARISON_LINEAR;

    ImmutableSamplerDesc ImtbSamplers[] =
    {
        {
            SHADER_TYPE_PIXEL,
            "g_ShadowMap",
            ComparsionSampler
        }
    };
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers =
        ImtbSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
        _countof(ImtbSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPlanePSO);

    m_pPlanePSO->GetStaticVariableByName(
        SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
}

void QxShadowMap::CreateShadowMapVisPSO()
{
    
}

void QxShadowMap::UpdateUI()
{
    
}

void QxShadowMap::CreateShadowMap()
{
    
}

void QxShadowMap::RenderShadowMap()
{
    
}

void QxShadowMap::RenderCube(const float4x4& CameraViewProj, bool IsShadowPass)
{
    
}

void QxShadowMap::RenderPlane()
{
    
}

void QxShadowMap::RenderShadowMapVis()
{
    
}
}
