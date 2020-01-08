/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
*
*  In no event and under no legal theory, whether in tort (including negligence),
*  contract, or otherwise, unless required by applicable law (such as deliberate
*  and grossly negligent acts) or agreed to in writing, shall any Contributor be
*  liable for any damages, including any direct, indirect, special, incidental,
*  or consequential damages of any character arising as a result of this License or
*  out of the use or inability to use the software (including but not limited to damages
*  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
*  all other commercial damages or losses), even if such Contributor has been advised
*  of the possibility of such damages.
*/

#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cmath>

#include "PlatformDefinitions.h"
#include "SampleApp.h"
#include "Errors.h"
#include "StringTools.h"
#include "MapHelper.h"
#include "Image.h"
#include "FileWrapper.h"

#if D3D11_SUPPORTED
#    include "EngineFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#    include "EngineFactoryD3D12.h"
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
#    include "EngineFactoryOpenGL.h"
#endif

#if VULKAN_SUPPORTED
#    include "EngineFactoryVk.h"
#endif

#if METAL_SUPPORTED
#    include "EngineFactoryMtl.h"
#endif

#include "imgui.h"
#include "ImGuiImplDiligent.h"

namespace Diligent
{

SampleApp::SampleApp() :
    m_TheSample{CreateSample()},
    m_AppTitle{m_TheSample->GetSampleName()}
{
}

SampleApp::~SampleApp()
{
    m_pImGui.reset();
    m_TheSample.reset();

    if (m_pImmediateContext)
        m_pImmediateContext->Flush();
    m_pDeferredContexts.clear();
    m_pImmediateContext.Release();
    m_pSwapChain.Release();
    m_pDevice.Release();
}


void SampleApp::InitializeDiligentEngine(
#if PLATFORM_LINUX
    void* display,
#endif
    void* NativeWindowHandle)
{
    SwapChainDesc SCDesc;
    if (m_ScreenCaptureInfo.AllowCapture)
        SCDesc.Usage |= SWAP_CHAIN_USAGE_COPY_SOURCE;

#if PLATFORM_MACOS
    // We need at least 3 buffers on Metal to avoid massive
    // peformance degradation in full screen mode.
    // https://github.com/KhronosGroup/MoltenVK/issues/808
    SCDesc.BufferCount = 3;
#endif

    std::vector<IDeviceContext*> ppContexts;
    switch (m_DeviceType)
    {
#if D3D11_SUPPORTED
        case DeviceType::D3D11:
        {
            EngineD3D11CreateInfo EngineCI;

#    ifdef DEVELOPMENT
            EngineCI.DebugFlags |=
                D3D11_DEBUG_FLAG_CREATE_DEBUG_DEVICE |
                D3D11_DEBUG_FLAG_VERIFY_COMMITTED_SHADER_RESOURCES;
#    endif
#    ifdef _DEBUG
            EngineCI.DebugFlags |= D3D11_DEBUG_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE;
#    endif

            if (m_ValidationLevel >= 1)
            {
                EngineCI.DebugFlags =
                    D3D11_DEBUG_FLAG_CREATE_DEBUG_DEVICE |
                    D3D11_DEBUG_FLAG_VERIFY_COMMITTED_SHADER_RESOURCES |
                    D3D11_DEBUG_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE;
            }
            else if (m_ValidationLevel == 0)
            {
                EngineCI.DebugFlags = D3D11_DEBUG_FLAG_NONE;
            }

            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, EngineCI, SCDesc);

#    if ENGINE_DLL
            GetEngineFactoryD3D11Type GetEngineFactoryD3D11 = nullptr;
            // Load the dll and import GetEngineFactoryD3D11() function
            LoadGraphicsEngineD3D11(GetEngineFactoryD3D11);
#    endif
            auto* pFactoryD3D11 = GetEngineFactoryD3D11();
            m_pEngineFactory    = pFactoryD3D11;
            Uint32 NumAdapters  = 0;
            pFactoryD3D11->EnumerateAdapters(EngineCI.MinimumFeatureLevel, NumAdapters, 0);
            std::vector<AdapterAttribs> Adapters(NumAdapters);
            if (NumAdapters > 0)
            {
                pFactoryD3D11->EnumerateAdapters(EngineCI.MinimumFeatureLevel, NumAdapters, Adapters.data());
            }
            else
            {
                LOG_ERROR_AND_THROW("Failed to find Direct3D11-compatible hardware adapters");
            }

            if (m_AdapterType == ADAPTER_TYPE_SOFTWARE)
            {
                for (Uint32 i = 0; i < Adapters.size(); ++i)
                {
                    if (Adapters[i].AdapterType == m_AdapterType)
                    {
                        m_AdapterId = i;
                        LOG_INFO_MESSAGE("Found software adapter '", Adapters[i].Description, "'");
                        break;
                    }
                }
            }

            m_AdapterAttribs = Adapters[m_AdapterId];
            if (m_AdapterType != ADAPTER_TYPE_SOFTWARE)
            {
                Uint32 NumDisplayModes = 0;
                pFactoryD3D11->EnumerateDisplayModes(EngineCI.MinimumFeatureLevel, m_AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, nullptr);
                m_DisplayModes.resize(NumDisplayModes);
                pFactoryD3D11->EnumerateDisplayModes(EngineCI.MinimumFeatureLevel, m_AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, m_DisplayModes.data());
            }

            EngineCI.AdapterId = m_AdapterId;
            ppContexts.resize(1 + EngineCI.NumDeferredContexts);
            pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, ppContexts.data());

            if (NativeWindowHandle != nullptr)
                pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, ppContexts[0], SCDesc, FullScreenModeDesc{}, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

#if D3D12_SUPPORTED
        case DeviceType::D3D12:
        {
            EngineD3D12CreateInfo EngineCI;

#    ifdef DEVELOPMENT
            EngineCI.EnableDebugLayer = true;
#    endif
            if (m_ValidationLevel >= 1)
            {
                EngineCI.EnableDebugLayer = true;
                if (m_ValidationLevel >= 2)
                    EngineCI.EnableGPUBasedValidation = true;
            }
            else if (m_ValidationLevel == 0)
            {
                EngineCI.EnableDebugLayer = false;
            }

            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, EngineCI, SCDesc);

#    if ENGINE_DLL
            GetEngineFactoryD3D12Type GetEngineFactoryD3D12 = nullptr;
            // Load the dll and import GetEngineFactoryD3D12() function
            LoadGraphicsEngineD3D12(GetEngineFactoryD3D12);
#    endif
            auto* pFactoryD3D12 = GetEngineFactoryD3D12();
            if (!pFactoryD3D12->LoadD3D12())
            {
                LOG_ERROR_AND_THROW("Failed to load Direct3D12");
            }

            m_pEngineFactory   = pFactoryD3D12;
            Uint32 NumAdapters = 0;
            pFactoryD3D12->EnumerateAdapters(EngineCI.MinimumFeatureLevel, NumAdapters, 0);
            std::vector<AdapterAttribs> Adapters(NumAdapters);
            if (NumAdapters > 0)
            {
                pFactoryD3D12->EnumerateAdapters(EngineCI.MinimumFeatureLevel, NumAdapters, Adapters.data());
            }
            else
            {
#    if D3D11_SUPPORTED
                LOG_ERROR_MESSAGE("Failed to find Direct3D12-compatible hardware adapters. Attempting to initialize the engine in Direct3D11 mode.");
                m_DeviceType = DeviceType::D3D11;
                InitializeDiligentEngine(NativeWindowHandle);
                return;
#    else
                LOG_ERROR_AND_THROW("Failed to find Direct3D12-compatible hardware adapters.");
#    endif
            }

            if (m_AdapterType == ADAPTER_TYPE_SOFTWARE)
            {
                for (Uint32 i = 0; i < Adapters.size(); ++i)
                {
                    if (Adapters[i].AdapterType == m_AdapterType)
                    {
                        m_AdapterId = i;
                        LOG_INFO_MESSAGE("Found software adapter '", Adapters[i].Description, "'");
                        break;
                    }
                }
            }

            m_AdapterAttribs = Adapters[m_AdapterId];
            if (m_AdapterType != ADAPTER_TYPE_SOFTWARE)
            {
                Uint32 NumDisplayModes = 0;
                pFactoryD3D12->EnumerateDisplayModes(EngineCI.MinimumFeatureLevel, m_AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, nullptr);
                m_DisplayModes.resize(NumDisplayModes);
                pFactoryD3D12->EnumerateDisplayModes(EngineCI.MinimumFeatureLevel, m_AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, m_DisplayModes.data());
            }

            EngineCI.AdapterId = m_AdapterId;
            ppContexts.resize(1 + EngineCI.NumDeferredContexts);
            pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, ppContexts.data());

