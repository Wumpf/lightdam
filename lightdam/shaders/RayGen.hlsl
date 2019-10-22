#include "Common.hlsl"

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0, space0);

void TraceRadianceRay(in RayDesc ray, inout RadianceRayHitInfo payload)
{
    // HitRay
    // In binding table we alternate between HitGroup and ShadowHitGroup, therefore 0 ray contribution and factor 2 for geometry index!
    TraceRay(SceneBVH, RAY_FLAG_NONE,
        0xFF, // InstanceInclusionMask
        0, // RayContributionToHitGroupIndex
        2, // MultiplierForGeometryContributionToHitGroupIndex
        0, // MissShaderIndex
        ray, payload);
}

#ifdef DEBUG_VISUALIZE_NORMALS
    #undef NUM_BOUNCES
    #define NUM_BOUNCES 1
#endif

[shader("raygeneration")]
export void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 screenCoord = ((launchIndex.xy + GlobalJitter.xy) / dims.xy) * 2.0 - 1.0f;

    // Thoughts on ideal random number relations for sampling:
    // * spatially (x/y): blue noise is most pleasant, want to equally distribute error
    // * temporarily: correlated/low discrepancy want to evently sample hemispheres over time
    // * from event to event: uncorrelated, one hit should not determine behavior of next

    int3 noiseTextureSize;
    BlueNoiseTexture.GetDimensions(0, noiseTextureSize.x, noiseTextureSize.y, noiseTextureSize.z);
    uint blueNoise = BlueNoiseTexture.Load(int3((launchIndex) % noiseTextureSize.xy, 0));

    RayDesc ray;
    ray.Origin = CameraPosition;
    ray.Direction = normalize(screenCoord.x * CameraU + screenCoord.y * CameraV + CameraW);
    ray.TMin = DefaultRayTMin;
    ray.TMax = DefaultRayTMax;

    RadianceRayHitInfo payload;
    payload.radiance = float3(0.0f, 0.0f, 0.0f);
    payload.distance = 0.0f;
    payload.pathThroughput_remainingBounces = FloatToHalf(float3(1.0f, 1.0f, 1.0f), NUM_BOUNCES);
    payload.sampleIndex = blueNoise ^ FrameSeed;
    float pathLength = 0.0f;

    TraceRadianceRay(ray, payload);

    uint remainingBounces = payload.pathThroughput_remainingBounces.y >> 16;

    while (remainingBounces > 0)
    {
        pathLength += payload.distance;
        // If we do path length filtering, pass path length back in. (pathLength should be automaticaly optimized out if not used here!)
        ray.Origin = ray.Origin + ray.Direction * payload.distance;
        ray.Direction = UnpackDirection(payload.nextRayDirection);
    #ifdef ENABLE_PATHLENGTH_FILTER
        payload.distance = pathLength;
    #endif
        TraceRadianceRay(ray, payload);
        remainingBounces = payload.pathThroughput_remainingBounces.y >> 16;
    }

    if (FrameNumber == 0)
        gOutput[launchIndex] = float4(payload.radiance, 1.f);
    else
        gOutput[launchIndex] += float4(payload.radiance, 1.f);
}
