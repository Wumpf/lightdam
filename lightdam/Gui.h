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

    void UpdateAndDraw(float timeSinceLastFrame, Application& application, const Scene& scene, ControllableCamera& activeCamera, PathTracer& pathTracer, struct ID3D12GraphicsCommandList* commandList);

private:
    void SetupUI(float timeSinceLastFrame, Application& application, const Scene& scene, ControllableCamera& activeCamera, PathTracer& pathTracer);

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
            FixedApproxRenderTime,
        };

        Mode mode = Mode::Continous;
        int fixedIterationCount = 10;
        float fixedApproximateRenderTimeS = 1.0f;
    };

    RenderingConfiguration m_renderConfiguration;
    // timing is rather approximate:
    // Doing D3D12 timer queries is cumbersome and realistically frametime is exactly what we want since cpu frame preparation is not the bottleneck.
    // If it were we would still be interested in CPU time as well since rendering the image does after all take longer ...
    float m_timeSpentRenderingSinceLastReset = 0.0f;
};