            if (!m_pSwapChain && NativeWindowHandle != nullptr)
                pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, ppContexts[0], SCDesc, FullScreenModeDesc{}, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case DeviceType::OpenGL:
        case DeviceType::OpenGLES:
        {
#    if !PLATFORM_MACOS
            VERIFY_EXPR(NativeWindowHandle != nullptr);
#    endif
#    if EXPLICITLY_LOAD_ENGINE_GL_DLL
            // Declare function pointer
            GetEngineFactoryOpenGLType GetEngineFactoryOpenGL = nullptr;
            // Load the dll and import GetEngineFactoryOpenGL() function
            LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGL);
#    endif
            auto* pFactoryOpenGL = GetEngineFactoryOpenGL();
            m_pEngineFactory     = pFactoryOpenGL;
            EngineGLCreateInfo CreationAttribs;
            CreationAttribs.pNativeWndHandle = NativeWindowHandle;
#    if PLATFORM_LINUX
            CreationAttribs.pDisplay = display;
#    endif
            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, CreationAttribs, SCDesc);
            if (CreationAttribs.NumDeferredContexts != 0)
            {
                LOG_ERROR_MESSAGE("Deferred contexts are not supported in OpenGL mode");
                CreationAttribs.NumDeferredContexts = 0;
            }
            ppContexts.resize(1 + CreationAttribs.NumDeferredContexts);
            pFactoryOpenGL->CreateDeviceAndSwapChainGL(
                CreationAttribs, &m_pDevice, ppContexts.data(), SCDesc, &m_pSwapChain);
        }
        break;
