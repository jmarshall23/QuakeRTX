// Ray payload for the second bounce
#include "Common.hlsl"

RWTexture2D<float4> gLightOutput : register(u1);

[shader("closesthit")] void SecondClosestHit(inout SecondHitInfo hit,
                                             Attributes bary) {
  hit.payload_color.xyz = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
  hit.payload_color.w = 1;
}

[shader("miss")] void SecondMiss(inout SecondHitInfo hit
                                  : SV_RayPayload) {
  
}