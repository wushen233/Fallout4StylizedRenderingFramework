#include "Render/FaceTransportPublicationContract.h"

#include <array>
#include <cassert>

int main()
{
	using cs::engine::face_transport::CanConsume;
	using cs::engine::face_transport::CanPublish;
	using cs::engine::face_transport::GetRegistrationPlan;
	using cs::engine::face_transport::PublicationGenerationMatches;
	using cs::engine::face_transport::PublicationState;

	const auto disabled = GetRegistrationPlan(false);
	assert(!disabled.shaderBindings && !disabled.sharedTargetContributor);
	const auto enabled = GetRegistrationPlan(true);
	assert(enabled.shaderBindings && enabled.sharedTargetContributor);
	assert(CanPublish(true, true, true, true, true));
	for (std::size_t failed = 0; failed < 5; ++failed) {
		std::array<bool, 5> facts{ true, true, true, true, true };
		facts[failed] = false;
		assert(!CanPublish(facts[0], facts[1], facts[2], facts[3], facts[4]));
	}
	assert(!CanPublish(true, true, true, true, true, false));              // Immediate context differs.
	assert(!CanPublish(true, true, true, true, true, true, false));        // No binding attempt.
	assert(!CanPublish(true, true, true, true, true, true, true, false));  // No actual RTV confirmation.
	assert(!CanPublish(true, true, false, true, true));                    // Invalid resource/publication transaction.

	PublicationState state;
	state.BeginFrame(100, 7, true, true, true);
	state.bindingAttempted = true;
	state.bindingConfirmed = true;
	state.srvBindingConfirmed = true;
	state.published = true;
	state.publishedGeneration = 7;
	state.publicationId = 12;
	state.consumerEligible = false;
	assert(state.GenerationMatches());
	assert(state.rgba8Fallback);
	assert(state.bindingAttempted && state.bindingConfirmed);
	assert(state.published && !state.consumerEligible);
	state.BeginFrame(101, 8, false, false, false);
	assert(!state.resourceReady && !state.bindingAttempted && !state.bindingConfirmed);
	assert(!state.srvBindingConfirmed && !state.published && !state.consumerEligible);
	assert(!state.GenerationMatches());
	assert(state.publicationId == 12);  // Publication IDs remain monotonic across frame reset.
	assert(PublicationGenerationMatches(8, 8));
	assert(!PublicationGenerationMatches(8, 7));
	assert(!PublicationGenerationMatches(0, 0));
	state.resourceReady = true;
	state.MarkBindingAttempted(true);
	assert(state.bindingAttempted && !state.bindingConfirmed);
	assert(!CanPublish(true, true, true, true, true, state.contextMatches, state.bindingAttempted, state.bindingConfirmed));
	state.BeginFrame(102, 9, true, false, false);
	state.MarkBindingAttempted(false);
	assert(!CanPublish(true, true, true, true, true, state.contextMatches, state.bindingAttempted, state.bindingConfirmed));
	assert(!state.contextMatches);
	state.BeginFrame(103, 10, true, true, true);
	state.MarkBindingAttempted(true);
	state.MarkBindingConfirmed(true);
	assert(CanPublish(true, true, true, true, true, state.contextMatches, state.bindingAttempted, state.bindingConfirmed));
	state.MarkPublished();
	assert(state.published && state.GenerationMatches());
	assert(!CanConsume(true, true, true, false, true, true));  // View identity absent.
	assert(!CanConsume(true, true, true, true, false, true));  // Unsupported writer.
	assert(!CanConsume(true, true, true, true, true, false));  // Candidate disabled.
	assert(CanConsume(true, true, true, true, true, true));
}
