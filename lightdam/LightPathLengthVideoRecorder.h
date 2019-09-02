#pragma once

#include <stdint.h>
#include <string>

class LightPathLengthVideoRecorder
{
public:
    struct Settings
    {
        float minLightPathLength = 1.0f;
        float maxLightPathLength = 10.0f;
        uint32_t numFrames = 30;
        uint32_t numIterationsPerFrame = 100;
        bool exitApplicationWhenDone = false;
    };

    void PerIterationUpdate(class Application& application, class PathTracer& pathTracer);

    void StartRecording(const Settings& settings, class Application& application, class PathTracer& pathTracer);
    
    void CancelRecording();
    bool IsRecording() const { return m_isRecording; }

private:
    bool m_isRecording = false;
    uint32_t m_numFramesLeft;
    Settings m_settings;
    std::string m_recordingDirectory;
};

