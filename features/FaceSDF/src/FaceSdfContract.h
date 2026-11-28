#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace cs::features::face_sdf
{
	using Float2 = std::array<float, 2>;
	using Float3 = std::array<float, 3>;

	struct EligibilityFacts
	{
		bool featureEnabled = false;
		bool explicitProfileMatched = false;
		bool faceDescriptor = false;
		bool skinTintDescriptor = false;
		bool opaquePass = false;
		bool immediateDraw = false;
		bool actorOwnershipResolved = false;
		bool sdfResourceReady = false;
		bool sharedTargetReady = false;
	};

	constexpr bool IsEligible(const EligibilityFacts& a_facts) noexcept
	{
		return a_facts.featureEnabled && a_facts.explicitProfileMatched &&
		       a_facts.faceDescriptor && !a_facts.skinTintDescriptor && a_facts.opaquePass &&
		       a_facts.immediateDraw && a_facts.actorOwnershipResolved &&
		       a_facts.sdfResourceReady && a_facts.sharedTargetReady;
	}

	inline bool IsFinite(const Float3& a_value) noexcept
	{
		return std::ranges::all_of(a_value, [](float a_component) { return std::isfinite(a_component); });
	}

	inline float Dot(const Float3& a_lhs, const Float3& a_rhs) noexcept
	{
		return a_lhs[0] * a_rhs[0] + a_lhs[1] * a_rhs[1] + a_lhs[2] * a_rhs[2];
	}

	inline Float3 Cross(const Float3& a_lhs, const Float3& a_rhs) noexcept
	{
		return {
			a_lhs[1] * a_rhs[2] - a_lhs[2] * a_rhs[1],
			a_lhs[2] * a_rhs[0] - a_lhs[0] * a_rhs[2],
			a_lhs[0] * a_rhs[1] - a_lhs[1] * a_rhs[0]
		};
	}

	inline bool IsValidBasis(
		const Float3& a_forward,
		const Float3& a_right,
		const Float3& a_up,
		float a_tolerance = 0.01f) noexcept
	{
		if (!IsFinite(a_forward) || !IsFinite(a_right) || !IsFinite(a_up))
			return false;
		constexpr float kUnitTolerance = 0.02f;
		const auto unitLength = [](const Float3& a_axis) {
			return std::abs(Dot(a_axis, a_axis) - 1.0f) <= kUnitTolerance;
		};
		if (!unitLength(a_forward) || !unitLength(a_right) || !unitLength(a_up))
			return false;
		if (std::abs(Dot(a_forward, a_right)) > a_tolerance ||
			std::abs(Dot(a_forward, a_up)) > a_tolerance ||
			std::abs(Dot(a_right, a_up)) > a_tolerance)
			return false;
		const auto handedness = Dot(Cross(a_right, a_up), a_forward);
		return std::isfinite(handedness) && handedness >= 1.0f - a_tolerance;
	}

	inline bool IsValidSdfTextureFormat(unsigned int a_dxgiFormat) noexcept
	{
		// DXGI_FORMAT_R8G8_UNORM and DXGI_FORMAT_R16G16_FLOAT. Neither is sRGB.
		return a_dxgiFormat == 49u || a_dxgiFormat == 34u;
	}

	inline float Smoothstep(float a_edge0, float a_edge1, float a_value) noexcept
	{
		const auto t = std::clamp((a_value - a_edge0) / (a_edge1 - a_edge0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	inline float EvaluateDirectionalSdf(Float2 a_rg, float a_lightRight, float a_softness) noexcept
	{
		if (!std::isfinite(a_rg[0]) || !std::isfinite(a_rg[1]) ||
			!std::isfinite(a_lightRight) || !std::isfinite(a_softness) || a_softness <= 0.0f || a_softness > 0.5f)
			return 1.0f;
		const auto distance = a_lightRight >= 0.0f ? a_rg[0] : a_rg[1];
		return Smoothstep(0.5f - a_softness, 0.5f + a_softness, distance);
	}

	inline bool MatchesDirectionalLight(
		const Float3& a_publishedWorldDirection,
		const Float3& a_consumerViewDirection,
		const std::array<Float3, 3>& a_worldToViewRows,
		float a_minDot = 0.999f) noexcept
	{
		if (!IsFinite(a_publishedWorldDirection) || !IsFinite(a_consumerViewDirection) ||
			!IsFinite(a_worldToViewRows[0]) || !IsFinite(a_worldToViewRows[1]) || !IsFinite(a_worldToViewRows[2]) ||
			!std::isfinite(a_minDot) || a_minDot < -1.0f || a_minDot > 1.0f)
			return false;
		const auto lengthSq = Dot(a_publishedWorldDirection, a_publishedWorldDirection);
		const auto consumerLengthSq = Dot(a_consumerViewDirection, a_consumerViewDirection);
		if (lengthSq < 1.0e-8f || consumerLengthSq < 1.0e-8f)
			return false;
		const Float3 worldDirection{
			a_publishedWorldDirection[0] / std::sqrt(lengthSq),
			a_publishedWorldDirection[1] / std::sqrt(lengthSq),
			a_publishedWorldDirection[2] / std::sqrt(lengthSq)
		};
		const Float3 viewDirection{
			a_consumerViewDirection[0] / std::sqrt(consumerLengthSq),
			a_consumerViewDirection[1] / std::sqrt(consumerLengthSq),
			a_consumerViewDirection[2] / std::sqrt(consumerLengthSq)
		};
		Float3 transformed{
			Dot(a_worldToViewRows[0], worldDirection),
			Dot(a_worldToViewRows[1], worldDirection),
			Dot(a_worldToViewRows[2], worldDirection)
		};
		const auto transformedLengthSq = Dot(transformed, transformed);
		if (!std::isfinite(transformedLengthSq) || transformedLengthSq < 1.0e-8f)
			return false;
		for (auto& value : transformed)
			value /= std::sqrt(transformedLengthSq);
		return Dot(transformed, viewDirection) >= a_minDot;
	}
}