#endif

#if VULKAN_SUPPORTED
        case DeviceType::Vulkan:
        {
#    if EXPLICITLY_LOAD_ENGINE_VK_DLL
            GetEngineFactoryVkType GetEngineFactoryVk = nullptr;
            // Load the dll and import GetEngineFactoryVk() function
            LoadGraphicsEngineVk(GetEngineFactoryVk);
#    endif
            EngineVkCreateInfo EngVkAttribs;
#    ifdef DEVELOPMENT
            EngVkAttribs.EnableValidation = true;
#    endif
            if (m_ValidationLevel >= 1)
            {
                EngVkAttribs.EnableValidation = true;
            }
            else if (m_ValidationLevel == 0)
            {
                EngVkAttribs.EnableValidation = false;
            }

            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, EngVkAttribs, SCDesc);
            ppContexts.resize(1 + EngVkAttribs.NumDeferredContexts);
            auto* pFactoryVk = GetEngineFactoryVk();
            m_pEngineFactory = pFactoryVk;
            pFactoryVk->CreateDeviceAndContextsVk(EngVkAttribs, &m_pDevice, ppContexts.data());
            if (!m_pDevice)
            {
                LOG_ERROR_AND_THROW("Failed to initialize Vulkan.");
            }
            if (!m_pSwapChain && NativeWindowHandle != nullptr)
                pFactoryVk->CreateSwapChainVk(m_pDevice, ppContexts[0], SCDesc, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif


#if METAL_SUPPORTED
        case DeviceType::Metal:
        {
            EngineMtlCreateInfo MtlAttribs;

            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, MtlAttribs, SCDesc);
            ppContexts.resize(1 + MtlAttribs.NumDeferredContexts);
            auto* pFactoryMtl = GetEngineFactoryMtl();
            pFactoryMtl->CreateDeviceAndContextsMtl(MtlAttribs, &m_pDevice, ppContexts.data());

            if (!m_pSwapChain && NativeWindowHandle != nullptr)
                pFactoryMtl->CreateSwapChainMtl(m_pDevice, ppContexts[0], SCDesc, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

        default:
            LOG_ERROR_AND_THROW("Unknown device type");
            break;
    }

    switch (m_DeviceType)
    {
        // clang-format off
        case DeviceType::D3D11:    m_AppTitle.append(" (D3D11)");    break;
        case DeviceType::D3D12:    m_AppTitle.append(" (D3D12)");    break;
        case DeviceType::OpenGL:   m_AppTitle.append(" (OpenGL)");   break;
        case DeviceType::OpenGLES: m_AppTitle.append(" (OpenGLES)"); break;
        case DeviceType::Vulkan:   m_AppTitle.append(" (Vulkan)");   break;
        case DeviceType::Metal:    m_AppTitle.append(" (Metal)");    break;
        default: UNEXPECTED("Unknown/unsupported device type");
            // clang-format on
    }

    m_pImmediateContext.Attach(ppContexts[0]);
    auto NumDeferredCtx = ppContexts.size() - 1;
    m_pDeferredContexts.resize(NumDeferredCtx);
    for (Uint32 ctx = 0; ctx < NumDeferredCtx; ++ctx)
        m_pDeferredContexts[ctx].Attach(ppContexts[1 + ctx]);

    if (m_ScreenCaptureInfo.AllowCapture)
    {
        if (m_GoldenImgMode == GoldenImageMode::Capture || m_GoldenImgMode == GoldenImageMode::Compare)
        {
            // Capture only one frame
            m_ScreenCaptureInfo.FramesToCapture = 1;
        }

        m_pScreenCapture.reset(new ScreenCapture(m_pDevice));
    }
}

