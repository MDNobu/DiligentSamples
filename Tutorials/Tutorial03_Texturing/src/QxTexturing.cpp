#include "QxTexturing.h"

#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "TextureLoader.h"
#include "TextureUtilities.h"

namespace Diligent
{


void QxTexturing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    CreatePSO();
    CreateVertexBuffer();
    CreateIndexBuffer();
    LoadTexture();
  }

void QxTexturing::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();

    const float ClearColor[] =  {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        MapHelper<float4x4> cbConstant(m_pImmediateContext, m_WVP_VS,
            MAP_WRITE, MAP_FLAG_DISCARD);
        *cbConstant = m_WorldViewProj.Transpose();
    }

    const Uint64 offset = 0;
    IBuffer* pBuffs[] ={m_CubeVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_pImmediateContext->SetPipelineState(m_pPSO);
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    DrawIndexedAttribs drawAttribs;
    drawAttribs.IndexType = VT_UINT32;
    drawAttribs.NumIndices = 36;
    drawAttribs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(drawAttribs);
}

void QxTexturing::Update(double CurrTime, double ElapsedTime)
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
    m_WorldViewProj = cubeModelTransform * viewMat * srfPreTransform * proj;
}

void QxTexturing::CreatePSO()
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

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    shaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    
    // create vs
    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Cube VS";
        shaderCI.FilePath = "QxCube.vsh";
        m_pDevice->CreateShader(shaderCI, &pVS);

        //
        CreateUniformBuffer(
            m_pDevice,
            sizeof(float4x4),
            "VS contant CB",
            &m_WVP_VS
            );
    }
    
    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Cube PS";
        shaderCI.FilePath = "QxCube.psh";
        m_pDevice->CreateShader(shaderCI, &pPS);
    }
    
    psoCreateInfo.pVS = pVS;
    psoCreateInfo.pPS = pPS;

    LayoutElement LayoutElements[] =
    {
        LayoutElement{0, 0, 3, VT_FLOAT32, false},
        LayoutElement{1, 0, 2, VT_FLOAT32, false}
    };
    psoCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElements;
    psoCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElements);

    psoCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc vars[]=
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    
    psoCreateInfo.PSODesc.ResourceLayout.Variables = vars;
    psoCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(vars);
    
    SamplerDesc samLinearDes
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc imtbSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_Texture", samLinearDes}
    };
    psoCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = imtbSamplers;
    psoCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(imtbSamplers);
    
    m_pDevice->CreateGraphicsPipelineState(psoCreateInfo, &m_pPSO);

    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "QxConstants")
    ->Set(m_WVP_VS);

    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);

}

void QxTexturing::CreateVertexBuffer()
{
    // Layout of this structure matches the one we defined in the pipeline state
    struct Vertex
    {
        float3 pos;
        float2 uv;
    };

    // Cube vertices

    //      (-1,+1,+1)________________(+1,+1,+1)
    //               /|              /|
    //              / |             / |
    //             /  |            /  |
    //            /   |           /   |
    //(-1,-1,+1) /____|__________/(+1,-1,+1)
    //           |    |__________|____|
    //           |   /(-1,+1,-1) |    /(+1,+1,-1)
    //           |  /            |   /
    //           | /             |  /
    //           |/              | /
    //           /_______________|/
    //        (-1,-1,-1)       (+1,-1,-1)
    //

    // clang-format off
    // This time we have to duplicate verices because texture coordinates cannot
    // be shared
    Vertex CubeVerts[] =
    {
        {float3(-1,-1,-1), float2(0,1)},
        {float3(-1,+1,-1), float2(0,0)},
        {float3(+1,+1,-1), float2(1,0)},
        {float3(+1,-1,-1), float2(1,1)},

        {float3(-1,-1,-1), float2(0,1)},
        {float3(-1,-1,+1), float2(0,0)},
        {float3(+1,-1,+1), float2(1,0)},
        {float3(+1,-1,-1), float2(1,1)},

        {float3(+1,-1,-1), float2(0,1)},
        {float3(+1,-1,+1), float2(1,1)},
        {float3(+1,+1,+1), float2(1,0)},
        {float3(+1,+1,-1), float2(0,0)},

        {float3(+1,+1,-1), float2(0,1)},
        {float3(+1,+1,+1), float2(0,0)},
        {float3(-1,+1,+1), float2(1,0)},
        {float3(-1,+1,-1), float2(1,1)},

        {float3(-1,+1,-1), float2(1,0)},
        {float3(-1,+1,+1), float2(0,0)},
        {float3(-1,-1,+1), float2(0,1)},
        {float3(-1,-1,-1), float2(1,1)},

        {float3(-1,-1,+1), float2(1,1)},
        {float3(+1,-1,+1), float2(0,1)},
        {float3(+1,+1,+1), float2(0,0)},
        {float3(-1,+1,+1), float2(1,0)}
    };
    // clang-format on

    BufferDesc vbDesc;
    vbDesc.Name = "Cube Vertex buffer";
    vbDesc.Usage = USAGE_IMMUTABLE;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.Size = sizeof(CubeVerts);
    BufferData vbData;
    vbData.pData = CubeVerts;
    vbData.DataSize = sizeof(CubeVerts);
    m_pDevice->CreateBuffer(vbDesc, &vbData, &m_CubeVertexBuffer);
}
void QxTexturing::CreateIndexBuffer()
{
    // clang-format off
    Uint32 Indices[] =
    {
        2,0,1,    2,3,0,
        4,6,5,    4,7,6,
        8,10,9,   8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,21,22, 20,22,23
    };
    // clang-format on

    BufferDesc ibDesc;
    ibDesc.Name = "Cube Index Buffer";
    ibDesc.Usage = USAGE_IMMUTABLE;
    ibDesc.BindFlags = BIND_INDEX_BUFFER;
    ibDesc.Size = sizeof(Indices);
    BufferData ibData;
    ibData.pData = Indices;
    ibData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(ibDesc, &ibData, &m_CubeIndexBuffer);
    
}
void QxTexturing::LoadTexture()
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> tex;
    CreateTextureFromFile("DGLogo.png",
        loadInfo, m_pDevice, &tex);

    m_TextureSRV = tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")
        ->Set(m_TextureSRV);
}
} // namespace Diligent