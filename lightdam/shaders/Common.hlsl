#pragma once

#include "Math.hlsl"
#include "Random.hlsl"

// TODO: Packing
struct RadianceRayHitInfo
{
    float3 radiance;
    float distance; // -1 on output means no hit
    uint2 pathThroughput_remainingBounces; // packed as half3 + 16 bit uint
    uint nextRayDirection;
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

cbuffer MeshConstants : register (b1)
{
    uint MeshIndex; // Index used for vertex/index buffer.
    float3 Diffuse; // Diffuse reflectivity (lambert)
}

struct Vertex
{
    float3 vertex; // Todo: Not actually needed, remove!
    float3 normal;
};
StructuredBuffer<Vertex> VertexBuffers[] : register(t0, space100);
StructuredBuffer<uint> IndexBuffers[] : register(t0, space101);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0, space0);
// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0, space0);

#define DefaultRayTMin 0.0001f
#define DefaultRayTMax 100000.0f