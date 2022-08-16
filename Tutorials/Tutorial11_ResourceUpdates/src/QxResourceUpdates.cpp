#include "QxResourceUpdates.h"

#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "TextureLoader.h"
#include "TextureUtilities.h"

namespace Diligent
{
namespace 
{
struct Vertex
{
    float3 pos;
    float2 uv;
};

// clang-format off
const Vertex CubeVerts[] =
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
}

void QxResourceUdpate::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    CreatePipelineStates();
    CreateVertexBuffers();
    CreateIndexBuffer();
    LoadTexture();

    // create texture update buffer
    {
        BufferDesc VertBufferDesc;
        VertBufferDesc.Name =
            "Texture update buffer";
        VertBufferDesc.Usage = USAGE_DYNAMIC;
        VertBufferDesc.BindFlags =
            BIND_VERTEX_BUFFER;
        VertBufferDesc.CPUAccessFlags =
            CPU_ACCESS_WRITE;
        // #TODO #Unkown这里这句是为什么
        VertBufferDesc.Size =
            MaxUpdateRegionSize * MaxUpdateRegionSize * 4;
        m_pDevice->CreateBuffer(
            VertBufferDesc, nullptr,
            &m_TextureUpdateBuffer);
    }
}

void QxResourceUdpate::Render()
{
    ITextureView* pRTV =
        m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV =
        m_pSwapChain->GetDepthBufferDSV();

    // Clear the back buffer;
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(
        pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(
        pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_pImmediateContext->SetPipelineState(m_pPSO);

    auto srfPreTransform =
        GetSurfacePretransformMatrix(
            float3(0, 0, 1));

    auto Proj =
        GetAdjustedProjectionMatrix(
            PI_F / 4.f, 0.1f, 100.f);
    auto ViewProj  =
        srfPreTransform * Proj;

    auto CubeRotation = float4x4::RotationY(static_cast<float>(m_CurrTime) * 0.5f) * float4x4::RotationX(-PI_F * 0.1f) * float4x4::Translation(0, 0, 12.0f);

    DrawCube(
        CubeRotation * float4x4::Translation(
            -2.f, -2.f, 0.f) * ViewProj,
            m_CubeVertexBuffer[0], m_SRBs[2]);

    DrawCube(
        CubeRotation *
        float4x4::Translation(2.f, -2.f, 0.f) * ViewProj,
        m_CuberIndexBuffer[0], m_SRBs[3]);

    DrawCube(
        CubeRotation *
        float4x4::Translation(-4.f, 2.f, 0.f) *
        ViewProj, m_CuberIndexBuffer[0], m_SRBs[0]);
    m_pImmediateContext->SetPipelineState(m_pPSO_NoCull);

    DrawCube(
        CubeRotation *
        float4x4::Translation(0.f, 2.f, 0.f) * ViewProj,
        m_CuberIndexBuffer[1], m_SRBs[0]
        );

    DrawCube(
        CubeRotation *
        float4x4::Translation(4.f, 2.f, 0.f) * ViewProj,
        m_CuberIndexBuffer[2],
        m_SRBs[1]);
}

void QxResourceUdpate::UpdateBuffer(Diligent::Uint32 BufferIndex)
{
    Uint32 NumVertsToUpdate =
        std::uniform_int_distribution<Uint32>{2, 8}(
            m_gen);
    Uint32 FirstVertToUpdate =
        std::uniform_int_distribution<Uint32>{0,
        static_cast<Uint32>(_countof(CubeVerts) - NumVertsToUpdate)(m_gen)};

    Vertex Vertices[_countof(CubeVerts)];
    for (int v = 0; v < NumVertsToUpdate; ++v)
    {
        auto SrcInd = FirstVertToUpdate + v;
        const auto& SrcVert =
            CubeVerts[SrcInd];

        Vertices[v].uv = SrcVert.uv;
        Vertices[v].pos     = SrcVert.pos *
          static_cast<float>(1 + 0.2 * sin(m_CurrentTime * (1.0 + SrcInd * 0.2)));
                
    }
    m_pImmediateContext->UpdateBuffer(
        m_CubeVertexBuffer[BufferIndex],
        FirstVertToUpdate * sizeof(Vertex),
        NumVertsToUpdate * sizeof(Vertex),
        Vertices,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION
        );
}

void QxResourceUdpate::MapDynamicBuffer(Diligent::Uint32 BufferIndex)
{
    
}

void QxResourceUdpate::UpdateTexture(Uint32 TexIndex)
{
    
}

void QxResourceUdpate::MapTexture(Uint32 TexIndex, bool MapEntireTexture)
{
    
}

void QxResourceUdpate::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    m_CurrentTime = CurrTime;

    static constexpr  const double UpdateBufferPreriod = 0.f;

    if (CurrTime - m_LastBufferUpdateTime > UpdateBufferPreriod)
    {
        m_LastBufferUpdateTime = CurrTime;
        UpdateBuffer(1);
    }

    MapDynamicBuffer(2);

    static constexpr  const double MaxTexturePerroid = 0.05f;
    const auto& deviceType = m_pDevice->GetDeviceInfo().Type;
    if (CurrTime - m_LastMapTime > MaxTexturePerroid *
        ((deviceType == RENDER_DEVICE_TYPE_D3D11) ? 10.f : 1.f
            ) )
    {
        m_LastMapTime = CurrTime;

        if (deviceType == RENDER_DEVICE_TYPE_D3D11 ||
            deviceType == RENDER_DEVICE_TYPE_D3D12 ||
            deviceType == RENDER_DEVICE_TYPE_VULKAN ||
            deviceType == RENDER_DEVICE_TYPE_METAL)
        {
            MapTexture(3,
                deviceType == RENDER_DEVICE_TYPE_D3D11);
        }
    }
}

void QxResourceUdpate::CreatePipelineStates()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name  = "Cube PSO";

    PSOCreateInfo.PSODesc.PipelineType =
        PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;

    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]
        = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat =
        m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
        PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode =
        CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc
        .DepthEnable = true;

    ShaderCreateInfo ShaderCI;

    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShaderSourceInputStreamFactory>
        pShaderFagory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(
        nullptr, &pShaderFagory);
    ShaderCI.pShaderSourceStreamFactory = pShaderFagory;

    //Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType =
            SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Cube VS";
        ShaderCI.FilePath = "QxCubeVS.hlsl";

        m_pDevice->CreateShader(ShaderCI, &pVS);

        CreateUniformBuffer(
            m_pDevice,
            sizeof(float4x4),
            "VS Constants CB",
            &m_VSConstants);
    }

    // Creat a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Cube PS";
        ShaderCI.FilePath = "QxCubePS.hlsl";

        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    LayoutElement LayoutElements[] =
    {
        // vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, false},
        LayoutElement{1, 0, 2, VT_FLOAT32, true}
    };

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElements;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements =
        _countof(LayoutElements);

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] =
    {
        {
            SHADER_TYPE_PIXEL, "g_Texture",
            SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE
        }
    };

    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };

    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {
            SHADER_TYPE_PIXEL,
            "g_Texture",
            SamLinearClampDesc
        }
    };

    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers =
        ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
        _countof(ImtblSamplers);
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    m_pPSO->GetStaticVariableByName(
        SHADER_TYPE_VERTEX,
        "Constants")->Set(m_VSConstants);

    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO_NoCull);
    m_pPSO_NoCull->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
}

