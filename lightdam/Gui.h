#pragma once

#include <wrl/client.h>
#include "LightPathLengthVideoRecorder.h"

using namespace Microsoft::WRL;

class Application;
class Scene;
class ControllableCamera;
class PathTracer;

class Gui
{
public:
    Gui(class Window* window, struct ID3D12Device* device);
    ~Gui();

    void UpdateAndDraw(Application& application, const Scene& scene, ControllableCamera& activeCamera, PathTracer& pathTracer, struct ID3D12GraphicsCommandList* commandList);

private:
    void SetupUI(Application& application, const Scene& scene, ControllableCamera& activeCamera, PathTracer& pathTracer);

    ComPtr<struct ID3D12DescriptorHeap> m_fontDescriptorHeap;

    Window* m_window;
    int m_windowProcHandlerHandle;

    LightPathLengthVideoRecorder m_lightPathVideoRecorder;
    LightPathLengthVideoRecorder::Settings m_lightPathVideoRecordingSettings;


    // todo: Separate controller class?
    struct RenderingConfiguration
    {
        enum class Mode : int
        {
            Continous,
            ManualIteration,
            FixedIterationCount,
            //FixedApproxRenderTime
        };

        Mode mode = Mode::Continous;
        uint32_t fixedIterationCount = 10;
        float fixedApproximateRenderTimeS = 1.0f;
    };

    RenderingConfiguration m_renderConfiguration;
    
};

