#include "Render/PrepassGeometryProbe.h"

#include "Log.h"
#include "Render/Engine.h"
#include "Render/PrepassTechnique.h"
#include "Settings/FeatureConfig.h"
#include "Settings/SettingsRegistry.h"

#include "RE/A/Actor.h"
#include "RE/B/BSFaceGenNiNode.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSRenderPass.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiTransform.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESObjectREFR.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace cs::engine::prepass_geometry_probe
{
	namespace
	{
		auto* L = cs::log::Get("cs.render.prepassgeometryprobe");

		constexpr std::size_t kActorSlots = 3;  // player plus two observed non-player actors
		constexpr std::uint32_t kSampleEveryFrames = 4;
		constexpr std::uint32_t kMaxTransformSamples = 512;
		constexpr std::uint32_t kMaxCommandBufferCreates = 128;
		constexpr std::size_t kMaxParentDepth = 16;

		struct ActorSampleSlot
		{
			std::atomic<std::uint32_t> formID{ 0 };
			std::atomic<std::uint32_t> lastFrame{ UINT32_MAX };
		};

		std::atomic_bool g_enabled{ false };
		std::array<ActorSampleSlot, kActorSlots> g_actorSlots{};
		std::atomic<std::uint32_t> g_transformSamples{ 0 };
		std::atomic<std::uint32_t> g_unresolvedSamples{ 0 };
		std::atomic<std::uint32_t> g_commandBufferCreates{ 0 };

		[[nodiscard]] const char* PathName(PrepassBakePath a_path) noexcept
		{
			return a_path == PrepassBakePath::kImmediate ? "Immediate" : "CommandBuffer";
		}

		[[nodiscard]] bool IsTargetDescriptor(std::uint32_t a_descriptor) noexcept
		{
			return (a_descriptor & (prepass_technique::kFace | prepass_technique::kSkinTint)) != 0;
		}

		[[nodiscard]] bool ReserveBounded(std::atomic<std::uint32_t>& a_counter, std::uint32_t a_limit) noexcept
		{
			auto current = a_counter.load(std::memory_order_relaxed);
			while (current < a_limit) {
				if (a_counter.compare_exchange_weak(current, current + 1, std::memory_order_relaxed))
					return true;
			}
			return false;
		}

		[[nodiscard]] ActorSampleSlot* ClaimActorSlot(RE::Actor* a_actor, std::uint32_t a_formID) noexcept
		{
			if (!a_actor || !a_formID)
				return nullptr;

			const auto* player = RE::PlayerCharacter::GetSingleton();
			const std::size_t first = a_actor == player ? 0 : 1;
			const std::size_t last = a_actor == player ? 1 : kActorSlots;
			for (std::size_t i = first; i < last; ++i) {
				auto& slot = g_actorSlots[i];
				auto current = slot.formID.load(std::memory_order_relaxed);
				if (current == a_formID)
					return &slot;
				if (current == 0 && slot.formID.compare_exchange_strong(current, a_formID, std::memory_order_relaxed))
					return &slot;
			}
			return nullptr;
		}

		[[nodiscard]] bool ClaimFrameSample(ActorSampleSlot& a_slot, std::uint32_t a_frame) noexcept
		{
			if (a_frame % kSampleEveryFrames != 0)
				return false;
			auto previous = a_slot.lastFrame.load(std::memory_order_relaxed);
			while (previous != a_frame) {
				if (a_slot.lastFrame.compare_exchange_weak(previous, a_frame, std::memory_order_relaxed))
					return true;
			}
			return false;
		}

		[[nodiscard]] std::string_view ObjectName(const RE::NiAVObject* a_object) noexcept
		{
			if (!a_object)
				return "<null>";
			const auto* name = a_object->name.c_str();
			return name ? std::string_view{ name } : std::string_view{ "<unnamed>" };
		}

		[[nodiscard]] bool HasHeadToken(std::string_view a_name) noexcept
		{
			return std::ranges::search(a_name, std::string_view{ "head" }, [](char a_lhs, char a_rhs) {
				const auto lower = [](unsigned char a_value) {
					return a_value >= 'A' && a_value <= 'Z' ? static_cast<unsigned char>(a_value + ('a' - 'A')) : a_value;
				};
				return lower(static_cast<unsigned char>(a_lhs)) == lower(static_cast<unsigned char>(a_rhs));
			}).begin() != a_name.end();
		}

		[[nodiscard]] std::string TransformText(const RE::NiTransform& a_transform)
		{
			return std::format(
				"R=[{:.4f},{:.4f},{:.4f};{:.4f},{:.4f},{:.4f};{:.4f},{:.4f},{:.4f}] T=[{:.3f},{:.3f},{:.3f}] S={:.5f}",
				a_transform.rotate[0].x, a_transform.rotate[0].y, a_transform.rotate[0].z,
				a_transform.rotate[1].x, a_transform.rotate[1].y, a_transform.rotate[1].z,
				a_transform.rotate[2].x, a_transform.rotate[2].y, a_transform.rotate[2].z,
				a_transform.translate.x, a_transform.translate.y, a_transform.translate.z,
				a_transform.scale);
		}

		[[nodiscard]] std::string RelativeTransformText(
			const RE::NiAVObject* a_root,
			const RE::NiAVObject* a_node)
		{
			if (!a_root || !a_node)
				return "unavailable";
			const auto& rootWorld = a_root->GetWorldTransform();
			if (!std::isfinite(rootWorld.scale) || std::abs(rootWorld.scale) < 1.0e-6F)
				return "invalid_root_scale";
			auto inverseRoot = rootWorld;
			inverseRoot.MakeInvert();
			return TransformText(inverseRoot * a_node->GetWorldTransform());
		}

		struct OwnerContext
		{
			RE::TESObjectREFR* reference{};
			RE::Actor* actor{};
			RE::NiAVObject* root{};
			RE::BSFaceGenNiNode* faceNode{};
			const char* failure{ "none" };
		};

		[[nodiscard]] OwnerContext ResolveOwner(const RE::BSRenderPass* a_pass) noexcept
		{
			OwnerContext result;
			if (!a_pass || !a_pass->geometry) {
				result.failure = "pass_or_geometry_null";
				return result;
			}
			result.reference = RE::TESObjectREFR::FindReferenceFor3D(a_pass->geometry);
			if (!result.reference) {
				result.failure = "FindReferenceFor3D_no_owner";
				return result;
			}
			result.actor = result.reference->As<RE::Actor>();
			if (!result.actor) {
				result.failure = "owner_not_actor";
				return result;
			}
			result.root = result.actor->Get3D();
			if (!result.root) {
				result.failure = "actor_3d_unavailable";
				return result;
			}
			result.faceNode = result.actor->GetFaceNode();
			if (!result.faceNode)
				result.failure = "actor_face_node_unavailable";
			return result;
		}

		[[nodiscard]] std::string ParentChain(
			const RE::BSGeometry* a_geometry,
			const RE::NiAVObject* a_actorRoot,
			const RE::NiAVObject*& a_namedHead,
			bool& a_rootIsAncestor)
		{
			std::string result;
			a_namedHead = nullptr;
			a_rootIsAncestor = false;
			const RE::NiAVObject* object = a_geometry;
			for (std::size_t depth = 0; object && depth < kMaxParentDepth; ++depth, object = object->parent) {
				a_rootIsAncestor = a_rootIsAncestor || object == a_actorRoot;
				if (!result.empty())
					result += " <- ";
				result += std::format("{:p}:{}", static_cast<const void*>(object), ObjectName(object));
				if (!a_namedHead && object != a_geometry && HasHeadToken(ObjectName(object)))
					a_namedHead = object;
			}
			if (object)
				result += " <- <depth_limit>";
			return result.empty() ? "unavailable" : result;
		}

		void LogSample(
			std::string_view a_event,
			const RE::BSRenderPass* a_pass,
			PrepassBakePath a_path,
			std::int32_t a_laneOffset,
			const std::array<float, 4>* a_lane,
			bool a_written,
			const OwnerContext& a_owner,
			std::uint32_t a_frame)
		{
			const auto* geometry = a_pass ? a_pass->geometry : nullptr;
			const RE::NiAVObject* namedHead = nullptr;
			bool rootIsAncestor = false;
			const auto chain = ParentChain(geometry, a_owner.root, namedHead, rootIsAncestor);
			const auto descriptor = a_pass ? a_pass->passEnum : 0u;
			const auto laneText = a_lane ?
			                          std::format("[{:.0f},{:.0f},{:.0f},{:.0f}]", (*a_lane)[0], (*a_lane)[1], (*a_lane)[2], (*a_lane)[3]) :
			                          "unavailable";
			const auto rootText = a_owner.root ? TransformText(a_owner.root->GetWorldTransform()) : "unavailable";
			const auto faceText = a_owner.faceNode ? TransformText(a_owner.faceNode->GetWorldTransform()) : "unavailable";
			const auto namedHeadText = namedHead ? TransformText(namedHead->GetWorldTransform()) : "no_head_named_ancestor";
			const auto relativeFace = RelativeTransformText(a_owner.root, a_owner.faceNode);
			const auto relativeNamedHead = RelativeTransformText(a_owner.root, namedHead);
			const auto* player = RE::PlayerCharacter::GetSingleton();
			const auto actorFormID = a_owner.actor ? a_owner.actor->GetFormID() : 0u;
			const auto actorRole = a_owner.actor ? (a_owner.actor == player ? "player" : "non_player") : "unresolved";

			L->info(
				"ARCH01D event={} frame={} path={} replay_draw=unobserved pass={:p} descriptor=0x{:08X} geometry={:p}:{} lane_offset={} lane={} lane_written={} owner_status={} actor_role={} actor_form=0x{:08X} actor={:p} root={:p}:{} root_is_geometry_ancestor={} root_world=({}) face_api={:p}:{} face_world=({}) face_relative=({}) head_named_ancestor={:p}:{} head_world=({}) head_relative=({}) geometry_parent_chain=[{}]",
				a_event, a_frame, PathName(a_path), static_cast<const void*>(a_pass), descriptor,
				static_cast<const void*>(geometry), ObjectName(geometry), a_laneOffset, laneText, a_written,
				a_owner.failure, actorRole, actorFormID, static_cast<const void*>(a_owner.actor),
				static_cast<const void*>(a_owner.root), ObjectName(a_owner.root), rootIsAncestor, rootText,
				static_cast<const void*>(a_owner.faceNode), ObjectName(a_owner.faceNode), faceText,
				relativeFace, static_cast<const void*>(namedHead), ObjectName(namedHead), namedHeadText,
				relativeNamedHead, chain);
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
			if (settings.prepassGeometryProbe)
				L->warn("ARCH-01D diagnostic probe enabled; samples are bounded and read-only");
		} catch (...) {
			g_enabled.store(false, std::memory_order_release);
			L->warn("ARCH-01D probe configuration failed; probe remains disabled");
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
		const auto frame = engine::GetFrameCount();
		if (a_path == PrepassBakePath::kImmediate && frame % kSampleEveryFrames != 0)
			return;
		try {
			const auto owner = ResolveOwner(a_pass);
			if (!owner.actor) {
				if (ReserveBounded(g_unresolvedSamples, 64))
					LogSample("lane_bake_unresolved", a_pass, a_path, a_laneOffset, &a_lane, a_written, owner, frame);
				return;
			}
			const auto actorFormID = owner.actor ? owner.actor->GetFormID() : 0u;
			if (!actorFormID) {
				if (ReserveBounded(g_unresolvedSamples, 64))
					LogSample("lane_bake_actor_identity_unavailable", a_pass, a_path, a_laneOffset, &a_lane, a_written, owner, frame);
				return;
			}
			auto* slot = ClaimActorSlot(owner.actor, actorFormID);
			if (!slot || !ClaimFrameSample(*slot, frame) || !ReserveBounded(g_transformSamples, kMaxTransformSamples))
				return;
			LogSample("lane_bake", a_pass, a_path, a_laneOffset, &a_lane, a_written, owner, frame);
		} catch (...) {
			// Diagnostics never affect lane classification or the draw.
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
		const auto createSequence = g_commandBufferCreates.fetch_add(1, std::memory_order_relaxed);
		if (createSequence >= kMaxCommandBufferCreates)
			return;
		try {
			const auto owner = ResolveOwner(a_pass);
			LogSample("command_buffer_create", a_pass, a_path, -1, nullptr, false, owner, engine::GetFrameCount());
			L->info("ARCH01D event=command_buffer_record_identity create_seq={} frame={} pass={:p} geometry={:p} record={:p} replay_draw=unobserved",
				createSequence, engine::GetFrameCount(), static_cast<const void*>(a_pass),
				static_cast<const void*>(a_pass->geometry), a_record);
		} catch (...) {
			// Diagnostics never affect the created record.
		}
	}
}
