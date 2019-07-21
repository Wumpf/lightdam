[shader("vertex")]
float4 FullScreenTriangle(uint vertexID : SV_VertexID) : SV_POSITION
{
    return float4(vertexID >> 1, vertexID & 1, 0.0f, 0.5f) * 4.0f - 1.0f;
}

Texture2D pathtracerOutput : register(t0);
SamplerState samplePoint : register(s0);

[shader("pixel")]
float4 ToneMap(float4 position : SV_Position) : SV_Target
{
    float2 textureSize;
    pathtracerOutput.GetDimensions(textureSize.x, textureSize.y);
    float4 raw = pathtracerOutput.Sample(samplePoint, position.xy / textureSize);
    return float4(raw.xyz / raw.w, 1.0f);
    //return fmod(raw.wwww, 100.0f) * 0.001f;
}
