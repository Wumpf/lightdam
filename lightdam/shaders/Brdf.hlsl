#pragma once

#include "Common.hlsl"
#include "Math.hlsl"

// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
// TODO: Use an approximation, this thing is scary!
float3 FresnelDieletricConductor(float3 eta, float3 etak, float cosTheta)
{  
   float cosTheta2 = cosTheta * cosTheta;
   float SinTheta2 = 1 - cosTheta2;
   float3 eta2 = eta * eta;
   float3 etak2 = etak * etak;

   float3 t0 = eta2 - etak2 - SinTheta2;
   float3 a2plusb2 = sqrt(t0 * t0 + 4 * eta2 * etak2);
   float3 t1 = a2plusb2 + cosTheta2;
   float3 a = sqrt(0.5f * (a2plusb2 + t0));
   float3 t2 = 2 * a * cosTheta;
   float3 Rs = (t1 - t2) / (t1 + t2);

   float3 t3 = cosTheta2 * a2plusb2 + SinTheta2 * SinTheta2;
   float3 t4 = t2 * SinTheta2;   
   float3 Rp = Rs * (t3 - t4) / (t3 + t4);

   return 0.5 * (Rp + Rs);
}

// PBR uses TrowbridgeReitz==GGX facet normal distribution and facet shadowing for Metals, so we follow suit here.

// PBR formulates Microfacet distribution functions quite a bit differently, less shader (or rather me having gamedev graphics mindset) friendly than
// amazing writeups like this one
// https://www.jordanstevenstechart.com/physically-based-rendering
// or this one
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// PBR see here:
// https://github.com/mmp/pbrt-v3/blob/3e9dfd72c6a669848616a18c22f347c0810a0b51/src/core/microfacet.cpp#L155
// (which surely is nice with the derivations in the book, but harder to read and shader optimize!)

// What confused me about Jordan Stevens writeup is that he claims there are two slightly different distribution functions in TrowbridgeReitz.
// Juggling the math a bit I found out it is exactly the same (image comparision yielded the same). Some missunderstanding?
// Also, what is described as "TrowbridgeReitz" there is what everyone else describes simply as GGX. 

float GGXNormalDistribution(float NdotH, float roughness)
{
    float roughnessSqr = roughness * roughness;
    float Distribution = NdotH * NdotH * (roughnessSqr - 1.0) + 1.0;
    return roughnessSqr / (PI * Distribution*Distribution);
}

float GGXSmithMasking(float NdotL, float NdotV, float roughness)
{
    float roughnessSqr = roughness*roughness;
    float NdotVSqr = NdotV*NdotV;
    float denomC = sqrt(roughnessSqr + (1.0f - roughnessSqr) * NdotVSqr) + NdotV;
    return 2.0f * NdotV / denomC;
}

float GGXSmithGeometricShadowingFunction(float NdotL, float NdotV, float roughness)
{
    float roughnessSqr = roughness*roughness;
    float NdotLSqr = NdotL*NdotL;
    float NdotVSqr = NdotV*NdotV;

    float SmithL = (2.0f * NdotL) / (NdotL + sqrt(roughnessSqr + (1.0f-roughnessSqr) * NdotLSqr));
    float SmithV = (2.0f * NdotV) / (NdotV + sqrt(roughnessSqr + (1.0f-roughnessSqr) * NdotVSqr));

    float Gs = SmithL * SmithV;
    return Gs;
}

// todo: there's quite a lot of math in this that can be simplified!
float3 EvaluateMicrofacetBrdf(float NdotL, float3 toLight, float NdotV, float3 toView, float3 normal, float3 eta, float3 k, float roughness)
{
    // https://github.com/mmp/pbrt-v3/blob/3e9dfd72c6a669848616a18c22f347c0810a0b51/src/core/reflection.cpp#L226
    float3 h = (toLight + toView); // half vector

    // degenerated cases. TODO: some of these can be caught earlier (and partly have been!)
    // TODO: Should have assertions instead and do checks elsewhere.
    if (NdotL <= 0.0f || NdotV <= 0.0f || all(h == float3(0,0,0)))
        return float3(0,0,0);

    h = normalize(h);
    float NdotH = dot(normal, h);
    float LdotH = dot(toLight, h);
    float D = GGXNormalDistribution(NdotH, roughness);
    float G = GGXSmithGeometricShadowingFunction(NdotL, NdotV, roughness); // todo: Is this equal to trowbridgereitz shadoing function (is there even such a thing?
    float3 F = FresnelDieletricConductor(eta, k, LdotH);
    return F * (D * G  / (4 * NdotV * NdotL));
}

// https://schuttejoe.github.io/post/ggximportancesamplingpart2/
float3 SampleGGXVisibleNormal(float3 toViewTS, float roughness, float2 randomSample)
{
    // Stretch the view vector so we are sampling as though
    // roughness==1
    float3 v = normalize(float3(toViewTS.x * roughness, toViewTS.y * roughness, toViewTS.z));

    // Build an orthonormal basis with v, t1, and t2
    float3 t1 = (v.z < 0.999f) ? normalize(cross(v, float3(0, 0, 1))) : float3(1, 0, 0);
    float3 t2 = cross(t1, v);

    // Choose a point on a disk with each half of the disk weighted
    // proportionally to its projection onto direction v
    float a = 1.0f / (1.0f + v.z);
    float r = sqrt(randomSample.x);
    float phi = (randomSample.y < a) ? (randomSample.y / a) * PI : PI + (randomSample.y - a) / (1.0f - a) * PI;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi) * ((randomSample.y < a) ? 1.0f : v.z);

    // Calculate the normal in this stretched tangent space
    float3 n = p1 * t1 + p2 * t2 + sqrt(max(0.0f, 1.0f - p1 * p1 - p2 * p2)) * v;

    // Unstretch and normalize the normal
    return normalize(float3(roughness * n.x, roughness * n.y, max(0.0f, n.z)));
}

float3 EvaluateLambertBrdf(float3 diffuse)
{
    return diffuse / PI;
}
