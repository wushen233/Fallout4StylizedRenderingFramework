#include "Render/PrepassExtraTargets.h"

#include "Log.h"
#include "LogThrottle.h"
#include "Render/Engine.h"
#include "Render/FaceTransportPublicationContract.h"
#include "Render/FrameBindings.h"
#include "Render/RenderHooks.h"
#include "Render/RuntimeCallback.h"
#include "Render/ShaderInjection.h"
#include "Render/SubstrateSlots.h"
#include "Utils/CSBuffer.h"

#include <d3d11.h>
#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

namespace cs::engine
{
	namespace
	{
		auto* L = cs::log::Get("cs.render.prepassextratargets");

		struct Entry
		{
			struct Contributor
			{
				Feature* owner = nullptr;
				std::function<bool()> active;
			};

			Feature* owner = nullptr;
			PrepassExtraTarget target;
			std::unique_ptr<cs::buffer::Texture2D> texture;
			std::vector<Contributor> contributors;
			ID3D11Device* device = nullptr;
			std::uint64_t resourceGeneration = 0;
			std::uint64_t publicationId = 0;
			std::uint32_t frameSerial = 0;
			bool sharedParallax = false;
			bool requestRGBA8 = false;
			bool rgbaFallback = false;
			bool writing = false;
			bool sawRenderTargetBinding = false;
			bool ready = false;
			bool published = false;
			bool consumerEligible = false;
			PrepassExtraTargetPublication::Veto veto = PrepassExtraTargetPublication::Veto::kUnverifiedViewAndLightAssociation;
		};

		constexpr std::size_t kMaxRenderTargets = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
		// Runs after the shared substrate resets the frame's bindings.
		constexpr auto kBeginPriority = static_cast<HookPriority>(101);

		// Entries are added only during Load, before rendering.
		std::vector<Entry> g_entries;
		bool g_hooked = false;
		std::uint64_t g_bindEpoch = UINT64_MAX;

		bool EnsureTexture(Entry& a_entry, const RE::BSGraphics::State& a_graphics)
		{
			auto& texture = a_entry.texture;
			auto* const currentDevice = GetDevice();
			if (!currentDevice)
				return false;
			const auto requestedFormat = a_entry.sharedParallax ?
			                                 (a_entry.requestRGBA8 && !a_entry.rgbaFallback ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8_UNORM) :
			                                 a_entry.target.format;
			if (texture && a_entry.device == currentDevice && texture->desc.Width == a_graphics.screenWidth &&
				texture->desc.Height == a_graphics.screenHeight && texture->desc.Format == requestedFormat) {
				winrt::com_ptr<ID3D11Device> resourceDevice;
				texture->resource->GetDevice(resourceDevice.put());
				if (resourceDevice.get() == currentDevice)
					return true;
			}
			if (a_entry.resourceGeneration == (std::numeric_limits<std::uint64_t>::max)())
				return false;
			const auto& name = a_entry.target.name;
			try {
				texture = cs::buffer::CreateRenderSizeTexture(
					a_graphics.screenWidth, a_graphics.screenHeight, requestedFormat,
					D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, name);
				a_entry.device = currentDevice;
				a_entry.target.format = requestedFormat;
				++a_entry.resourceGeneration;
				a_entry.published = false;
				a_entry.consumerEligible = false;
				return true;
			} catch (const std::exception& e) {
				CS_LOG_EVERY_MS(L, 2000, spdlog::level::warn, "Prepass target {} unavailable: {}", name, e.what());
			} catch (...) {
				CS_LOG_EVERY_MS(L, 2000, spdlog::level::warn, "Prepass target {} unavailable.", name);
			}
			if (a_entry.sharedParallax && a_entry.requestRGBA8) {
				try {
					texture = cs::buffer::CreateRenderSizeTexture(
						a_graphics.screenWidth, a_graphics.screenHeight, DXGI_FORMAT_R8_UNORM,
						D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, name + "/ParallaxFallback");
					a_entry.device = currentDevice;
					a_entry.target.format = DXGI_FORMAT_R8_UNORM;
					a_entry.rgbaFallback = true;
					++a_entry.resourceGeneration;
					a_entry.published = false;
					a_entry.consumerEligible = false;
					return true;
				} catch (...) {
					CS_LOG_EVERY_MS(L, 2000, spdlog::level::warn, "Shared RT6 RGBA8 and R8 fallback allocations failed.");
				}
			}
			return false;
		}

