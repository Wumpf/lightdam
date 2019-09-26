#include "Common.hlsl"
#include "Brdf.hlsl"

#define MATERIAL_MATTE 0
#define MATERIAL_METAL 1
#define MATERIAL_SUBSTRATE 2

bool ShadowRay(float3 worldPosition, float3 toLight, float lightDistance = DefaultRayTMax)
{
    RayDesc shadowRay;
    shadowRay.Origin = worldPosition;
    shadowRay.Direction = toLight;
    shadowRay.TMin = DefaultRayTMin;
    shadowRay.TMax = lightDistance;

    ShadowHitInfo shadowPayLoad;
    shadowPayLoad.isHit = true;

    // ShadowRay
    // In binding table we alternate between HitGroup and ShadowHitGroup, therefore 1 ray contribution and factor 2 for geometry index!
    // Same goes for MissShader
    TraceRay(SceneBVH,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0xFF, // InstanceInclusionMask
        1, // RayContributionToHitGroupIndex
        2, // MultiplierForGeometryContributionToHitGroupIndex
        1, // MissShaderIndex
        shadowRay, shadowPayLoad);

    return shadowPayLoad.isHit;
}

Vertex GetSurfaceHit(Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint primitiveIdx = 3 * PrimitiveIndex();
    uint vertexIdx0 = IndexBuffers[MeshIndex][primitiveIdx + 0];
    uint vertexIdx1 = IndexBuffers[MeshIndex][primitiveIdx + 1];
    uint vertexIdx2 = IndexBuffers[MeshIndex][primitiveIdx + 2];
    
    Vertex outVertex;
    outVertex.normal = normalize(
        BarycentricLerp(
            VertexBuffers[MeshIndex][vertexIdx0].normal,
            VertexBuffers[MeshIndex][vertexIdx1].normal,
            VertexBuffers[MeshIndex][vertexIdx2].normal, barycentrics)
        );
    [flatten] if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
        outVertex.normal = -outVertex.normal;
    
    outVertex.texcoord = BarycentricLerp(
            VertexBuffers[MeshIndex][vertexIdx0].texcoord,
            VertexBuffers[MeshIndex][vertexIdx1].texcoord,
            VertexBuffers[MeshIndex][vertexIdx2].texcoord, barycentrics);

    return outVertex;
}