void SampleApp::InitializeSample()
{
#if PLATFORM_WIN32
    if (!m_DisplayModes.empty())
    {
        const HWND hDesktop = GetDesktopWindow();

        RECT rc;
        GetWindowRect(hDesktop, &rc);
        Uint32 ScreenWidth  = static_cast<Uint32>(rc.right - rc.left);
        Uint32 ScreenHeight = static_cast<Uint32>(rc.bottom - rc.top);
        for (int i = 0; i < static_cast<int>(m_DisplayModes.size()); ++i)
        {
            if (ScreenWidth == m_DisplayModes[i].Width && ScreenHeight == m_DisplayModes[i].Height)
            {
                m_SelectedDisplayMode = i;
                break;
            }
        }
    }
#endif

    //auto UIScale = m_TheSample->GetUIScale();
    //if(UIScale != 1)
    //{
    //    std::stringstream fontscaling;
    //    fontscaling << " GLOBAL fontscaling=" << UIScale;
    //    TwDefine(fontscaling.str().c_str());
    //}

    const auto& SCDesc = m_pSwapChain->GetDesc();

    std::vector<IDeviceContext*> ppContexts(1 + m_pDeferredContexts.size());
    ppContexts[0]         = m_pImmediateContext;
    Uint32 NumDeferredCtx = static_cast<Uint32>(m_pDeferredContexts.size());
    for (size_t ctx = 0; ctx < m_pDeferredContexts.size(); ++ctx)
        ppContexts[1 + ctx] = m_pDeferredContexts[ctx];
    m_TheSample->Initialize(m_pEngineFactory, m_pDevice, ppContexts.data(), NumDeferredCtx, m_pSwapChain);

    m_TheSample->WindowResize(SCDesc.Width, SCDesc.Height);
}

void SampleApp::UpdateAdaptersDialog()
{
#if PLATFORM_WIN32
    if (m_DeviceType == DeviceType::D3D11 || m_DeviceType == DeviceType::D3D12)
    {
        const auto& SCDesc = m_pSwapChain->GetDesc();

        Uint32 AdaptersWndWidth = std::min(330u, SCDesc.Width);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(AdaptersWndWidth), 0), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(std::max(SCDesc.Width - AdaptersWndWidth, 10U) - 10), 10), ImGuiCond_Always);
        ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Adapters", nullptr, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextDisabled("Adapter: %s (%d MB)", m_AdapterAttribs.Description, m_AdapterAttribs.DedicatedVideoMemory >> 20);

            if (!m_DisplayModes.empty())
            {
                std::vector<const char*> DisplayModes(m_DisplayModes.size());
                std::vector<std::string> DisplayModeStrings(m_DisplayModes.size());
                for (int i = 0; i < static_cast<int>(m_DisplayModes.size()); ++i)
                {
                    static constexpr const char* ScalingModeStr[] =
                        {
                            ""
                            " Centered",
                            " Stretched" //
                        };
                    const auto& Mode = m_DisplayModes[i];

                    std::stringstream ss;

                    float RefreshRate = static_cast<float>(Mode.RefreshRateNumerator) / static_cast<float>(Mode.RefreshRateDenominator);
                    ss << Mode.Width << "x" << Mode.Height << "@" << std::fixed << std::setprecision(2) << RefreshRate << " Hz" << ScalingModeStr[static_cast<int>(Mode.Scaling)];
                    DisplayModeStrings[i] = ss.str();
                    DisplayModes[i]       = DisplayModeStrings[i].c_str();
                }

                ImGui::SetNextItemWidth(220);
                ImGui::Combo("Display Modes", &m_SelectedDisplayMode, DisplayModes.data(), static_cast<int>(DisplayModes.size()));
            }

            if (m_bFullScreenMode)
            {
                if (ImGui::Button("Go Windowed"))
                {
                    SetWindowedMode();
                }
            }
            else
            {
                if (!m_DisplayModes.empty())
                {
                    if (ImGui::Button("Go Full Screen"))
                    {
                        const auto& SelectedMode = m_DisplayModes[m_SelectedDisplayMode];
                        SetFullscreenMode(SelectedMode);
                    }
                }
            }

            ImGui::Checkbox("VSync", &m_bVSync);
        }
        ImGui::End();
    }
