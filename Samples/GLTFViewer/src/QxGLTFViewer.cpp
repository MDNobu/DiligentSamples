#include "QxGLTFViewer.h"
#include <cmath>
#include <array>

#include "GLTFViewer.hpp"
#include "MapHelper.hpp"
#include "BasicMath.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "CommonlyUsedStates.h"
#include "ShaderMacroHelper.hpp"
#include "FileSystem.hpp"
#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"
#include "CallbackWrapper.hpp"
// #include "Shaders/Common/public/BasicStructures.fxh"


namespace Diligent
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh"

namespace 
{
struct EnvMapRenderAttribs
{
    ToneMappingAttribs ToneMapAttribs;

    float AverageLogLum;
    float MipLevel;
    float Unusued1;
    float Unusued2;
};

}


// clang-format off
const std::pair<const char*, const char*> QxGLTFViewer::GLTFModels[] =
{
    {"Damaged Helmet",      "models/DamagedHelmet/DamagedHelmet.gltf"},
    {"Metal Rough Spheres", "models/MetalRoughSpheres/MetalRoughSpheres.gltf"},
    {"Flight Helmet",       "models/FlightHelmet/FlightHelmet.gltf"},
    {"Cesium Man",          "models/CesiumMan/CesiumMan.gltf"},
    {"Boom Box",            "models/BoomBoxWithAxes/BoomBoxWithAxes.gltf"},
    {"Normal Tangent Test", "models/NormalTangentTest/NormalTangentTest.gltf"}
};

QxGLTFViewer::~QxGLTFViewer()
{
}

void QxGLTFViewer::ProcessCommandLine(const char* CmdLine)
{
    const char* pos = strchr(CmdLine, '-');
    while (pos != nullptr)
    {
        ++pos;
        std::string Arg;
        // if (!(Arg == GetArgument(pos, "use_cache")).empty)
        // {
        //     
        // }
        pos = strchr(pos, '-');
    }
}

void QxGLTFViewer::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    ResetView();

    RefCntAutoPtr<ITexture> EnvironmentMap;
    CreateTextureFromFile(
        "textures/papermill.ktx",
        TextureLoadInfo{"Environment map"},
        m_pDevice,
        &EnvironmentMap);
    m_EnvironmentMapSRV =
        EnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    TEXTURE_FORMAT BackBufferFormat =
        m_pSwapChain->GetDesc().ColorBufferFormat;
    TEXTURE_FORMAT DepthBufferFormat =
        m_pSwapChain->GetDesc().DepthBufferFormat;

    GLTF_PBR_Renderer::CreateInfo RenderCI;
    RenderCI.RTVFmt = BackBufferFormat;
    RenderCI.DSVFmt = DepthBufferFormat;
    RenderCI.AllowDebugView = true;
    RenderCI.UseIBL = true;
    RenderCI.FrontCCW = true;
    RenderCI.UseTextureAtlas = m_bUseResourceCache;
    m_GLTFRender.reset(
        new GLTF_PBR_Renderer(
            m_pDevice, m_pImmediateContext, RenderCI));

    CreateUniformBuffer(m_pDevice,
        sizeof(CameraAttribs),
        "Camera attribs buffer",
        &m_CameraAttribsCB);
    CreateUniformBuffer(
        m_pDevice,
        sizeof(LightAttribs),
        "Light Attribs buffer",
        &m_LightAttribsCB);
    CreateUniformBuffer(
        m_pDevice,
        sizeof(EnvMapRenderAttribs),
        "Env map render attribs buffer",
        &m_EnvMapRenderAttribsCB);

    StateTransitionDesc Barrires[] =
    {
        {m_CameraAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_LightAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_EnvMapRenderAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {EnvironmentMap, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE}
    };

    m_pImmediateContext->TransitionResourceStates(
        _countof(Barrires), Barrires);

    m_GLTFRender->PrecomputeCubemaps(
        m_pDevice, m_pImmediateContext,
        m_EnvironmentMapSRV);

    RefCntAutoPtr<IRenderStateNotationParser> pRSNParser;
    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pStreamFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(
            "render_states", &pStreamFactory);

        CreateRenderStateNotationParser({}, &pRSNParser);
        pRSNParser->ParseFile("RenderStates.json", pStreamFactory);
    }

    RefCntAutoPtr<IRenderStateNotationLoader> pRSNLoader;
    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pStreamFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(
            "shaders", &pStreamFactory);
        CreateRenderStateNotationLoader(
            {m_pDevice, pRSNParser, pStreamFactory},
            &pRSNLoader);
    }

    CreateEnvMapPSO(pRSNLoader);

    CreateBoundBoxPSO(pRSNLoader);

    m_LightDirection = normalize(float3(0.5f, -0.6f, -0.2f));

    if (m_bUseResourceCache)
    {
        CreateGLTFResourceCache();
    }

    LoadModel(GLTFModels[m_SelectedModel].second);
}

