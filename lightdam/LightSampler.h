#pragma once

#include "Scene.h"

class LightSampler
{
public:
    struct LightSample
    {
        DirectX::SimpleMath::Vector3 position;
        float _padding0;
        DirectX::SimpleMath::Vector3 normal; // todo: pack normal!
        float _padding1;
        DirectX::SimpleMath::Vector3 intensity;
        float _padding2;
    };

    LightSampler(const std::vector<Scene::AreaLightTriangle>& triangles);

    void GenerateRandomSamples(int samplingSeed, LightSample* destinationBuffer, uint32_t numSamples, float positionOffsetFromAreaLightTriangle = 0.00001f);

private:
    const std::vector<Scene::AreaLightTriangle>& m_areaLights;
    float m_totalAreaLightFlux;
    float m_totalAreaLightArea;

    // Normalized (to [0,1]) summed area for all light triangles
    std::vector<float> m_areaLightSummedFluxTable;
};
