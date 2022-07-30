#include "QxDataStreaming.h"

#include <random>

#include "GraphicsUtilities.h"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "MapHelper.hpp"
#include "TextureLoader.h"
#include "TextureUtilities.h"

namespace Diligent
{
class QxStreamingBuffer
{
public:
    QxStreamingBuffer(IRenderDevice* pDevice,
        BIND_FLAGS BindFlags,
        Uint32 Size,
        size_t NumContexts,
        const Char* Name):
            m_BufferSize(Size),
            m_MapInfo(NumContexts)
    {
        BufferDesc BufferDesc;
        BufferDesc.Name = Name;
        BufferDesc.Usage = USAGE_DYNAMIC;
        BufferDesc.BindFlags = BindFlags;
        BufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        BufferDesc.Size = Size;
        pDevice->CreateBuffer(
            BufferDesc,
            nullptr,
            &m_pBuffer);
    }

    Uint32 Allocate(IDeviceContext* pCtx,
        Uint32 Size,
        size_t CtxNum)
    {
        MapInfo& MapInfo = m_MapInfo[CtxNum];
        // 检查buffer中是否有足够空间
        if (MapInfo.m_CurrOffset + Size > m_BufferSize)
        {
            // Unmap the buffer
            Flush(CtxNum);
        }

        if (MapInfo.m_MappedData == nullptr)
        {
            MapInfo.m_MappedData.Map(
                pCtx, m_pBuffer,
                MAP_WRITE,
                MapInfo.m_CurrOffset == 0 ?
                    MAP_FLAG_DISCARD : MAP_FLAG_NO_OVERWRITE
                );
        }

        Uint32 Offset = MapInfo.m_CurrOffset;
        // Update Offset
        MapInfo.m_CurrOffset += Size;
        return  Offset;
    }

    void Release(size_t CtxNum)
    {
        if (!m_AllowPersistentMap)
        {
            m_MapInfo[CtxNum].m_MappedData.Unmap();
        }
    }
    
    void Flush(size_t CtxNum)
    {
        m_MapInfo[CtxNum].m_MappedData.Unmap();
        m_MapInfo[CtxNum].m_CurrOffset = 0;
    }

    IBuffer* GetBuffer() {return m_pBuffer;}
    void* GetMappedCPUAddress(size_t CtxNum)
    {
        return m_MapInfo[CtxNum].m_MappedData;
    }

    void AllowPersistentMapping(bool AllowMapping)
    {
        m_AllowPersistentMap = AllowMapping;
    }
private:
    RefCntAutoPtr<IBuffer> m_pBuffer;
    const Uint32 m_BufferSize;

    bool m_AllowPersistentMap = false;

    struct MapInfo
    {
        MapHelper<Uint8> m_MappedData;
        Uint32 m_CurrOffset = 0;
    };

    // 我们需要跟踪每个context 的Mapped data
    std::vector<MapInfo> m_MapInfo;
};

QxDataStreaming::~QxDataStreaming()
{
    throw std::logic_error("Not implemented");
}

void QxDataStreaming::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);
    Attribs.EngineCI.NumDeferredContexts =
        std::max(
            std::thread::hardware_concurrency() - 1, 2u);
}

void QxDataStreaming::LoadTextures( std::vector<StateTransitionDesc>& OutBarriers)
{
    RefCntAutoPtr<ITexture> pTextureArray;
    for (int texIndex = 0; texIndex < NumTextures; ++texIndex)
    {
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> SrcTex;
        std::stringstream FileNameSS;
        FileNameSS << "DGLogo" << texIndex << ".png";

        auto FileName =
            FileNameSS.str();
        CreateTextureFromFile(
            FileName.c_str(),
            loadInfo,
            m_pDevice,
            &SrcTex);

        m_TextureSRV[texIndex] =
            SrcTex->GetDefaultView(
                TEXTURE_VIEW_SHADER_RESOURCE);

        const auto& TexDesc = SrcTex->GetDesc();
        if (pTextureArray == nullptr)
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
                &pTextureArray);
        }

        // 每个miplevel 都拷贝到texture array
        for (Uint32 mipLevel = 0; mipLevel < TexDesc.MipLevels; ++mipLevel)
        {
            CopyTextureAttribs CopyAttribs(
                SrcTex,
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                pTextureArray,
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION
                );
            CopyAttribs.SrcMipLevel = mipLevel;
            CopyAttribs.DstMipLevel = mipLevel;
            CopyAttribs.DstSlice = texIndex;
            m_pImmediateContext->CopyTexture(CopyAttribs);
        }
        OutBarriers.emplace_back(
            SrcTex,
            RESOURCE_STATE_UNKNOWN,
            RESOURCE_STATE_SHADER_RESOURCE,
            STATE_TRANSITION_FLAG_UPDATE_STATE);
    }

    m_TextureArraySRV =
        pTextureArray->GetDefaultView(
            TEXTURE_VIEW_SHADER_RESOURCE);
    OutBarriers.emplace_back(
        pTextureArray,
        RESOURCE_STATE_UNKNOWN,
        RESOURCE_STATE_SHADER_RESOURCE,
        STATE_TRANSITION_FLAG_UPDATE_STATE);

    // set texture srv
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
        &m_BatchSRB, true);
    m_BatchSRB->GetVariableByName(
        SHADER_TYPE_PIXEL,
        "g_Texture")->Set(m_TextureArraySRV);
}

