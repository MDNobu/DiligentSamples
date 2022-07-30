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
class QxQuads final : public SampleBase
{
public:
    ~QxQuads() override;
    void        ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override;
    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "QxQuads";
    };
private:
    void UpdateUI();
    void UpdateQuads(float elapsedTime);
    void CreatePipelineStates(std::vector<StateTransitionDesc>& Barriers);
    void LoadTextures(std::vector<StateTransitionDesc>& Barriers);
    void InitializeQuads();
    void CreateInstanceBuffer();
    void StartWorkderThreads(int InNumWorkerThreads);
    void  StopWorkerThreads();
    template<bool UseBatch>
    void  RenderSubset(IDeviceContext* pCtx, Uint32 SubsetIndex);

    static void WorkerThreadFunc(
        QxQuads* pThis, Uint32 ThreadIndex);
    
private:

    Threading::Signal m_RenderSubSetSignal;
    Threading::Signal m_ExecuteCommandListsSignal;
    Threading::Signal m_GotoNextFrameSignal;

    std::atomic_int m_NumThreadsCompleted;
    std::atomic_int m_NumThreadsReady;

    std::vector<std::thread> m_WorkerThreads;
    std::vector<RefCntAutoPtr<ICommandList>> m_CmdLists;
    std::vector<ICommandList*> m_CmdListPtrs;

    static constexpr  int NumStates = 5;
    // 这里是创建了NumStates个blend state，绑定到不同的pso上
    // 同时pso还区分batched 和unbatched 版本，所以这里是二维数组
    RefCntAutoPtr<IPipelineState> m_pPSO[2][NumStates];
    RefCntAutoPtr<IBuffer> m_QuadAttribsCB;
    RefCntAutoPtr<IBuffer> m_BatchDataBuffer;
    
    
    static constexpr int NumTextures = 4;
    // 加载NumTexture个纹理，每张纹理都有texture view,和srb
    // 并且组装成一个texture array
    RefCntAutoPtr<IShaderResourceBinding> m_SRB[NumTextures];
    RefCntAutoPtr<IShaderResourceBinding> m_BatchSRB;
    RefCntAutoPtr<ITextureView> m_TextureSRV[NumTextures];
    RefCntAutoPtr<ITextureView> m_TexArraySRV;

    static constexpr int MaxQuads = 100000;
    static constexpr int MaxBatchSize = 100;

    int m_NumQuads = 1000;
    int m_BatchSize = 5;

    int m_MaxThreads = 8;
    int m_NumWorkerThreads = 4;

    struct QuadData
    {
        float2 Pos;
        float2 MoveDir;
        float Size = 0;
        float Angle = 0;
        float RotSpeed = 0;
        int TextureInd = 0;
        int StateIndex = 0;
    };
    std::vector<QuadData> m_Quads;

    struct InstanceData
    {
        float4 QuadRotationAndScale;
        float2 QuadCenter;
        float TexArrInd;
    };
    
};


}
