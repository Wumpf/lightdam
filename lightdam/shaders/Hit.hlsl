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
    float3 diffuseColor = float3(0.8f, 0.8f, 0.8f);
    float3 dirToLight = normalize(float3(2.0f, 1.0f, 4.0f)); // todo

    // TODO proper brdf etc.
    float3 radianceFromLight = saturate(dot(normal, dirToLight)) * diffuseColor;
    if (ShadowRay(worldPosition, dirToLight))
        radianceFromLight = 0.0f;
    
    payload.distance = RayTCurrent();

    uint remainingBounces;
    float3 pathThroughput = HalfToFloat(payload.pathThroughput_remainingBounces, remainingBounces);
    remainingBounces -= 1;
    payload.radiance = radianceFromLight * pathThroughput;
    
    if (remainingBounces == 0)
    {
        payload.pathThroughput_remainingBounces.y = 0;
        return;
    }

    // pathpathThroughput *= brdf * cos(Out, N) / pdf
    // With SampleHemisphereCosine: pdf == cos(Out, N) / PI
    // Lambert brdf: DiffuseColor / PI;
    float3 throughput = diffuseColor;

#ifdef RUSSIAN_ROULETTE
    float continuationPropability = saturate(GetLuminance(throughput));
    if (Random(payload.randomSeed) >= continuationPropability) // if continuationPropability is zero, path should be stoped -> >=
    {
        payload.pathThroughput_remainingBounces.y = 0;
        return;
    }
    pathThroughput /= continuationPropability; // Only change in spectrum, no energy loss.
#endif

    pathThroughput *= throughput;
    payload.pathThroughput_remainingBounces = FloatToHalf(pathThroughput, remainingBounces);

    float3 U, V;
    CreateONB(normal, U, V);
    float3 nextRayDir = SampleHemisphereCosine(Random2(payload.randomSeed), U, V, normal);
    payload.nextRayDirection = PackDirection(nextRayDir);
}
