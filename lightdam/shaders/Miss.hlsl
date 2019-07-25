#include "Common.hlsl"

[shader("miss")]
export void Miss(inout RadianceRayHitInfo payload : SV_RayPayload)
{
	//uint2 launchIndex = DispatchRaysIndex().xy;
	//float2 dims = float2(DispatchRaysDimensions().xy);

	//float ramp = launchIndex.y / dims.y;
	payload.radiance_remainingBounces = uint2(0, 0);
}