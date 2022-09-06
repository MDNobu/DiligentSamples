#pragma once

#include <vector>
#include "SampleBase.hpp"
#include "RenderStateNotationLoader.h"
#include "GLTFLoader.hpp"
#include "GLTF_PBR_Renderer.hpp"
#include "BasicMath.hpp"
#include "QxGLTF_PBR_Render.hpp"

namespace Diligent
{
class QxGLTFViewer final : public SampleBase
{
public:
    ~QxGLTFViewer();

    virtual void ProcessCommandLine(const char* CmdLine) override;
    virtual void Initialize(const SampleInitInfo& InitInfo) override;
    virtual void Render() override;
    virtual void Update(double CurrTime, double ElapsedTime) override;

    virtual const Char* GetSampleName() const override
    {
        return "Qx GLTF Viewer";
    }

private:
    void CreateEnvMapPSO(IRenderStateNotationLoader* pRSNLoader);
    void CreateEnvMapSRB();
    void CreateBoundBoxPSO(IRenderStateNotationLoader* pRSNLoader);
    void LoadModel(const char* Path);
    void ResetView();
    void UpdateUI();
    void CreateGLTFResourceCache();

private:
    enum class BackgroundMode
    {
        None,
        EnvironmentMap,
        Irradiance,
        PrefilteredEnvMap,
        NumModes
    };
    BackgroundMode m_BackgroundMode = BackgroundMode::PrefilteredEnvMap;
    
    QxGLTF_PBR_Render::RenderInfo m_RenderParams;
    Quaternion m_CameraRotation = {0, 0, 0, 1};
    Quaternion m_ModelRotation =
        Quaternion::RotationFromAxisAngle(
            float3{0, 1, 0}, -PI_F / 2.f);

    float m_CameraDist = 0.9f;

    float3 m_LightDirection;
    float4 m_LightColor = float4(1, 1, 1, 1);
    float m_LightIntensity = 3.f;
    float m_EnvMapMipLevel = 1.f;
    int m_SelectedModel = 0;

    static const std::pair<const char*,const char*> GLTFModels[];

    enum class BoundBoxMode : int
    {
        None = 0,
        Local,
        Global
    };

    BoundBoxMode m_BoundBoxMode = BoundBoxMode::None;
    bool m_PlayAnimation = false;
    int m_AnimationIndex = 0;
    std::vector<float> m_AnimationTimers;

    // std::unique_ptr<GLTF_PBR_Renderer> m_GLTFRender;
    std::unique_ptr<QxGLTF_PBR_Render> m_GLTFRender;
    std::unique_ptr<GLTF::Model> m_Model;
    RefCntAutoPtr<IBuffer> m_CameraAttribsCB;
    RefCntAutoPtr<IBuffer> m_LightAttribsCB;
    RefCntAutoPtr<IPipelineState> m_EnvMapPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_EnvMapSRB;
    RefCntAutoPtr<ITextureView> m_EnvironmentMapSRV;
    RefCntAutoPtr<IBuffer> m_EnvMapRenderAttribsCB;
    RefCntAutoPtr<IPipelineState> m_BoundBoxPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_BoundBoxSRB;

    bool m_bUseResourceCache = false;
    RefCntAutoPtr<GLTF::ResourceManager> m_pResourceMgr;
    GLTF::ResourceCacheUseInfo m_CacheUseInfo;

    QxGLTF_PBR_Render::ModelResourceBindings m_ModelResourceBindings;
    QxGLTF_PBR_Render::ResourceCacheBindings m_CacheBindings;

    MouseState m_LastMouseState;
    float m_CameraYaw = 0;
    float m_CamearPitch  =0 ;

    Uint32 m_CameraId = 0;

    std::vector<const GLTF::Camera*> m_Cameras;
};
}
