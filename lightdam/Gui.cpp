#include "Gui.h"
#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"
#include "Window.h"
#include "dx12/SwapChain.h"
#include "ErrorHandling.h"

#include "Application.h"
#include "Camera.h"
#include "PathTracer.h"
#include "Scene.h"

#include <numeric>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Gui::Gui(Window* window, ID3D12Device* device)
    : m_window(window)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_fontDescriptorHeap)));

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(window->GetHandle()))
        throw new std::exception("Failed ImGUI Win32 init");
    // If our swapchain format changes (HDR?) we need to pass swapchain through here, but that's not in sight.
    if (!ImGui_ImplDX12_Init(device, SwapChain::MaxFramesInFlight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        m_fontDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_fontDescriptorHeap->GetGPUDescriptorHandleForHeapStart()))
        throw new std::exception("Failed ImGUI DX12 init");

    m_windowProcHandlerHandle = window->AddProcHandler(ImGui_ImplWin32_WndProcHandler);
}

Gui::~Gui()
{
    m_window->RemoveProcHandler(m_windowProcHandlerHandle);
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Gui::UpdateAndDraw(float timeSinceLastFrame, Application& application, const Scene& scene, ControllableCamera& activeCamera, PathTracer& pathTracer, ID3D12GraphicsCommandList* commandList)
{
    // Start new frame.
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    m_lightPathVideoRecorder.PerIterationUpdate(application, pathTracer);

    // Queue application render iterations.
    if (m_lightPathVideoRecorder.IsRecording() || m_renderConfiguration.mode == RenderingConfiguration::Mode::Continous ||
        (m_renderConfiguration.mode == RenderingConfiguration::Mode::FixedIterationCount && pathTracer.GetScheduledIterationNumber() < m_renderConfiguration.fixedIterationCount) ||
        (m_renderConfiguration.mode == RenderingConfiguration::Mode::FixedApproxRenderTime && m_timeSpentRenderingSinceLastReset < m_renderConfiguration.fixedApproximateRenderTimeS))
    {
        application.DoSamplingIterationInNextFrame();
        m_timeSpentRenderingSinceLastReset += timeSinceLastFrame; // This is quite inaccurate - misses the time the last frame takes and takes in the time of a frame before we start rendering.
    }

    SetupUI(timeSinceLastFrame, application, scene, activeCamera, pathTracer);

    // write out to command list.
    commandList->SetDescriptorHeaps(1, m_fontDescriptorHeap.GetAddressOf());
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void Gui::SetupUI(float timeSinceLastFrame, Application& application, const Scene& scene, ControllableCamera& activeCamera, PathTracer& pathTracer)
{
    static const auto categoryTextColor = ImVec4(1, 1, 0, 1);

    ImGui::Begin("Lightdam");
    ImGui::Text("%.3f ms/frame (%.1f FPS)", timeSinceLastFrame * 1000.0f, 1.0f / timeSinceLastFrame);
    ImGui::Text("# iterations %i", pathTracer.GetScheduledIterationNumber());
    if (m_renderConfiguration.mode != RenderingConfiguration::Mode::ManualIteration)
        ImGui::Text("Total time rendering %.02fs", m_timeSpentRenderingSinceLastReset);
    ImGui::Text("Resolution: %.0fx%.0f", ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

    if (m_lightPathVideoRecorder.IsRecording())
    {
        if (ImGui::Button("Cancel Recording"))
            m_lightPathVideoRecorder.CancelRecording();
        ImGui::End();
        return;
    }

    if (ImGui::Button("Save Screenshot (pfm)"))
        application.SaveImage(FrameCapture::FileFormat::Pfm);
    if (ImGui::Button("Save Screenshot (bmp)"))
        application.SaveImage(FrameCapture::FileFormat::Bmp);

    if (ImGui::CollapsingHeader("Iterations"))
    {
        const char* renderingModeLabels[] =
        {
            "Continuous",
            "Manual Trigger",
            "Fixed iteration count",
            "Approximate time limit",
        };
        if (ImGui::Combo("Mode", (int*)& m_renderConfiguration.mode, renderingModeLabels, _countof(renderingModeLabels)))
        {
            m_timeSpentRenderingSinceLastReset = 0.0f;
            pathTracer.RestartSampling();
            application.DoSamplingIterationInNextFrame();
        }
        switch (m_renderConfiguration.mode)
        {
        case RenderingConfiguration::Mode::ManualIteration:
            if (ImGui::Button("Render Single Iteration"))
                application.DoSamplingIterationInNextFrame();
            break;
        case RenderingConfiguration::Mode::FixedIterationCount:
            ImGui::DragInt("# Iterations", &m_renderConfiguration.fixedIterationCount, 1, 1, 1000);
            break;
        case RenderingConfiguration::Mode::FixedApproxRenderTime:
            ImGui::DragFloat("Rendering Time", &m_renderConfiguration.fixedApproximateRenderTimeS, 0.1f, 0.1f, 24 * 60 * 60.0f, "%.01f s");
            break;
        }

        if (ImGui::Button("Restart Sampling"))
        {
            m_timeSpentRenderingSinceLastReset = 0.0f;
            pathTracer.RestartSampling();
            application.DoSamplingIterationInNextFrame();
        }
    }
    if (ImGui::CollapsingHeader("PathLength Video Recording"))
    {
        if (ImGui::Button("Start Recording"))
            m_lightPathVideoRecorder.StartRecording(m_lightPathVideoRecordingSettings, scene.GetName(), application, pathTracer);
        ImGui::InputFloat("PathLength Filter Start", &m_lightPathVideoRecordingSettings.minLightPathLength, 0.05f, 0.1f, "%.2f");
        ImGui::InputFloat("PathLength Filter End", &m_lightPathVideoRecordingSettings.maxLightPathLength, 0.05f, 0.1f, "%.2f");
        ImGui::InputScalar("# Iterations per Frame", ImGuiDataType_U32, &m_lightPathVideoRecordingSettings.numIterationsPerFrame);
        ImGui::InputScalar("# Frames", ImGuiDataType_U32, &m_lightPathVideoRecordingSettings.numFrames);
        ImGui::Checkbox("Exit when done", &m_lightPathVideoRecordingSettings.exitApplicationWhenDone);
    }
    if (ImGui::CollapsingHeader("PathTracer"))
    {
        bool pathLengthFilterEnabled = pathTracer.GetPathLengthFilterEnabled();
        if (ImGui::Checkbox("Enable PathLength Filter", &pathLengthFilterEnabled))
            pathTracer.SetPathLengthFilterEnabled(pathLengthFilterEnabled, application);
        if (pathLengthFilterEnabled)
        {
            float pathLengthFilterMax = pathTracer.GetPathLengthFilterMax();
            if (ImGui::DragFloat("PathLength Filter Max", &pathLengthFilterMax, 0.05f, 0.1f, 1000.0f, "%.2f", 4.0f))
                pathTracer.SetPathLengthFilterMax(pathLengthFilterMax);
        }
    }
    if (ImGui::CollapsingHeader("Scene"))
    {
        ImGui::LabelText("Active Scene", "%s", scene.GetFilePath().c_str());
        ImGui::LabelText("Number of Meshes", "%i", scene.GetMeshes().size());
        uint64_t totalTriangleCount = 0;
        for (const auto& mesh : scene.GetMeshes()) totalTriangleCount += mesh.indexCount / 3;
        ImGui::LabelText("Total Triangle Count", "%i", totalTriangleCount);
        for (int i=0; i<scene.GetCameras().size(); ++i)
        {
            if (ImGui::Button((std::string("Reset Camera to Scene Camera ") + std::to_string(i)).c_str()))
                activeCamera = scene.GetCameras()[i];
        }
    }
    if (ImGui::CollapsingHeader("Camera"))
    {
        auto pos = activeCamera.GetPosition();
        if (ImGui::DragFloat3("Position", pos.m128_f32, 0.1f, -100.0f, 100.f))
            activeCamera.SetPosition(pos);

        auto dir = activeCamera.GetDirection();
        if (ImGui::DragFloat3("Direction", dir.m128_f32, 0.1f, -1.0f, 1.0f))
            activeCamera.SetDirection(DirectX::XMVector3Normalize(dir));

        auto up = activeCamera.GetUp();
        if (ImGui::DragFloat3("Up", up.m128_f32, 0.1f, -1.0f, 1.0f))
            activeCamera.SetUp(DirectX::XMVector3Normalize(up));

        float fovRad = activeCamera.GetFovRad();
        if (ImGui::SliderAngle("vFOV", &fovRad, 1.0f, 179.0f))
            activeCamera.SetFovRad(fovRad);

        float speed = activeCamera.GetMoveSpeed();
        if (ImGui::DragFloat("Speed", &speed, 1.0f, 1.0f, 1000.f))
            activeCamera.SetMoveSpeed(speed);
    }

    ImGui::End();
}