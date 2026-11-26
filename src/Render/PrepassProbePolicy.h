#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace cs::engine::prepass_geometry_probe::policy
{
	enum class Domain : std::uint8_t
	{
		kFace,
		kSkinTint
	};

	enum class Path : std::uint8_t
	{
		kImmediate,
		kCommandBuffer
	};

	struct Limits
	{
		std::uint32_t ownerResolutions;
		std::uint32_t samples;
		std::uint32_t commandBufferCreates;
	};

	inline constexpr std::uint32_t kTotalOwnerResolutionLimit = 608;
	inline constexpr std::uint32_t kImmediateSampleInterval = 4;
	inline constexpr std::size_t kActorSlotCount = 3;

	[[nodiscard]] constexpr std::optional<Domain> SelectDomain(
		std::uint32_t a_descriptor,
		std::uint32_t a_faceBit,
		std::uint32_t a_skinTintBit) noexcept
	{
		if ((a_descriptor & a_faceBit) != 0)
			return Domain::kFace;
		if ((a_descriptor & a_skinTintBit) != 0)
			return Domain::kSkinTint;
		return std::nullopt;
	}

	[[nodiscard]] constexpr bool IsTargetDescriptor(
		std::uint32_t a_descriptor,
		std::uint32_t a_faceBit,
		std::uint32_t a_skinTintBit) noexcept
	{
		return (a_descriptor & (a_faceBit | a_skinTintBit)) != 0;
	}

	[[nodiscard]] constexpr Limits GetLimits(Domain a_domain, Path a_path) noexcept
	{
		// Separate reservations keep SkinTint activity from consuming Face capacity.
		if (a_domain == Domain::kFace)
			return a_path == Path::kImmediate ? Limits{ 384, 96, 0 } : Limits{ 96, 32, 16 };
		return a_path == Path::kImmediate ? Limits{ 96, 24, 0 } : Limits{ 32, 8, 8 };
	}
	static_assert(
		GetLimits(Domain::kFace, Path::kImmediate).ownerResolutions +
			GetLimits(Domain::kFace, Path::kCommandBuffer).ownerResolutions +
			GetLimits(Domain::kSkinTint, Path::kImmediate).ownerResolutions +
			GetLimits(Domain::kSkinTint, Path::kCommandBuffer).ownerResolutions ==
		kTotalOwnerResolutionLimit);

	[[nodiscard]] inline bool ReserveBounded(
		std::atomic<std::uint32_t>& a_counter,
		std::uint32_t a_limit,
		std::uint32_t* a_ticket = nullptr) noexcept
	{
		auto current = a_counter.load(std::memory_order_relaxed);
		while (current < a_limit) {
			if (a_counter.compare_exchange_weak(current, current + 1, std::memory_order_relaxed)) {
				if (a_ticket)
					*a_ticket = current;
				return true;
			}
		}
		return false;
	}

	class ActorSlots
	{
	public:
		// These append-only diagnostic slots store FormIDs only, never engine pointers.
		[[nodiscard]] std::optional<std::size_t> Claim(std::uint32_t a_formID, bool a_isPlayer) noexcept
		{
			if (a_formID == 0)
				return std::nullopt;
			if (a_isPlayer)
				return ClaimAt(0, a_formID);

			const auto first = 1 + static_cast<std::size_t>(a_formID % 2);
			for (std::size_t offset = 0; offset < 2; ++offset) {
				const auto index = 1 + ((first - 1 + offset) % 2);
				if (const auto claimed = ClaimAt(index, a_formID))
					return claimed;
			}
			return std::nullopt;
		}

	private:
		[[nodiscard]] std::optional<std::size_t> ClaimAt(std::size_t a_index, std::uint32_t a_formID) noexcept
		{
			auto& slot = _formIDs[a_index];
			auto current = slot.load(std::memory_order_relaxed);
			if (current == a_formID)
				return a_index;
			if (current != 0)
				return std::nullopt;
			if (slot.compare_exchange_strong(current, a_formID, std::memory_order_relaxed))
				return a_index;
			if (current == a_formID)
				return a_index;
			return std::nullopt;
		}

		std::array<std::atomic<std::uint32_t>, kActorSlotCount> _formIDs{};
	};

	struct FrameClaims
	{
		FrameClaims() noexcept
		{
			for (auto& domain : stamps)
				for (auto& path : domain)
					path.store(UINT32_MAX, std::memory_order_relaxed);
		}

		std::array<std::array<std::atomic<std::uint32_t>, 2>, 2> stamps{};
	};

	[[nodiscard]] inline bool ClaimFrame(
		FrameClaims& a_claims,
		Domain a_domain,
		Path a_path,
		std::uint32_t a_frame) noexcept
	{
		auto& lastFrame = a_claims.stamps[static_cast<std::size_t>(a_domain)][static_cast<std::size_t>(a_path)];
		auto previous = lastFrame.load(std::memory_order_relaxed);
		while (previous != a_frame) {
			if (lastFrame.compare_exchange_weak(previous, a_frame, std::memory_order_relaxed))
				return true;
		}
		return false;
	}
}
