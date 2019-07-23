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
