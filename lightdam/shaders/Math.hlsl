#pragma once

#define PI 3.14159265358979
#define PI_2 6.28318530717958

float BarycentricLerp(in float v0, in float v1, in float v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float2 BarycentricLerp(in float2 v0, in float2 v1, in float2 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float3 BarycentricLerp(in float3 v0, in float3 v1, in float3 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float4 BarycentricLerp(in float4 v0, in float4 v1, in float4 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float GetLuminance(float3 rgb)
{
	const float3 W = float3(0.212671, 0.715160, 0.072169);
	return dot(rgb, W);
}

// Create ONB from normalized vector
void CreateONB(in float3 n, out float3 U, out float3 V)
{
    // todo optimize
	U = cross(n, float3(0.0f, 1.0f, 0.0f));
	if (all(abs(U) < float(0.0001f).xxx))
	 	U = cross(n, float3(1.0f, 0.0f, 0.0f));
	U = normalize(U);
	V = cross(n, U);
}

static const uint BitMask16 = 0x0000ffff;
static const float BitMask16f = float(BitMask16);
static const float BitMask16invf = 1.0f / BitMask16f;

// Packs two floats ranging from [0; 1]
uint PackUNorm16(float2 v)
{
    uint2 d = uint2(floor(v * BitMask16f + 0.5f));
    return (d.y << 16) | d.x;
}
uint2 PackUNorm16(float3 v, uint extra16Bit)
{
    uint3 d = uint3(floor(v * BitMask16f + 0.5f));
    return uint2((d.y << 16) | d.x, (extra16Bit << 16) | d.z);
}
// Packs two floats ranging from [-1; 1]
uint PackSNorm16(float2 v)
{
	return PackUNorm16(0.5f * v.xy + float2(0.5f, 0.5f));
}
// Unpacks two UNorm values
float2 UnpackUNorm16(uint packed)
{
    uint2 d = uint2(packed, packed >> 16) & BitMask16;
    return d * BitMask16invf;
}
float3 UnpackUNorm16(uint2 packed, out uint extra16Bit)
{
    uint4 d = uint4(packed.x, packed.x >> 16, packed.y, packed.y >> 16) & BitMask16;
    extra16Bit = d.w;
    return d.xyz * BitMask16invf;
}
// Unpacks two SNorm values
float2 UnpackSNorm16(uint packed)
{
	return UnpackUNorm16(packed) * 2.0f - 1.0;
}

// Compresses a direction vector using octahedral mapping
// See for implementation https://www.shadertoy.com/view/Mtfyzl
// and for theory http://jcgt.org/published/0003/02/01/paper.pdf
uint PackDirection(float3 dir)
{
    dir /= abs(dir.x) + abs(dir.y) + abs(dir.z);
    dir.xy = dir.z >= 0.0f ? dir.xy : ((float2(1.0f, 1.0f) - abs(dir.yx)) * sign(dir.xy));
    return PackSNorm16(dir.xy);
}

// See PackDirection
float3 UnpackDirection(uint compressedDir)
{
    float2 v = UnpackSNorm16(compressedDir);

    float3 dir = float3(v, 1.0 - abs(v.x) - abs(v.y));
    float t = max(-dir.z, 0.0);
    dir.x += (dir.x > 0.0) ? -t : t;
    dir.y += (dir.y > 0.0) ? -t : t;
 
    return normalize(dir);
}

uint2 FloatToHalf(float4 v)
{
    uint4 raw = f32tof16(v);
    return raw.xz | (raw.yw << 16);
}

uint2 FloatToHalf(float3 v, uint extra16Bit)
{
    uint3 raw = f32tof16(v);
    return uint2(raw.x | (raw.y << 16), raw.z | (extra16Bit << 16));
}

float4 HalfToFloat(uint2 v)
{
    uint4 raw;
    raw.xz = v & 0x0000ffff;
    raw.yw = v >> 16;
    return f16tof32(raw);
}

float3 HalfToFloat(uint2 v, out uint extra16Bit)
{
    extra16Bit = v.y >> 16;
    return f16tof32(uint3(v.x & 0x0000ffff, v.x >> 16, v.y & 0x0000ffff));
}