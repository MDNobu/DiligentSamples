#include "QxShadowMap.h"

#include "GraphicsUtilities.h"
#include "imgui.h"
#include "imGuIZMO.h"
#include "MapHelper.hpp"
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
        );
    
}

void QxShadowMap::Render()
{
    // Render shadow map
    m_pImmediateContext->SetRenderTargets(
        0,
        nullptr,
        m_ShadowMapDSV,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        m_ShadowMapDSV,
        CLEAR_DEPTH_FLAG,
        1.f,
        0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    RenderShadowMap();

    // bind main back buffer
    ITextureView* pRTV =
        m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV =
        m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->SetRenderTargets(
        1, &pRTV, pDSV,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(
        pRTV, ClearColor,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    RenderCube(m_CameraVieweProjMatrix, false);
    RenderPlane();
    RenderShadowMapVis();
    
}

void QxShadowMap::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    m_CubeWorldMatrix =
        float4x4::RotationY(static_cast<float>(CurrTime) * 1.f);

    float4x4 CameraView =
        float4x4::Translation(0.f, -5.f, -10.f) *
            float4x4::RotationY(PI_F) *
                float4x4::RotationX(-PI_F * 0.2);

    float4x4 SrfPreTransform =
        GetSurfacePretransformMatrix(float3{0, 0, 1});

    float4x4 Proj =
        GetAdjustedProjectionMatrix(
            PI_F / 4.f, 0.1f, 100.f);

    m_CameraVieweProjMatrix =
        CameraView * SrfPreTransform * Proj;
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

    auto constantShaderVariable = m_pCubePSO->GetStaticVariableByName(
        SHADER_TYPE_VERTEX, "Constants");
    constantShaderVariable->Set(m_VSConstants);
    // m_pCubePSO->GetStaticVariableByName(
    //     SHADER_TYPE_PIXEL, "Constants")->Set(m_VSConstants);

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
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] =
        m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat =
        m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
        PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode =
        CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable =
        false;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(
        nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create shadow map visualization vertex shader
    RefCntAutoPtr<IShader> pShadowMapVisVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis VS";
        ShaderCI.FilePath        = "shadow_map_vis.vsh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisVS);
    }

    // Create shadow map visualization pixel shader
    RefCntAutoPtr<IShader> pShadowMapVisPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis PS";
        ShaderCI.FilePath        = "shadow_map_vis.psh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisPS);
    }
    PSOCreateInfo.pVS = pShadowMapVisVS;
    PSOCreateInfo.pPS = pShadowMapVisPS;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    // clang-format off
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SamLinearClampDesc}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(
        PSOCreateInfo, &m_pShadowMapVisPSO);
}

void QxShadowMap::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10),
        ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        constexpr  int MinShadowMapSize = 256;
        int ShadowMapComboId = 0;
        while ((MinShadowMapSize << ShadowMapComboId) !=
            static_cast<int>(m_ShadowMapSize))
        {
            ShadowMapComboId++;
        }
        if (ImGui::Combo("Shadow map size", &ShadowMapComboId,
            "256\0"
            "512\0"
            "1024\0\0"))
        {
            m_ShadowMapSize = MinShadowMapSize << ShadowMapComboId;
            CreateShadowMap();
        }
        ImGui::gizmo3D("##LightDirection",
            m_LightDirection, ImGui::GetTextLineHeight() * 10);
    }
    ImGui::End();
}

