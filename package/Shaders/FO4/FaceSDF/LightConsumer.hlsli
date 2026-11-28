#ifndef FO4_FACE_SDF_LIGHT_CONSUMER
#define FO4_FACE_SDF_LIGHT_CONSUMER

#include "FO4/FO4ShaderData.hlsli"

cbuffer FaceSdfConsumer : register(b8)
{
	uint FaceSdfConsumerValid;
	uint3 FaceSdfConsumerReserved;
};

float FaceSdfDirectDiffuseFactor(float2 pixelPosition, float3 lightDirectionView, float3 sunDirectionView)
{
	if (FaceSdfConsumerValid == 0 || any(!isfinite(float4(lightDirectionView, 1.0))))
		return 1.0;
	const float lightLengthSq = dot(lightDirectionView, lightDirectionView);
	const float sunLengthSq = dot(sunDirectionView, sunDirectionView);
	if (!isfinite(lightLengthSq) || !isfinite(sunLengthSq) || lightLengthSq < 1.0e-8 || sunLengthSq < 1.0e-8)
		return 1.0;
	const float3 light = lightDirectionView * rsqrt(lightLengthSq);
	const float3 sun = sunDirectionView * rsqrt(sunLengthSq);
	if (dot(light, sun) < 0.9995)
		return 1.0;

	uint width, height;
	ExtendedMaterials::ParallaxShadowTexture.GetDimensions(width, height);
	if (width == 0 || height == 0)
		return 1.0;
	const uint2 texel = min((uint2)pixelPosition, uint2(width - 1, height - 1));
	const float4 payload = ExtendedMaterials::ParallaxShadowTexture.Load(int3(texel, 0));
	// B is cleared for every non-opted draw; R remains the ParallaxShadow channel.
	if (!all(isfinite(payload)) || payload.b < 0.99)
		return 1.0;
	return saturate(payload.g);
}

#endif
