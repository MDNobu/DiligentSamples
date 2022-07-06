#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
class QxImguiDemo : public SampleBase
{
public:
    void        Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    
     void        Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "Qx Imgui";
    };
private:
    void         UpdateUI();
private:

    bool m_ShowDemoWindow = true;
    bool m_ShowAnotherWindow = false;

    float4 m_ClearColor = {0.45f, 0.55f, 0.60f, 1.00f};
};
}
