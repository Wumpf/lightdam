#include "Common.hlsl"

bool ShadowRay(float3 worldPosition, float3 dirToLight, float lightDistance = DefaultRayTMax)
{
    RayDesc shadowRay;
    shadowRay.Origin = worldPosition;
    shadowRay.Direction = dirToLight;
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

//#define RUSSIAN_ROULETTE

[shader("closesthit")]
export void ClosestHit(inout RadianceRayHitInfo payload, Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint primitiveIdx = 3 * PrimitiveIndex();
    uint vertexIdx0 = IndexBuffers[MeshIndex][primitiveIdx + 0];
    uint vertexIdx1 = IndexBuffers[MeshIndex][primitiveIdx + 1];
    uint vertexIdx2 = IndexBuffers[MeshIndex][primitiveIdx + 2];
    float3 normal = normalize(
        BarycentricLerp(
            VertexBuffers[MeshIndex][vertexIdx0].normal,
            VertexBuffers[MeshIndex][vertexIdx1].normal,
            VertexBuffers[MeshIndex][vertexIdx2].normal, barycentrics)
        );
    [flatten] if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
        normal = -normal;
    float3 worldPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    uint remainingBounces;
    float3 pathThroughput = HalfToFloat(payload.pathThroughput_remainingBounces, remainingBounces);
    remainingBounces -= 1;
    payload.distance = RayTCurrent();
    payload.radiance = float3(0.0f, 0.0f, 0.0f);

#ifdef DEBUG_VISUALIZE_NORMALS
    payload.radiance = normal * 0.5 + float3(0.5f,0.5f,0.5f);
    payload.pathThroughput_remainingBounces.y = 0;
    return;
#endif

    uint randomSampleOffset = RandomUInt(payload.randomSeed) % (NUM_LIGHT_SAMPLES_AVAILABLE - NUM_LIGHT_SAMPLES_PERHIT + 1);
    for (uint i=randomSampleOffset; i<randomSampleOffset + NUM_LIGHT_SAMPLES_PERHIT; ++i)
    {
        float3 dirToLight = AreaLightSamples[i].Position - worldPosition;
        float lightDistSq = dot(dirToLight, dirToLight);
        dirToLight *= rsqrt(lightDistSq);

        float surfaceCos = dot(dirToLight,normal);
        if (surfaceCos <= 0.0f)
            continue;
        float lightSampleCos = dot(-dirToLight, AreaLightSamples[i].Normal);
        if (lightSampleCos <= 0.0f)
            continue;

        // Hemispherical lambert emitter.
        // lightIntensity = lightIntensity in normal direction (often called I0) -> seen area is smaller to the border
        // We factor this in last, to keep the scalar factors together (== multiplying by lightSampleCos)
        float3 brdfLightSample = Diffuse / PI;
        float irradianceLightSample = surfaceCos / lightDistSq; // Need to divide by pdf for this sample. Everything was already normalized beforehand though, so no need here!
        if (ShadowRay(worldPosition, dirToLight, sqrt(lightDistSq)))
            irradianceLightSample = 0.0f;
        
        payload.radiance += (pathThroughput * irradianceLightSample * lightSampleCos) * brdfLightSample * AreaLightSamples[i].Intensity; // sample radiance contribution
    }
    payload.radiance /= NUM_LIGHT_SAMPLES_PERHIT;

    // pathpathThroughput *= brdfNextSample * cos(Out, N) / pdfNextSampleGen
    // With SampleHemisphereCosine: pdfNextSampleGen = cos(Out, N) / PI
    // Lambert brdf: brdfNextSample = Diffuse / PI;
    // -> throughput for Lambert: Diffuse
    float3 throughput = Diffuse; // brdfNextSample * saturate(dot(nextRayDir, normal)) / pdf;

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
    CreateONB(normal, U, V);
    float3 nextRayDir = SampleHemisphereCosine(Random2(payload.randomSeed), U, V, normal); // todo: use low discrepancy sampling here!
    payload.nextRayDirection = PackDirection(nextRayDir);
}
