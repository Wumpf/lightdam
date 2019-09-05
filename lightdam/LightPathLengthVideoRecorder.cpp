#include "LightPathLengthVideoRecorder.h"
#include "PathTracer.h"
#include "Application.h"
#include "ErrorHandling.h"

void LightPathLengthVideoRecorder::PerIterationUpdate(Application& application, PathTracer& pathTracer)
{
    if (!m_isRecording)
        return;

    if (m_settings.numIterationsPerFrame > pathTracer.GetScheduledIterationNumber())
        return;


    assert(m_settings.numIterationsPerFrame == pathTracer.GetScheduledIterationNumber());
    --m_numFramesLeft;
    auto frameNumber = m_settings.numFrames - m_numFramesLeft;
    LogPrint(LogLevel::Info, "Finished frame %i/%i", frameNumber, m_settings.numFrames);
    std::string filename = m_recordingDirectory + std::to_string(frameNumber) + ".bmp";
    application.SaveImage(FrameCapture::FileFormat::Bmp, filename.c_str());

    if (m_numFramesLeft == 0)
    {
        m_isRecording = false;
        if (m_settings.exitApplicationWhenDone)
            PostQuitMessage(0);
    }
    else
    {
        float pathLength = (m_settings.maxLightPathLength - m_settings.minLightPathLength) * ((float)frameNumber / m_settings.numFrames) + m_settings.minLightPathLength;
        pathTracer.SetPathLengthFilterMax(pathLength);
    }
}

void LightPathLengthVideoRecorder::StartRecording(const Settings& settings, const std::string& sceneName, Application& application, PathTracer& pathTracer)
{
    m_numFramesLeft = settings.numFrames;
    m_settings = settings;

    application.SetRenderingMode(Application::RenderingMode::ProgressiveContinous);
    pathTracer.SetPathLengthFilterEnabled(true, application);
    pathTracer.SetPathLengthFilterMax(m_settings.minLightPathLength);

    const char* recordingsFolder = "LightPathLengthVideos/";
    CreateDirectoryA(recordingsFolder, nullptr);

    struct stat info;
    int recordingIndex = 0;
    do
    {
        m_recordingDirectory = recordingsFolder + sceneName + "_" + std::to_string(recordingIndex) + "/";
        ++recordingIndex;
    } while (stat(m_recordingDirectory.c_str(), &info) == 0);
    
    if (!CreateDirectoryA(m_recordingDirectory.c_str(), nullptr))
    {
        LogPrint(LogLevel::Failure, "Failed to create directory \"%s\"", m_recordingDirectory.c_str());
        return;
    }

    // TODO Write a text file with our settings for convenience so it's easy to see the properties of a recording after the fact.

    LogPrint(LogLevel::Info, "Stared Light Path Video Recording");
    m_isRecording = true;
}

void LightPathLengthVideoRecorder::CancelRecording()
{
    m_isRecording = false;
    LogPrint(LogLevel::Info, "Canceled Light Path Video Recording");
}