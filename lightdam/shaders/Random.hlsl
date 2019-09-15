#pragma once

// Use this function to get an initial random seed.
// For details on why this is a good idea check out:
// http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
// Don't use frameNumber for frameseed, otherwise the random number just "move" accross the screen!
uint InitRandomSeed(uint frameSeed, uint invocationID)
{
	uint seed = frameSeed + invocationID;

	// Wang hash.
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

uint RandomUInt(inout uint seed)
{
	// Xorshift32
	seed ^= (seed << 13);
	seed ^= (seed >> 17);
	seed ^= (seed << 5);
	return seed;
}

float Random(inout uint seed)
{
	return float(RandomUInt(seed) % 8388593) / 8388593.0;
}

float2 Random2(inout uint seed)
{
	return float2(Random(seed), Random(seed));
}

// Sample hemisphere with cosine density.
//    randomSample is a random number between 0-1
float3 SampleHemisphereCosine(float2 randomSample, float3x3 tangentToWorld)
{
    float phi = PI_2 * randomSample.x;
    float sinTheta = sqrt(randomSample.y);	// sin(acos(sqrt(1-x))) = sqrt(x)
    float3 v;
    v.x = sinTheta * cos(phi);
    v.y = sinTheta * sin(phi);
    v.z = sqrt(1.0 - randomSample.y);	// sqrt(1-sin(theta)^2)

    return mul(v, tangentToWorld);
}
