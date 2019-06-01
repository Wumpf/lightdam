#pragma once

#include <wrl/client.h>

using namespace Microsoft::WRL;

class Gui
{
public:
    Gui(class Window* window, struct ID3D12Device* device);
    ~Gui();

    void Draw(struct ID3D12GraphicsCommandList* commandList);

private:
    void SetupUI();

    ComPtr<struct ID3D12DescriptorHeap> m_fontDescriptorHeap;

    Window* m_window;
    int m_windowProcHandlerHandle;
};