void QxShadowMap::CreateShadowMap()
{
    TextureDesc SMDesc;
    SMDesc.Name = "Shadow map";
    SMDesc.Type = RESOURCE_DIM_TEX_2D;
    SMDesc.Width = m_ShadowMapSize;
    SMDesc.Height = m_ShadowMapSize;
    SMDesc.Format = m_ShadowMapFormat;
    SMDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    RefCntAutoPtr<ITexture> ShadowMap;
    m_pDevice->CreateTexture(SMDesc, nullptr, &ShadowMap);
    m_ShadowMapDSV = ShadowMap->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    m_ShadowMapSRV = ShadowMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    m_PlaneSRB.Release();
    m_pPlanePSO->CreateShaderResourceBinding(
        &m_PlaneSRB, true);
    m_PlaneSRB->GetVariableByName(
        SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(
            m_ShadowMapSRV);
    
    m_ShadowMapVisSRB.Release();
    m_pShadowMapVisPSO->CreateShaderResourceBinding(
        &m_ShadowMapVisSRB, true);
    m_ShadowMapVisSRB->GetVariableByName(
        SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(
            m_ShadowMapSRV);
}

void QxShadowMap::RenderShadowMap()
{
    float3 lightSpaceX,lightSpaceY,lightSpaceZ;
#pragma region ContructLightSpaceAxis
    lightSpaceZ = normalize(m_LightDirection);

    // 选择light Direction最短分量的一个轴作为cross依据，
    // 一般简单做法是直接用world up，
    {
        const float min_cmp =
            std::min(
                std::min(
                    std::abs(m_LightDirection.x),
                    std::abs(m_LightDirection.y)
                    ),
                    std::abs(m_LightDirection.z)
                );
        if (min_cmp == std::abs(m_LightDirection.x))
        {
            lightSpaceX = float3(1, 0, 0);
        } else if (min_cmp == std::abs(m_LightDirection.y))
        {
            lightSpaceX = float3(0, 1, 0);
        }
        else
        {
            lightSpaceX = float3(0, 0, 1);
        }

        lightSpaceY = cross(lightSpaceZ, lightSpaceX);
        lightSpaceX = cross(lightSpaceY, lightSpaceZ);
        lightSpaceX = normalize(lightSpaceX);
        lightSpaceY = normalize(lightSpaceY);
    }
    float4x4 WorldToLightViewSpaceMatr =
        float4x4::ViewFromBasis(
            lightSpaceX, lightSpaceY, lightSpaceZ);

    // For this tutorial we know that the scene center is at (0,0,0).
    // Real applications will want to compute tight bounds
    float3 sceneCenter = float3(0, 0, 0);
    float sceneRadius = std::sqrt(3.f);
    float3 minXYZ =
        sceneCenter - float3(sceneRadius, sceneRadius, sceneRadius);
    float3 maxXYZ =
        sceneCenter + float3(sceneRadius, sceneRadius,
            sceneRadius * 5);
    float3 sceneExtent = maxXYZ - minXYZ;

    const auto& DevInfo = m_pDevice->GetDeviceInfo();
    const bool IsGL = DevInfo.IsGLDevice();
    float4 lightSpaceScale;
    lightSpaceScale.x = 2.f / sceneExtent.x;
    lightSpaceScale.y = 2.f / sceneExtent.y;
    lightSpaceScale.z = (IsGL ? 2.f : 1.f) / sceneExtent.z;

    float4 f4LightSpaceScaledBias;
    f4LightSpaceScaledBias.x = -minXYZ.x * lightSpaceScale.x - 1.f;
    f4LightSpaceScaledBias.y = -minXYZ.y * lightSpaceScale.y - 1.f;
    f4LightSpaceScaledBias.z = -minXYZ.z * lightSpaceScale.z + (IsGL ? -1.f : 0.f);

    float4x4 ScaleMatrix      =
        float4x4::Scale(lightSpaceScale.x, lightSpaceScale.y, lightSpaceScale.z);
    float4x4 ScaledBiasMatrix =
        float4x4::Translation(f4LightSpaceScaledBias.x, f4LightSpaceScaledBias.y, f4LightSpaceScaledBias.z);

    // Note: bias is applied after scaling!
    float4x4 ShadowProjMatr = ScaleMatrix * ScaledBiasMatrix;

    float4x4 WorldToLightProjSpaceMatrx =
            WorldToLightViewSpaceMatr * ShadowProjMatr;

    const auto& NDCAttribs = DevInfo.GetNDCAttribs();
    float4x4 ProjToUVScale =
        float4x4::Scale(0.5f, NDCAttribs.YtoVScale, NDCAttribs.ZtoDepthScale);
    float4x4 ProjToUVBias =
        float4x4::Translation(0.5f, 0.5f, NDCAttribs.GetZtoDepthBias());

    m_WorldToShadowMapUVDepthMatr =
        WorldToLightProjSpaceMatrx * ProjToUVScale * ProjToUVBias;

    RenderCube(WorldToLightViewSpaceMatr, true);
#pragma endregion
    
}

void QxShadowMap::RenderCube(const float4x4& CameraViewProj, bool IsShadowPass)
{
    // Update Constant buffer
    {
        struct Constants
        {
            float4x4 WorldViewProj;
            float4x4 NormalTransform;
            float4 LightDirection;
        };
        MapHelper<Constants> CBConstants(
            m_pImmediateContext,
            m_VSConstants,
            MAP_WRITE,
            MAP_FLAG_DISCARD);
        CBConstants->WorldViewProj =
            (m_CubeWorldMatrix * CameraViewProj).Transpose();
        auto NormalMatrix =
            m_CubeWorldMatrix.RemoveTranslation().Inverse();
        // We need to do inverse-transpose, but we also need to transpose the matrix
        // before writing it to the buffer
        CBConstants->NormalTransform = NormalMatrix;
        CBConstants->LightDirection = m_LightDirection;
    }

    // Bind Vertex buffer
    IBuffer* pBuffers[] = {m_CuberVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(
        0, 1, pBuffers, nullptr,
        RESOURCE_STATE_TRANSITION_MODE_VERIFY,
        SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(
        m_CubeIndexBufffer, 0,
        RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    
    // Set pipeline state and commit resources
    if (IsShadowPass)
    {
        m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
        m_pImmediateContext->CommitShaderResources(
            m_CubeShadowSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    }
    else
    {
        m_pImmediateContext->SetPipelineState(m_pCubePSO);
        m_pImmediateContext->CommitShaderResources(
            m_CubeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    }

    // draw call
    DrawIndexedAttribs DrawAttrs(
        36, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void QxShadowMap::RenderPlane()
{
    {
        struct Constants
        {
            float4x4 CameraViewProj;
            float4x4 WorldToShadowMapUVDepth;
            float4 LightDirection;
        };
        MapHelper<Constants> CBConstants(
            m_pImmediateContext, m_VSConstants,
            MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->CameraViewProj =
            m_CameraVieweProjMatrix.Transpose();
        CBConstants->WorldToShadowMapUVDepth =
            m_WorldToShadowMapUVDepthMatr.Transpose();
        CBConstants->LightDirection =
            m_LightDirection;
    }

    m_pImmediateContext->SetPipelineState(m_pPlanePSO);
    m_pImmediateContext->CommitShaderResources(
        m_PlaneSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
}

void QxShadowMap::RenderShadowMapVis()
{
    m_pImmediateContext->SetPipelineState(m_pShadowMapVisPSO);
    m_pImmediateContext->CommitShaderResources(
        m_ShadowMapVisSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
}
}
