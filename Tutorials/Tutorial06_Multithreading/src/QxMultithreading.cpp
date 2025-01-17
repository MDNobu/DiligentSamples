#include "QxMultithreading.h"

#include <random>
#include "ImGuiUtils.hpp"
#include "GraphicsUtilities.h"
#include "imgui.h"
#include "MapHelper.hpp"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{
QxMultithreading::~QxMultithreading()
{
    StopWorkerThreads();
}

void QxMultithreading::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    m_MaxThreads = static_cast<int> (m_pDeferredContexts.size());
    m_NumWorkerThreads = std::min(4, m_MaxThreads);

    std::vector<StateTransitionDesc> Barriers;

    CreatePipelineState(Barriers);

    // load textured cube
    m_CubeVertexBuffer =
        TexturedCube::CreateVertexBuffer(
            m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV);
    m_CubeIndexBuffer =
        TexturedCube::CreateIndexBuffer(m_pDevice);

    //显式转换vb/ib到需要的状态
    Barriers.emplace_back(m_CubeIndexBuffer,
        RESOURCE_STATE_UNKNOWN,
        RESOURCE_STATE_VERTEX_BUFFER,
        STATE_TRANSITION_FLAG_UPDATE_STATE);
    Barriers.emplace_back(m_CubeIndexBuffer,
        RESOURCE_STATE_UNKNOWN,
        RESOURCE_STATE_INDEX_BUFFER,
        STATE_TRANSITION_FLAG_UPDATE_STATE);

    LoadTextures(Barriers);

    // excuted all barriers
    m_pImmediateContext->TransitionResourceStates(
    static_cast<Uint32>(Barriers.size()),
    Barriers.data()
    );

    PopulateInstanceData();
    StartWorkerThreads(m_NumWorkerThreads);
}

void QxMultithreading::Render()
{
    ITextureView* pRTV =
        m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV =
        m_pSwapChain->GetDepthBufferDSV();

    // clear the back buffer
    const float clearColor[] =  {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(pRTV,
        clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV,
        CLEAR_DEPTH_FLAG,1.0F, 0,  RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (!m_WorkerThreads.empty())
    {
        m_NumThreadsCompleted.store(0);
        m_RenderSubsetSignal.Trigger(true);
    }

    RenderSubset(m_pImmediateContext, 0);

    if (!m_WorkerThreads.empty())
    {
        m_ExecuteCommandListsSignal.Wait(true,
            1);

        m_CmdListPtrs.resize(m_CmdLists.size());
        for (Uint32 i = 0; i < m_CmdLists.size(); ++i)
        {
            m_CmdListPtrs[i] = m_CmdLists[i];
        }

        m_pImmediateContext->ExecuteCommandLists(
            static_cast<Uint32>(m_CmdListPtrs.size()),
            m_CmdListPtrs.data()
            );

        for (auto& cmdList : m_CmdLists)
        {
            // 释放command list来释放所有外部的引用
            cmdList.Release();
        }
        
        m_NumThreadsReady.store(0);
        m_GotoNextFrameSignal.Trigger(true);
    }
}


void QxMultithreading::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    // Set the cube view matrix
    float4x4 View = float4x4::RotationX(-0.6f) * float4x4::Translation(0.f, 0.f, 4.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute view-projection matrix
    m_ViewProjMat = View * SrfPreTransform * Proj;

    // Global rotation matrix
    m_RotationMat = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-static_cast<float>(CurrTime) * 0.25f);
}

void QxMultithreading::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);
    Attribs.EngineCI.NumDeferredContexts =
        std::max(std::thread::hardware_concurrency() - 1, 2u);
}



