#include "QxTexturing.h"

namespace Diligent
{


void QxTexturing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
}

void QxTexturing::Render()
{

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

} // namespace Diligent