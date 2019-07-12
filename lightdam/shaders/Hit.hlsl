#include "Common.hlsl"

struct Vertex
{
    float3 vertex;
    float3 normal;
};

StructuredBuffer<Vertex> VertexBuffer : register(t0, space0);
StructuredBuffer<uint> IndexBuffer : register(t0, space1);

[shader("closesthit")]
export void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint primitiveIdx = 3 * PrimitiveIndex();
    uint vertexIdx0 = IndexBuffer[primitiveIdx + 0];
    uint vertexIdx1 = IndexBuffer[primitiveIdx + 1];
    uint vertexIdx2 = IndexBuffer[primitiveIdx + 2];
    float3 normal = normalize(BarycentricLerp(VertexBuffer[vertexIdx0].normal, VertexBuffer[vertexIdx1].normal, VertexBuffer[vertexIdx2].normal, barycentrics));

    float light = saturate(dot(normal, normalize(float3(0.0f, 1.0f, 1.0f))));
    payload.colorAndDistance = float4(light, light, light, RayTCurrent());
    //payload.colorAndDistance = float4(normal * 0.5f + 0.5f, RayTCurrent());
}
