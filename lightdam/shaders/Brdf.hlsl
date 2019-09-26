#pragma once

#include "Common.hlsl"
#include "Math.hlsl"

// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
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
float3 FresnelDieletricConductorApprox(float3 Eta, float3 Etak, float CosTheta)
{
    float  CosTheta2 = CosTheta * CosTheta;
    float3 TwoEtaCosTheta = 2 * Eta * CosTheta;

    float3 t0 = Eta * Eta + Etak * Etak;
    float3 t1 = t0 * CosTheta2;
    float3 Rs = (t0 - TwoEtaCosTheta + CosTheta2) / (t0 + TwoEtaCosTheta + CosTheta2);
    float3 Rp = (t1 - TwoEtaCosTheta + 1) / (t1 + TwoEtaCosTheta + 1);

    return 0.5* (Rp + Rs);
}

float3 SchlickFresnel(float3 eta, float cosTheta)
{
    return eta + pow(1.0f - cosTheta, 5.0f) * (float3(1.0f, 1.0f, 1.0f) - eta);
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

float GGXNormalDistribution(float NdotH, float roughnessSq)
{
    float Distribution = NdotH * NdotH * (roughnessSq - 1.0) + 1.0;
    return roughnessSq / (PI * Distribution*Distribution + 0.00000001f);
}

// Samples GGX normal distribution's half vector in tangent space
// From https://github.com/mmp/pbrt-v3/blob/9f717d847a807793fa966cf0eaa366852efef167/src/core/microfacet.cpp#L307
// PDF: GGXNormalDistribution * NdotH
float3 SampleGGXNormalDistributionHalfVector(float2 randomSample, float roughnessSq)
{
    float tanTheta2 = roughnessSq * randomSample.x / (1.0f - randomSample.x);
    float cosTheta = 1.0f / sqrt(1.0f + tanTheta2);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = (2.0f * PI) * randomSample.y;
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float GGXSmithMasking(float NdotL, float NdotV, float roughnessSq)
{
    float NdotVSqr = NdotV*NdotV;
    float denomC = sqrt(roughnessSq + (1.0f - roughnessSq) * NdotVSqr) + NdotV;
    return 2.0f * NdotV / denomC;
}

float GGXSmithGeometricShadowingFunction(float NdotL, float NdotV, float roughnessSq)
{
    float NdotLSqr = NdotL*NdotL;
    float NdotVSqr = NdotV*NdotV;

    float SmithL = (2.0f * NdotL) / (NdotL + sqrt(roughnessSq + (1.0f-roughnessSq) * NdotLSqr));
    float SmithV = (2.0f * NdotV) / (NdotV + sqrt(roughnessSq + (1.0f-roughnessSq) * NdotVSqr));

    float Gs = SmithL * SmithV;
    return Gs;
}

// GGX microfacet terms:
// D = GGXNormalDistribution
// G = GGXSmithGeometricShadowingFunction
// (D * G  / (4 * NdotV * NdotL))
// Algebraically simplified.
float GGXSpecular(float NdotH, float NdotL, float NdotV, float roughnessSq)
{
    float D = GGXNormalDistribution(NdotH, roughnessSq);

    float term_i = NdotL + sqrt(roughnessSq + (1.0f-roughnessSq) * NdotL * NdotL);
    float term_o = NdotV + sqrt(roughnessSq + (1.0f-roughnessSq) * NdotV * NdotV);

    return D / (term_i * term_o);
}

// todo: there's quite a lot of math in this that can be simplified!
float3 EvaluateMicrofacetBrdf(float NdotL, float3 toLight, float NdotV, float3 toView, float3 normal, float3 eta, float3 k, float roughnessSq)
{
    // https://github.com/mmp/pbrt-v3/blob/3e9dfd72c6a669848616a18c22f347c0810a0b51/src/core/reflection.cpp#L226
    float3 h = normalize(toLight + toView); // half vector
    // degenerated case
    if (isinf(h.x))
        return float3(0,0,0);

    float NdotH = dot(normal, h);
    float LdotH = dot(toLight, h);
    float3 F = FresnelDieletricConductorApprox(eta, k, LdotH);
    return F * GGXSpecular(NdotH, NdotL, NdotV, roughnessSq);
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

float3 EvaluateAshikminShirleySubstrateBrdf(float NdotL, float3 toLight, float NdotV, float3 toView, float3 normal, float3 k, float roughnessSq, float3 diffuse)
{
    // Ashikhmin and Shirley two layer brdf as described in
    // http://www.pbr-book.org/3ed-2018/Reflection_Models/Fresnel_Incidence_Effects.html#fragment-FresnelBlendPrivateData-0

    float3 h = normalize(toLight + toView); // half vector
    float NdotH = dot(normal, h);
    float LdotH = dot(toLight, h);

    float3 diffusePart = (28.0f / (23.0f * PI)) * diffuse * (float3(1.0f, 1.0f, 1.0f) - k) * 
                        (1.0f - pow(1.0f - 0.5f * NdotL, 5.0f)) * 
                        (1.0f - pow(1.0f - 0.5f * NdotV, 5.0f));

    float3 specularPart = GGXNormalDistribution(NdotH, roughnessSq) / (4 * LdotH * max(NdotL, NdotV)) * SchlickFresnel(k, LdotH);
    //[flatten] if (isinf(h.x) || LdotH <= 0.0f) specularPart = float3(0,0,0);

    return diffusePart + specularPart;
}

float3 SampleAshikminShirleySubstrateBrdf(float3 toViewTS, float2 randomSample, float3 k, float roughnessSq, float3 diffuse, out float3 throughput)
{
    float3 nextRayDirTS, halfVector;
    if (randomSample.x < 0.5f)
    {
        randomSample.x *= 2.0f;
        nextRayDirTS = SampleHemisphereCosine(randomSample);
        halfVector = normalize(toViewTS + nextRayDirTS);
    }
    else
    {
        randomSample.x = randomSample.x * 2.0f - 1.0f;
        halfVector = SampleGGXNormalDistributionHalfVector(randomSample, roughnessSq);
        nextRayDirTS = reflect(-toViewTS, halfVector);
    }

    // todo simplifiy:
    float3 brdf = EvaluateAshikminShirleySubstrateBrdf(nextRayDirTS.z, nextRayDirTS, toViewTS.z, toViewTS, float3(0.0f, 0.0f, 1.0f), k, roughnessSq, diffuse);
    float pdf = 0.5f * (
        nextRayDirTS.z / PI + 
        GGXNormalDistribution(halfVector.z, roughnessSq) * halfVector.z  / (4.0f * dot(toViewTS, halfVector))
    );
    throughput = nextRayDirTS.z * brdf / pdf;
    return nextRayDirTS;
}