		void BeginPrepass()
		{
			for (auto& entry : g_entries) {
				entry.ready = false;
				entry.published = false;
				entry.consumerEligible = false;
				entry.writing = false;
				entry.sawRenderTargetBinding = false;
			}
			auto* context = GetImmediateContext();
			if (!context)
				return;
			// The prepass renders into the target, so no stage may still sample it.
			ID3D11ShaderResourceView* none = nullptr;
			for (const auto& entry : g_entries) {
				if (entry.target.publishSlot)
					BindFrameShaderResources(context, ShaderStage::kPixel, *entry.target.publishSlot, 1, &none);
			}
			const auto* graphics = GetGraphicsState();
			if (!graphics)
				return;
			for (auto& entry : g_entries) {
				bool active = false;
				if (entry.sharedParallax) {
					for (const auto& contributor : entry.contributors) {
						bool contributorActive = false;
						if (!cs::render::RunRenderCallback(contributor.owner, "ParallaxFaceTransportTarget", [&] { contributorActive = contributor.active(); }))
							continue;
						active |= contributorActive;
					}
				} else {
					cs::render::RunRenderCallback(entry.owner, "PrepassExtraTarget", [&] { active = entry.target.active(); });
				}
				if (!active || !EnsureTexture(entry, *graphics))
					continue;
				context->ClearRenderTargetView(entry.texture->rtv.get(), entry.target.clear.data());
				entry.ready = true;
				entry.writing = true;
				entry.frameSerial = graphics->frameCount;
			}
		}

		std::uint32_t ReadyRenderTargetCount() noexcept
		{
			std::uint32_t count = 0;
			for (const auto& entry : g_entries) {
				if (entry.ready)
					count = (std::max)(count, entry.target.rtvSlot + 1);
			}
			return count;
		}

		// Lets a_rebind edit the bound set for ready targets; one OMSet if any changed.
		template <class Rebind>
		void RebindReadyTargets(ID3D11DeviceContext* a_context, std::uint32_t a_count, Rebind&& a_rebind)
		{
			std::array<ID3D11RenderTargetView*, kMaxRenderTargets> targets{};
			winrt::com_ptr<ID3D11DepthStencilView> depth;
			a_context->OMGetRenderTargets(a_count, targets.data(), depth.put());
			auto next = targets;
			bool changed = false;
			for (const auto& entry : g_entries) {
				if (entry.ready)
					changed |= a_rebind(entry, next[entry.target.rtvSlot]);
			}
			if (changed)
				a_context->OMSetRenderTargets(a_count, next.data(), depth.get());
			for (auto* target : targets) {
				if (target)
					target->Release();
			}
		}

		void BindTargets(ID3D11DeviceContext* a_context)
		{
			// Only an engine output commit or plugin work can unbind a target.
			const auto epoch = GetFrameBindingEpoch();
			const auto count = ReadyRenderTargetCount();
			if (count == 0 || epoch == g_bindEpoch)
				return;
			g_bindEpoch = epoch;
			// FO4: extra slots follow the engine's six G-buffer targets, left as bound.
			RebindReadyTargets(a_context, count, [](const Entry& a_entry, ID3D11RenderTargetView*& a_slot) {
				auto* const rtv = a_entry.texture->rtv.get();
				const bool changed = a_slot != rtv;
				a_slot = rtv;
				return changed;
			});
			for (auto& entry : g_entries) {
				if (entry.ready && entry.target.rtvSlot < count)
					entry.sawRenderTargetBinding = true;
			}
		}

		void PublishTargets()
		{
			auto* context = GetImmediateContext();
			const auto count = ReadyRenderTargetCount();
			if (!context || count == 0)
				return;
			RebindReadyTargets(context, count, [](const Entry& a_entry, ID3D11RenderTargetView*& a_slot) {
				const bool changed = a_slot == a_entry.texture->rtv.get();
				if (changed)
					a_slot = nullptr;
				return changed;
			});
			for (const auto& entry : g_entries) {
				if (!entry.ready || !entry.target.publishSlot)
					continue;
				auto* srv = entry.texture->srv.get();
				BindFrameShaderResources(context, ShaderStage::kPixel, *entry.target.publishSlot, 1, &srv);
			}
			for (auto& entry : g_entries) {
				if (!entry.ready || !entry.writing || !entry.sawRenderTargetBinding || !entry.texture || entry.device != GetDevice())
					continue;
				D3D11_TEXTURE2D_DESC actual{};
				entry.texture->resource->GetDesc(&actual);
				if (actual.Width != entry.texture->desc.Width || actual.Height != entry.texture->desc.Height ||
					actual.Format != entry.target.format || actual.Format == DXGI_FORMAT_UNKNOWN)
					continue;
				bool bindingsValid = true;
				if (entry.target.publishSlot) {
					ID3D11ShaderResourceView* bound = nullptr;
					GetStageBindings(context, ShaderStage::kPixel, *entry.target.publishSlot, 1, &bound);
					bindingsValid = bound == entry.texture->srv.get();
					if (bound)
						bound->Release();
				}
				std::array<ID3D11RenderTargetView*, kMaxRenderTargets> outputs{};
				winrt::com_ptr<ID3D11DepthStencilView> depth;
				context->OMGetRenderTargets(entry.target.rtvSlot + 1, outputs.data(), depth.put());
				bindingsValid &= outputs[entry.target.rtvSlot] != entry.texture->rtv.get();
				for (auto* output : outputs) {
					if (output)
						output->Release();
				}
				if (!face_transport::CanPublish(entry.writing, entry.sawRenderTargetBinding,
						actual.Width == entry.texture->desc.Width && actual.Height == entry.texture->desc.Height && actual.Format == entry.target.format && actual.Format != DXGI_FORMAT_UNKNOWN,
						bindingsValid, outputs[entry.target.rtvSlot] != entry.texture->rtv.get()) ||
					entry.resourceGeneration == (std::numeric_limits<std::uint64_t>::max)() ||
					entry.publicationId == (std::numeric_limits<std::uint64_t>::max)())
					continue;
				entry.published = true;
				entry.writing = false;
				++entry.publicationId;
				// View identity, unsupported writers and light snapshot association are not established here.
				entry.consumerEligible = face_transport::CanConsume(entry.published, false, false, false, false, false);
				entry.veto = entry.sharedParallax && entry.requestRGBA8 && actual.Format != DXGI_FORMAT_R8G8B8A8_UNORM ?
				                 PrepassExtraTargetPublication::Veto::kDiagnosticFormatUnavailable :
				                 PrepassExtraTargetPublication::Veto::kUnverifiedViewAndLightAssociation;
			}
		}
	}

