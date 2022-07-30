#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "ThreadSignal.hpp"

namespace Diligent
{
class QxDataStreaming final : public SampleBase
{
public:
    ~QxDataStreaming();
    void        ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override;
    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "Qx Data Streaming";
    };


    
private:
    void StopWorkerThreads();
    void InitializePolygons();
    void CreateInstanceBuffer();
    void  StartWorkerThreads(int InTheadNum);
    void UpdateUI();
    void UpdatePolygons(float deltaTime);
    void         CreatePipelineStates(
         std::vector<StateTransitionDesc>& OutBarriers);
    void         LoadTextures( std::vector<StateTransitionDesc>& OutBarriers);

    template<bool UseBatch>
    void  RenderSubset(const RefCntAutoPtr<IDeviceContext>& pCtx, int SubsetIndex);


    static void WorkerThreadFunc(QxDataStreaming* pThis, Uint32 ThreadIndex);
private:

    Threading::Signal m_RenderSubsetSignal;
    Threading::Signal m_ExecuteCommandListSignal;
    Threading::Signal m_GotoNextFrameSignal;

    std::atomic_int m_NumThreadsCompleted;
    std::atomic_int m_NumThreadsReady;

    std::vector<std::thread> m_WorkerTheads;

    std::vector<RefCntAutoPtr<ICommandList>> m_CmdLists;
    std::vector<ICommandList*> m_CmdListPtrs;

    static constexpr  const int NumStates = 5;
    RefCntAutoPtr<IPipelineState> m_pPSO[2][NumStates];
    RefCntAutoPtr<IBuffer> m_PolygonAttribsCB;
    RefCntAutoPtr<IBuffer> m_BatchDataBuffer;

    static constexpr  const int MaxVertsInStreamingBuffer = 1024;
    std::unique_ptr<class QxStreamingBuffer> m_StreamingVB;
    std::unique_ptr<class QxStreamingBuffer> m_StreamingIB;

    static constexpr  int NumTextures = 4;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB[NumTextures];
    RefCntAutoPtr<IShaderResourceBinding> m_BatchSRB;
    RefCntAutoPtr<ITextureView> m_TextureSRV[NumTextures];
    RefCntAutoPtr<ITextureView> m_TextureArraySRV;

    static constexpr int MaxPolygons = 100000;
    static constexpr int MaxBatchSize = 100;

    int m_NumPolgyons = 1000;
    int m_BatchSize = 5;

    int m_MaxThreads = 8;
    int m_NumWorkerThreads = 4;

    struct PolygonData
    {
        float2 Pos;
        float2 MoveDir;
        float Size  = 0;
        float Angle = 0;
        float RotSpeed = 0;
        int TextureInd = 0;
        int StateInd = 0;
        int NumVerts = 0;
    };

    std::vector<PolygonData> m_Polygons;

    struct InstanceData
    {
        float4 PolygonRotationAndScale;
        float2 PolygonCenter;
        float TexArrInd;
    };

    static constexpr const Uint32 MinPolygonVerts = 3;
    static constexpr const Uint32 MaxPolygonVerts = 10;

    struct PolygonGemetry
    {
        std::vector<float2> Verts;
        std::vector<Uint32> Inds;
    };

    std::vector<PolygonGemetry> m_PolygonGeo;
    bool m_bAllowPersistentMap = false;
    std::pair<Uint32, Uint32> WritePolygon(
        const PolygonGemetry& PolygonGeo,
        IDeviceContext* pCtx,
        size_t CtxNum);
};


}