#endif
}


std::string GetArgument(const char*& pos, const char* ArgName)
{
    size_t      ArgNameLen = 0;
    const char* delimeters = " \n\r";
    while (pos[ArgNameLen] != 0 && strchr(delimeters, pos[ArgNameLen]) == nullptr)
        ++ArgNameLen;

    if (StrCmpNoCase(pos, ArgName, ArgNameLen) == 0)
    {
        pos += ArgNameLen;
        while (*pos != 0 && strchr(delimeters, *pos) != nullptr)
            ++pos;
        std::string Arg;
        while (*pos != 0 && strchr(delimeters, *pos) == nullptr)
            Arg.push_back(*(pos++));
        return Arg;
    }
    else
    {
        return std::string{};
    }
}

// Command line example to capture frames:
//
//     -mode d3d11 -adapters_dialog 0 -capture_path . -capture_fps 15 -capture_name frame -width 640 -height 480 -capture_format jpg -capture_quality 100 -capture_frames 3 -capture_alpha 0
//
// Image magick command to create animated gif:
//
//     magick convert  -delay 6  -loop 0 -layers Optimize -compress LZW -strip -resize 240x180   frame*.png   Animation.gif
//
void SampleApp::ProcessCommandLine(const char* CmdLine)
{
    const auto* pos = strchr(CmdLine, '-');
    while (pos != nullptr)
    {
        ++pos;
        std::string Arg;
        if (!(Arg = GetArgument(pos, "mode")).empty())
        {
            if (StrCmpNoCase(Arg.c_str(), "D3D11", Arg.length()) == 0)
            {
#if D3D11_SUPPORTED
                m_DeviceType = DeviceType::D3D11;
#else
                LOG_ERROR_MESSAGE("Direct3D11 is not supported. Please select another device type");
#endif
            }
            else if (StrCmpNoCase(Arg.c_str(), "D3D12", Arg.length()) == 0)
            {
#if D3D12_SUPPORTED
                m_DeviceType = DeviceType::D3D12;
#else
                LOG_ERROR_MESSAGE("Direct3D12 is not supported. Please select another device type");
#endif
            }
            else if (StrCmpNoCase(Arg.c_str(), "GL", Arg.length()) == 0)
            {
#if GL_SUPPORTED || GLES_SUPPORTED
                m_DeviceType = DeviceType::OpenGL;
#else
                LOG_ERROR_MESSAGE("OpenGL is not supported. Please select another device type");
#endif
            }
            else if (StrCmpNoCase(Arg.c_str(), "VK", Arg.length()) == 0)
            {
#if VULKAN_SUPPORTED
                m_DeviceType = DeviceType::Vulkan;
#else
                LOG_ERROR_MESSAGE("Vulkan is not supported. Please select another device type");
#endif
            }
            else
            {
                LOG_ERROR_MESSAGE("Unknown device type: '", pos, "'. Only the following types are supported: D3D11, D3D12, GL, VK");
            }
        }
        else if (!(Arg = GetArgument(pos, "capture_path")).empty())
        {
            m_ScreenCaptureInfo.Directory    = std::move(Arg);
            m_ScreenCaptureInfo.AllowCapture = true;
        }
        else if (!(Arg = GetArgument(pos, "capture_name")).empty())
        {
            m_ScreenCaptureInfo.FileName     = std::move(Arg);
            m_ScreenCaptureInfo.AllowCapture = true;
        }
        else if (!(Arg = GetArgument(pos, "capture_fps")).empty())
        {
            m_ScreenCaptureInfo.CaptureFPS = atof(Arg.c_str());
        }
        else if (!(Arg = GetArgument(pos, "capture_frames")).empty())
        {
            m_ScreenCaptureInfo.FramesToCapture = atoi(Arg.c_str());
        }
        else if (!(Arg = GetArgument(pos, "capture_format")).empty())
        {
            if (StrCmpNoCase(Arg.c_str(), "jpeg", Arg.length()) == 0 || StrCmpNoCase(Arg.c_str(), "jpg", Arg.length()) == 0)
            {
                m_ScreenCaptureInfo.FileFormat = EImageFileFormat::jpeg;
            }
            else if (StrCmpNoCase(Arg.c_str(), "png", Arg.length()) == 0)
            {
                m_ScreenCaptureInfo.FileFormat = EImageFileFormat::png;
            }
            else
            {
                LOG_ERROR_MESSAGE("Unknown capture format. The following are allowed values: 'jpeg', 'jpg', 'png'");
            }
        }
        else if (!(Arg = GetArgument(pos, "capture_quality")).empty())
        {
            m_ScreenCaptureInfo.JpegQuality = atoi(Arg.c_str());
        }
        else if (!(Arg = GetArgument(pos, "capture_alpha")).empty())
        {
            m_ScreenCaptureInfo.KeepAlpha = (StrCmpNoCase(Arg.c_str(), "true", Arg.length()) == 0) || Arg == "1";
        }
        else if (!(Arg = GetArgument(pos, "width")).empty())
        {
            m_InitialWindowWidth = atoi(Arg.c_str());
        }
        else if (!(Arg = GetArgument(pos, "height")).empty())
        {
            m_InitialWindowHeight = atoi(Arg.c_str());
        }
        else if (!(Arg = GetArgument(pos, "validation")).empty())
        {
            m_ValidationLevel = atoi(Arg.c_str());
        }
        else if (!(Arg = GetArgument(pos, "adapter")).empty())
        {
            if (StrCmpNoCase(Arg.c_str(), "sw", Arg.length()) == 0)
            {
                m_AdapterType = ADAPTER_TYPE_SOFTWARE;
            }
            else
            {
                auto AdapterId = atoi(Arg.c_str());
                VERIFY_EXPR(AdapterId >= 0);
                m_AdapterId = static_cast<Uint32>(AdapterId >= 0 ? AdapterId : 0);
            }
        }
        else if (!(Arg = GetArgument(pos, "adapters_dialog")).empty())
        {
            m_bShowAdaptersDialog = (StrCmpNoCase(Arg.c_str(), "true", Arg.length()) == 0) || Arg == "1";
        }
        else if (!(Arg = GetArgument(pos, "show_ui")).empty())
        {
            m_bShowUI = (StrCmpNoCase(Arg.c_str(), "true", Arg.length()) == 0) || Arg == "1";
        }
        else if (!(Arg = GetArgument(pos, "golden_image_mode")).empty())
        {
            if (StrCmpNoCase(Arg.c_str(), "none", Arg.length()) == 0)
            {
                m_GoldenImgMode = GoldenImageMode::None;
            }
            else if (StrCmpNoCase(Arg.c_str(), "capture", Arg.length()) == 0)
            {
                m_GoldenImgMode = GoldenImageMode::Capture;
            }
            else if (StrCmpNoCase(Arg.c_str(), "compare", Arg.length()) == 0)
            {
                m_GoldenImgMode = GoldenImageMode::Compare;
            }
            else
            {
                LOG_ERROR_MESSAGE("Unknown golden image mode. The following are allowed values: 'none', 'capture', 'compare'");
            }
        }
        else if (!(Arg = GetArgument(pos, "golden_image_tolerance")).empty())
        {
            m_GoldenImgPixelTolerance = atoi(Arg.c_str());
        }

        pos = strchr(pos, '-');
    }

    if (m_DeviceType == DeviceType::Undefined)
    {
        SelectDeviceType();
        if (m_DeviceType == DeviceType::Undefined)
        {
#if D3D12_SUPPORTED
            m_DeviceType = DeviceType::D3D12;
#elif VULKAN_SUPPORTED
            m_DeviceType = DeviceType::Vulkan;
#elif D3D11_SUPPORTED
            m_DeviceType = DeviceType::D3D11;
#elif GL_SUPPORTED || GLES_SUPPORTED
            m_DeviceType = DeviceType::OpenGL;
#endif
        }
    }

    m_TheSample->ProcessCommandLine(CmdLine);
}

