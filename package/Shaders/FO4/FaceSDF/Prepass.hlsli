#ifndef FO4_FACE_SDF_PREPASS
#define FO4_FACE_SDF_PREPASS

#include "FO4/LinearSampler.hlsli"

#if defined(BSDFPREPASS_VS_SOURCE) && defined(FACE_SDF) && FACE && SKINNED && !BLEND && !TESSELLATE_DISP_HEIGHT && !defined(FO4_TERRAIN_BLENDING)
cbuffer FaceSdfProfile : register(b8)
{
	float4 FaceSdfForward;
	float4 FaceSdfRight;
	float4 FaceSdfUp;
	float4 FaceSdfUvScaleBias;
	float4 FaceSdfParameters;  // x enabled, y softness, z strength
};
#endif

#if defined(BSDFPREPASS_PS_SOURCE) && defined(FACE_SDF) && FACE && SKINNED && !BLEND && !TESSELLATE_DISP_HEIGHT && !defined(FO4_TERRAIN_BLENDING)
cbuffer FaceSdfProfile : register(b8)
{
	float4 FaceSdfForward;
	float4 FaceSdfRight;
	float4 FaceSdfUp;
	float4 FaceSdfUvScaleBias;
	float4 FaceSdfParameters;
};

Texture2D<float2> FaceSdfTexture : register(t9);

float EvaluateFaceSdf(float2 uv, float3 forwardView, float3 rightView, float3 upView, float3 sunDirectionView)
{
	if (FaceSdfParameters.x < 0.5 ||
		any(!isfinite(float4(forwardView, 1.0))) || any(!isfinite(float4(rightView, 1.0))) || any(!isfinite(float4(upView, 1.0))))
		return 1.0;

	const float forwardLength = dot(forwardView, forwardView);
	if (!isfinite(forwardLength) || forwardLength < 1.0e-8)
		return 1.0;
	forwardView *= rsqrt(forwardLength);
	rightView -= forwardView * dot(forwardView, rightView);
	const float rightLength = dot(rightView, rightView);
	if (!isfinite(rightLength) || rightLength < 1.0e-8)
		return 1.0;
	rightView *= rsqrt(rightLength);
	upView -= forwardView * dot(forwardView, upView) + rightView * dot(rightView, upView);
	const float upLength = dot(upView, upView);
	if (!isfinite(upLength) || upLength < 1.0e-8)
		return 1.0;
	upView *= rsqrt(upLength);
	if (dot(cross(rightView, upView), forwardView) < 0.75)
		return 1.0;

	uint width, height;
	FaceSdfTexture.GetDimensions(width, height);
	if (width == 0 || height == 0)
		return 1.0;
	const float lightLength = dot(sunDirectionView, sunDirectionView);
	if (!isfinite(lightLength) || lightLength < 1.0e-8)
		return 1.0;
	const float2 sdf = FaceSdfTexture.Sample(LinearSampler, uv * FaceSdfUvScaleBias.zw + FaceSdfUvScaleBias.xy);
	if (any(!isfinite(sdf)))
		return 1.0;
	const float signedDistance = dot(sunDirectionView * rsqrt(lightLength), rightView) >= 0.0 ? sdf.x : sdf.y;
	const float softness = FaceSdfParameters.y;
	if (!isfinite(softness) || softness <= 0.001 || softness > 0.25)
		return 1.0;
	const float visibility = smoothstep(0.5 - softness, 0.5 + softness, signedDistance);
	return lerp(1.0, visibility, saturate(FaceSdfParameters.z));
}
#endif

#endif
