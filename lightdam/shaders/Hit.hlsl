#include "Common.hlsl"

bool ShadowRay(float3 worldPosition, float3 dirToLight)
{
    RayDesc shadowRay;
    shadowRay.Origin = worldPosition;
    shadowRay.Direction = dirToLight;
    shadowRay.TMin = DefaultRayTMin;
    shadowRay.TMax = DefaultRayTMax;

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

#define RUSSIAN_ROULETTE

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
    float3 worldPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    // TODO
    float3 dirToLight = normalize(float3(2.0f, 1.0f, 4.0f)); // todo

    float3 brdfLightSample = Diffuse / PI;
    float pdfLightSample = 1.0f / PI_2;
    float irradianceLightSample = saturate(dot(normal, dirToLight)) / pdfLightSample;
    if (ShadowRay(worldPosition, dirToLight))
        irradianceLightSample = 0.0f;
    
    payload.distance = RayTCurrent();

    uint remainingBounces;
    float3 pathThroughput = HalfToFloat(payload.pathThroughput_remainingBounces, remainingBounces);
    remainingBounces -= 1;
    payload.radiance = pathThroughput * irradianceLightSample * brdfLightSample; // sample radiance contribution
    
    if (remainingBounces == 0)
    {
        payload.pathThroughput_remainingBounces.y = 0;
        return;
    }

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
    float3 nextRayDir = SampleHemisphereCosine(Random2(payload.randomSeed), U, V, normal);
    payload.nextRayDirection = PackDirection(nextRayDir);
}
