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

    float PathLengthFilterMax;
};

struct AreaLightSample
{
    float3 Position;
    float _padding0;
    float3 Normal;      // todo: pack normal?
    float _padding1;
    float3 Intensity;   // todo: pack intensity?
    float _padding2;
};

cbuffer AreaLightSamples_ : register(b1)
{
    AreaLightSample AreaLightSamples[NUM_LIGHT_SAMPLES_AVAILABLE];
}

cbuffer MeshConstants : register (b2)
{
    uint MeshIndex; // Index used for vertex/index buffer.
    bool IsMetal;
    bool IsEmitter;
    uint DiffuseTextureIndex;

    float3 AreaLightRadiance;
    float3 Eta;
    float Roughness;
    float3 Ks;
}

struct Vertex
{
    float3 normal;
    float2 texcoord;
};
StructuredBuffer<Vertex> VertexBuffers[] : register(t0, space100);
StructuredBuffer<uint> IndexBuffers[] : register(t0, space101);
Texture2D DiffuseTextures[] : register(t0, space102);

SamplerState SamplerLinear : register(s0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0, space0);
// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0, space0);

#define DefaultRayTMin 0.00001f
#define DefaultRayTMax 100000.0f
