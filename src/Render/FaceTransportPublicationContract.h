#pragma once

#include <cstdint>

namespace cs::engine::face_transport
{
	struct RegistrationPlan
	{
		bool shaderBindings = false;
		bool sharedTargetContributor = false;
	};

	constexpr RegistrationPlan GetRegistrationPlan(bool a_configured) noexcept
	{
		return { .shaderBindings = a_configured, .sharedTargetContributor = a_configured };
	}

	constexpr bool RequiresRegistration(const RegistrationPlan& a_plan) noexcept
	{
		return a_plan.shaderBindings || a_plan.sharedTargetContributor;
	}

	constexpr bool PublicationGenerationMatches(std::uint64_t a_resourceGeneration, std::uint64_t a_publishedGeneration) noexcept
	{
		return a_resourceGeneration != 0 && a_resourceGeneration == a_publishedGeneration;
	}

	struct PublicationState
	{
		std::uint64_t resourceGeneration = 0;
		std::uint64_t publishedGeneration = 0;
		std::uint64_t publicationId = 0;
		std::uint32_t frameSerial = 0;
		bool resourceReady = false;
		bool bindingAttempted = false;
		bool bindingConfirmed = false;
		bool srvBindingConfirmed = false;
		bool published = false;
		bool consumerEligible = false;
		bool rgba8Fallback = false;
		bool contextMatches = false;

		void BeginFrame(
			std::uint32_t a_frameSerial,
			std::uint64_t a_resourceGeneration,
			bool a_resourceReady,
			bool a_rgba8Fallback,
			bool a_contextMatches) noexcept
		{
			resourceGeneration = a_resourceGeneration;
			publishedGeneration = 0;
			frameSerial = a_frameSerial;
			resourceReady = a_resourceReady;
			bindingAttempted = false;
			bindingConfirmed = false;
			srvBindingConfirmed = false;
			published = false;
			consumerEligible = false;
			rgba8Fallback = a_rgba8Fallback;
			contextMatches = a_contextMatches;
		}

		void MarkBindingAttempted(bool a_contextMatches) noexcept
		{
			bindingAttempted = true;
			contextMatches = a_contextMatches;
		}

		void MarkBindingConfirmed(bool a_confirmed) noexcept
		{
			bindingConfirmed = a_confirmed;
		}

		void MarkPublished() noexcept
		{
			publishedGeneration = resourceGeneration;
			published = true;
		}

		bool GenerationMatches() const noexcept
		{
			return PublicationGenerationMatches(resourceGeneration, publishedGeneration);
		}
	};

	constexpr bool CanPublish(
		bool a_inProgress,
		bool a_targetObserved,
		bool a_resourceMatches,
		bool a_srvBound,
		bool a_rtvUnbound,
		bool a_contextMatches = true,
		bool a_bindingAttempted = true,
		bool a_bindingConfirmed = true) noexcept
	{
		return a_inProgress && a_targetObserved && a_resourceMatches && a_srvBound && a_rtvUnbound &&
		       a_contextMatches && a_bindingAttempted && a_bindingConfirmed;
	}

	constexpr bool CanConsume(
		bool a_published,
		bool a_generationMatches,
		bool a_viewMatches,
		bool a_lightMatches,
		bool a_writersSupported,
		bool a_enabled) noexcept
	{
		return a_published && a_generationMatches && a_viewMatches && a_lightMatches && a_writersSupported && a_enabled;
	}
}