void QxGLTFViewer::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();

    const float ClearColor[] = {0.032f, 0.032f, 0.032f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(
        pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    float YFov = PI_F / 4.f;
    float ZNear = 0.1f;
    float ZFar = 100.f;

    float4x4 CameraView;
    if (m_CameraId == 0)
    {
        CameraView =
            m_CameraRotation.ToMatrix() *
                float4x4::Translation(0.f, 0.f, m_CameraDist);
        m_RenderParams.ModelTransform =
            m_ModelRotation.ToMatrix();
    }
    else
    {
        const auto* pCamera = m_Cameras[m_CameraId - 1];

        float4x4 InvZAxis = float4x4::Identity();
        InvZAxis.33 = -1;

        CameraView = pCamera->matrix.Inverse() * InvZAxis;
        YFov = pCamera->Perspective.YFov;
        ZNear = pCamera->Perspective.ZNear;
        ZFar = pCamera->Perspective.ZFar;

        m_RenderParams.ModelTransform = float4x4::Identity();
    }

    // Apply pretransform matrix that rotates the scene according the surface orientation
    CameraView *= GetSurfacePretransformMatrix(float3{0, 0, 1});

    float4x4 CameraToWorld = CameraView.Inverse();

    // get projection matrix adjusted to the current screen orientation
    const auto CameraProj = GetAdjustedProjectionMatrix(YFov, ZNear, ZFar);
    const auto CameraViewProj =
        CameraView* CameraProj;

    float3 CameraWorldPos =
        float3::MakeVector(CameraToWorld[3]);

    {
        MapHelper<CameraAttribs> CamAttribs(m_pImmediateContext,
            m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CamAttribs->mProjT =
            CameraProj.Transpose();
        CamAttribs->mViewProjT =
            CameraViewProj.Transpose();
        CamAttribs->mViewProjInvT = CameraViewProj.Inverse().Transpose();
        CamAttribs->f4Position = float4(CameraWorldPos, 1);

        if (m_BoundBoxMode != BoundBoxMode::None)
        {
            float4x4 BBTransform;
            if (m_BoundBoxMode == BoundBoxMode::Local)
            {
                   
            } else if (m_BoundBoxMode == BoundBoxMode::Global)
            {
                
            }
            else
            {
                UNEXPECTED("Unexpected bound box mode");
            }

            for (int row = 0; row < 4; ++row)
            {
                CamAttribs->f4ExtraData[row] =
                    float4::MakeVector(BBTransform[row]);
            }
        }
    }
}

void QxGLTFViewer::Update(double CurrTime, double ElapsedTime)
{
    if (m_CameraId == 0)
    {
        const MouseState& mouseState = m_InputController.GetMouseState();

        float MouseDeltaX = 0;
        float MouseDeltaY = 0;
        if (m_LastMouseState.PosX >= 0 &&
            m_LastMouseState.PosY >= 0 &&
            m_LastMouseState.ButtonFlags != MouseState::BUTTON_FLAG_NONE)
        {
            MouseDeltaX = mouseState.PosX - m_LastMouseState.PosX;
            MouseDeltaY = mouseState.PosY - m_LastMouseState.PosX;
        }
        m_LastMouseState = mouseState;

        const float RotationSpeed = 0.005f;
        float YawDelta = MouseDeltaX * RotationSpeed;
        float PitchDelta = MouseDeltaY * RotationSpeed;
        if (mouseState.ButtonFlags & MouseState::BUTTON_FLAG_LEFT)
        {
            m_CameraYaw += YawDelta;
            m_CamearPitch += PitchDelta;
            m_CamearPitch = std::max(m_CamearPitch, -PI_F / 2.f);
            m_CamearPitch = std::min(m_CamearPitch, +PI_F / 2.f);
        }
        
        // Apply extra rotations to adjust the view to match Khronos GLTF viewer
        m_CameraRotation =
            Quaternion::RotationFromAxisAngle(float3(1, 0, 0), -m_CamearPitch)
            * Quaternion::RotationFromAxisAngle(float3(0, 1, 0), -m_CameraYaw)
            * Quaternion::RotationFromAxisAngle(float3(0.75f, 0.75f, 0.75f), PI_F);
        
        if (mouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT)
        {
            float4x4 CameraView = m_CameraRotation.ToMatrix();
            float4x4 CameraWorld = CameraView.Transpose();

            float3 CameraRight = float3::MakeVector(CameraWorld[0]);
            float3 CameraUp =
                float3::MakeVector(CameraWorld[1]);
            m_ModelRotation =
                Quaternion::RotationFromAxisAngle(CameraRight, -PitchDelta)
                * Quaternion::RotationFromAxisAngle(CameraUp, -YawDelta)
                * m_ModelRotation;
        }
        
        m_CameraDist -= mouseState.WheelDelta * 0.25f;
        m_CameraDist = clamp(m_CameraDist, 0.1f, 5.f);
        
        const bool bResetHit =
            m_InputController.GetKeyState(InputKeys::Reset) & INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
        if (bResetHit)
        {
            ResetView();
        }
    }
    
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    if (!m_Model->Animations.empty() && m_PlayAnimation)
    {
        float& AnimationTimer =
            m_AnimationTimers[m_AnimationIndex];
        AnimationTimer += static_cast<float>(ElapsedTime);
        AnimationTimer =
            std::fmod(AnimationTimer,
                m_Model->Animations[m_AnimationIndex].End);
        m_Model->UpdateAnimation(m_AnimationIndex, AnimationTimer);
    }
}
void QxGLTFViewer::CreateEnvMapPSO(IRenderStateNotationLoader* pRSNLoader)
{
    auto ModifyCI = MakeCallback(
        [this](PipelineStateCreateInfo& PipelineCI) {
            auto& GraphicPipelineCI{static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI)};
            GraphicPipelineCI.GraphicsPipeline.RTVFormats[0] =
                m_pSwapChain->GetDesc().ColorBufferFormat;
            GraphicPipelineCI.GraphicsPipeline.DSVFormat =
                m_pSwapChain->GetDesc().DepthBufferFormat;
            GraphicPipelineCI.GraphicsPipeline.NumRenderTargets = 1;
        }
    );

    pRSNLoader->LoadPipelineState(
        {"EnvMap PSO", PIPELINE_TYPE_GRAPHICS,
        true, ModifyCI,
        ModifyCI}, &m_EnvMapPSO
        );

    m_EnvMapPSO->GetStaticVariableByName(
        SHADER_TYPE_PIXEL, "cbCameraAttribs")->Set(
            m_CameraAttribsCB);
    m_EnvMapPSO->GetStaticVariableByName(
        SHADER_TYPE_PIXEL, "cbEnvMapRenderAttribs")->Set(
            m_EnvMapRenderAttribsCB);
    CreateEnvMapSRB();
}

void QxGLTFViewer::CreateEnvMapSRB()
{
    if (m_BackgroundMode == BackgroundMode::None)
        return;

    m_EnvMapSRB.Release();
    m_EnvMapPSO->CreateShaderResourceBinding(
        &m_EnvMapSRB, true);
    ITextureView* pEnvMapSRV  = nullptr;
    switch (m_BackgroundMode)
    {
        case BackgroundMode::EnvironmentMap:
            pEnvMapSRV = m_EnvironmentMapSRV;
            break;
        case BackgroundMode::Irradiance:
            pEnvMapSRV = m_GLTFRender->GetIrradianceCubeSRV();
            break;
        case BackgroundMode::PrefilteredEnvMap:
            pEnvMapSRV = m_GLTFRender->GetPrefilteredEnvMapSRV();
            break;
        default:
            UNEXPECTED("Unexpected background mod");
    }
}

void QxGLTFViewer::CreateBoundBoxPSO(IRenderStateNotationLoader* pRSNLoader)
{
    auto ModifyCI = MakeCallback(
        [this](PipelineStateCreateInfo& PipelineCI) {
            auto& GraphicPipelineCI{static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI)};
            GraphicPipelineCI.GraphicsPipeline.RTVFormats[0] =
                m_pSwapChain->GetDesc().ColorBufferFormat;
            GraphicPipelineCI.GraphicsPipeline.DSVFormat =
                m_pSwapChain->GetDesc().DefaultStencilValue;
            GraphicPipelineCI.GraphicsPipeline.NumRenderTargets = 1;
        }
    );
    pRSNLoader->LoadPipelineState(
        LoadPipelineStateInfo{
            "BoundBox PSO", PIPELINE_TYPE_GRAPHICS,
            true, ModifyCI,
            ModifyCI},
        &m_BoundBoxPSO
        );
    m_BoundBoxPSO->GetStaticVariableByName(
        SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(
            m_CameraAttribsCB);
    m_BoundBoxPSO->CreateShaderResourceBinding(
        &m_BoundBoxSRB, true);
}

void QxGLTFViewer::LoadModel(const char* Path)
{
    if (m_Model)
    {
        m_PlayAnimation = false;
        m_AnimationIndex = 0;
        m_AnimationTimers.clear();
    }

    GLTF::Model::CreateInfo ModelCI;
    ModelCI.FileName = Path;
    ModelCI.pCacheInfo = m_bUseResourceCache ? &m_CacheUseInfo : nullptr;
    m_Model.reset(
        new GLTF::Model{m_pDevice, m_pImmediateContext, ModelCI}
        );

    m_ModelResourceBindings =
        m_GLTFRender->CreateResourceBindings(
            *m_Model,m_CameraAttribsCB, m_LightAttribsCB);

    // center and scale model
    float3 ModelScale{
        m_Model->AABBTransform[0][0], m_Model->AABBTransform[1][1], m_Model->AABBTransform[2][2]};
    float Scale =
        (1.f / std::max(std::max(ModelScale.x, ModelScale.y), ModelScale.z)) * 0.5f;
    auto Translate =
        -float3(m_Model->AABBTransform[3][0], m_Model->AABBTransform[3][1], m_Model->AABBTransform[3][2]);
    Translate += -0.5f * ModelScale;
    float4x4 InvYAxis = float4x4::Identity();
    InvYAxis._22 = -1;

    auto ModelTransform =
        float4x4::Translation(Translate) * float4x4::Scale(Scale) * InvYAxis;
    m_Model->Transform(ModelTransform);
    if (!m_Model->Animations.empty())
    {
        m_AnimationTimers.resize(m_Model->Animations.size());
        m_AnimationIndex = 0;
        m_PlayAnimation = true;
    }

    m_CameraId = 0;
    m_Cameras.clear();
    for (const GLTF::Node* node : m_Model->LinearNodes)
    {
        if (node->pCamera &&
            node->pCamera->Type ==
            GLTF::Camera::Projection::Perspective)
        {
            m_Cameras.push_back(node->pCamera.get());
        }
    }
}

void QxGLTFViewer::ResetView()
{
    m_CameraYaw = 0;
    m_CamearPitch = 0;
    m_ModelRotation =
        Quaternion::RotationFromAxisAngle(
            float3(0.f, 1.f, 0.f), -PI_F / 2.f);
    m_CameraRotation =
        Quaternion::RotationFromAxisAngle(
            float3(0.75f, 0.f, 0.75f), PI_F);
}

void QxGLTFViewer::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        {
            const char* Models[_countof(GLTFModels)];
            for (size_t i = 0; i < _countof(GLTFModels); ++i)
            {
                Models[i] = GLTFModels[i].first;
            }
            if (ImGui::Combo("Model",
                &m_SelectedModel,
                Models,
                _countof(GLTFModels)))
            {
                LoadModel(
                    GLTFModels[m_SelectedModel].second);
            }
        }

        if (!m_Cameras.empty())
        {
            std::vector<std::pair<Uint32, std::string>> CamList;
            CamList.emplace_back(0, "default");
            for (Uint32 i = 0; i < m_Cameras.size(); ++i)
            {
                const auto& Cam =
                    m_Cameras[i];
                CamList.emplace_back(
                    i + 1,
                    Cam->Name.empty() ? std::to_string(i) : Cam->Name);
            }
            ImGui::Combo("Camera",
                &m_CameraId,
                CamList.data(),
                static_cast<int>(CamList.size()));
        }

        if (m_CameraId == 0)
        {
            ImGui::gizmo3D("Model Rotation",
                m_ModelRotation, ImGui::GetTextLineHeight() * 10);
            ImGui::SameLine();
            ImGui::gizmo3D("Ligt direction",
                m_LightDirection,
                ImGui::GetTextLineHeight() * 10);

            if (ImGui::Button("Rest View"))
            {
                ResetView();
            }

            ImGui::SliderFloat("Camera Distatnce",
                &m_CameraDist, 0.1f, 5.0f);
        }

        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Lighting"))
        {
            ImGui::ColorEdit3("Light Color", &m_LightColor.r);
            ImGui::SliderFloat("Light Intensity", &m_LightIntensity,
                0.1f, 50.f);
            ImGui::SliderFloat("Occlusion strength",
                &m_RenderParams.OcclusionStrength,
                0.f, 1.f);
            ImGui::SliderFloat("Emission scale",
                &m_RenderParams.EmissionScale,
                0.f, 1.f);
            ImGui::SliderFloat("IBL scale",
                &m_RenderParams.IBLScale,
                0.f, 1.f);
            ImGui::TreePop();
        }

        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Tone mapping"))
        {
            // clang-format off
            ImGui::SliderFloat("Average log lum",    &m_RenderParams.AverageLogLum,     0.01f, 10.0f);
            ImGui::SliderFloat("Middle gray",        &m_RenderParams.MiddleGray,        0.01f,  1.0f);
            ImGui::SliderFloat("White point",        &m_RenderParams.WhitePoint,        0.1f,  20.0f);
            // clang-format on
            ImGui::TreePop();
        }

        {
            std::array<const char*, static_cast<size_t>(BackgroundMode::NumModes)> BackgroundModes;
            BackgroundModes[static_cast<int>(BackgroundMode::None)] = "Nonw";
            BackgroundModes[static_cast<int>(BackgroundMode::EnvironmentMap)] = "Environmen Map";
            BackgroundModes[static_cast<int>(BackgroundMode::Irradiance)] = "Irradiance";
            BackgroundModes[static_cast<int>(BackgroundMode::PrefilteredEnvMap)] = "PrefilteredEnvMap";
            if (ImGui::Combo("Background mode",
                reinterpret_cast<int*>(&m_BackgroundMode),
                BackgroundModes.data(),
                static_cast<int>(BackgroundModes.size())))
            {
                CreateEnvMapSRB();
            }
        }

        ImGui::SliderFloat("Env map mip", &m_EnvMapMipLevel,
            0.f, 7.f);

                {
            std::array<const char*, static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NumDebugViews)> DebugViews;

            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::None)]            = "None";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::BaseColor)]       = "Base Color";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Transparency)]    = "Transparency";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NormalMap)]       = "Normal Map";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Occlusion)]       = "Occlusion";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Emissive)]        = "Emissive";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Metallic)]        = "Metallic";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Roughness)]       = "Roughness";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::DiffuseColor)]    = "Diffuse color";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::SpecularColor)]   = "Specular color (R0)";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Reflectance90)]   = "Reflectance90";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::MeshNormal)]      = "Mesh normal";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::PerturbedNormal)] = "Perturbed normal";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NdotV)]           = "n*v";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::DiffuseIBL)]      = "Diffuse IBL";
            DebugViews[static_cast<size_t>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::SpecularIBL)]     = "Specular IBL";
            ImGui::Combo("Debug view", reinterpret_cast<int*>(&m_RenderParams.DebugView), DebugViews.data(), static_cast<int>(DebugViews.size()));
        }

        ImGui::Combo("Bound box mode",
            reinterpret_cast<int*>(&m_BoundBoxMode),
            "None\0"
            "Local\0"
            "Global\0\0"
            );
    }

    ImGui::End();
}