// Returns sampled radiance
float3 SampleAreaLight(AreaLightSample areaLightSample, Vertex hit, float3 worldPosition, float3 toView, float NdotV, float3 diffuse, float RoughnessSq)
{
    float3 toLight = areaLightSample.Position - worldPosition;
    float lightDistanceSq = dot(toLight, toLight);
    float lightDistanceInv = rsqrt(lightDistanceSq);
    float lightDistance = 1.0f / lightDistanceInv;
    toLight *= rsqrt(lightDistanceSq);

    float NdotL = dot(toLight, hit.normal);
    float lightSampleCos = dot(-toLight, areaLightSample.Normal);

    if (ShadowRay(worldPosition, toLight, lightDistance))
        return float3(0.0f, 0.0f, 0.0f);
    
    // It may be anti-intuitive, but it is better to always check the ShadowRay since DXR really hates it to early before TraceRay calls!
    // Doing these condition before tracing the shadow ray gives a major performance hit!
    // https://devblogs.nvidia.com/rtx-best-practices/
    // "[...] can help the compiler streamline the generated code and improve performance"
    if (NdotL <= 0.0f || lightSampleCos <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
#ifdef ENABLE_PATHLENGTH_FILTER
    if (pathLength + lightDistance > PathLengthFilterMax)
        return float3(0.0f, 0.0f, 0.0f);
#endif

    float3 brdfLightSample;

    if (MaterialType == MATERIAL_SUBSTRATE)
    {
        brdfLightSample = EvaluateAshikminShirleySubstrateBrdf(NdotL, toLight, NdotV, toView, hit.normal, Ks, RoughnessSq, diffuse);
    }
    else if (MaterialType == MATERIAL_METAL)
    {
        brdfLightSample = EvaluateMicrofacetBrdf(NdotL, toLight, NdotV, toView, hit.normal, Eta, Ks, RoughnessSq);
    }
    else
    {
        brdfLightSample = EvaluateLambertBrdf(diffuse);
    }

    // lightIntensity = lightIntensity in normal direction (often called I0) -> seen area is smaller to the border
    float irradianceLightSample = NdotL / lightDistanceSq; // Needs to be divided by pdf for this sample. Everything was already normalized beforehand though, so no need here!
    return (irradianceLightSample * lightSampleCos) * brdfLightSample * areaLightSample.Intensity; // sample radiance contribution
}

// Computes next ray direction in tangent space
// throughput = brdf / pdf
bool ComputeNextRay(out float3 nextRayDirTS, out float3 throughput, inout uint randomSeed, float3 toViewTS, float3 diffuse, float RoughnessSq)
{
    float2 randomSample = Random2(randomSeed); // todo: use low discrepancy sampling here!

    // For checking importance sampling correctness:
    // throughput *= brdfNextSample * cos(Out, N) / pdfNextSampleGen
    // With SampleHemisphereCosine: pdfNextSampleGen = cos(Out, N) / PI
    // -> throughput = brdfNextSample * PI

    if (MaterialType == MATERIAL_SUBSTRATE)
    {
        nextRayDirTS = SampleAshikminShirleySubstrateBrdf(toViewTS, randomSample, Ks, RoughnessSq, diffuse, throughput);

        // Check importance sampling correctness:
        //nextRayDirTS = SampleHemisphereCosine(randomSample);
        //float3 brdfNextSample = EvaluateAshikminShirleySubstrateBrdf(nextRayDirTS.z, nextRayDirTS, toViewTS.z, toViewTS, float3(0,0,1), Ks, RoughnessSq, diffuse);
        //throughput = brdfNextSample * PI;
    }
    else if (MaterialType == MATERIAL_METAL)
    {
        float3 microfacetNormalTS = SampleGGXVisibleNormal(toViewTS, Roughness, randomSample);
        nextRayDirTS = reflect(-toViewTS, microfacetNormalTS);
        float NdotL = nextRayDirTS.z;
        float3 F = FresnelDieletricConductorApprox(Eta, Ks, NdotL);
        //float G1 = GGXSmithMasking(NdotL, NdotV, RoughnessSq);
        //float G2 = GGXSmithGeometricShadowingFunction(NdotL, NdotV, RoughnessSq);
        //throughput = F * (G2 / G1);
        float G2_div_G1 = (2.0f * NdotL) / (NdotL + sqrt(RoughnessSq + (1.0f-RoughnessSq) * NdotL * NdotL));
        throughput = F * G2_div_G1;

        // Check importance sampling correctness:
        //nextRayDirTS = SampleHemisphereCosine(randomSample);
        //float3 brdfNextSample = EvaluateMicrofacetBrdf(nextRayDirTS.z, nextRayDirTS, NdotV, toViewTS, float3(0,0,1), eta, k, RoughnessSq);
        //throughput = brdfNextSample * PI;
    }
    else
    {
        // throughput *= brdfNextSample * cos(Out, N) / pdfNextSampleGen
        // With SampleHemisphereCosine: pdfNextSampleGen = cos(Out, N) / PI
        // Lambert brdf: brdfNextSample = diffuse / PI;
        // -> throughput for Lambert: diffuse
        throughput = diffuse; // brdfNextSample * saturate(dot(nextRayDir, hit.normal)) / pdf;
        nextRayDirTS = SampleHemisphereCosine(randomSample);
    }
    if (nextRayDirTS.z <= 0.0f)
        return false;

#ifdef RUSSIAN_ROULETTE
    float continuationProbability = saturate(GetLuminance(throughput));
    if (Random(randomSeed) >= continuationProbability) // if continuationProbability is zero, path should be stoped -> >=
        return false;
    throughput /= continuationProbability; // Only change in spectrum, no energy loss.
#endif

    return true;
}

void SurfaceInteraction(inout RadianceRayHitInfo payload, Vertex hit)
{
    // Unpack throughput/reamaining bounces.
    uint remainingBounces;
    float3 pathThroughput = HalfToFloat(payload.pathThroughput_remainingBounces, remainingBounces);
    remainingBounces -= 1;

    // Start by assuming this is the last event, this makes early outing easier to write!
    payload.pathThroughput_remainingBounces.y = 0;

#ifdef ENABLE_PATHLENGTH_FILTER
    float pathLength = payload.distance + RayTCurrent();
    if (pathLength > PathLengthFilterMax)
        return;
#endif

    float3 worldPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3x3 tangentToWorld;
    CreateONB(hit.normal, tangentToWorld);
    float3 toView = -WorldRayDirection();
    float3 toViewTS = mul(toView, transpose(tangentToWorld));
    float NdotV = toViewTS.z;

#ifdef DEBUG_VISUALIZE_NORMALS
    payload.radiance = hit.normal * 0.5 + float3(0.5f,0.5f,0.5f);
    return;
#endif
#ifdef DEBUG_VISUALIZE_TEXCOORD
    payload.radiance = float3(abs(hit.texcoord), 0.0f);
    return;
#endif

    if (IsEmitter)
    {
        if (remainingBounces == NUM_BOUNCES-1) // an eye ray
            payload.radiance += AreaLightRadiance;
        return;
    }
    
    float3 diffuse = DiffuseTextures[DiffuseTextureIndex].SampleLevel(SamplerLinear, hit.texcoord, 0).xyz;
#ifdef DEBUG_VISUALIZE_DIFFUSETEXTURE
    payload.radiance = diffuse;
    return;
#endif

    float RoughnessSq = Roughness * Roughness;

    // Sample area lights.
    uint randomSampleOffset = RandomUInt(payload.randomSeed) % (NUM_LIGHT_SAMPLES_AVAILABLE - NUM_LIGHT_SAMPLES_PERHIT + 1);
    float3 radiance = float3(0.0f, 0.0f, 0.0f);
    for (uint i=0; i<NUM_LIGHT_SAMPLES_PERHIT; ++i)
        radiance += SampleAreaLight(AreaLightSamples[randomSampleOffset + i], hit, worldPosition, toView, NdotV, diffuse, RoughnessSq);
    payload.radiance += pathThroughput * radiance / NUM_LIGHT_SAMPLES_PERHIT;

    // Compute next ray.
    if (remainingBounces == 0)
        return;
    float3 nextRayDirTS;
    float3 throughput;
    if (!ComputeNextRay(nextRayDirTS, throughput, payload.randomSeed, toViewTS, diffuse, RoughnessSq))
        return;

    // Pack data for next ray.
    pathThroughput *= throughput;
    payload.pathThroughput_remainingBounces = FloatToHalf(pathThroughput, remainingBounces);
    float3 nextRayDir = mul(nextRayDirTS, tangentToWorld);
    payload.nextRayDirection = PackDirection(nextRayDir);
    payload.distance = RayTCurrent();
}

[shader("closesthit")]
export void ClosestHit(inout RadianceRayHitInfo payload, Attributes attrib)
{
    SurfaceInteraction(payload, GetSurfaceHit(attrib));
}