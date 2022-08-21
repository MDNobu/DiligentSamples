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

}

void QxGLTFViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();
}
void QxGLTFViewer::CreateEnvMapPSO(IRenderStateNotationLoader* pRSNLoader)
{
    
}

void QxGLTFViewer::CreateEnvMapSRB()
{
    
}

void QxGLTFViewer::CreateBoundBoxPSO(IRenderStateNotationLoader* pRSNLoader)
{
    
}

void QxGLTFViewer::LoadModel(const char* Path)
{
    
}

void QxGLTFViewer::ResetView()
{
    
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
    
}
}
