#include "Common.hlsl"

struct Vertex
{
    float3 vertex;
    float3 normal;
};

StructuredBuffer<Vertex> VertexBuffer : register(t0, space1);
StructuredBuffer<uint> IndexBuffer : register(t0, space2);

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

[shader("closesthit")]
export void ClosestHit(inout RadianceRayHitInfo payload, Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint primitiveIdx = 3 * PrimitiveIndex();
    uint vertexIdx0 = IndexBuffer[primitiveIdx + 0];
    uint vertexIdx1 = IndexBuffer[primitiveIdx + 1];
    uint vertexIdx2 = IndexBuffer[primitiveIdx + 2];
    float3 normal = normalize(BarycentricLerp(VertexBuffer[vertexIdx0].normal, VertexBuffer[vertexIdx1].normal, VertexBuffer[vertexIdx2].normal, barycentrics));
    float3 worldPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    // TODO
    float3 dirToLight = normalize(float3(2.0f, 1.0f, 4.0f)); // todo

    // TODO proper brdf etc.
    float radianceFromLight = saturate(dot(normal, dirToLight));
    if (ShadowRay(worldPosition, dirToLight))
        radianceFromLight = 0.0f;
    
    payload.distance = RayTCurrent();
    uint remainingBounces = 0;
    if (payload.distance > 0.0f)
        remainingBounces = (payload.radiance_remainingBounces.y >> 16) - 1;

    payload.radiance_remainingBounces = FloatToHalf(radianceFromLight.xxx, remainingBounces);

    // todo: russion roulette, ray throughput

    if (remainingBounces > 0)
    {
        float3 U, V;
	    CreateONB(normal, U, V);
        float3 nextRayDir = SampleHemisphereCosine(Random2(payload.randomSeed), U, V, normal);
        payload.nextRayDirection = PackDirection(nextRayDir);
    }
}