	std::optional<PrepassExtraTargetId> RegisterPrepassExtraTarget(Feature& a_owner, PrepassExtraTarget a_target)
	{
		if (a_target.rtvSlot >= kMaxRenderTargets || !a_target.active ||
			std::ranges::any_of(g_entries, [&](const Entry& a_entry) { return a_entry.target.rtvSlot == a_target.rtvSlot; })) {
			L->error("Prepass target {} has an invalid or taken render target slot {}.", a_target.name, a_target.rtvSlot);
			return std::nullopt;
		}
		if (!g_hooked) {
			if (!RegisterPreDeferredPrePass(&BeginPrepass, kBeginPriority) ||
				!RegisterPostDeferredPrePass(&PublishTargets)) {
				return std::nullopt;
			}
			g_hooked = true;
		}
		g_entries.push_back({ .owner = &a_owner, .target = std::move(a_target) });
		return g_entries.size() - 1;
	}

	std::optional<PrepassExtraTargetId> RegisterParallaxFaceTransportTarget(
		Feature& a_owner, std::function<bool()> a_active, bool a_requestRGBA8)
	{
		if (!RenderHookRegistrationAllowed("RegisterParallaxFaceTransportTarget") || !a_active)
			return std::nullopt;
		auto found = std::ranges::find_if(g_entries, [](const Entry& a_entry) { return a_entry.sharedParallax; });
		if (found == g_entries.end()) {
			if (std::ranges::any_of(g_entries, [](const Entry& a_entry) { return a_entry.target.rtvSlot == render::kParallaxShadowTargetSlot; }))
				return std::nullopt;
			if (!g_hooked) {
				if (!RegisterPreDeferredPrePass(&BeginPrepass, kBeginPriority) || !RegisterPostDeferredPrePass(&PublishTargets))
					return std::nullopt;
				g_hooked = true;
			}
			g_entries.push_back({ .owner = &a_owner,
				.target = { .name = "Shared/ParallaxFaceTransport", .rtvSlot = render::kParallaxShadowTargetSlot, .format = DXGI_FORMAT_R8_UNORM, .clear = { 1, 0, 0, 0 }, .publishSlot = render::kParallaxShadowSlot },
				.sharedParallax = true });
			found = std::prev(g_entries.end());
		}
		found->requestRGBA8 |= a_requestRGBA8;
		found->contributors.push_back({ .owner = &a_owner, .active = std::move(a_active) });
		return static_cast<PrepassExtraTargetId>(std::distance(g_entries.begin(), found));
	}

	void AttachParallaxFaceTransportTarget(ShaderReplacementRegistration& a_registration)
	{
		if (a_registration.targetId == ShaderInjectionTarget::kDeferredPrepass)
			a_registration.bind = &BindTargets;
	}

	void AttachPrepassExtraTargets(ShaderReplacementRegistration& a_registration)
	{
		if (a_registration.targetId == ShaderInjectionTarget::kDeferredPrepass)
			a_registration.bind = &BindTargets;
	}

	ID3D11ShaderResourceView* GetPrepassExtraTargetSRV(PrepassExtraTargetId a_id) noexcept
	{
		if (a_id >= g_entries.size())
			return nullptr;
		const auto& entry = g_entries[a_id];
		return entry.ready ? entry.texture->srv.get() : nullptr;
	}

	PrepassExtraTargetPublication GetPrepassExtraTargetPublication(PrepassExtraTargetId a_id) noexcept
	{
		if (a_id >= g_entries.size())
			return {};
		const auto& entry = g_entries[a_id];
		return { .srv = entry.published && entry.texture ? entry.texture->srv.get() : nullptr,
			.resourceGeneration = entry.resourceGeneration,
			.publicationId = entry.publicationId,
			.frameSerial = entry.frameSerial,
			.format = entry.target.format,
			.published = entry.published,
			.consumerEligible = entry.consumerEligible,
			.veto = entry.veto };
	}
}
