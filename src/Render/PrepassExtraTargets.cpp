#include "Render/PrepassExtraTargets.h"

#include "Log.h"
#include "LogThrottle.h"
#include "Render/Engine.h"
#include "Render/FaceTransportPublicationContract.h"
#include "Render/FrameBindings.h"
#include "Render/RenderHooks.h"
#include "Render/RuntimeCallback.h"
#include "Render/ShaderInjection.h"
#include "Render/SharedData.h"
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
				PrepassExtraTargetPublicationCallback publication;
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
			bool ready = false;
			bool diagnosticActive = false;
			bool bindingAttempted = false;
			bool bindingConfirmed = false;
			bool srvBindingConfirmed = false;
			bool contextMatches = false;
			bool published = false;
			bool consumerEligible = false;
			std::uint64_t beginResourceGeneration = 0;
			std::uintptr_t prepassContextIdentity = 0;
			winrt::com_ptr<ID3D11Buffer> producerSharedData;
			bool producerViewAmbiguous = false;
			render::DirectionalLightSnapshot producerLight{};
			bool producerLightSnapshotValid = false;
			bool producerLightSnapshotAmbiguous = false;
			face_transport::PublicationState publicationState;
			PrepassExtraTargetPublication::Veto veto = PrepassExtraTargetPublication::Veto::kUnverifiedViewAndLightAssociation;
		};

		constexpr std::size_t kMaxRenderTargets = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
		// Runs after the shared substrate resets the frame's bindings.
		constexpr auto kBeginPriority = static_cast<HookPriority>(101);

		// Entries are added only during Load, before rendering.
		std::vector<Entry> g_entries;
		bool g_hooked = false;

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

		void PublishDiagnosticSnapshots();

		void BeginPrepass()
		{
			const auto* graphics = GetGraphicsState();
			const auto frameSerial = graphics ? graphics->frameCount : 0;
			for (auto& entry : g_entries) {
				entry.ready = false;
				entry.published = false;
				entry.consumerEligible = false;
				entry.writing = false;
				entry.diagnosticActive = false;
				entry.bindingAttempted = false;
				entry.bindingConfirmed = false;
				entry.srvBindingConfirmed = false;
				entry.contextMatches = false;
				entry.prepassContextIdentity = 0;
				entry.producerSharedData = nullptr;
				entry.producerViewAmbiguous = false;
				entry.producerLight = {};
				entry.producerLightSnapshotValid = false;
				entry.producerLightSnapshotAmbiguous = false;
				entry.beginResourceGeneration = entry.resourceGeneration;
				entry.frameSerial = frameSerial;
				entry.publicationState.BeginFrame(frameSerial, entry.resourceGeneration, false, entry.rgbaFallback, false);
				entry.veto = entry.sharedParallax && entry.requestRGBA8 ?
				                 PrepassExtraTargetPublication::Veto::kResourceUnavailable :
				                 PrepassExtraTargetPublication::Veto::kDiagnosticDisabled;
			}
			auto* context = GetImmediateContext();
			if (!context) {
				PublishDiagnosticSnapshots();
				return;
			}
			for (auto& entry : g_entries) {
				if (entry.sharedParallax)
					entry.prepassContextIdentity = reinterpret_cast<std::uintptr_t>(context);
			}
			// The prepass renders into the target, so no stage may still sample it.
			ID3D11ShaderResourceView* none = nullptr;
			for (const auto& entry : g_entries) {
				if (entry.target.publishSlot)
					BindFrameShaderResources(context, ShaderStage::kPixel, *entry.target.publishSlot, 1, &none);
			}
			if (!graphics) {
				PublishDiagnosticSnapshots();
				return;
			}
			for (auto& entry : g_entries) {
				bool active = false;
				if (entry.sharedParallax) {
					for (const auto& contributor : entry.contributors) {
						bool contributorActive = false;
						if (!cs::render::RunRenderCallback(contributor.owner, "ParallaxFaceTransportTarget", [&] { contributorActive = contributor.active(); }))
							continue;
						active |= contributorActive;
						entry.diagnosticActive |= contributorActive && static_cast<bool>(contributor.publication);
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
				entry.beginResourceGeneration = entry.resourceGeneration;
				if (entry.sharedParallax)
					entry.publicationState.BeginFrame(entry.frameSerial, entry.resourceGeneration, true, entry.rgbaFallback, true);
			}
			for (auto& entry : g_entries) {
				if (entry.sharedParallax && entry.requestRGBA8 && !entry.diagnosticActive)
					entry.veto = PrepassExtraTargetPublication::Veto::kDiagnosticDisabled;
			}
			PublishDiagnosticSnapshots();
		}

		void PublishDiagnosticSnapshots()
		{
			for (auto& entry : g_entries) {
				if (!entry.sharedParallax)
					continue;
				PrepassExtraTargetPublication snapshot{
					.resourceGeneration = entry.resourceGeneration,
					.publishedGeneration = entry.publicationState.publishedGeneration,
					.publicationId = entry.publicationState.publicationId,
					.frameSerial = entry.publicationState.frameSerial,
					.requestedFormat = entry.requestRGBA8 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8_UNORM,
					.actualFormat = entry.texture ? entry.texture->desc.Format : DXGI_FORMAT_UNKNOWN,
					.resourceReady = entry.publicationState.resourceReady,
					.bindingAttempted = entry.publicationState.bindingAttempted,
					.bindingConfirmed = entry.publicationState.bindingConfirmed,
					.srvBindingConfirmed = entry.publicationState.srvBindingConfirmed,
					.published = entry.publicationState.published,
					.consumerEligible = entry.consumerEligible,
					.viewIdentityConfirmed = entry.producerSharedData && !entry.producerViewAmbiguous,
					.rgba8Fallback = entry.publicationState.rgba8Fallback,
					.veto = entry.veto
				};
				for (const auto& contributor : entry.contributors) {
					if (!contributor.publication)
						continue;
					cs::render::RunRenderCallback(contributor.owner, "FaceTransportPublicationTelemetry", [&] {
						contributor.publication(snapshot);
					});
				}
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
			if (count == 0)
				return;
			for (auto& entry : g_entries) {
				if (!entry.sharedParallax || !entry.diagnosticActive || !entry.ready || entry.target.rtvSlot >= count)
					continue;
				entry.bindingAttempted = true;
				entry.contextMatches = a_context && reinterpret_cast<std::uintptr_t>(a_context) == entry.prepassContextIdentity;
				entry.publicationState.MarkBindingAttempted(entry.contextMatches);
				if (!entry.contextMatches)
					entry.veto = PrepassExtraTargetPublication::Veto::kContextMismatch;
			}
			// FrameBindingEpoch is thread-local: include context and resource identity in this thread-local cache.
			struct BindCache
			{
				std::uint64_t epoch = UINT64_MAX;
				std::uint64_t resourceGeneration = UINT64_MAX;
				ID3D11DeviceContext* context = nullptr;
			};
			thread_local BindCache cache;
			std::uint64_t resourceGeneration = 0;
			for (const auto& entry : g_entries) {
				if (entry.sharedParallax && entry.ready) {
					resourceGeneration = entry.resourceGeneration;
					break;
				}
			}
			if (epoch == cache.epoch && resourceGeneration == cache.resourceGeneration && a_context == cache.context)
				return;
			cache = { epoch, resourceGeneration, a_context };
			// FO4: extra slots follow the engine's six G-buffer targets, left as bound.
			RebindReadyTargets(a_context, count, [](const Entry& a_entry, ID3D11RenderTargetView*& a_slot) {
				auto* const rtv = a_entry.texture->rtv.get();
				const bool changed = a_slot != rtv;
				a_slot = rtv;
				return changed;
			});
		}

		void PublishTargets()
		{
			auto* context = GetImmediateContext();
			const auto count = ReadyRenderTargetCount();
			if (!context || count == 0) {
				for (auto& entry : g_entries) {
					if (entry.sharedParallax && entry.diagnosticActive)
						entry.veto = context ? PrepassExtraTargetPublication::Veto::kResourceUnavailable : PrepassExtraTargetPublication::Veto::kContextMismatch;
				}
				PublishDiagnosticSnapshots();
				return;
			}
			// Confirm the shared diagnostic RTV on this immediate context before unbinding it.
			for (auto& entry : g_entries) {
				if (!entry.sharedParallax || !entry.diagnosticActive || !entry.ready || entry.target.rtvSlot >= count)
					continue;
				entry.contextMatches = reinterpret_cast<std::uintptr_t>(context) == entry.prepassContextIdentity;
				entry.publicationState.contextMatches = entry.contextMatches;
				if (!entry.contextMatches || !entry.bindingAttempted || !entry.texture)
					continue;
				std::array<ID3D11RenderTargetView*, kMaxRenderTargets> outputs{};
				winrt::com_ptr<ID3D11DepthStencilView> depth;
				context->OMGetRenderTargets(entry.target.rtvSlot + 1, outputs.data(), depth.put());
				entry.bindingConfirmed = outputs[entry.target.rtvSlot] == entry.texture->rtv.get();
				entry.publicationState.MarkBindingConfirmed(entry.bindingConfirmed);
				for (auto* output : outputs) {
					if (output)
						output->Release();
				}
			}
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
				if (!entry.sharedParallax || !entry.diagnosticActive)
					continue;
				const auto setVeto = [&entry](PrepassExtraTargetPublication::Veto a_veto) { entry.veto = a_veto; };
				if (!entry.ready || !entry.writing || !entry.texture || entry.device != GetDevice()) {
					setVeto(PrepassExtraTargetPublication::Veto::kResourceUnavailable);
					continue;
				}
				if (!entry.contextMatches) {
					setVeto(PrepassExtraTargetPublication::Veto::kContextMismatch);
					continue;
				}
				if (!face_transport::PublicationGenerationMatches(entry.beginResourceGeneration, entry.resourceGeneration)) {
					setVeto(PrepassExtraTargetPublication::Veto::kGenerationMismatch);
					continue;
				}
				if (!entry.bindingAttempted) {
					setVeto(PrepassExtraTargetPublication::Veto::kBindingNotAttempted);
					continue;
				}
				if (!entry.bindingConfirmed) {
					setVeto(PrepassExtraTargetPublication::Veto::kBindingNotConfirmed);
					continue;
				}
				D3D11_TEXTURE2D_DESC actual{};
				entry.texture->resource->GetDesc(&actual);
				if (actual.Width != entry.texture->desc.Width || actual.Height != entry.texture->desc.Height ||
					actual.Format != entry.target.format || actual.Format == DXGI_FORMAT_UNKNOWN) {
					setVeto(PrepassExtraTargetPublication::Veto::kPublicationFailed);
					continue;
				}
				bool srvBound = true;
				if (entry.target.publishSlot) {
					ID3D11ShaderResourceView* bound = nullptr;
					GetStageBindings(context, ShaderStage::kPixel, *entry.target.publishSlot, 1, &bound);
					srvBound = bound == entry.texture->srv.get();
					if (bound)
						bound->Release();
				}
				std::array<ID3D11RenderTargetView*, kMaxRenderTargets> outputs{};
				winrt::com_ptr<ID3D11DepthStencilView> depth;
				context->OMGetRenderTargets(entry.target.rtvSlot + 1, outputs.data(), depth.put());
				const bool rtvUnbound = outputs[entry.target.rtvSlot] != entry.texture->rtv.get();
				for (auto* output : outputs) {
					if (output)
						output->Release();
				}
				entry.srvBindingConfirmed = srvBound;
				entry.publicationState.srvBindingConfirmed = srvBound;
				if (!face_transport::CanPublish(entry.writing, entry.bindingAttempted,
						actual.Width == entry.texture->desc.Width && actual.Height == entry.texture->desc.Height && actual.Format == entry.target.format && actual.Format != DXGI_FORMAT_UNKNOWN,
						srvBound, rtvUnbound, entry.contextMatches, entry.bindingAttempted, entry.bindingConfirmed) ||
					!srvBound || !rtvUnbound || entry.resourceGeneration != entry.beginResourceGeneration ||
					entry.publicationId == (std::numeric_limits<std::uint64_t>::max)()) {
					setVeto(PrepassExtraTargetPublication::Veto::kPublicationFailed);
					continue;
				}
				entry.published = true;
				entry.writing = false;
				++entry.publicationId;
				entry.publicationState.publicationId = entry.publicationId;
				entry.publicationState.MarkPublished();
				// Consumer view/light association is checked at the DFLight bind; no publication alone enables sampling.
				entry.consumerEligible = false;
				entry.veto = entry.requestRGBA8 && actual.Format != DXGI_FORMAT_R8G8B8A8_UNORM ?
				                 PrepassExtraTargetPublication::Veto::kDiagnosticFormatUnavailable :
				                 PrepassExtraTargetPublication::Veto::kUnverifiedViewAndLightAssociation;
			}
			PublishDiagnosticSnapshots();
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
		Feature& a_owner, std::function<bool()> a_active, bool a_requestRGBA8,
		PrepassExtraTargetPublicationCallback a_publication)
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
		found->contributors.push_back({ .owner = &a_owner, .active = std::move(a_active), .publication = std::move(a_publication) });
		return static_cast<PrepassExtraTargetId>(std::distance(g_entries.begin(), found));
	}

	void AttachParallaxFaceTransportTarget(ShaderReplacementRegistration& a_registration)
	{
		AttachParallaxFaceTransportTargetWithCallback(a_registration, {});
	}

	void CaptureParallaxFaceTransportProducerView(ID3D11DeviceContext* a_context) noexcept
	{
		if (!a_context || a_context != GetImmediateContext())
			return;
		ID3D11Buffer* sharedData = nullptr;
		a_context->PSGetConstantBuffers(render::kSharedDataSlot, 1, &sharedData);
		for (auto& entry : g_entries) {
			if (!entry.sharedParallax || !entry.ready || !entry.writing)
				continue;
			if (!sharedData) {
				entry.producerViewAmbiguous = true;
				entry.producerLightSnapshotAmbiguous = true;
				continue;
			}
			if (!entry.producerSharedData)
				entry.producerSharedData.copy_from(sharedData);
			else if (entry.producerSharedData.get() != sharedData)
				entry.producerViewAmbiguous = true;
			render::DirectionalLightSnapshot snapshot{};
			if (!render::GetDirectionalLightSnapshot(sharedData, snapshot) || snapshot.frameSerial != entry.frameSerial) {
				entry.producerLightSnapshotAmbiguous = true;
				continue;
			}
			if (!entry.producerLightSnapshotValid) {
				entry.producerLight = snapshot;
				entry.producerLightSnapshotValid = true;
			} else if (entry.producerLight.frameSerial != snapshot.frameSerial ||
					   entry.producerLight.directionWS != snapshot.directionWS) {
				entry.producerLightSnapshotAmbiguous = true;
			}
		}
		if (sharedData)
			sharedData->Release();
	}

	bool ValidateParallaxFaceTransportConsumerView(
		ID3D11DeviceContext* a_context,
		std::uint64_t* a_resourceGeneration,
		std::uint64_t* a_publicationId) noexcept
	{
		if (a_resourceGeneration)
			*a_resourceGeneration = 0;
		if (a_publicationId)
			*a_publicationId = 0;
		if (!a_context || a_context != GetImmediateContext())
			return false;
		const auto found = std::ranges::find_if(g_entries, [](const Entry& a_entry) { return a_entry.sharedParallax; });
		if (found == g_entries.end() || !found->published || !found->publicationState.GenerationMatches() ||
			found->frameSerial != (GetGraphicsState() ? GetGraphicsState()->frameCount : 0) ||
			found->target.format != DXGI_FORMAT_R8G8B8A8_UNORM || found->rgbaFallback ||
			!found->producerSharedData || found->producerViewAmbiguous || !found->producerLightSnapshotValid ||
			found->producerLightSnapshotAmbiguous || !found->texture ||
			found->resourceGeneration != found->beginResourceGeneration)
			return false;

		ID3D11Buffer* consumerSharedData = nullptr;
		a_context->PSGetConstantBuffers(render::kSharedDataSlot, 1, &consumerSharedData);
		const bool viewMatches = consumerSharedData && consumerSharedData == found->producerSharedData.get();
		if (consumerSharedData)
			consumerSharedData->Release();
		if (!viewMatches)
			return false;
		render::DirectionalLightSnapshot consumerLight{};
		if (!render::GetDirectionalLightSnapshot(found->producerSharedData.get(), consumerLight) ||
			consumerLight.frameSerial != found->producerLight.frameSerial ||
			consumerLight.frameSerial != found->frameSerial ||
			consumerLight.directionWS != found->producerLight.directionWS)
			return false;

		ID3D11ShaderResourceView* target = nullptr;
		a_context->PSGetShaderResources(*found->target.publishSlot, 1, &target);
		const bool targetMatches = target && target == found->texture->srv.get();
		if (target)
			target->Release();
		if (!targetMatches)
			return false;

		D3D11_TEXTURE2D_DESC descriptor{};
		found->texture->resource->GetDesc(&descriptor);
		if (descriptor.Width != found->texture->desc.Width || descriptor.Height != found->texture->desc.Height ||
			descriptor.Format != DXGI_FORMAT_R8G8B8A8_UNORM || descriptor.SampleDesc.Count != 1)
			return false;
		if (a_resourceGeneration)
			*a_resourceGeneration = found->resourceGeneration;
		if (a_publicationId)
			*a_publicationId = found->publicationId;
		return true;
	}

	void AttachParallaxFaceTransportTargetWithCallback(ShaderReplacementRegistration& a_registration, std::function<void(ID3D11DeviceContext*)> a_afterBind)
	{
		if (a_registration.targetId != ShaderInjectionTarget::kDeferredPrepass)
			return;
		a_registration.bind = [afterBind = std::move(a_afterBind)](ID3D11DeviceContext* a_context) {
			BindTargets(a_context);
			CaptureParallaxFaceTransportProducerView(a_context);
			if (afterBind)
				afterBind(a_context);
		};
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

}
