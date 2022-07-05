#include "QxCube.h"

#include "MapHelper.hpp"

namespace Diligent
{
void QxCube::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();
}
void QxCube::Render()
{
    ITextureView* pRTV =  m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // clear back buffer
    //const float clearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    const float clearColor[] = {0.350f, 0.35f, 0.350f, 1.0f};

    m_pImmediateContext->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        MapHelper<float4x4> cbConstants(m_pImmediateContext, m_VSContants, MAP_WRITE, MAP_FLAG_DISCARD);
        //cbConstants->Transpose();
        *cbConstants = m_WorldViewProjMatrix.Transpose();
    }

    // 绑定vertex 和index buffer
    const Uint64 offset = 0;
    IBuffer* pBuffers[] = {m_CubeVertxBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffers, &offset,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // 设置pso
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // commit shader resource。 transition模式自动将资源转换到需要的状态
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttribs;
    drawAttribs.IndexType  = VT_UINT32;
    drawAttribs.NumIndices = 36;
    drawAttribs.Flags      = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(drawAttribs);
}
void QxCube::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    float4x4 cubeModelTransform = float4x4::RotationY(
        static_cast<float>(CurrTime) * 1.0f)
        * float4x4::RotationX(-PI_F * 0.1F);

    float4x4 viewMat = float4x4::Translation(0.f, 0.f, 5.f);

    float4x4 srfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // get projection matrix
    float4x4 proj = GetAdjustedProjectionMatrix(PI_F / 4.f, 0.1f, 100.f);

    // 计算mvp
    m_WorldViewProjMatrix = cubeModelTransform * viewMat * srfPreTransform * proj;
}

void QxCube::CreatePipelineState()
{
    GraphicsPipelineStateCreateInfo psoCreateInfo;

    psoCreateInfo.PSODesc.Name = "Cube PSO";
    psoCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    psoCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    psoCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    psoCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;

    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.UseCombinedTextureSamplers = true;

    // 测试从文件加载shader
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    shaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    // 创建vs 和uniform buffer
    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Cube VS";
        shaderCI.FilePath = "QxCube.vsh";
        m_pDevice->CreateShader(shaderCI, &pVS);

        // 创建dynamic uniform buffer存储WVP矩阵，可以经常被CPU更新
        BufferDesc cbDesc;
        cbDesc.Name = "VS Constants CB";
        cbDesc.Size = sizeof(float4x4);
        cbDesc.Usage = USAGE_DYNAMIC;
        cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(cbDesc, nullptr, &m_VSContants);
    }

    // 创建ps
    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Cube PS";
        shaderCI.FilePath = "QxCube.psh";
        m_pDevice->CreateShader(shaderCI, &pPS);
    }

    // 声明vs input layout
    LayoutElement LayoutElements[] =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, false},
            LayoutElement{1, 0, 4, VT_FLOAT32, false}
        };
    psoCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElements;
    psoCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElements);

    psoCreateInfo.pVS = pVS;
    psoCreateInfo.pPS = pPS;

    // 定义pso 默认使用的 shader resoure 类型, 这里的static 指变量和buffer绑定之后不会解绑、和其他绑定，并且是和通过pso绑定
    psoCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(psoCreateInfo, &m_pPSO);

    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSContants);

    // 创建resource bing 对象，并且初始化static resources
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
}

void QxCube::CreateVertexBuffer()
{
    // 这里定义的结构一定要和pso 中定义的layout一致
    struct Vertex
    {
        float3 pos;
        float4 color;
    };

    // clang-format off
    Vertex CubeVerts[8] =
    {
        {float3(-1,-1,-1), float4(1,0,0,1)},
        {float3(-1,+1,-1), float4(0,1,0,1)},
        {float3(+1,+1,-1), float4(0,0,1,1)},
        {float3(+1,-1,-1), float4(1,1,1,1)},

        {float3(-1,-1,+1), float4(1,1,0,1)},
        {float3(-1,+1,+1), float4(0,1,1,1)},
        {float3(+1,+1,+1), float4(1,0,1,1)},
        {float3(+1,-1,+1), float4(0.2f,0.2f,0.2f,1)},
    };
    // clang-format on

    // 创建cube 的 vertex buffer
    BufferDesc vbDesc;
    vbDesc.Name = "Cube Vertex Buffer";
    vbDesc.Usage = USAGE_IMMUTABLE;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.Size = sizeof(CubeVerts);
    BufferData vbData;
    vbData.pData = CubeVerts;
    vbData.DataSize = sizeof(CubeVerts);
    m_pDevice->CreateBuffer(vbDesc, &vbData, &m_CubeVertxBuffer);
    
}

void QxCube::CreateIndexBuffer()
{
    Uint32 indices[] =
    {
        2,0,1, 2,3,0,
        4,6,5, 4,7,6,
        0,7,4, 0,3,7,
        1,0,4, 1,4,5,
        1,5,2, 5,6,2,
        3,6,7, 3,2,6
    };

    BufferDesc ibDesc;
    ibDesc.Name  = "Cube Index buffer";
    ibDesc.Usage = USAGE_IMMUTABLE;
    ibDesc.BindFlags = BIND_INDEX_BUFFER;
    ibDesc.Size = sizeof(indices);
    BufferData ibData;
    ibData.pData = indices;
    ibData.DataSize = sizeof(indices);
    m_pDevice->CreateBuffer(ibDesc, &ibData, &m_CubeIndexBuffer);
}
}
