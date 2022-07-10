
#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
class QxGeometryShader final : public SampleBase
{
public:
    inline void Initialize(const SampleInitInfo& InitInfo) override;
    void        Render() override;
    inline void Update(double CurrTime, double ElapsedTime) override;
    const Char* GetSampleName() const override
    {
        return "Qx Geomety Shader";
    };

    
};
}