void SampleApp::WindowResize(int width, int height)
{
    if (m_pSwapChain)
    {
        m_pSwapChain->Resize(width, height);
        auto SCWidth  = m_pSwapChain->GetDesc().Width;
        auto SCHeight = m_pSwapChain->GetDesc().Height;
        m_TheSample->WindowResize(SCWidth, SCHeight);
    }
}

void SampleApp::Update(double CurrTime, double ElapsedTime)
{
    m_CurrentTime = CurrTime;

    if (m_pImGui)
    {
        m_pImGui->NewFrame();
        if (m_bShowAdaptersDialog)
        {
            UpdateAdaptersDialog();
        }
    }
    if (m_pDevice)
    {
        m_TheSample->Update(CurrTime, ElapsedTime);
        m_TheSample->GetInputController().ClearState();
    }
}

void SampleApp::Render()
{
    if (!m_pImmediateContext)
        return;

    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_TheSample->Render();

    // Restore default render target in case the sample has changed it
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (m_pImGui)
    {
        if (m_bShowUI)
        {
            // No need to call EndFrame as ImGui::Render calls it automatically
            m_pImGui->Render(m_pImmediateContext);
        }
        else
        {
            m_pImGui->EndFrame();
        }
    }
}

void SampleApp::CompareGoldenImage(const std::string& FileName, ScreenCapture::CaptureInfo& Capture)
{
    RefCntAutoPtr<Image> pGoldenImg;
    CreateImageFromFile(FileName.c_str(), &pGoldenImg, nullptr);
    if (!pGoldenImg)
    {
        LOG_ERROR_MESSAGE("Failed to load golden image from file ", FileName);
        m_ExitCode = -1;
        return;
    }

    const auto& TexDesc       = Capture.pTexture->GetDesc();
    const auto& GoldenImgDesc = pGoldenImg->GetDesc();
    if (GoldenImgDesc.Width != TexDesc.Width)
    {
        LOG_ERROR_MESSAGE("Golden image width (", GoldenImgDesc.Width, ") does not match the captured image width (", TexDesc.Width, ")");
        m_ExitCode = -1;
        return;
    }
    if (GoldenImgDesc.Height != TexDesc.Height)
    {
        LOG_ERROR_MESSAGE("Golden image height (", GoldenImgDesc.Height, ") does not match the captured image height (", TexDesc.Height, ")");
        m_ExitCode = -1;
        return;
    }

    MappedTextureSubresource TexData;
    m_pImmediateContext->MapTextureSubresource(Capture.pTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, TexData);
    auto CapturedPixels = Image::ConvertImageData(TexDesc.Width, TexDesc.Height,
                                                  reinterpret_cast<const Uint8*>(TexData.pData), TexData.Stride,
                                                  TexDesc.Format, TEX_FORMAT_RGBA8_UNORM, false /*Keep alpha*/);
    m_pImmediateContext->UnmapTextureSubresource(Capture.pTexture, 0, 0);
    m_pScreenCapture->RecycleStagingTexture(std::move(Capture.pTexture));

    auto* pGoldenImgPixels = reinterpret_cast<const Uint8*>(pGoldenImg->GetData()->GetDataPtr());

    m_ExitCode = 0;
    for (Uint32 row = 0; row < TexDesc.Height; ++row)
    {
        for (Uint32 col = 0; col < TexDesc.Width; ++col)
        {
            const auto* SrcPixel = &CapturedPixels[(col + row * TexDesc.Width) * 3];
            const auto* DstPixel = pGoldenImgPixels + row * GoldenImgDesc.RowStride + col * GoldenImgDesc.NumComponents;
            if (std::abs(int{SrcPixel[0]} - int{DstPixel[0]}) > m_GoldenImgPixelTolerance ||
                std::abs(int{SrcPixel[1]} - int{DstPixel[1]}) > m_GoldenImgPixelTolerance ||
                std::abs(int{SrcPixel[2]} - int{DstPixel[2]}) > m_GoldenImgPixelTolerance)
                ++m_ExitCode;
        }
    }
}

