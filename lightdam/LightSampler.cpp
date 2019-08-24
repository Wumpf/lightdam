#include "LightSampler.h"
#include "../external/SimpleMath.h"
#include "MathUtils.h"
#include <algorithm>

using namespace DirectX::SimpleMath;

LightSampler::LightSampler(const std::vector<Scene::AreaLightTriangle>& triangles)
    : m_areaLights(triangles)
    , m_totalAreaLightFlux(0.0f)
    , m_totalAreaLightArea(0.0f)
{
    m_areaLightSummedFluxTable.reserve(triangles.size());

    for (const auto& triangle : triangles)
    {
        m_totalAreaLightFlux += Vector3(0.2126f, 0.7152f, 0.0722f).Dot(triangle.emittedRadiance) * triangle.area * PI; // pi is the integral over all solid angles of the cosine lobe
        m_totalAreaLightArea += triangle.area;
        m_areaLightSummedFluxTable.push_back(m_totalAreaLightArea);
    }
    for (auto& v : m_areaLightSummedFluxTable)
        v /= m_totalAreaLightArea;
}

void LightSampler::GenerateRandomSamples(int samplingSeed, LightSample* destinationBuffer, uint32_t numSamples, float positionOffsetFromAreaLightTriangle)
{
    if (m_areaLights.empty())
        return;

    // We're not dividing by the number of samples, since we don't know how many samples we will evaluate in our shader.
    float sampleWeight = m_totalAreaLightArea; // / numSamples;

    for (uint32_t i = 0; i < numSamples; ++i)
    {
        const int haltonIndex = i + samplingSeed * numSamples;

        // Search triangle.
        float randomTriangle = ComputeHaltonSequence(haltonIndex, 1);
        auto lightTriangleTableIt = std::lower_bound(m_areaLightSummedFluxTable.begin(), m_areaLightSummedFluxTable.end(), randomTriangle);
        unsigned int triangleIdx = static_cast<unsigned int>(lightTriangleTableIt - m_areaLightSummedFluxTable.begin());
        auto& areaLightTriangle = m_areaLights[triangleIdx];

        // Sample random (barycentric) point on triangle.
        // See section 4.2 in http://graphics.stanford.edu/courses/cs468-08-fall/pdf/osada.pdf
        float xi0 = ComputeHaltonSequence(haltonIndex, 3);
        float xi1 = ComputeHaltonSequence(haltonIndex, 4);
        xi0 = sqrtf(xi0);
        float alpha = (1.0f - xi0);
        float beta = xi0 * (1.0f - xi1);
        //float gamma = xi0 * xi1;

        // Compute light sample and write to buffer.
        destinationBuffer[i].position = Vector3::Barycentric(areaLightTriangle.positions[0], areaLightTriangle.positions[1], areaLightTriangle.positions[2], alpha, beta);
        destinationBuffer[i].normal = Vector3::Barycentric(areaLightTriangle.normals[0], areaLightTriangle.normals[1], areaLightTriangle.normals[2], alpha, beta);
        destinationBuffer[i].normal.Normalize();
        destinationBuffer[i].intensity = areaLightTriangle.emittedRadiance * sampleWeight; // Area factor is already contained, since it determines the sample probability.

        // Move a bit along the normal to avoid intersection precision issues.
        destinationBuffer[i].position += destinationBuffer[i].normal * positionOffsetFromAreaLightTriangle;
    }
}