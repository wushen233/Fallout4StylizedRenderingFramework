#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <dxgiformat.h>

struct ID3D11ShaderResourceView;

namespace cs
{
	class Feature;
}

namespace cs::engine
{
	struct ShaderReplacementRegistration;

	// A feature-owned target the deferred prepass writes beside the G-buffer.
	struct PrepassExtraTarget
	{
		std::string name;                          // debug-name prefix
		std::uint32_t rtvSlot = 0;                 // SV_Target index, past the engine's six
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;  // screen-sized, render target and SRV
		std::array<float, 4> clear{};
		std::optional<std::uint32_t> publishSlot;  // pixel SRV slot bound after the prepass
		std::function<bool()> active;              // evaluated once per prepass start
	};

	using PrepassExtraTargetId = std::size_t;

	// Load/OnPostPostLoad only; nullopt if the slot is taken or hooks failed.
	[[nodiscard]] std::optional<PrepassExtraTargetId> RegisterPrepassExtraTarget(Feature& a_owner, PrepassExtraTarget a_target);
	// RT6 is shared only by the existing Parallax producer and the bounded Face transport diagnostic.
	[[nodiscard]] std::optional<PrepassExtraTargetId> RegisterParallaxFaceTransportTarget(
		Feature& a_owner, std::function<bool()> a_active, bool a_requestRGBA8);
	void AttachParallaxFaceTransportTarget(ShaderReplacementRegistration& a_registration);
	// Plugs the target bind into a feature's kDeferredPrepass registration.
	void AttachPrepassExtraTargets(ShaderReplacementRegistration& a_registration);
	// The target's SRV for a compute consumer; null unless written this frame.
	[[nodiscard]] ID3D11ShaderResourceView* GetPrepassExtraTargetSRV(PrepassExtraTargetId a_id) noexcept;
	struct PrepassExtraTargetPublication
	{
		enum class Veto : std::uint8_t
		{
			kNone,
			kDiagnosticFormatUnavailable,
			kUnverifiedViewAndLightAssociation
		};

		ID3D11ShaderResourceView* srv = nullptr;
		std::uint64_t resourceGeneration = 0;
		std::uint64_t publicationId = 0;
		std::uint32_t frameSerial = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		bool published = false;
		bool consumerEligible = false;
		Veto veto = Veto::kUnverifiedViewAndLightAssociation;
	};
	[[nodiscard]] PrepassExtraTargetPublication GetPrepassExtraTargetPublication(PrepassExtraTargetId a_id) noexcept;
}
