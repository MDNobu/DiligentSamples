
#include "QxQuads.h"

#include "GraphicsUtilities.h"

namespace Diligent
{
    QxQuads::~QxQuads() {}
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
        
    }

    void QxQuads::UpdateQuads(float elapsedTime)
    {
        
    }

    void QxQuads::CreatePipelineStates(std::vector<StateTransitionDesc>& Barriers)
    {
        BlendStateDesc BlendStates[NumStates];
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
            ShaderCI.FilePath = "QxQuadBatch.vsh";
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

        ShaderResourceDesc Vars[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables =
            _countof(Vars);

        SamplerDesc SamLinearClampDesc
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
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
            PSOCreateInfo.GraphicsPipeline.BlendDesc = BlendStates[stateIndex];

            m_pDevice->CreateGraphicsPipelineState(
                PSOCreateInfo,
                &m_pPSO[0][stateIndex]);

            m_pPSO[0][stateIndex]->GetStaticVariableByName(
                SHADER_TYPE_VERTEX,
                "QuadAttribs"
                )->Set(m_QuadAttribsCB);

            if (stateIndex > 0)
            {
                VERIFY(
                    m_pPSO[0][stateIndex]->IsCompatibleWith(
                        m_pPSO[0][0]), "PSOs are expected to be compatible");
            }
        }        
    }

    void QxQuads::LoadTextures(std::vector<StateTransitionDesc>& Barriers)
    {
        
    }

    void QxQuads::InitializeQuads()
    {
        
    }

    void QxQuads::CreateInstanceBuffer()
    {
        
    }

    void QxQuads::StartWorkderThreads(int InNumWorkerThreads)
    {
        
    }
}
