#include "Render/PrepassGeometryProbe.h"

#include "Log.h"
#include "Render/Engine.h"
#include "Render/PrepassProbePolicy.h"
#include "Render/PrepassTechnique.h"
#include "Settings/FeatureConfig.h"
#include "Settings/SettingsRegistry.h"

#include "RE/A/Actor.h"
#include "RE/B/BSRenderPass.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESObjectREFR.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace cs::engine::prepass_geometry_probe
{
	namespace
	{
		auto* L = cs::log::Get("cs.render.prepassgeometryprobe");
		using policy::Domain;
		using policy::Path;

		constexpr std::uint32_t kFaceBit = prepass_technique::kFace;
		constexpr std::uint32_t kSkinTintBit = prepass_technique::kSkinTint;
		constexpr std::uint32_t kMaxUnresolvedLogsPerCell = 4;

		struct BudgetCell
		{
			std::atomic<std::uint32_t> ownerResolutions{ 0 };
			std::atomic<std::uint32_t> samples{ 0 };
			std::atomic<std::uint32_t> commandBufferCreates{ 0 };
			std::atomic<std::uint32_t> unresolvedLogs{ 0 };
		};

		struct ActorSampleSlot
		{
			policy::FrameClaims frameClaims{};
		};

		struct OwnerIdentity
		{
			RE::Actor* actor{};
			std::uint32_t formID{};
			bool isPlayer{};
			const char* failure{ "none" };
		};

		std::atomic_bool g_enabled{ false };
		std::atomic<std::uint32_t> g_totalOwnerResolutions{ 0 };
		std::array<std::array<BudgetCell, 2>, 2> g_budgets{};
		policy::ActorSlots g_actorSlots{};
		std::array<ActorSampleSlot, policy::kActorSlotCount> g_actorSampleSlots{};

		[[nodiscard]] constexpr std::size_t DomainIndex(Domain a_domain) noexcept
		{
			return static_cast<std::size_t>(a_domain);
		}

		[[nodiscard]] constexpr std::size_t PathIndex(Path a_path) noexcept
		{
			return static_cast<std::size_t>(a_path);
		}

		[[nodiscard]] constexpr bool IsTargetDescriptor(std::uint32_t a_descriptor) noexcept
		{
			return policy::IsTargetDescriptor(a_descriptor, kFaceBit, kSkinTintBit);
		}

		[[nodiscard]] constexpr Path GetPath(PrepassBakePath a_path) noexcept
		{
			return a_path == PrepassBakePath::kImmediate ? Path::kImmediate : Path::kCommandBuffer;
		}

		[[nodiscard]] constexpr const char* PathName(Path a_path) noexcept
		{
			return a_path == Path::kImmediate ? "Immediate" : "CommandBuffer";
		}

		[[nodiscard]] constexpr const char* DomainName(Domain a_domain) noexcept
		{
			return a_domain == Domain::kFace ? "Face" : "SkinTint";
		}

		[[nodiscard]] BudgetCell& GetBudget(Domain a_domain, Path a_path) noexcept
		{
			return g_budgets[DomainIndex(a_domain)][PathIndex(a_path)];
		}

		[[nodiscard]] bool ReserveOwnerResolution(BudgetCell& a_budget, Domain a_domain, Path a_path) noexcept
		{
			const auto limits = policy::GetLimits(a_domain, a_path);
			return policy::ReserveBounded(a_budget.ownerResolutions, limits.ownerResolutions) &&
			       policy::ReserveBounded(g_totalOwnerResolutions, policy::kTotalOwnerResolutionLimit);
		}

		[[nodiscard]] OwnerIdentity ResolveActorOwner(const RE::BSRenderPass* a_pass) noexcept
		{
			// Match the existing SSS ownership lookup; retain only a transient Actor and its FormID.
			OwnerIdentity result;
			if (!a_pass || !a_pass->geometry) {
				result.failure = "pass_or_geometry_null";
				return result;
			}
			auto* reference = RE::TESObjectREFR::FindReferenceFor3D(a_pass->geometry);
			if (!reference) {
				result.failure = "FindReferenceFor3D_no_owner";
				return result;
			}
			result.actor = reference->As<RE::Actor>();
			if (!result.actor) {
				result.failure = "owner_not_actor";
				return result;
			}
			result.formID = result.actor->GetFormID();
			if (result.formID == 0) {
				result.failure = "actor_form_id_unavailable";
				return result;
			}
			result.isPlayer = result.actor == RE::PlayerCharacter::GetSingleton();
			return result;
		}

		[[nodiscard]] std::optional<std::size_t> ClaimActor(
			const OwnerIdentity& a_owner) noexcept
		{
			if (!a_owner.actor || a_owner.formID == 0)
				return std::nullopt;
			return g_actorSlots.Claim(a_owner.formID, a_owner.isPlayer);
		}

		void LogOwnerEvent(
			const char* a_event,
			const char* a_ownerStatus,
			const RE::BSRenderPass* a_pass,
			Domain a_domain,
			Path a_path,
			std::int32_t a_laneOffset,
			const std::array<float, 4>* a_lane,
			bool a_laneWritten,
			const OwnerIdentity& a_owner,
			std::uint32_t a_frame)
		{
			const auto descriptor = a_pass ? a_pass->passEnum : 0u;
			const auto* geometry = a_pass ? a_pass->geometry : nullptr;
			const std::string lane = a_lane ?
			                             std::format("[{:.0f},{:.0f},{:.0f},{:.0f}]", (*a_lane)[0], (*a_lane)[1], (*a_lane)[2], (*a_lane)[3]) :
			                             "unavailable";
			const auto role = a_owner.actor ? (a_owner.isPlayer ? "player" : "non_player") : "unresolved";
			L->info(
				"ARCH01D event={} frame={} domain={} path={} replay_draw=unobserved pass={:p} descriptor=0x{:08X} geometry_id={:p} owner_status={} actor_role={} actor_form=0x{:08X} actor_id={:p} lane_offset={} lane={} lane_written={} head_transform=unavailable_no_verified_pose_sync",
				a_event, a_frame, DomainName(a_domain), PathName(a_path), static_cast<const void*>(a_pass), descriptor,
				static_cast<const void*>(geometry), a_ownerStatus,
				role, a_owner.formID, static_cast<const void*>(a_owner.actor), a_laneOffset, lane, a_laneWritten);
		}

		void LogUnresolved(
			BudgetCell& a_budget,
			const RE::BSRenderPass* a_pass,
			Domain a_domain,
			Path a_path,
			std::int32_t a_laneOffset,
			const std::array<float, 4>* a_lane,
			bool a_laneWritten,
			const OwnerIdentity& a_owner,
			std::uint32_t a_frame)
		{
			if (policy::ReserveBounded(a_budget.unresolvedLogs, kMaxUnresolvedLogsPerCell))
				LogOwnerEvent("owner_unresolved", a_owner.failure, a_pass, a_domain, a_path,
					a_laneOffset, a_lane, a_laneWritten, a_owner, a_frame);
		}
	}

	void Configure() noexcept
	{
		try {
			std::string error;
			const auto settings = settings::ParseCoreSection(
				settings::core::kDiagnostics, feature_config::GetRoot(), "diagnostics", error);
			if (!error.empty())
				L->warn("ARCH-01D probe setting: {}; using the default", error);
			g_enabled.store(settings.prepassGeometryProbe, std::memory_order_release);
			L->info("ARCH01D activation config_enabled={}", settings.prepassGeometryProbe);
		} catch (...) {
			g_enabled.store(false, std::memory_order_release);
			L->warn("ARCH01D activation config_enabled=false reason=config_parse_exception");
		}
	}

	void ReportLaneActivation(bool a_attempted, bool a_hooksInstalled, bool a_classifierRegistered) noexcept
	{
		if (!g_enabled.load(std::memory_order_acquire)) {
			L->info("ARCH01D activation config_enabled=false lane_hook_attempted={} lane_hooks_installed={} classifier_registered={}",
				a_attempted, a_hooksInstalled, a_classifierRegistered);
			return;
		}
		L->info("ARCH01D activation config_enabled=true lane_hook_attempted={} lane_hooks_installed={} classifier_registered={}",
			a_attempted, a_hooksInstalled, a_classifierRegistered);
		if (a_attempted && !a_hooksInstalled)
			L->warn("ARCH01D probe cannot sample: existing Prepass lane hooks did not all install successfully");
		if (!a_classifierRegistered) {
			L->warn("ARCH01D probe cannot sample: no existing Prepass classifier registered; load Subsurface Scattering for the documented test setup");
		} else if (a_hooksInstalled) {
			L->info("ARCH01D probe ready; Head/Root/Face transforms remain unavailable without a verified synchronized pose callback");
		}
	}

	void RecordLaneBake(
		const RE::BSRenderPass* a_pass,
		PrepassBakePath a_path,
		std::int32_t a_laneOffset,
		const std::array<float, 4>& a_lane,
		bool a_written) noexcept
	{
		if (!g_enabled.load(std::memory_order_acquire) || !a_pass || !IsTargetDescriptor(a_pass->passEnum))
			return;

		const auto selectedDomain = policy::SelectDomain(a_pass->passEnum, kFaceBit, kSkinTintBit);
		if (!selectedDomain)
			return;
		const auto domain = *selectedDomain;
		const auto path = GetPath(a_path);
		const auto frame = engine::GetFrameCount();
		if (path == Path::kImmediate && frame % policy::kImmediateSampleInterval != 0)
			return;

		auto& budget = GetBudget(domain, path);
		const auto limits = policy::GetLimits(domain, path);
		if (budget.samples.load(std::memory_order_relaxed) >= limits.samples)
			return;
		if (!ReserveOwnerResolution(budget, domain, path))
			return;
		try {
			const auto owner = ResolveActorOwner(a_pass);
			if (!owner.actor || owner.formID == 0) {
				LogUnresolved(budget, a_pass, domain, path, a_laneOffset, &a_lane, a_written, owner, frame);
				return;
			}
			const auto slotIndex = ClaimActor(owner);
			if (!slotIndex || !policy::ClaimFrame(g_actorSampleSlots[*slotIndex].frameClaims, domain, path, frame))
				return;
			if (!policy::ReserveBounded(budget.samples, limits.samples))
				return;
			LogOwnerEvent("lane_bake", "resolved_actor", a_pass, domain, path,
				a_laneOffset, &a_lane, a_written, owner, frame);
		} catch (...) {
			// This contains diagnostic formatting/logging failures; it provides no engine-object synchronization.
		}
	}

	void RecordCommandBufferCreate(
		const RE::BSRenderPass* a_pass,
		PrepassBakePath a_path,
		const void* a_record) noexcept
	{
		if (!g_enabled.load(std::memory_order_acquire) || a_path != PrepassBakePath::kCommandBuffer ||
			!a_pass || !IsTargetDescriptor(a_pass->passEnum))
			return;

		const auto selectedDomain = policy::SelectDomain(a_pass->passEnum, kFaceBit, kSkinTintBit);
		if (!selectedDomain)
			return;
		const auto domain = *selectedDomain;
		constexpr auto path = Path::kCommandBuffer;
		auto& budget = GetBudget(domain, path);
		const auto limits = policy::GetLimits(domain, path);
		std::uint32_t createOrdinal = 0;
		if (!policy::ReserveBounded(budget.commandBufferCreates, limits.commandBufferCreates, &createOrdinal) ||
			!ReserveOwnerResolution(budget, domain, path))
			return;
		try {
			const auto owner = ResolveActorOwner(a_pass);
			if (!owner.actor || owner.formID == 0) {
				LogUnresolved(budget, a_pass, domain, path, -1, nullptr, false, owner, engine::GetFrameCount());
				return;
			}
			if (!ClaimActor(owner))
				return;
			const auto frame = engine::GetFrameCount();
			const auto descriptor = a_pass->passEnum;
			const auto* geometry = a_pass->geometry;
			L->info(
				"ARCH01D event=command_buffer_create frame={} domain={} path=CommandBuffer replay_draw=unobserved create_ordinal={} pass={:p} descriptor=0x{:08X} geometry_id={:p} actor_form=0x{:08X} actor_role={} actor_id={:p} record_id={:p} head_transform=unavailable_no_verified_pose_sync",
				frame, DomainName(domain), createOrdinal, static_cast<const void*>(a_pass), descriptor,
				static_cast<const void*>(geometry), owner.formID, owner.isPlayer ? "player" : "non_player",
				static_cast<const void*>(owner.actor), a_record);
		} catch (...) {
			// This contains diagnostic formatting/logging failures; it provides no engine-object synchronization.
		}
	}
}
