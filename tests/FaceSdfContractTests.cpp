#include "FaceSdfContract.h"

#include <array>
#include <cassert>
#include <limits>

int main()
{
	using namespace cs::features::face_sdf;

	EligibilityFacts eligible{
		.featureEnabled = true,
		.explicitProfileMatched = true,
		.faceDescriptor = true,
		.skinTintDescriptor = false,
		.opaquePass = true,
		.immediateDraw = true,
		.actorOwnershipResolved = true,
		.sdfResourceReady = true,
		.sharedTargetReady = true
	};
	assert(IsEligible(eligible));
	auto skinTint = eligible;
	skinTint.skinTintDescriptor = true;
	assert(!IsEligible(skinTint));
	auto noOptIn = eligible;
	noOptIn.explicitProfileMatched = false;
	assert(!IsEligible(noOptIn));
	auto commandBuffer = eligible;
	commandBuffer.immediateDraw = false;
	assert(!IsEligible(commandBuffer));

	constexpr Float3 forward{ 0.0f, 0.0f, 1.0f };
	constexpr Float3 right{ 1.0f, 0.0f, 0.0f };
	constexpr Float3 up{ 0.0f, 1.0f, 0.0f };
	assert(IsValidBasis(forward, right, up));
	assert(!IsValidBasis(forward, Float3{ -1.0f, 0.0f, 0.0f }, up));
	assert(IsValidSdfTextureFormat(49));
	assert(IsValidSdfTextureFormat(34));
	assert(!IsValidSdfTextureFormat(28));

	assert(EvaluateDirectionalSdf({ 1.0f, 0.0f }, 1.0f, 0.03f) > 0.99f);
	assert(EvaluateDirectionalSdf({ 1.0f, 0.0f }, -1.0f, 0.03f) < 0.01f);
	assert(EvaluateDirectionalSdf({ 0.0f, 1.0f }, 0.0f, 0.03f) < 0.01f);
	assert(EvaluateDirectionalSdf({ 0.0f, 0.0f }, 1.0f, 0.0f) == 1.0f);
	assert(EvaluateDirectionalSdf({ std::numeric_limits<float>::quiet_NaN(), 0.0f }, 1.0f, 0.03f) == 1.0f);

	constexpr std::array<Float3, 3> identityRows{
		Float3{ 1.0f, 0.0f, 0.0f },
		Float3{ 0.0f, 1.0f, 0.0f },
		Float3{ 0.0f, 0.0f, 1.0f }
	};
	assert(MatchesDirectionalLight({ 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, identityRows));
	constexpr std::array<Float3, 3> quarterTurnRows{
		Float3{ 0.0f, -1.0f, 0.0f },
		Float3{ 1.0f, 0.0f, 0.0f },
		Float3{ 0.0f, 0.0f, 1.0f }
	};
	assert(MatchesDirectionalLight({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, quarterTurnRows));
	assert(!MatchesDirectionalLight({ 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, quarterTurnRows));
	assert(!MatchesDirectionalLight({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, quarterTurnRows,
		std::numeric_limits<float>::quiet_NaN()));
}
