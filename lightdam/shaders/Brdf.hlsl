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

float GGXGeometricShadowingFunction(float NdotL, float NdotV, float roughness)
{
    float roughnessSqr = roughness*roughness;
    float NdotLSqr = NdotL*NdotL;
    float NdotVSqr = NdotV*NdotV;

    float SmithL = (2.0f * NdotL) / (NdotL + sqrt(roughnessSqr + (1.0f-roughnessSqr) * NdotLSqr));
    float SmithV = (2.0f * NdotV) / (NdotV + sqrt(roughnessSqr + (1.0f-roughnessSqr) * NdotVSqr));

    float Gs = SmithL * SmithV;
    return Gs;
}

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
    float G = GGXGeometricShadowingFunction(NdotL, NdotV, roughness); // todo: Is this equal to trowbridgereitz shadoing function (is there even such a thing?
    float3 F = FresnelDieletricConductor(eta, k, LdotH);
    return F * (D * G  / (4 * NdotV * NdotL));
}

float3 EvaluateLambertBrdf(float3 diffuse)
{
    return diffuse / PI;
}