void QxDataStreaming::CreatePipelineStates(
    std::vector<StateTransitionDesc>& OutBarriers)
{
    BlendStateDesc BlendStates[NumStates];
    BlendStates[1].RenderTargets[0].BlendEnable = true;
    BlendStates[1].RenderTargets[0].SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
    BlendStates[1].RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;

    BlendStates[2].RenderTargets[0].BlendEnable = true;
    BlendStates[2].RenderTargets[0].SrcBlend    = BLEND_FACTOR_INV_SRC_ALPHA;
    BlendStates[2].RenderTargets[0].DestBlend   = BLEND_FACTOR_SRC_ALPHA;

    BlendStates[3].RenderTargets[0].BlendEnable = true;
    BlendStates[3].RenderTargets[0].SrcBlend    = BLEND_FACTOR_SRC_COLOR;
    BlendStates[3].RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_COLOR;

    BlendStates[4].RenderTargets[0].BlendEnable = true;
    BlendStates[4].RenderTargets[0].SrcBlend    = BLEND_FACTOR_INV_SRC_COLOR;
    BlendStates[4].RenderTargets[0].DestBlend   = BLEND_FACTOR_SRC_COLOR;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "Qx Polygon PSO";

    PSOCreateInfo.PSODesc.PipelineType =
        PIPELINE_TYPE_GRAPHICS;

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
        nullptr,
        &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    // Create vertex shader
    RefCntAutoPtr<IShader> pVS,pVSBatched;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Qx Polygon VS";
        ShaderCI.FilePath = "QxPolygon.vsh";

        m_pDevice->CreateShader(ShaderCI, &pVS);
        VERIFY(pVS.RawPtr());

        ShaderCI.Desc.Name = "Qx Polygon VS Batched";
        ShaderCI.FilePath = "QxPolygonBatch.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVSBatched);
        VERIFY(pVSBatched.RawPtr());

        // 创建存储transform等信息的constant buffer
        CreateUniformBuffer(
            m_pDevice,
            sizeof(float4x4),
            "Instance Constants CB",
            &m_PolygonAttribsCB);

        OutBarriers.emplace_back(
            m_PolygonAttribsCB,
            RESOURCE_STATE_UNKNOWN,
            RESOURCE_STATE_CONSTANT_BUFFER,
            STATE_TRANSITION_FLAG_UPDATE_STATE);
    }

    RefCntAutoPtr<IShader> pPS, pPSBatched;
    // Create Pixel Shader
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Qx Polygon PS";
        ShaderCI.FilePath = "QxPolygon.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
        VERIFY(pPS.RawPtr());
        
        ShaderCI.Desc.Name = "Qx Polygon PS Batched";
        ShaderCI.FilePath = "QxPolygonBatch.psh";
        m_pDevice->CreateShader(ShaderCI, &pPSBatched);
        VERIFY(pPSBatched.RawPtr());
    }

    LayoutElement LayoutElems[] =
    {
        LayoutElement{
            0, 0,
            2, VT_FLOAT32,
            false,
            LAYOUT_ELEMENT_AUTO_OFFSET,
            LAYOUT_ELEMENT_AUTO_STRIDE,
        INPUT_ELEMENT_FREQUENCY_PER_VERTEX}    
    };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements =
        LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements =
        _countof(LayoutElems);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] =
    {
        {
            SHADER_TYPE_PIXEL, "g_Texture",
            SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE
        }
    };
    PSOCreateInfo.PSODesc.ResourceLayout.Variables =
        Vars;
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
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {
            SHADER_TYPE_PIXEL,
            "g_Texture", SamLinearClampDesc
        }
    };

    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers =
        ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
        _countof(ImtblSamplers);

    for (int stateIndex = 0; stateIndex < NumStates; ++stateIndex)
    {
        PSOCreateInfo.GraphicsPipeline.BlendDesc =
            BlendStates[stateIndex];
        m_pDevice->CreateGraphicsPipelineState(
            PSOCreateInfo,
            &m_pPSO[0][stateIndex]);
        m_pPSO[0][stateIndex]->GetStaticVariableByName(
            SHADER_TYPE_VERTEX,
            "PolygonAttribs")->Set(m_PolygonAttribsCB);
        if (stateIndex > 0)
        {
            VERIFY(
                m_pPSO[0][stateIndex]->IsCompatibleWith(
                    m_pPSO[0][0]), "PSO are expected to be compatilble");
            
        }
    }

    PSOCreateInfo.PSODesc.Name =
        "Qx Batched Polygon PSO";
    LayoutElement BatchLayoutElems[] =
    {
        // Attribute 0 - PolygonXY
        LayoutElement{
            0, 0, 2,
            VT_FLOAT32,
            false,
            INPUT_ELEMENT_FREQUENCY_PER_VERTEX
        },
        // Attribute 1 - PolygonRotationAndScale
        LayoutElement{
            1, 1, 4,
            VT_FLOAT32,
            false,
            INPUT_ELEMENT_FREQUENCY_PER_INSTANCE
        },
        // Attribute 2 - PolygonCenter
        LayoutElement{
            2, 1, 2,
            VT_FLOAT32,
            false,
            INPUT_ELEMENT_FREQUENCY_PER_INSTANCE
        },
        // Attribute 3 - TexArrInd
        LayoutElement{
            3, 1, 1,
            VT_FLOAT32,
            false,
            INPUT_ELEMENT_FREQUENCY_PER_INSTANCE
        }
    };

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements =
        BatchLayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements =
        _countof(BatchLayoutElems);

    PSOCreateInfo.pVS = pVSBatched;
    PSOCreateInfo.pPS = pPSBatched;

    for (int stateIndex = 0; stateIndex < NumStates; ++stateIndex)
    {
        PSOCreateInfo.GraphicsPipeline.BlendDesc =
            BlendStates[stateIndex];
        m_pDevice->CreateGraphicsPipelineState(
            PSOCreateInfo,
            &m_pPSO[1][stateIndex]);
#if DILIGENT_DEBUG
        if (stateIndex > 0)
        {
            VERIFY(
                m_pPSO[1][stateIndex]->IsCompatibleWith(
                    m_pPSO[1][0]), "pso supporse to be camptible with this ");
        }
#endif
    }
}