void QxGLTFViewer::CreateGLTFResourceCache()
{
   std::array<BufferSuballocatorCreateInfo, 3> Buffers = {};

    Buffers[0].Desc.Name      = "GLTF basic vertex attribs buffer";
    Buffers[0].Desc.BindFlags = BIND_VERTEX_BUFFER;
    Buffers[0].Desc.Usage     = USAGE_DEFAULT;
    Buffers[0].Desc.Size      = sizeof(GLTF::Model::VertexBasicAttribs) * 16 << 10;

    Buffers[1].Desc.Name      = "GLTF skin attribs buffer";
    Buffers[1].Desc.BindFlags = BIND_VERTEX_BUFFER;
    Buffers[1].Desc.Usage     = USAGE_DEFAULT;
    Buffers[1].Desc.Size      = sizeof(GLTF::Model::VertexSkinAttribs) * 16 << 10;

    Buffers[2].Desc.Name      = "GLTF index buffer";
    Buffers[2].Desc.BindFlags = BIND_INDEX_BUFFER;
    Buffers[2].Desc.Usage     = USAGE_DEFAULT;
    Buffers[2].Desc.Size      = sizeof(Uint32) * 8 << 10;

    std::array<DynamicTextureAtlasCreateInfo, 1> Atlases;
    Atlases[0].Desc.Name      = "GLTF texture atlas";
    Atlases[0].Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    Atlases[0].Desc.Usage     = USAGE_DEFAULT;
    Atlases[0].Desc.BindFlags = BIND_SHADER_RESOURCE;
    Atlases[0].Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    Atlases[0].Desc.Width     = 4096;
    Atlases[0].Desc.Height    = 4096;
    Atlases[0].Desc.MipLevels = 6;

    GLTF::ResourceManager::CreateInfo ResourceMgrCI;
    ResourceMgrCI.BuffSuballocators    = Buffers.data();
    ResourceMgrCI.NumBuffSuballocators = static_cast<Uint32>(Buffers.size());
    ResourceMgrCI.TexAtlases           = Atlases.data();
    ResourceMgrCI.NumTexAtlases        = static_cast<Uint32>(Atlases.size());

    ResourceMgrCI.DefaultAtlasDesc.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    ResourceMgrCI.DefaultAtlasDesc.Desc.Usage     = USAGE_DEFAULT;
    ResourceMgrCI.DefaultAtlasDesc.Desc.BindFlags = BIND_SHADER_RESOURCE;
    ResourceMgrCI.DefaultAtlasDesc.Desc.Width     = 4096;
    ResourceMgrCI.DefaultAtlasDesc.Desc.Height    = 4096;
    ResourceMgrCI.DefaultAtlasDesc.Desc.MipLevels = 6;

    m_pResourceMgr = GLTF::ResourceManager::Create(m_pDevice, ResourceMgrCI);

    m_CacheUseInfo.pResourceMgr     = m_pResourceMgr;
    m_CacheUseInfo.VertexBuffer0Idx = 0;
    m_CacheUseInfo.VertexBuffer1Idx = 1;
    m_CacheUseInfo.IndexBufferIdx   = 2;

    m_CacheUseInfo.BaseColorFormat    = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.PhysicalDescFormat = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.NormalFormat       = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.OcclusionFormat    = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.EmissiveFormat     = TEX_FORMAT_RGBA8_UNORM;
}
}
