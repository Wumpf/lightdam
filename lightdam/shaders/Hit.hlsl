#include "Common.hlsl"
#include "Brdf.hlsl"

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

[shader("closesthit")]
export void ClosestHit(inout RadianceRayHitInfo payload, Attributes attrib)
{
#ifdef ENABLE_PATHLENGTH_FILTER
    float pathLength = payload.distance + RayTCurrent();
    if (pathLength > PathLengthFilterMax)
    {
        payload.pathThroughput_remainingBounces.y = 0;
        return;
    }
#endif

    Vertex hit = GetSurfaceHit(attrib);
    float3 worldPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 toView = -WorldRayDirection();

    uint remainingBounces;
    float3 pathThroughput = HalfToFloat(payload.pathThroughput_remainingBounces, remainingBounces);
    remainingBounces -= 1;
    payload.distance = RayTCurrent();

#ifdef DEBUG_VISUALIZE_NORMALS
    payload.radiance = hit.normal * 0.5 + float3(0.5f,0.5f,0.5f);
    payload.pathThroughput_remainingBounces.y = 0;
    return;
#endif
#ifdef DEBUG_VISUALIZE_TEXCOORD
    payload.radiance = float3(abs(hit.texcoord), 0.0f);
    payload.pathThroughput_remainingBounces.y = 0;
    return;
#endif

    if (IsEmitter)
    {
        if (remainingBounces == NUM_BOUNCES-1) // an eye ray
            payload.radiance += AreaLightRadiance;
        payload.pathThroughput_remainingBounces.y = 0;
        return;
    }
    
    float3 diffuse = DiffuseTextures[DiffuseTextureIndex].SampleLevel(SamplerLinear, hit.texcoord, 0).xyz;
#ifdef DEBUG_VISUALIZE_DIFFUSETEXTURE
    payload.radiance = diffuse;
    payload.pathThroughput_remainingBounces.y = 0;
    return;
#endif

    float RoughnessSq = Roughness * Roughness;

    float3x3 tangentToWorld;
    CreateONB(hit.normal, tangentToWorld);
    float3 toViewTS = mul(toView, transpose(tangentToWorld));
    float NdotV = toViewTS.z;

    uint randomSampleOffset = RandomUInt(payload.randomSeed) % (NUM_LIGHT_SAMPLES_AVAILABLE - NUM_LIGHT_SAMPLES_PERHIT + 1);
    float3 radiance = float3(0.0f, 0.0f, 0.0f);
    for (uint i=randomSampleOffset; i<randomSampleOffset + NUM_LIGHT_SAMPLES_PERHIT; ++i)
    {
        float3 toLight = AreaLightSamples[i].Position - worldPosition;
        float lightDistanceSq = dot(toLight, toLight);
        toLight *= rsqrt(lightDistanceSq);

        float NdotL = dot(toLight, hit.normal);
        if (NdotL <= 0.0f)
            continue;
        float lightSampleCos = dot(-toLight, AreaLightSamples[i].Normal);
        if (lightSampleCos <= 0.0f)
            continue;

        float lightDistance = sqrt(lightDistanceSq);

#ifdef ENABLE_PATHLENGTH_FILTER
        if (pathLength + lightDistance > PathLengthFilterMax)
            continue;
#endif

        if (ShadowRay(worldPosition, toLight, lightDistance))
            continue;
        
        float3 brdfLightSample;
        if (IsMetal)
        {
            brdfLightSample = EvaluateMicrofacetBrdf(NdotL, toLight, NdotV, toView, hit.normal, Eta, Ks, RoughnessSq);
        }
        else
        {
            // Hemispherical lambert emitter.
            brdfLightSample = EvaluateLambertBrdf(diffuse);
        }

        // lightIntensity = lightIntensity in normal direction (often called I0) -> seen area is smaller to the border
        float irradianceLightSample = NdotL / lightDistanceSq; // Needs to be divided by pdf for this sample. Everything was already normalized beforehand though, so no need here!
        radiance += pathThroughput * (irradianceLightSample * lightSampleCos) * brdfLightSample * AreaLightSamples[i].Intensity; // sample radiance contribution
    }
    payload.radiance += radiance / NUM_LIGHT_SAMPLES_PERHIT;

    if (remainingBounces == 0)
    {
        payload.pathThroughput_remainingBounces.y = 0;
        return;
    }

    float2 randomSample = Random2(payload.randomSeed); // todo: use low discrepancy sampling here!
    float3 nextRayDirTS;
    float3 throughput;
    if (IsMetal)
    {
        float3 microfacetNormalTS = SampleGGXVisibleNormal(toViewTS, Roughness, randomSample);
        nextRayDirTS = reflect(-toViewTS, microfacetNormalTS);

        float NdotL = nextRayDirTS.z;
        if (NdotL <= 0.0f)
        {
            payload.pathThroughput_remainingBounces.y = 0;
            return;
        }

        float3 F = FresnelDieletricConductorApprox(Eta, Ks, NdotL);
        //float G1 = GGXSmithMasking(NdotL, NdotV, RoughnessSq);
        //float G2 = GGXSmithGeometricShadowingFunction(NdotL, NdotV, RoughnessSq);
        //throughput = F * (G2 / G1);
        float G2_div_G1 = (2.0f * NdotL) / (NdotL + sqrt(RoughnessSq + (1.0f-RoughnessSq) * NdotL * NdotL));
        throughput = F * G2_div_G1;

        // Without importance sampling - use this to proove previous
        // throughput *= brdfNextSample * cos(Out, N) / pdfNextSampleGen
        // With SampleHemisphereCosine: pdfNextSampleGen = cos(Out, N) / PI
        // -> throughput = brdfNextSample * PI
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

#ifdef RUSSIAN_ROULETTE
    float continuationProbability = saturate(GetLuminance(throughput));
    if (Random(payload.randomSeed) >= continuationProbability) // if continuationProbability is zero, path should be stoped -> >=
    {
        payload.pathThroughput_remainingBounces.y = 0;
        return;
    }
    pathThroughput /= continuationProbability; // Only change in spectrum, no energy loss.
#endif

    pathThroughput *= throughput;
    payload.pathThroughput_remainingBounces = FloatToHalf(pathThroughput, remainingBounces);
    float3 nextRayDir = mul(nextRayDirTS, tangentToWorld);
    payload.nextRayDirection = PackDirection(nextRayDir);
}