void QxMultithreading::CreatePipelineState(std::vector<StateTransitionDesc>& OutBarriers)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(
        nullptr, &pShaderSourceFactory);

    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice = m_pDevice;
    CubePsoCI.RTVFormat =
        m_pSwapChain->GetDesc().ColorBufferFormat;
    CubePsoCI.DSVFormat =
        m_pSwapChain->GetDesc().DepthBufferFormat;
    CubePsoCI.pShaderSourceFactory =
        pShaderSourceFactory;
    CubePsoCI.VSFilePath = "QxCube.vsh";
    CubePsoCI.PSFilePath = "QxCube.psh";

    CubePsoCI.Components =
        TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV;

    m_pPSO = TexturedCube::CreatePipelineState(CubePsoCI);

    CreateUniformBuffer(m_pDevice, sizeof(float4x4)*2,
        "VSConstants", &m_VSConstants);
    CreateUniformBuffer(m_pDevice,
        sizeof(float4x4), "Instance constatnts CB",
        &m_InstanceConstants);
    // 显式的转换到constant buffer state
    OutBarriers.emplace_back(
        m_VSConstants,
        RESOURCE_STATE_COMMON,
        RESOURCE_STATE_CONSTANT_BUFFER,
        STATE_TRANSITION_FLAG_UPDATE_STATE);
    OutBarriers.emplace_back(
        m_InstanceConstants,
        RESOURCE_STATE_COMMON,
        RESOURCE_STATE_CONSTANT_BUFFER,
        STATE_TRANSITION_FLAG_UPDATE_STATE);

    //由于我们没有显式指定"Constants"和"InstanceData"的类型
    // 则使用默认的SHADER_RESOURCE_VARIABLE_TYPE_STATIC类型，
    // 直接绑定到pso并且不会变化(这里的变化是绑定状态),所以用get static的方法访问
    m_pPSO->GetStaticVariableByName(
        SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pPSO->GetStaticVariableByName(
        SHADER_TYPE_VERTEX, "InstanceData")->Set(m_InstanceConstants);
}
void QxMultithreading::LoadTextures(std::vector<StateTransitionDesc>& Barriers)
{
    // load textures
    for (int texIndex = 0; texIndex < NumTextures; ++texIndex)
    {
        std::stringstream FileNameSS;
        FileNameSS << "DGLogo" << texIndex << ".png";
        auto FileName = FileNameSS.str();

        RefCntAutoPtr<ITexture> SrcTex =
            TexturedCube::LoadTexture(m_pDevice, FileName.c_str());
        // get shader resource view for the texture
        m_TextureSRV[texIndex] =
            SrcTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        // 转换到shader resource state
        Barriers.emplace_back(
            SrcTex,
            RESOURCE_STATE_UNKNOWN,
            RESOURCE_STATE_SHADER_RESOURCE,
            STATE_TRANSITION_FLAG_UPDATE_STATE
            );
    }

    // set texture srv in the srb
    for (int texIndex = 0; texIndex < NumTextures; ++texIndex)
    {
        // 为每个texture 创建一个shader resource binding
        m_pPSO->CreateShaderResourceBinding(
            &m_SRB[texIndex],
            true
            );
        m_SRB[texIndex]->GetVariableByName(SHADER_TYPE_PIXEL,
            "g_Texture")->Set(m_TextureSRV[texIndex]);
    }
}

