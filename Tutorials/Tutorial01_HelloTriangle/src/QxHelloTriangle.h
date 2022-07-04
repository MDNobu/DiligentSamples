
#pragma once

#include "SampleBase.hpp"

namespace Diligent
{
class QxHelloTriangle final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Qx Hello Triangle"; }

private:
    //RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IPipelineState> m_pPSO;
}
;

} // namespace Diligent
