#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "ThreadSignal.hpp"

namespace Diligent
{
class QxMultithreading final : public SampleBase
{
public:
    ~QxMultithreading() override;
    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "Qx Multi threading";
    };
    void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override;

private:
    void StopWorkerThreads();
    void CreatePipelineState(std::vector<StateTransitionDesc>& OutBarriers);
    void LoadTextures(std::vector<StateTransitionDesc>& Barriers);

    void PopulateInstanceData();
    void StartWorkerThreads(size_t NumThreads);
    void UpdateUI();

    void RenderSubset(IDeviceContext* pCtx, Uint32 Subset);
    
    static void WorkerThreadFunc(QxMultithreading* pThis,
        Uint32 ThreadNum);
public:
private:
    Threading::Signal m_RenderSubsetSignal;
    Threading::Signal m_ExecuteCommandListsSignal;
    Threading::Signal m_GotoNextFrameSignal; 
    std::atomic_int m_NumThreadsCompleted;
    std::atomic_int m_NumThreadsReady;
    std::vector<std::thread> m_WorkerThreads;

    std::vector<RefCntAutoPtr<ICommandList>> m_CmdLists;
    std::vector<ICommandList*> m_CmdListPtrs;
    
    RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IBuffer> m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer> m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer> m_InstanceConstants;
    RefCntAutoPtr<IBuffer> m_VSConstants;

    static constexpr int NumTextures = 4;

    RefCntAutoPtr<IShaderResourceBinding> m_SRB[NumTextures];
    RefCntAutoPtr<ITextureView> m_TextureSRV[NumTextures];

    float4x4 m_ViewProjMat;
    float4x4 m_RotationMat;
    int m_GridSize = 5;

    int m_MaxThreads = 8;
    int m_NumWorkerThreads = 4;

    struct InstanceData;
    {
        float4x4 Matrix;
        int TextureInd = 0;
    };
    std::vector<InstanceData> m_InstanceData;
};


}
