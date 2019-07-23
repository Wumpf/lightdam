#pragma once

#include "Math.hlsl"
#include "Random.hlsl"

// TODO: Packing
struct RadianceRayHitInfo
{
    float4 radiance_remainingBounces;
    float4 nextRayDirection;
    float distance; // -1 on output means no hit
    uint randomSeed;
};

struct ShadowHitInfo
{
    bool isHit;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
    float2 bary;
};

// Global constant buffer
cbuffer GlobalConstants : register(b0)
{
    float3 CameraU;
    float3 CameraV;
    float3 CameraW;
    float3 CameraPosition;

    // Jitter value between 0 and 1, different every frame.
    float2 GlobalJitter;
    uint FrameNumber;
    uint FrameSeed; // A single random number for every frame!
};

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0, space0);
// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0, space0);

#define DefaultRayTMin 0.001f
#define DefaultRayTMax 100000.0f