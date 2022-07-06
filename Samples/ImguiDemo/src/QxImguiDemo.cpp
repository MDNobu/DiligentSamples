#include "QxImguiDemo.h"

#include "imgui.h"
#include "ImGuiUtils.hpp"

namespace Diligent
{
void QxImguiDemo::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
}

void QxImguiDemo::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    m_pImmediateContext->ClearRenderTarget(pRTV, &m_ClearColor.x, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}
void QxImguiDemo::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    UpdateUI();
}

void QxImguiDemo::UpdateUI()
{
    if (m_ShowDemoWindow)
    {
        // 窗口内部实现参考 这个函数的内部实现
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }

    // 显示一个我们创建的窗口, 用begin / end pair 来创建一个named window
    {
        ImGui::Begin("Hello, world");

        ImGui::Text("this is test text content");

        ImGui::Checkbox("Demmo window", &m_ShowDemoWindow);
        ImGui::Checkbox("Anothter window", &m_ShowAnotherWindow);

        static float f = 0.f;
        ImGui::SliderFloat("float", &f, 0.f, 1.f);
        ImGui::ColorEdit3("clear color", (float*)&m_ClearColor.x);

        static int counter = 0;
        if (ImGui::Button("Test Button"))
        {
            counter++;
        }
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        
        ImGui::End();
    }

    if (m_ShowAnotherWindow)
    {
        ImGui::Begin("Another window");

        ImGui::Text("hello from another window");
        if (ImGui::Button("Close me"))
        {
            m_ShowAnotherWindow = false;
        }
        
        ImGui::End();
    }
 }
}
