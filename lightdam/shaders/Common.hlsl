#pragma once

#include "Config.hlsl"
#include "Math.hlsl"
#include "Random.hlsl"

// TODO: Packing
struct RadianceRayHitInfo
{
    float3 radiance;
    float distance; // -1 on output means no hit
    uint2 pathThroughput_remainingBounces; // packed as half3 + 16 bit uint
    uint nextRayDirection;
    uint sampleIndex;
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

    float PathLengthFilterMax;
};

SamplerState SamplerLinearWrap : register(s0);
SamplerState SamplerPointRepeat : register(s1);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0, space0);
Texture2D<uint> BlueNoiseTexture : register(t1, space0);

#define DefaultRayTMin 0.00001f
#define DefaultRayTMax 100000.0f
