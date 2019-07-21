#include "Common.hlsl"

struct Vertex
{
    float3 vertex;
    float3 normal;
};

StructuredBuffer<Vertex> VertexBuffer : register(t0, space1);
StructuredBuffer<uint> IndexBuffer : register(t0, space2);

[shader("closesthit")]
export void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint primitiveIdx = 3 * PrimitiveIndex();
    uint vertexIdx0 = IndexBuffer[primitiveIdx + 0];
    uint vertexIdx1 = IndexBuffer[primitiveIdx + 1];
    uint vertexIdx2 = IndexBuffer[primitiveIdx + 2];
    float3 normal = normalize(BarycentricLerp(VertexBuffer[vertexIdx0].normal, VertexBuffer[vertexIdx1].normal, VertexBuffer[vertexIdx2].normal, barycentrics));

    float3 dirToLight = normalize(float3(2.0f, 1.0f, 4.0f)); // todo

    float3 worldPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    RayDesc shadowRay;
    shadowRay.Origin = worldPosition;
    shadowRay.Direction = dirToLight;
    shadowRay.TMin = 0.01f;
    shadowRay.TMax = 100000;

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


    float light = saturate(dot(normal, dirToLight));
    if (shadowPayLoad.isHit)
        light = 0.0f;
    payload.colorAndDistance = float4(light, light, light, RayTCurrent());
    //payload.colorAndDistance = float4(normal * 0.5f + 0.5f, RayTCurrent());
}
