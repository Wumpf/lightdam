#include "Common.hlsl"

// Raytracing output texture, accessed as a UAV
RWTexture2D< float4 > gOutput : register(u0, space0);

[shader("raygeneration")]
export void RayGen()
{
    // Initialize the ray payload
    HitInfo payload;
    payload.colorAndDistance = float4(0, 0, 0, 0);

    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 screenCoord = ((launchIndex.xy + 0.5f) / dims.xy) * 2.0 - 1.0f;

    RayDesc ray;
    ray.Origin = CameraPosition;
    ray.Direction = normalize(screenCoord.x * CameraU + screenCoord.y * CameraV + CameraW);
    ray.TMin = 0;
    ray.TMax = 100000;

    // HitRay
    // In binding table we alternate between HitGroup and ShadowHitGroup, therefore 0 ray contribution and factor 2 for geometry index!
    TraceRay(SceneBVH, RAY_FLAG_NONE,
            0xFF, // InstanceInclusionMask
            0, // RayContributionToHitGroupIndex
            2, // MultiplierForGeometryContributionToHitGroupIndex
            0, // MissShaderIndex
            ray, payload);

    gOutput[launchIndex] = float4(payload.colorAndDistance.rgb, 1.f);
}
