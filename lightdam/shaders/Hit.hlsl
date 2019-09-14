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

    float NdotV = dot(toView, hit.normal);

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
        
        // Hemispherical lambert emitter.
        // lightIntensity = lightIntensity in normal direction (often called I0) -> seen area is smaller to the border
        // We factor this in last, to keep the scalar factors together (== multiplying by lightSampleCos)

        float3 eta = float3(2.865601, 2.119182, 1.940077);
        float3 k = float3(3.032326, 2.056108, 1.616293);
        float roughness = 0.1;
        float3 brdfLightSample = EvaluateMicrofacetBrdf(NdotL, toLight, NdotV, toView, hit.normal, eta, k, roughness);
        //float3 brdfLightSample = EvaluateLambertBrdf(diffuse);

        float irradianceLightSample = NdotL / lightDistanceSq; // Need to divide by pdf for this sample. Everything was already normalized beforehand though, so no need here!

        radiance += pathThroughput * (irradianceLightSample * lightSampleCos) * brdfLightSample * AreaLightSamples[i].Intensity; // sample radiance contribution
    }
    payload.radiance += radiance / NUM_LIGHT_SAMPLES_PERHIT;


    // pathpathThroughput *= brdfNextSample * cos(Out, N) / pdfNextSampleGen
    // With SampleHemisphereCosine: pdfNextSampleGen = cos(Out, N) / PI
    // Lambert brdf: brdfNextSample = diffuse / PI;
    // -> throughput for Lambert: diffuse
    float3 throughput = diffuse; // brdfNextSample * saturate(dot(nextRayDir, hit.normal)) / pdf;

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

    float3 U, V;
    CreateONB(hit.normal, U, V);
    float3 nextRayDir = SampleHemisphereCosine(Random2(payload.randomSeed), U, V, hit.normal); // todo: use low discrepancy sampling here!
    payload.nextRayDirection = PackDirection(nextRayDir);
}
