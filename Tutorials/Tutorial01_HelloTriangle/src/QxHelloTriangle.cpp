#include "QxHelloTriangle.h"

namespace Diligent
{
static const char* Qx_VSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn) 
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);

    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue

    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

// Pixel shader simply outputs interpolated vertex color
static const char* Qx_PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";


//SampleBase* CreateSample()
//{
//    return new QxHelloTriangle();
//}


void QxHelloTriangle::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // 创建和初始化pso
    GraphicsPipelineStateCreateInfo psoCreateInfo;

    psoCreateInfo.PSODesc.Name = "Simple Triangle PSO";

    psoCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    psoCreateInfo.GraphicsPipeline.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
    psoCreateInfo.GraphicsPipeline.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;

    psoCreateInfo.GraphicsPipeline.PrimitiveTopology       = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

    psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

    // 创建shader
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // opengl 模仿dx
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    shaderCI.UseCombinedTextureSamplers = true;

    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.EntryPoint      = "main";
        shaderCI.Desc.Name       = "Trinagle vertex shader";
        shaderCI.Source          = Qx_VSSource;
        m_pDevice->CreateShader(shaderCI, &pVS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.EntryPoint      = "main";
        shaderCI.Desc.Name       = "Trinagle pixel shader";
        shaderCI.Source          = Qx_PSSource;
        m_pDevice->CreateShader(shaderCI, &pPS);
    }

    m_pDevice->CreateGraphicsPipelineState(
        psoCreateInfo
        , &m_pPSO);
}
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)

void QxHelloTriangle::Render()
{
    //SampleBase
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};

    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor,
                                           RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        pDSV,
        CLEAR_DEPTH_FLAG, 1.f, 
        0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);



    m_pImmediateContext->SetPipelineState(m_pPSO);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3;
    m_pImmediateContext->Draw(drawAttrs);
}
void QxHelloTriangle::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
}

}

