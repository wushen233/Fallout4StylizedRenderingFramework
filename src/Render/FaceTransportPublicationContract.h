#pragma once

namespace cs::engine::face_transport
{
	constexpr bool CanPublish(
		bool a_inProgress,
		bool a_targetObserved,
		bool a_resourceMatches,
		bool a_srvBound,
		bool a_rtvUnbound) noexcept
	{
		return a_inProgress && a_targetObserved && a_resourceMatches && a_srvBound && a_rtvUnbound;
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
