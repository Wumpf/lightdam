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

#define NUM_INDIRECT_BOUNCES 4

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
    payload.radiance_remainingBounces = half4(0.0f, 0.0f, 0.0f, NUM_INDIRECT_BOUNCES);
    payload.randomSeed = InitRandomSeed(FrameSeed, launchIndex.x * dims.y + launchIndex.y);

    TraceRadianceRay(ray, payload);

    float3 totalRadiance = payload.radiance_remainingBounces.rgb;

    while (payload.radiance_remainingBounces.w > 0.0f)
    {
        ray.Origin = ray.Origin + ray.Direction * payload.distance;
        ray.Direction = payload.nextRayDirection.xyz;
        TraceRadianceRay(ray, payload);
        totalRadiance += payload.radiance_remainingBounces.rgb;
        // TODO: Ray throughput
    }

    if (FrameNumber == 0)
        gOutput[launchIndex] = float4(totalRadiance, 1.f);
    else
        gOutput[launchIndex] += float4(totalRadiance, 1.f);
}
