
#include "QxQuads.h"

#include <random>

#include "GraphicsUtilities.h"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "MapHelper.hpp"
#include "TextureLoader.h"
#include "TextureUtilities.h"

namespace Diligent
{
    QxQuads::~QxQuads()
    {
        StopWorkerThreads();
    }
    void QxQuads::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
    {
        SampleBase::ModifyEngineInitInfo(Attribs);
        Attribs.EngineCI.NumDeferredContexts =
            std::max(
                std::thread::hardware_concurrency() - 1, 2u);
    }

    void QxQuads::Initialize(const SampleInitInfo& InitInfo)
    {
        SampleBase::Initialize(InitInfo);
        m_MaxThreads =
            static_cast<int>(m_pDeferredContexts.size());
        m_NumWorkerThreads =
            std::min(m_NumWorkerThreads, m_MaxThreads);

        std::vector<StateTransitionDesc> Barriers;
        CreatePipelineStates(Barriers);
        LoadTextures(Barriers);

        m_pImmediateContext->TransitionResourceStates(
            static_cast<Uint32>(Barriers.size()),
            Barriers.data());

        InitializeQuads();

        if (m_BatchSize > 1)
        {
            CreateInstanceBuffer();
        }

        StartWorkderThreads(m_NumWorkerThreads);
    }

