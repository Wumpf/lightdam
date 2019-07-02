#include "Gui.h"
#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"
#include "Window.h"
#include "dx12/SwapChain.h"
#include "ErrorHandling.h"
#include "Camera.h"

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
    if (!ImGui_ImplDX12_Init(device, SwapChain::MaxFramesInFlight, DXGI_FORMAT_R8G8B8A8_UNORM,
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

void Gui::Draw(Camera& camera, ID3D12GraphicsCommandList* commandList)
{
    // Start new frame.
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    SetupUI(camera);

    // write out to command list.
    commandList->SetDescriptorHeaps(1, m_fontDescriptorHeap.GetAddressOf());
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void Gui::SetupUI(Camera& camera)
{
    static const auto categoryTextColor = ImVec4(1, 1, 0, 1);

    ImGui::Begin("Lightdam");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    {
        ImGui::TextColored(categoryTextColor, "Camera");
        ImGui::BeginChild("camera");
        auto pos = camera.GetPosition();
        if (ImGui::DragFloat3("Position", pos.m128_f32, 0.1f, -100.0f, 100.f))
            camera.SetPosition(pos);

        auto dir = camera.GetDirection();
        if (ImGui::DragFloat3("Direction", dir.m128_f32, 0.1f, -1.0f, 1.0f))
            camera.SetDirection(DirectX::XMVector3Normalize(dir));

        float fov = camera.GetHFovDegree();
        if (ImGui::DragFloat("vFOV", &fov, 1.0f, 1.0f, 180.f))
            camera.SetVFovDegree(fov);

        ImGui::EndChild();
    }


    ImGui::End();
}