void QxMultithreading::PopulateInstanceData()
{
    const auto zGridSize =
        static_cast<size_t>(m_GridSize);
    m_InstanceData.resize(zGridSize * zGridSize * zGridSize);
    // populate instance data buffer
    float fGridSize =
        static_cast<float>(m_GridSize);

    std::mt19937 gen;
    std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
    std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
    std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);
    std::uniform_int_distribution<Int32>  tex_distr(0, NumTextures - 1);

    float baseScale = 0.6f / fGridSize;
    int   instId    = 0;
    for (int x = 0; x < m_GridSize; ++x)
    {
        for (int y = 0; y < m_GridSize; ++y)
        {
            for (int z = 0; z < m_GridSize; ++z)
            {
                // Add random offset from central position in the grid
                float xOffset = 2.f * (x + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float yOffset = 2.f * (y + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float zOffset = 2.f * (z + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                // Random scale
                float scale = baseScale * scale_distr(gen);
                // Random rotation
                float4x4 rotation = float4x4::RotationX(rot_distr(gen)) * float4x4::RotationY(rot_distr(gen)) * float4x4::RotationZ(rot_distr(gen));
                // Combine rotation, scale and translation
                float4x4 matrix   = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(xOffset, yOffset, zOffset);
                auto&    CurrInst = m_InstanceData[instId++];
                CurrInst.Matrix   = matrix;
                // Texture array index
                CurrInst.TextureInd = tex_distr(gen);
            }
        }
    }
}

void QxMultithreading::StartWorkerThreads(size_t NumThreads)
{
    m_WorkerThreads.resize(NumThreads);
    for (Uint32 t = 0; t < m_WorkerThreads.size(); ++t)
    {
        m_WorkerThreads[t] =
            std::thread(WorkerThreadFunc, this, t);
    }
    m_CmdLists.resize(NumThreads);
}

void QxMultithreading::WorkerThreadFunc(QxMultithreading* pThis, Uint32 ThreadIndex)
{
    // 每个显示使用自己的device context
    IDeviceContext* pDeferredCtx = pThis->m_pDeferredContexts[ThreadIndex];
    const int NumWorkerThreads = static_cast<int>(pThis->m_WorkerThreads.size());
    for (;;)
    {
        auto SignaledValue = pThis->m_RenderSubsetSignal.Wait(true, NumWorkerThreads);
        // <0意味结束执行
        if (SignaledValue < 0)
        {
            return;
        }

        // record command list之前必须调用
        pDeferredCtx->Begin(0);

        pThis->RenderSubset(pDeferredCtx, 1 + ThreadIndex);
        
        RefCntAutoPtr<ICommandList> pCmdList;
        pDeferredCtx->FinishCommandList(&pCmdList);
        pThis->m_CmdLists[ThreadIndex] = pCmdList;

        {
            // 自动增长完成的线程数量
            const auto NumThreadsCompleted = pThis->m_NumThreadsCompleted.fetch_add(1) + 1; //+1是为什么#TODO
            if (NumThreadsCompleted == NumWorkerThreads)
            {
                pThis->m_ExecuteCommandListsSignal.Trigger();
            }
        }

        // NumWorkerThreads意思是有这么多线程在等待
        pThis->m_GotoNextFrameSignal.Wait(true, NumWorkerThreads);

        //调用这句deferred context 才会释放动态分配的资源  
        pDeferredCtx->FinishFrame();

        pThis->m_NumThreadsReady.fetch_add(1);

        while (pThis->m_NumThreadsReady < NumWorkerThreads)
        {
            std::this_thread::yield();
        }
        VERIFY_EXPR(!pThis->m_GotoNextFrameSignal.IsTriggered());
    }
    
}

void QxMultithreading::StopWorkerThreads()
{
    m_RenderSubsetSignal.Trigger(true, -1);

    for (auto& thread : m_WorkerThreads)
    {
        thread.join();
    }
    m_RenderSubsetSignal.Reset();
    m_WorkerThreads.clear();
    m_CmdLists.clear();
}

void QxMultithreading::UpdateUI()
{
    // ImGUI::SetNextWindowPos(ImVec2(10, 10),)
    ImGui::SetNextWindowPos(ImVec2(10, 10),
        ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt(
            "Grid size", &m_GridSize, 1, 32))
        {
            PopulateInstanceData();
        }
        {
            ImGui::ScopedDisabler disable(m_MaxThreads == 0);
            if (ImGui::SliderInt("Workder Threads",
                &m_NumWorkerThreads, 0, m_MaxThreads))
            {
                StopWorkerThreads();
                StartWorkerThreads(m_NumWorkerThreads);
            }
        }
    }

    ImGui::End();
}

void QxMultithreading::RenderSubset(IDeviceContext* pCtx, Uint32 SubsetIndex)
{
    // deferred context start in default context. 我们必须绑定需要的资源到context 上
    // render target已经在主线程设置好trasition状态，这里当前线程只需要验证
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    pCtx->SetRenderTargets(1, &pRTV, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    {
        // map buffer and write wvp
        MapHelper<float4x4> CBConstants(pCtx, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMat.Transpose();
        CBConstants[1] = m_RotationMat.Transpose();
    }

    // 绑定vb/ib，每个context都需要
    IBuffer* pBuffers[] = {m_CubeVertexBuffer};
    pCtx->SetVertexBuffers(0, _countof(pBuffers),
        pBuffers, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
    pCtx->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawIndexedAttribs DrawAttribs;
    DrawAttribs.IndexType = VT_UINT32;
    DrawAttribs.NumIndices = 36;
    DrawAttribs.Flags = DRAW_FLAG_VERIFY_ALL;

    //set the pipeline state
    pCtx->SetPipelineState(m_pPSO);
    Uint32 NumSubsets = Uint32{1} + static_cast<Uint32>(m_WorkerThreads.size());
    Uint32 NumInstances = static_cast<Uint32>(m_InstanceData.size());
    Uint32 SubsetSize = NumInstances / NumSubsets;
    Uint32 StartInst = SubsetSize * SubsetIndex;
    // 最后一组inst可能不满
    Uint32 EndInstIndex = (SubsetIndex < NumSubsets - 1) ?
        SubsetSize * (SubsetIndex + 1) : NumInstances;
    for (size_t instIndex = StartInst; instIndex < EndInstIndex; ++instIndex)
    {
        const auto& CurInstData =
            m_InstanceData[instIndex];

        pCtx->CommitShaderResources(
            m_SRB[CurInstData.TextureInd], RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        {
            // map buffer 提交per instance的信息
            MapHelper<float4x4> InstData(pCtx,
                m_InstanceConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            if (InstData == nullptr)
            {
                LOG_ERROR_MESSAGE("Failed to map instance data buffer");
                break;
            }
            *InstData = CurInstData.Matrix.Transpose();
        }

        // 每个instance执行一次draw
        pCtx->DrawIndexed(DrawAttribs);
    }
    
}
}