void SampleApp::SaveScreenCapture(const std::string& FileName, ScreenCapture::CaptureInfo& Capture)
{
    MappedTextureSubresource TexData;
    m_pImmediateContext->MapTextureSubresource(Capture.pTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, TexData);
    const auto& TexDesc = Capture.pTexture->GetDesc();

    Image::EncodeInfo Info;
    Info.Width       = TexDesc.Width;
    Info.Height      = TexDesc.Height;
    Info.TexFormat   = TexDesc.Format;
    Info.KeepAlpha   = m_ScreenCaptureInfo.KeepAlpha;
    Info.pData       = TexData.pData;
    Info.Stride      = TexData.Stride;
    Info.FileFormat  = m_ScreenCaptureInfo.FileFormat;
    Info.JpegQuality = m_ScreenCaptureInfo.JpegQuality;

    RefCntAutoPtr<IDataBlob> pEncodedImage;
    Image::Encode(Info, &pEncodedImage);
    m_pImmediateContext->UnmapTextureSubresource(Capture.pTexture, 0, 0);
    m_pScreenCapture->RecycleStagingTexture(std::move(Capture.pTexture));

    FileWrapper pFile(FileName.c_str(), EFileAccessMode::Overwrite);
    if (pFile)
    {
        auto res = pFile->Write(pEncodedImage->GetDataPtr(), pEncodedImage->GetSize());
        if (!res)
        {
            LOG_ERROR_MESSAGE("Failed to write screen capture file '", FileName, "'.");
        }
        pFile.Close();
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create screen capture file '", FileName, "'. Verify that the directory exists and the app has sufficient rights to write to this directory.");
    }
}

