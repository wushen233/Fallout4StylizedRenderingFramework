#pragma once

#include <array>
#include <cstdint>

namespace RE
{
	class BSRenderPass;
}

namespace cs::engine
{
	enum class PrepassBakePath;

	namespace prepass_geometry_probe
	{
		void Configure() noexcept;
		void ReportLaneActivation(bool a_attempted, bool a_hooksInstalled, bool a_classifierRegistered) noexcept;
		void RecordLaneBake(
			const RE::BSRenderPass* a_pass,
			PrepassBakePath a_path,
			std::int32_t a_laneOffset,
			const std::array<float, 4>& a_lane,
			bool a_written) noexcept;
		void RecordCommandBufferCreate(
			const RE::BSRenderPass* a_pass,
			PrepassBakePath a_path,
			const void* a_record) noexcept;
	}
}
