#pragma once

uint WangHash(inout uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint XorShiftUInt(inout uint seed)
{
    // Xorshift32
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    return seed;
}

float XorShift(inout uint seed)
{
    return float(XorShiftUInt(seed) % 8388593) / 8388593.0;
}

float2 XorShift2(inout uint seed)
{
    return float2(XorShift(seed), XorShift(seed));
}

// 2D low discrepancy sequence as detailed here http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
// Integer solution to avoid rounding from https://www.shadertoy.com/view/3lsXW2
float2 WeylSequence2D(inout uint n)
{
    ++n;
    return frac(float2(n*12664745, n*9560333) / exp2(24.0f));
}

float WeylSequence(inout uint n)
{
    ++n;
    return frac(float(n*12664745) / exp2(24.0f));
}

// Sample hemisphere with cosine density in tangent space.
//    randomSample is a random number between 0-1
float3 SampleHemisphereCosine(float2 randomSample)
{
    float phi = PI_2 * randomSample.x;
    float sinTheta = sqrt(randomSample.y);	// sin(acos(sqrt(1-x))) = sqrt(x)
    float3 v;
    v.x = sinTheta * cos(phi);
    v.y = sinTheta * sin(phi);
    v.z = sqrt(1.0 - randomSample.y);	// sqrt(1-sin(theta)^2)

    return v;
}
