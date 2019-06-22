#include "Common.hlsl"

// Raytracing output texture, accessed as a UAV
RWTexture2D< float4 > gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

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

    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    gOutput[launchIndex] = float4(payload.colorAndDistance.rgb, 1.f);
}
