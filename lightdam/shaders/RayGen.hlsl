#include "Common.hlsl"

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
    #define NUM_BOUNCES 1
#else
    #define NUM_BOUNCES 5
#endif

[shader("raygeneration")]
export void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 screenCoord = ((launchIndex.xy + GlobalJitter.xy) / dims.xy) * 2.0 - 1.0f;

    RayDesc ray;
    ray.Origin = CameraPosition;
    ray.Direction = normalize(screenCoord.x * CameraU + screenCoord.y * CameraV + CameraW);
    ray.TMin = DefaultRayTMin;
    ray.TMax = DefaultRayTMax;

    RadianceRayHitInfo payload;
    payload.radiance = float3(0.0f, 0.0f, 0.0f);
    payload.pathThroughput_remainingBounces = FloatToHalf(float3(1.0f, 1.0f, 1.0f), NUM_BOUNCES);
    payload.randomSeed = InitRandomSeed(FrameSeed, launchIndex.x * dims.y + launchIndex.y);

    TraceRadianceRay(ray, payload);

    uint remainingBounces = payload.pathThroughput_remainingBounces.y >> 16;
    float3 totalRadiance = payload.radiance;

    while (remainingBounces > 0)
    {
        ray.Origin = ray.Origin + ray.Direction * payload.distance;
        ray.Direction = UnpackDirection(payload.nextRayDirection);
        payload.radiance = float3(0.0f, 0.0f, 0.0f);
        TraceRadianceRay(ray, payload);
        totalRadiance += payload.radiance;
        remainingBounces = payload.pathThroughput_remainingBounces.y >> 16;
    }

    if (FrameNumber == 0)
        gOutput[launchIndex] = float4(totalRadiance, 1.f);
    else
        gOutput[launchIndex] += float4(totalRadiance, 1.f);
}