    void QxQuads::Render()
    {
        ITextureView* pRTV =
            m_pSwapChain->GetCurrentBackBufferRTV();
        ITextureView* pDSV =
            m_pSwapChain->GetDepthBufferDSV();
        const float ClearColor[] =
            {0.35f, 0.35f, 0.35f, 1.f};
        m_pImmediateContext->ClearRenderTarget(
            pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(
            pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!m_WorkerThreads.empty())
        {
            m_NumThreadsCompleted.store(0);
            m_RenderSubSetSignal.Trigger(true);
        }

        if (m_BatchSize > 1)
        {
            RenderSubset<true>(m_pImmediateContext, 0);
        }
        else
        {
            RenderSubset<false>(
                m_pImmediateContext, 0);
        }

        if (!m_WorkerThreads.empty())
        {
            m_ExecuteCommandListsSignal.Wait(true, 1);

            m_CmdListPtrs.resize(m_CmdLists.size());
            for (int i = 0; i < m_CmdLists.size(); ++i)
            {
                m_CmdListPtrs[i] = m_CmdLists[i];
            }
            m_pImmediateContext->ExecuteCommandLists(
                static_cast<Uint32>(m_CmdListPtrs.size()),
                m_CmdListPtrs.data());

            for (auto& cmdList : m_CmdLists)
            {
                cmdList.Release();
            }

            m_NumThreadsReady.store(0);
            m_GotoNextFrameSignal.Trigger(true);
        }
    }

    void QxQuads::Update(double CurrTime, double ElapsedTime)
    {
        SampleBase::Update(CurrTime, ElapsedTime);

        UpdateUI();

        UpdateQuads(
            static_cast<float>(std::min(ElapsedTime, 0.25)));
    }

    void QxQuads::UpdateUI()
    {
        ImGui::SetNextWindowPos(
            ImVec2(10, 10),
            ImGuiCond_FirstUseEver
            );
        if (ImGui::Begin(
            "Settings",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::InputInt(
                "Num Quads",
                &m_NumQuads,
                100,
                1000,
                ImGuiInputTextFlags_EnterReturnsTrue))
            {
                m_NumQuads =
                    clamp(m_NumQuads, 1,
                        MaxQuads);
                InitializeQuads();
            }
            if (ImGui::InputInt(
                "Batch Size",
                &m_BatchSize,
                1,
                5))
            {
                m_BatchSize =
                    clamp(m_BatchSize,
                        1, MaxBatchSize);
                CreateInstanceBuffer();
            }

            {
                ImGui::ScopedDisabler Disable(
                    m_MaxThreads == 0);
                if (ImGui::SliderInt(
                    "Worker Threads",
                    &m_NumWorkerThreads,
                    0, m_MaxThreads))
                {
                    StopWorkerThreads();
                    StartWorkderThreads(m_NumWorkerThreads);
                }
            }
        }
        ImGui::End();
    }




    void QxQuads::UpdateQuads(float elapsedTime)
    {
        std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
        // to generate consistent distribution.

        std::uniform_real_distribution<float> rot_distr(-PI_F * 0.5f, +PI_F * 0.5f);
        for (int quad = 0; quad < m_NumQuads; ++quad)
        {
            auto& CurrInst = m_Quads[quad];
            CurrInst.Angle += CurrInst.RotSpeed * elapsedTime;
            if (std::abs(CurrInst.Pos.x + CurrInst.MoveDir.x * elapsedTime) > 0.95)
            {
                CurrInst.MoveDir.x *= -1.f;
                CurrInst.RotSpeed = rot_distr(gen);
            }
            CurrInst.Pos.x += CurrInst.MoveDir.x * elapsedTime;
            if (std::abs(CurrInst.Pos.y + CurrInst.MoveDir.y * elapsedTime) > 0.95)
            {
                CurrInst.MoveDir.y *= -1.f;
                CurrInst.RotSpeed = rot_distr(gen);
            }
            CurrInst.Pos.y += CurrInst.MoveDir.y * elapsedTime;
        }
    }

    void QxQuads::CreatePipelineStates(std::vector<StateTransitionDesc>& Barriers)
    {
        BlendStateDesc BlendStates[NumStates];
#pragma region InitBlendStates
        BlendStates[1].RenderTargets[0].BlendEnable = true;
        BlendStates[1].RenderTargets[0].SrcBlend =
            BLEND_FACTOR_SRC_ALPHA;
        BlendStates[1].RenderTargets[0].DestBlend =
            BLEND_FACTOR_INV_SRC_ALPHA;

        BlendStates[2].RenderTargets[0].BlendEnable = true;
        BlendStates[2].RenderTargets[0].SrcBlend =
            BLEND_FACTOR_INV_SRC_ALPHA;
        BlendStates[2].RenderTargets[0].DestBlend =
            BLEND_FACTOR_SRC_ALPHA;

        BlendStates[3].RenderTargets[0].BlendEnable = true;
        BlendStates[3].RenderTargets[0].SrcBlend =
            BLEND_FACTOR_SRC_COLOR;
        BlendStates[3].RenderTargets[0].DestBlend =
            BLEND_FACTOR_INV_SRC_COLOR;

        BlendStates[4].RenderTargets[0].BlendEnable = true;
        BlendStates[4].RenderTargets[0].SrcBlend
            = BLEND_FACTOR_INV_SRC_COLOR;
        BlendStates[4].RenderTargets[0].DestBlend =
            BLEND_FACTOR_SRC_COLOR;
#pragma endregion
        // pipeline state object encompasses configurration of
        // all GPU stages
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.Name =
            "Quda PSO";

        PSOCreateInfo.PSODesc.PipelineType =
            PIPELINE_TYPE_GRAPHICS;

        PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0] =
            m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.DSVFormat =
            m_pSwapChain->GetDesc().DepthBufferFormat;
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
            PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode =
            CULL_MODE_NONE;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable =
            false;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        ShaderCI.UseCombinedTextureSamplers = true;

        RefCntAutoPtr<IShaderSourceInputStreamFactory>
            pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(
            nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory =
            pShaderSourceFactory;

        //Create a vertex shader
        RefCntAutoPtr<IShader> pVS, pVSBatched;
        {
            ShaderCI.Desc.ShaderType =
                SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint =
                "main";
            ShaderCI.Desc.Name = "Quad VS";
            ShaderCI.FilePath = "QxQuad.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);

            ShaderCI.Desc.Name = "Quad VS Batched";
            ShaderCI.FilePath = "QxQuadBatch.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVSBatched);

            CreateUniformBuffer(
                m_pDevice,
                sizeof(float4x4),
                "Instance Constance CB",
                &m_QuadAttribsCB
                );
            Barriers.emplace_back(
                m_QuadAttribsCB,
                RESOURCE_STATE_UNKNOWN,
                RESOURCE_STATE_CONSTANT_BUFFER,
                STATE_TRANSITION_FLAG_UPDATE_STATE);
        }

        RefCntAutoPtr<IShader> pPS, pPSBatched;
        //Create Pixel shader
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Quad PS";

            ShaderCI.FilePath =
                "QxQuad.psh";

            m_pDevice->CreateShader(ShaderCI, &pPS);

            ShaderCI.Desc.Name = "Quad PS Batched";
            ShaderCI.FilePath = "QxQuadBatch.psh";

            m_pDevice->CreateShader(
                ShaderCI,
                &pPSBatched);
        }

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType =
            SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        ShaderResourceVariableDesc Vars[] =
        {
            {SHADER_TYPE_PIXEL,
                "g_Texture",
                SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables =
            _countof(Vars);

        SamplerDesc SamLinearClampDesc
        {
            FILTER_TYPE_LINEAR,
            FILTER_TYPE_LINEAR,
            FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_CLAMP,
            TEXTURE_ADDRESS_CLAMP,
            TEXTURE_ADDRESS_CLAMP
        };

        ImmutableSamplerDesc ImtbleSamplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers =
            ImtbleSamplers;
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
            _countof(ImtbleSamplers);

        for (int stateIndex = 0; stateIndex < NumStates; ++stateIndex)
        {
            PSOCreateInfo.GraphicsPipeline.BlendDesc =
                BlendStates[stateIndex];

            m_pDevice->CreateGraphicsPipelineState(
                PSOCreateInfo,
                &m_pPSO[0][stateIndex]);

            m_pPSO[0][stateIndex]->GetStaticVariableByName(
                SHADER_TYPE_VERTEX,
                "QuadAttribs"
                )->Set(m_QuadAttribsCB);

            if (stateIndex > 0)
            {
                // 保证pso 兼容
                VERIFY(
                    m_pPSO[0][stateIndex]->IsCompatibleWith(
                        m_pPSO[0][0]), "PSOs are expected to be compatible");
            }
        }

        PSOCreateInfo.PSODesc.Name = "Batched Quads PSO";
        LayoutElement LayoutElems[] =
        {
            LayoutElement{0, 0, 4,
                VT_FLOAT32,false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{1, 0, 2,
                VT_FLOAT32,false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{2, 0, 1,
                VT_FLOAT32, false, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE
                }
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements =
            LayoutElems;
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements =
            _countof(LayoutElems);

        PSOCreateInfo.pVS = pVSBatched;
        PSOCreateInfo.pPS = pPSBatched;

        for (int stateIndex = 0; stateIndex < NumStates; ++stateIndex)
        {
            PSOCreateInfo.GraphicsPipeline.BlendDesc =
                BlendStates[stateIndex];
            m_pDevice->CreateGraphicsPipelineState(
                PSOCreateInfo, &m_pPSO[1][stateIndex]);
#ifdef DILIGENT_DEBUG
            if (stateIndex > 0)
            {
                VERIFY(
                    m_pPSO[1][stateIndex]->IsCompatibleWith(
                        m_pPSO[1][0]),
                        "PSO are expected to be compatibal");
            }
#endif
        }
        
    }

    void QxQuads::LoadTextures(std::vector<StateTransitionDesc>& Barriers)
    {
        RefCntAutoPtr<ITexture> pTexArray;
        // 创建texture array
        // 读取NumTextures数量的的纹理文件，
        // 将其拷贝到texture array的slice中
        for (int texIndex = 0; texIndex < NumTextures; ++texIndex)
        {
            TextureLoadInfo loadInfo;
            loadInfo.IsSRGB = true;
            RefCntAutoPtr<ITexture> SrcTex;

            std::stringstream FileNameSS;
            FileNameSS << "DGLogo" << texIndex << ".png";
            auto FileName = FileNameSS.str();

            CreateTextureFromFile(
                FileName.c_str(), loadInfo,
                m_pDevice, &SrcTex);
            m_TextureSRV[texIndex] =
                SrcTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

            const auto& TexDesc =
                SrcTex->GetDesc();
            // 创建texture array, if not exist
            if (pTexArray == nullptr)
            {
                TextureDesc TexArrDesc = TexDesc;
                TexArrDesc.ArraySize = NumTextures;
                TexArrDesc.Type =
                    RESOURCE_DIM_TEX_2D_ARRAY;
                TexArrDesc.Usage = USAGE_DEFAULT;
                TexArrDesc.BindFlags =
                    BIND_SHADER_RESOURCE;
                m_pDevice->CreateTexture(
                    TexArrDesc,
                    nullptr,
                    &pTexArray);
            }

            for (Uint32 mipLevel = 0;
                mipLevel < TexDesc.MipLevels; ++mipLevel)
            {
                CopyTextureAttribs CopyAttribs(
                    SrcTex,
                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                    pTexArray,
                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION
                    );
                CopyAttribs.SrcMipLevel = mipLevel;
                CopyAttribs.DstMipLevel = mipLevel;
                CopyAttribs.DstSlice = texIndex;
                m_pImmediateContext->CopyTexture(
                    CopyAttribs);
            }

            Barriers.emplace_back(
                SrcTex,
                RESOURCE_STATE_UNKNOWN,
                RESOURCE_STATE_SHADER_RESOURCE,
                STATE_TRANSITION_FLAG_UPDATE_STATE);
        }

        m_TexArraySRV =
            pTexArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        Barriers.emplace_back(
            pTexArray,
            RESOURCE_STATE_UNKNOWN,
            RESOURCE_STATE_SHADER_RESOURCE,
            STATE_TRANSITION_FLAG_UPDATE_STATE);

        // set texture srv in the srb
        for (int texIndex = 0; texIndex < NumTextures; ++texIndex)
        {
            m_pPSO[0][0]->CreateShaderResourceBinding(
                &m_SRB[texIndex], true);
            m_SRB[texIndex]->GetVariableByName(
                SHADER_TYPE_PIXEL,
                "g_Texture")->Set(
                    m_TextureSRV[texIndex]);
        }

        m_pPSO[1][0]->CreateShaderResourceBinding(
            &m_BatchSRB,
            true);
        m_BatchSRB->GetVariableByName(
            SHADER_TYPE_PIXEL,
            "g_Texture")->Set(
                m_TexArraySRV);
    }

    void QxQuads::InitializeQuads()
    {
        m_Quads.resize(m_NumQuads);

        std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
        // to generate consistent distribution.

        std::uniform_real_distribution<float> scale_distr(0.01f, 0.05f);
        std::uniform_real_distribution<float> pos_distr(-0.95f, +0.95f);
        std::uniform_real_distribution<float> move_dir_distr(-0.1f, +0.1f);
        std::uniform_real_distribution<float> angle_distr(-PI_F, +PI_F);
        std::uniform_real_distribution<float> rot_distr(-PI_F * 0.5f, +PI_F * 0.5f);
        std::uniform_int_distribution<Int32>  tex_distr(0, NumTextures - 1);
        std::uniform_int_distribution<Int32>  state_distr(0, NumStates - 1);

        for (int quad = 0; quad < m_NumQuads; ++quad)
        {
            auto& CurrInst     = m_Quads[quad];
            CurrInst.Size      = scale_distr(gen);
            CurrInst.Angle     = angle_distr(gen);
            CurrInst.Pos.x     = pos_distr(gen);
            CurrInst.Pos.y     = pos_distr(gen);
            CurrInst.MoveDir.x = move_dir_distr(gen);
            CurrInst.MoveDir.y = move_dir_distr(gen);
            CurrInst.RotSpeed  = rot_distr(gen);
            // Texture array index
            CurrInst.TextureInd = tex_distr(gen);
            CurrInst.StateIndex   = state_distr(gen);
        } 
    }

    void QxQuads::CreateInstanceBuffer()
    {
        BufferDesc InstBufferDesc;
        InstBufferDesc.Name =  "Batch data buffer";

        InstBufferDesc.Usage = USAGE_DYNAMIC;
        InstBufferDesc.BindFlags = BIND_VERTEX_BUFFER;
        InstBufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        InstBufferDesc.Size =
            sizeof(InstanceData) * m_BatchSize;
        m_BatchDataBuffer.Release();
        m_pDevice->CreateBuffer(
            InstBufferDesc, nullptr, &m_BatchDataBuffer);

        StateTransitionDesc Barrier(
            m_BatchDataBuffer,
            RESOURCE_STATE_UNKNOWN,
            RESOURCE_STATE_CONSTANT_BUFFER,
            STATE_TRANSITION_FLAG_UPDATE_STATE);
        m_pImmediateContext->TransitionResourceStates(
            1, &Barrier);
    }

    void QxQuads::StartWorkderThreads(int InNumWorkerThreads)
    {
        m_WorkerThreads.resize(InNumWorkerThreads);
        for (Uint32 threadIndex = 0; threadIndex < m_WorkerThreads.size(); ++threadIndex)
        {
            m_WorkerThreads[threadIndex] =
                std::thread(WorkerThreadFunc, this, threadIndex);
        }
        m_CmdLists.resize(InNumWorkerThreads);
    }

void QxQuads::StopWorkerThreads()
    {
        m_RenderSubSetSignal.Trigger(
            true, -1);

        for (auto& thread : m_WorkerThreads)
        {
            thread.join();
        }
        m_RenderSubSetSignal.Reset();
        m_WorkerThreads.clear();
        m_CmdLists.clear();
    }

    template <bool UseBatch>
    void QxQuads::RenderSubset(
        IDeviceContext* pCtx, Uint32 SubsetIndex)
    {
        // 主线程完成state trainsition 这里只需要verify
        ITextureView* pRTV =
            m_pSwapChain->GetCurrentBackBufferRTV();
        pCtx->SetRenderTargets(1, &pRTV,
            m_pSwapChain->GetDepthBufferDSV(),
            RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        // 使用batch 时，需要绑定顶点的per itsnace buffer到管线
        if (UseBatch)
        {
            IBuffer* pBuffs[] = {m_BatchDataBuffer};
            pCtx->SetVertexBuffers(
                0, _countof(pBuffs),
                pBuffs, nullptr,
                RESOURCE_STATE_TRANSITION_MODE_VERIFY,
                SET_VERTEX_BUFFERS_FLAG_RESET);
        }
        
        DrawAttribs DrawAttrs;
        DrawAttrs.Flags =
            DRAW_FLAG_VERIFY_ALL;
        // DrawAttrs.NumInstances = 4;
        DrawAttrs.NumVertices = 4;
        
        Uint32 NumSubsets =
            Uint32{1} + static_cast<Uint32>(
                m_WorkerThreads.size());
        const Uint32 TotalQuads =
            static_cast<Uint32>(m_Quads.size());
        const Uint32 TotalBatches =
            (TotalQuads + m_BatchSize - 1) / m_BatchSize;
        const Uint32 SubsetSize =
            TotalBatches / NumSubsets;
        const Uint32 StartBatchIndex =
            SubsetSize * SubsetIndex;
        const Uint32 EndBatchIndex =
            (SubsetIndex < NumSubsets - 1) ?
                SubsetSize * (SubsetIndex + 1) : TotalBatches;
        for (Uint32 batchIndex = StartBatchIndex; batchIndex < EndBatchIndex; ++batchIndex)
        {
            const Uint32 StartInst =
                batchIndex * m_BatchSize;
            const Uint32 EndInst =
                std::min(
                    StartInst + static_cast<Uint32>(
                        m_BatchSize),
                        static_cast<Uint32>(m_NumQuads));

            auto StateInd =
                m_Quads[StartInst].StateIndex;
            pCtx->SetPipelineState(
                m_pPSO[UseBatch ? 1 : 0][StateInd]);

            MapHelper<InstanceData> BatchData;
            if (UseBatch)
            {
                pCtx->CommitShaderResources(
                    m_BatchSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
                BatchData.Map(
                    pCtx,
                    m_BatchDataBuffer, 
                    MAP_WRITE,
                    MAP_FLAG_DISCARD);
            }

            for (Uint32 instIndex = StartInst; instIndex < EndInst; ++instIndex)
            {
                const QuadData& CurInstData = m_Quads[instIndex];

                if (!UseBatch)
                {
                    pCtx->CommitShaderResources(
                        m_SRB[CurInstData.TextureInd],
                        RESOURCE_STATE_TRANSITION_MODE_VERIFY);
                }

                {
                    float2x2 ScaleMatrx
                    {
                        CurInstData.Size, 0.f,
                        0.f, CurInstData.Size
                    };

                    float sinAngle = sinf(CurInstData.Angle);
                    float cosAngle = cosf(CurInstData.Angle);
                    float2x2 RotMatr(cosAngle, -sinAngle,
                        sinAngle, cosAngle);

                    auto Matr =
                        ScaleMatrx * RotMatr;
                    float4 QuadRotationAndScale(
                        Matr.m00, Matr.m10,
                        Matr.m01, Matr.m11
                        );

                    if (UseBatch)
                    {
                        InstanceData& CurrQuad =
                            BatchData[instIndex - StartInst];
                        CurrQuad.QuadRotationAndScale =
                            QuadRotationAndScale;
                        CurrQuad.QuadCenter =
                            CurInstData.Pos;
                        CurrQuad.TexArrInd =
                            static_cast<float>(CurInstData.TextureInd);
                    }
                    else
                    {
                        struct QuadAttribs
                        {
                            float4 g_QuadRotationAndScale;
                            float4 g_QuadCenter;
                        };

                        MapHelper<QuadAttribs> InstData(
                            pCtx,
                            m_QuadAttribsCB,
                            MAP_WRITE,
                            MAP_FLAG_DISCARD);

                        InstData->g_QuadRotationAndScale
                            = QuadRotationAndScale;
                        InstData->g_QuadCenter.x =
                            CurInstData.Pos.x;
                        InstData->g_QuadCenter.y =
                            CurInstData.Pos.y;
                    }
                }
            }

            if (UseBatch)
            {
                BatchData.Unmap();
            }

            DrawAttrs.NumInstances =
                EndInst - StartInst;
            pCtx->Draw(DrawAttrs);
        }
    }
    void QxQuads::WorkerThreadFunc(QxQuads* pThis, Uint32 ThreadIndex)
    {
        IDeviceContext* pDeferredCtx =
            pThis->m_pDeferredContexts[ThreadIndex];
        const int NumWorkerThreads = static_cast<int>(pThis->m_WorkerThreads.size());
        VERIFY(NumWorkerThreads > 0);
        for (;;)
        {
            auto SignalValue =
                pThis->m_RenderSubSetSignal.Wait(true,
                    NumWorkerThreads);
            if (SignalValue < 0)
            {
                return;
            }
            pDeferredCtx->Begin(0);

            if (pThis->m_BatchSize > 1)
            {
                pThis->RenderSubset<true>(pDeferredCtx,
                    1 + ThreadIndex);
            }
            else
            {
                pThis->RenderSubset<false>(
                    pDeferredCtx,
                    1 + ThreadIndex);
            }

            RefCntAutoPtr<ICommandList> pCmdList;
            pDeferredCtx->FinishCommandList(&pCmdList);
            pThis->m_CmdLists[ThreadIndex] = pCmdList;

            {
                const auto NumThreadsCompleted =
                    pThis->m_NumThreadsCompleted.fetch_add(
                        1) + 1;
                if (NumThreadsCompleted == NumWorkerThreads)
                {
                    pThis->m_ExecuteCommandListsSignal.Trigger();
                }
            }

            pThis->m_GotoNextFrameSignal.Wait(true, NumWorkerThreads);

            pDeferredCtx->FinishFrame();

            pThis->m_NumThreadsReady.fetch_add(1);

            // 这句的意思是等待所有线程完成
            while (pThis->m_NumThreadsReady.load() < NumWorkerThreads)
            {
                std::this_thread::yield();
            }

            // 这里是保证go to next frame singal 在同一帧中不触发2次
            VERIFY(
                !pThis->m_GotoNextFrameSignal.IsTriggered());
        }
    }
}