void QxDataStreaming::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    m_MaxThreads = static_cast<int>(m_pDeferredContexts.size());
    m_NumWorkerThreads =
        std::min(m_NumWorkerThreads, m_MaxThreads);

    std::vector<StateTransitionDesc> Barriers;
    CreatePipelineStates(Barriers);
    LoadTextures(Barriers);

    m_StreamingVB =
        std::make_unique<QxStreamingBuffer>(
            m_pDevice,
            BIND_VERTEX_BUFFER,
            MaxVertsInStreamingBuffer * Uint32{sizeof(float2)},
            1u + InitInfo.NumDeferredCtx,
            "Streaming vertex buffer");
}

template <bool UseBatch> void QxDataStreaming::RenderSubset(const RefCntAutoPtr<IDeviceContext>& pCtx, int SubsetIndex)
{
    throw std::logic_error("Not implemented");
}

void QxDataStreaming::Render()
{
    ITextureView* pRTV =
        m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV =
        m_pSwapChain->GetDepthBufferDSV();
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(
        pRTV,
        ClearColor,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        pDSV,
        CLEAR_DEPTH_FLAG,
        1.0,
        0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_StreamingIB->AllowPersistentMapping(m_bAllowPersistentMap);
    m_StreamingVB->AllowPersistentMapping(m_bAllowPersistentMap);

    if (!m_WorkerTheads.empty())
    {
        m_NumThreadsCompleted.store(0);
        m_RenderSubsetSignal.Trigger(true);
    }

    if (m_BatchSize > 1)
    {
        RenderSubset<true>(m_pImmediateContext, 0);
    }
    else
    {
        RenderSubset<false>(m_pImmediateContext, 0);
    }

    if (!m_WorkerTheads.empty())
    {
        m_ExecuteCommandListSignal.Wait(true, 1);

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

void QxDataStreaming::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    UpdatePolygons(static_cast<float>(std::min(ElapsedTime, 0.25)));
}

void QxDataStreaming::StopWorkerThreads()
{
    m_RenderSubsetSignal.Trigger(true, -1);

    for (auto& thread : m_WorkerTheads)
    {
        thread.join();
    }
    m_RenderSubsetSignal.Reset();
    m_WorkerTheads.clear();
    m_CmdLists.clear();
}

void QxDataStreaming::InitializePolygons()
{
    throw std::logic_error("Not implemented");
}

void QxDataStreaming::CreateInstanceBuffer()
{
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Qx Batch data Buffer";

    InstBuffDesc.Usage = USAGE_DYNAMIC;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.CPUAccessFlags =
        CPU_ACCESS_WRITE;
    InstBuffDesc.Size =
        sizeof(InstanceData) * m_BatchSize;
    m_BatchDataBuffer.Release();
    m_pDevice->CreateBuffer(
        InstBuffDesc,
        nullptr,
        &m_BatchDataBuffer);
    
}
inline void QxDataStreaming::WorkerThreadFunc(
    QxDataStreaming* pThis, Uint32 ThreadIndex)
{
    IDeviceContext* pDeferredCtx =
        pThis->m_pDeferredContexts[ThreadIndex];
    const int NumWorkerThreads =
        static_cast<int>(pThis->m_WorkerTheads.size());
    VERIFY_EXPR(NumWorkerThreads > 0);

    for (;;)
    {
        auto SignalValue =
            pThis->m_RenderSubsetSignal.Wait(true, NumWorkerThreads);
        if (SignalValue  < 0)
        {
            return;
        }

        pDeferredCtx->Begin(0);

        RefCntAutoPtr<ICommandList> pCmdList;
        pDeferredCtx->FinishCommandList(&pCmdList);
        pThis->m_CmdLists[ThreadIndex] = pCmdList;

        {
            const auto NumThreadCompleted =
                pThis->m_NumThreadsCompleted.fetch_add(1) + 1;
            if (NumThreadCompleted == NumWorkerThreads)
            {
                pThis->m_ExecuteCommandListSignal.Trigger();
            }
        }

        pThis->m_GotoNextFrameSignal.Wait(
            true, NumWorkerThreads);

        pDeferredCtx->FinishFrame();

        while (pThis->m_NumThreadsReady.load() < NumWorkerThreads)
        {
            std::this_thread::yield();
        }
        VERIFY_EXPR(!pThis->m_GotoNextFrameSignal.IsTriggered());
    }
}

void QxDataStreaming::StartWorkerThreads(int InTheadNum)
{
    m_WorkerTheads.resize(InTheadNum);

    for (Uint32 threadIndex = 0; threadIndex < m_WorkerTheads.size(); ++threadIndex)
    {
        m_WorkerTheads[threadIndex] =
            std::thread(WorkerThreadFunc, this, threadIndex);
    }
    m_CmdLists.resize(InTheadNum);
}

void QxDataStreaming::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Settings",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::InputInt("Num Polygons",
            &m_NumPolgyons,
            100,
            1000,
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            m_NumPolgyons =
                clamp(m_NumPolgyons, 1, MaxPolygons);
            InitializePolygons();
        }

        if (ImGui::InputInt("Batch Size",
            &m_BatchSize,
            1,
            5))
        {
            m_BatchSize =
                clamp(m_BatchSize, 1, MaxBatchSize);
            CreateInstanceBuffer();
        }

        {
            ImGui::ScopedDisabler Disable(
                m_MaxThreads == 0);
            if (ImGui::SliderInt("Worker Threads",
                &m_NumWorkerThreads, 0, m_MaxThreads))
            {
                StopWorkerThreads();
                StartWorkerThreads(m_NumWorkerThreads);
            }
        }

        if (m_pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D12)
        {
            ImGui::Checkbox("Persistent map", &m_bAllowPersistentMap);
        }
    }

    ImGui::End();
}

void QxDataStreaming::UpdatePolygons(float elapsedTime)
{
    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
    // to generate consistent distribution.

    std::uniform_real_distribution<float> rot_distr(-PI_F * 0.5f, +PI_F * 0.5f);
    for (int Polygon = 0; Polygon < m_NumPolgyons; ++Polygon)
    {
        auto& CurrInst = m_Polygons[Polygon];
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

std::pair<Uint32, Uint32> QxDataStreaming::WritePolygon(
    const PolygonGemetry& PolygonGeo, IDeviceContext* pCtx, size_t CtxNum)
{
    throw std::logic_error("Not implemented");
}
}