void SampleApp::Present()
{
    if (!m_pSwapChain)
        return;

    if (m_pScreenCapture && m_ScreenCaptureInfo.FramesToCapture > 0)
    {
        if (m_CurrentTime - m_ScreenCaptureInfo.LastCaptureTime >= 1.0 / m_ScreenCaptureInfo.CaptureFPS)
        {
            m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
            m_pScreenCapture->Capture(m_pSwapChain, m_pImmediateContext, m_ScreenCaptureInfo.CurrentFrame);

            m_ScreenCaptureInfo.LastCaptureTime = m_CurrentTime;

            --m_ScreenCaptureInfo.FramesToCapture;
            ++m_ScreenCaptureInfo.CurrentFrame;

            if (m_GoldenImgMode != GoldenImageMode::None)
            {
                VERIFY(m_ScreenCaptureInfo.FramesToCapture == 0, "Only single frame is expected to be captured in golden image capture/comparison modes");
                // Idle the context to make the capture available
                m_pImmediateContext->WaitForIdle();
                if (!m_pScreenCapture->HasCapture())
                {
                    LOG_ERROR_MESSAGE("Screen capture is not available after idling the context");
                    m_ExitCode = -1;
                }
            }
        }
    }

    m_pSwapChain->Present(m_bVSync ? 1 : 0);

    if (m_pScreenCapture)
    {
        while (auto Capture = m_pScreenCapture->GetCapture())
        {
            std::string FileName;
            {
                std::stringstream FileNameSS;
                if (!m_ScreenCaptureInfo.Directory.empty())
                {
                    FileNameSS << m_ScreenCaptureInfo.Directory;
                    if (m_ScreenCaptureInfo.Directory.back() != '/')
                        FileNameSS << '/';
                }
                FileNameSS << m_ScreenCaptureInfo.FileName;
                if (m_GoldenImgMode == GoldenImageMode::None)
                {
                    FileNameSS << std::setw(3) << std::setfill('0') << Capture.Id;
                }
                FileNameSS << (m_ScreenCaptureInfo.FileFormat == EImageFileFormat::jpeg ? ".jpg" : ".png");
                FileName = FileNameSS.str();
            }

            if (m_GoldenImgMode == GoldenImageMode::Compare)
            {
                CompareGoldenImage(FileName, Capture);
            }
            else
            {
                SaveScreenCapture(FileName, Capture);
            }
        }
    }
}

} // namespace Diligent