void QxResourceUdpate::CreateVertexBuffers()
{
    for (int i = 0; i < _countof(m_CubeVertexBuffer); ++i)
    {
        RefCntAutoPtr<IBuffer>& VertexBuffer  = m_CubeVertexBuffer[i];

        // Create vertex buffer that store cube vertices
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Cuber Vertex Buffer";

        if (i == 0)
        {
            VertBuffDesc.Usage = USAGE_IMMUTABLE;
        } else if (i == 1)
        {
            VertBuffDesc.Usage = USAGE_DEFAULT;
        }
        else
        {
            VertBuffDesc.Usage = USAGE_DYNAMIC;
            VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        }

        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.Size = sizeof(CubeVerts);

        BufferData VBData;
        VBData.pData = CubeVerts;
        VBData.DataSize = sizeof(CubeVerts);
        m_pDevice->CreateBuffer(
            VertBuffDesc,
            i < 2 ? &VBData : nullptr, &VertexBuffer);
    }
}

void QxResourceUdpate::CreateIndexBuffer()
{
    Uint32 Indices[] =
   {
        2,0,1,     2,3,0,
        4,6,5,     4,7,6,
        8,10,9,    8,11,10,
        12,14,13,  12,15,14,
        16,18,17,  16,19,18,
        20,21,22,  20,22,23
    };
//Create index buffer
    BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "Cube index buffer";
    IndBuffDesc.Usage = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size = sizeof(Indices);

    BufferData IBData;
    IBData.pData = Indices;
    IBData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(
        IndBuffDesc, &IBData, &m_CuberIndexBuffer);
    
}

void QxResourceUdpate::LoadTexture()
{
    for (int i = 0; i < m_Textures.size(); ++i)
    {
        TextureLoadInfo loadInfo;
        std::stringstream FileNameSSS;

        FileNameSSS << "DGLogo" << i << ".png";
        auto FileName = FileNameSSS.str();
        loadInfo.IsSRGB = true;
        loadInfo.Usage = USAGE_IMMUTABLE;
        if (i == 2)
        {
            loadInfo.Usage = USAGE_DEFAULT;
            loadInfo.MipLevels = 1;
        } else if (i == 3)
        {
            loadInfo.MipLevels = 1;
            loadInfo.Usage = USAGE_DYNAMIC;
            loadInfo.CPUAccessFlags = CPU_ACCESS_WRITE;
        }

        auto& Tex = m_Textures[i];
        CreateTextureFromFile(FileName.c_str(),
            loadInfo, m_pDevice, &Tex);

        auto TextureSRV =
            Tex->GetDefaultView(TEXTURE_VIEW_SHADING_RATE);

        m_pPSO->CreateShaderResourceBinding(
            &(m_SRBs[i]), true);

        m_SRBs[i]->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_Texture"
            )->Set(TextureSRV);
    }
}

void QxResourceUdpate::DrawCube(const float4x4 WVPMatrix,
    IBuffer* pVertexBuffer,
    IShaderResourceBinding* pSRB)
{
    IBuffer* pBuffers[] = {pVertexBuffer};

    m_pImmediateContext->SetVertexBuffers(
        0, 1,
        pBuffers, nullptr,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        SET_VERTEX_BUFFERS_FLAG_RESET
        );

    m_pImmediateContext->SetIndexBuffer(
        m_CuberIndexBuffer,
        0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_pImmediateContext->CommitShaderResources(
        pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // map buffer 并提交WVP矩阵, dynamic buffer用这种方式提交数据合适
        MapHelper<float4x4> CBConstants(m_pImmediateContext,
            m_VSConstants,
            MAP_WRITE,
            MAP_FLAG_DISCARD);
        *CBConstants = WVPMatrix.Transpose();
        
    }

    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType =
        VT_UINT32;
    DrawAttrs.NumIndices =
        36;
    DrawAttrs.Flags =
        DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}
}
