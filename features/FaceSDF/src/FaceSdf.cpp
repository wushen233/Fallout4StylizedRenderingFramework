#include "FaceSdf.h"

#include "FaceSdfContract.h"
#include "Log.h"
#include "Render/Annotation.h"
#include "Render/Engine.h"
#include "Render/FeatureShaderBindings.h"
#include "Render/PrepassTechnique.h"
#include "Render/ShaderInjection.h"
#include "Render/ShaderSubclassHooks.h"
#include "Render/SubstrateSlots.h"
#include "Telemetry/Telemetry.h"
#include "Utils/CSBuffer.h"

#include <DirectXTex.h>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <d3d11.h>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <ranges>
#include <string_view>

#include "RE/A/Actor.h"
#include "RE/B/BSLightingShaderMaterialBase.h"
#include "RE/B/BSLightingShaderProperty.h"
#include "RE/B/BSRenderPass.h"
#include "RE/T/TESDataHandler.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESRace.h"

namespace cs::features
{
	struct FaceSDF::Profile
	{
		std::string id;
		std::string racePlugin;
		std::uint32_t raceLocalFormId = 0;
		std::uint32_t raceFormId = 0;
		std::string shape;
		std::wstring texturePath;
		std::array<float, 3> forward{};
		std::array<float, 3> right{};
		std::array<float, 3> up{};
		std::array<float, 4> uvScaleBias{ 0.0f, 0.0f, 1.0f, 1.0f };
		float softness = 0.03f;
		float strength = 1.0f;
		winrt::com_ptr<ID3D11ShaderResourceView> srv;
	};

	struct FaceSDF::ProfileSet
	{
		std::vector<Profile> values;
	};

	struct alignas(16) FaceSDF::DrawConstants
	{
		DirectX::XMFLOAT4 forward{};
		DirectX::XMFLOAT4 right{};
		DirectX::XMFLOAT4 up{};
		DirectX::XMFLOAT4 uvScaleBias{};
		DirectX::XMFLOAT4 parameters{};
	};
	static_assert(sizeof(DirectX::XMFLOAT4) * 5 == 80);

	namespace
	{
		auto* L = cs::log::Get("cs.feature.facesdf");
		constexpr std::size_t kSelectionStackCapacity = 16;

		struct ImmediateSelection
		{
			FaceSDF* owner = nullptr;
			RE::BSRenderPass* pass = nullptr;
			std::array<float, 20> constants{};
			winrt::com_ptr<ID3D11ShaderResourceView> texture;
			bool eligible = false;
		};
		thread_local ImmediateSelection t_selection;
		thread_local std::array<ImmediateSelection, kSelectionStackCapacity> t_selectionStack{};
		thread_local std::size_t t_selectionDepth = 0;
		thread_local std::size_t t_selectionOverflow = 0;

		bool ReadFloat3(const toml::table& a_table, std::string_view a_key, std::array<float, 3>& a_value)
		{
			const auto* array = a_table[a_key].as_array();
			if (!array || array->size() != 3)
				return false;
			for (std::size_t i = 0; i < a_value.size(); ++i) {
				const auto value = (*array)[i].value<double>();
				if (!value || !std::isfinite(*value))
					return false;
				a_value[i] = static_cast<float>(*value);
			}
			return true;
		}

		bool ReadFloat4(const toml::table& a_table, std::string_view a_key, std::array<float, 4>& a_value)
		{
			const auto* array = a_table[a_key].as_array();
			if (!array || array->size() != 4)
				return false;
			for (std::size_t i = 0; i < a_value.size(); ++i) {
				const auto value = (*array)[i].value<double>();
				if (!value || !std::isfinite(*value))
					return false;
				a_value[i] = static_cast<float>(*value);
			}
			return true;
		}

		bool IsSafeRelativePath(std::string_view a_path)
		{
			return !a_path.empty() && a_path.size() <= 240 && a_path.starts_with("Data\\") &&
			       a_path.find("..") == std::string_view::npos && a_path.find(':') == std::string_view::npos &&
			       a_path.find('/') == std::string_view::npos &&
			       std::ranges::all_of(a_path, [](unsigned char c) { return c >= 0x20 && c < 0x7F; });
		}

		bool IsValidProfileId(std::string_view a_id)
		{
			if (a_id.empty() || a_id.size() > 96)
				return false;
			const auto separator = a_id.find(':');
			if (separator == 0 || separator == std::string_view::npos || separator + 1 == a_id.size() ||
				a_id.find(':', separator + 1) != std::string_view::npos)
				return false;
			const auto validPart = [](std::string_view a_part) {
				return std::ranges::all_of(a_part, [](unsigned char c) {
					return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
				});
			};
			return validPart(a_id.substr(0, separator)) && validPart(a_id.substr(separator + 1));
		}

		bool ParseHexFormId(std::string_view a_text, std::uint32_t& a_formId)
		{
			if (a_text.empty() || a_text.size() > 8)
				return false;
			const auto [end, error] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), a_formId, 16);
			return error == std::errc{} && end == a_text.data() + a_text.size() && a_formId != 0;
		}

		template <class Profile>
		bool MatchesExplicitProfile(RE::BSRenderPass* a_pass, const Profile& a_profile, RE::Actor*& a_actor) noexcept
		{
			a_actor = nullptr;
			using namespace cs::engine::prepass_technique;
			constexpr std::uint32_t kRequired = kFace | kSkinned | kTexture | kNormals | kBinormalTangent;
			constexpr std::uint32_t kForbidden = kSkinTint | kEye | kBlend | kAlphaTest | kLandscape | kGrass |
			                                     kLodLandscape | kLodObjectInstanced | kInstanced | kCombined | kTessellateDispHeight |
			                                     kTessellateDispNormals | kDismemberment | kDismembermentMeatCuff | kPipboyScreen | kMenuScreen;
			if (!a_pass || !a_pass->geometry || (a_pass->passEnum & kRequired) != kRequired ||
				(a_pass->passEnum & kForbidden) != 0)
				return false;
			const auto* shapeName = a_pass->geometry->name.c_str();
			if (!shapeName || std::string_view(shapeName) != a_profile.shape)
				return false;
			auto* property = netimmerse_cast<RE::BSLightingShaderProperty*>(a_pass->GetShaderProperty());
			auto* material = property ? static_cast<RE::BSLightingShaderMaterialBase*>(property->material) : nullptr;
			if (!material || material->GetFeature() != RE::BSShaderMaterial::Feature::kFace)
				return false;
			auto* reference = RE::TESObjectREFR::FindReferenceFor3D(a_pass->geometry);
			a_actor = reference ? reference->As<RE::Actor>() : nullptr;
			return a_actor && a_actor->race && a_actor->race->GetFormID() == a_profile.raceFormId;
		}

		bool SupportsSdfFormat(DXGI_FORMAT a_format) noexcept
		{
			return face_sdf::IsValidSdfTextureFormat(static_cast<unsigned int>(a_format));
		}
	}

	FaceSDF* FaceSDF::GetSingleton()
	{
		static FaceSDF instance;
		return &instance;
	}

	bool FaceSDF::Configure(const toml::table& a_config, std::string& a_error)
	{
		a_error.clear();
		auto parsed = std::make_shared<ProfileSet>();
		const auto* node = a_config.get("profiles");
		if (!node) {
			_profiles.store(std::shared_ptr<const ProfileSet>(parsed), std::memory_order_release);
			{
				std::scoped_lock lock(_telemetryMutex);
				_configurationState = "no_explicit_profiles";
			}
			_resourcesReady.store(false, std::memory_order_release);
			return true;
		}
		const auto* array = node->as_array();
		if (!array || array->empty() || array->size() > 16) {
			a_error = "profiles must be an array containing 1 to 16 explicit profiles";
			return false;
		}
		for (const auto& element : *array) {
			const auto* table = element.as_table();
			if (!table) {
				a_error = "each FaceSDF profile must be a table";
				return false;
			}
			Profile profile;
			std::string formId;
			std::string path;
			const auto idValue = (*table)["id"].value<std::string>();
			const auto pluginValue = (*table)["race_plugin"].value<std::string>();
			const auto localIdValue = (*table)["race_local_form_id"].value<std::string>();
			const auto shapeValue = (*table)["shape"].value<std::string>();
			const auto textureValue = (*table)["sdf_texture"].value<std::string>();
			if (!idValue || !pluginValue || !localIdValue || !shapeValue || !textureValue) {
				a_error = "a profile needs id, race_plugin, race_local_form_id, shape and sdf_texture";
				return false;
			}
			profile.id = *idValue;
			profile.racePlugin = *pluginValue;
			formId = *localIdValue;
			profile.shape = *shapeValue;
			path = *textureValue;
			if (!IsValidProfileId(profile.id) || profile.racePlugin.empty() || profile.racePlugin.size() > 128 ||
				profile.shape.empty() || profile.shape.size() > 128 ||
				!std::ranges::all_of(profile.shape, [](unsigned char c) { return c >= 0x20 && c < 0x7F; }) ||
				!IsSafeRelativePath(path) || !ParseHexFormId(formId, profile.raceLocalFormId) ||
				!ReadFloat3(*table, "forward", profile.forward) || !ReadFloat3(*table, "right", profile.right) ||
				!ReadFloat3(*table, "up", profile.up) || !face_sdf::IsValidBasis(profile.forward, profile.right, profile.up)) {
				a_error = "profile identity, path or orthonormal basis is invalid";
				return false;
			}
			if (table->get("uv_scale_bias") && !ReadFloat4(*table, "uv_scale_bias", profile.uvScaleBias)) {
				a_error = "uv_scale_bias must contain four finite numbers";
				return false;
			}
			if (const auto softness = (*table)["softness"].value<double>(); softness)
				profile.softness = static_cast<float>(*softness);
			if (const auto strength = (*table)["strength"].value<double>(); strength)
				profile.strength = static_cast<float>(*strength);
			if (!std::isfinite(profile.softness) || profile.softness <= 0.001f || profile.softness > 0.25f ||
				!std::isfinite(profile.strength) || profile.strength < 0.0f || profile.strength > 1.0f) {
				a_error = "softness or strength is outside its supported range";
				return false;
			}
			profile.texturePath.assign(path.begin(), path.end());
			if (std::ranges::any_of(parsed->values, [&](const Profile& a_existing) {
					return a_existing.id == profile.id ||
				           (a_existing.racePlugin == profile.racePlugin && a_existing.raceLocalFormId == profile.raceLocalFormId && a_existing.shape == profile.shape);
				})) {
				a_error = "duplicate profile id or ambiguous race/shape selector";
				return false;
			}
			parsed->values.push_back(std::move(profile));
		}
		_profiles.store(std::shared_ptr<const ProfileSet>(std::move(parsed)), std::memory_order_release);
		{
			std::scoped_lock lock(_telemetryMutex);
			_configurationState = "profiles_parsed_unresolved";
		}
		_resourcesReady.store(false, std::memory_order_release);
		return true;
	}

	void FaceSDF::Load()
	{
		if (!engine::RegisterFeatureShaderBindings(*this, [this](engine::ShaderReplacementRegistration& a_registration) {
				if (a_registration.targetId == engine::ShaderInjectionTarget::kDeferredPrepass) {
					engine::AttachParallaxFaceTransportTargetWithCallback(a_registration, [this](ID3D11DeviceContext* a_context) {
						BindDraw(a_context);
					});
					a_registration.stages |= engine::ShaderStageBit(engine::ShaderStage::kVertex);
					a_registration.bindWhileDisabled = true;
				} else if (a_registration.targetId == engine::ShaderInjectionTarget::kBsdfLight) {
					a_registration.bind = [this](ID3D11DeviceContext* a_context) { BindLighting(a_context); };
					a_registration.bindWhileDisabled = true;
				}
			})) {
			FailLoad("Face SDF shader contribution registration failed.");
			return;
		}
		if (!engine::RegisterPrepassImmediateDrawCallback(*this, &PrepareImmediateDraw, this)) {
			FailLoad("Face SDF could not join the existing immediate prepass SetupGeometry hook.");
			return;
		}
		if (!engine::RegisterParallaxFaceTransportTarget(*this, [this] {
				const auto profiles = _profiles.load(std::memory_order_acquire);
				return IsEnabled() && _resourcesReady.load(std::memory_order_acquire) && profiles && !profiles->values.empty(); }, true)) {
			FailLoad("Face SDF could not register as an RGBA8 shared RT6 contributor.");
			return;
		}
		_registrationsReady.store(true, std::memory_order_release);
	}

	void FaceSDF::OnDataLoaded()
	{
		auto current = _profiles.load(std::memory_order_acquire);
		auto resolved = std::make_shared<ProfileSet>();
		if (current)
			resolved->values = current->values;
		auto* handler = RE::TESDataHandler::GetSingleton();
		std::size_t count = 0;
		for (auto& profile : resolved->values) {
			auto* form = handler ? handler->LookupForm(profile.raceLocalFormId, profile.racePlugin) : nullptr;
			auto* race = form ? form->As<RE::TESRace>() : nullptr;
			if (!race) {
				L->warn("Ignoring Face SDF profile {}: race {}:{:06X} is missing or not a RACE", profile.id, profile.racePlugin, profile.raceLocalFormId);
				continue;
			}
			profile.raceFormId = race->GetFormID();
			++count;
		}
		_profiles.store(std::shared_ptr<const ProfileSet>(std::move(resolved)), std::memory_order_release);
		{
			std::scoped_lock lock(_telemetryMutex);
			_configurationState = count == 0 ? "profiles_unresolved" : "profiles_resolved";
		}
	}

	void FaceSDF::OnD3D11Ready(IDXGIAdapter*, ID3D11Device* a_device)
	{
		if (!_registrationsReady.load(std::memory_order_acquire) || !a_device)
			return;
		try {
			auto drawDesc = buffer::ConstantBufferDesc<DrawConstants>();
			DX::ThrowIfFailed(a_device->CreateBuffer(&drawDesc, nullptr, _drawConstants.put()));
			cs::render::annotation::SetName(_drawConstants.get(), "FaceSDF/DrawConstants");
			const std::array<std::byte, buffer::GetCBufferSize(sizeof(DrawConstants))> drawOff{};
			auto drawOffDesc = buffer::ConstantBufferDesc<DrawConstants>(false);
			D3D11_SUBRESOURCE_DATA drawOffData{ .pSysMem = drawOff.data() };
			DX::ThrowIfFailed(a_device->CreateBuffer(&drawOffDesc, &drawOffData, _drawConstantsOff.put()));
			cs::render::annotation::SetName(_drawConstantsOff.get(), "FaceSDF/DrawConstantsOff");
			struct alignas(16) ConsumerConstants
			{
				std::uint32_t valid = 0;
				std::uint32_t padding[15]{};
			};
			static_assert(sizeof(ConsumerConstants) == buffer::GetCBufferSize(16));
			auto consumerDesc = buffer::ConstantBufferDesc<ConsumerConstants>();
			consumerDesc.Usage = D3D11_USAGE_DEFAULT;
			consumerDesc.CPUAccessFlags = 0;
			const ConsumerConstants consumerOn{ .valid = 1 };
			const ConsumerConstants consumerOff{};
			D3D11_SUBRESOURCE_DATA consumerOnData{ .pSysMem = &consumerOn };
			D3D11_SUBRESOURCE_DATA consumerOffData{ .pSysMem = &consumerOff };
			DX::ThrowIfFailed(a_device->CreateBuffer(&consumerDesc, &consumerOnData, _consumerConstants.put()));
			DX::ThrowIfFailed(a_device->CreateBuffer(&consumerDesc, &consumerOffData, _consumerConstantsOff.put()));
			cs::render::annotation::SetName(_consumerConstants.get(), "FaceSDF/ConsumerConstants");
			cs::render::annotation::SetName(_consumerConstantsOff.get(), "FaceSDF/ConsumerConstantsOff");
			auto current = _profiles.load(std::memory_order_acquire);
			auto prepared = std::make_shared<ProfileSet>();
			if (current)
				prepared->values = current->values;
			std::size_t loaded = 0;
			for (auto& profile : prepared->values) {
				DirectX::ScratchImage image;
				DirectX::TexMetadata metadata{};
				if (FAILED(DirectX::LoadFromDDSFile(profile.texturePath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image)) ||
					metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 || !SupportsSdfFormat(metadata.format)) {
					L->warn("Ignoring Face SDF profile {}: expected a two-channel R8G8_UNORM or R16G16_FLOAT DDS", profile.id);
					continue;
				}
				winrt::com_ptr<ID3D11ShaderResourceView> srv;
				DX::ThrowIfFailed(DirectX::CreateShaderResourceView(a_device, image.GetImages(), image.GetImageCount(), metadata, srv.put()));
				cs::render::annotation::SetName(srv.get(), std::format("FaceSDF/{}", profile.id));
				profile.srv = std::move(srv);
				++loaded;
			}
			_profiles.store(std::shared_ptr<const ProfileSet>(std::move(prepared)), std::memory_order_release);
			_resourcesReady.store(loaded != 0, std::memory_order_release);
			_consumerReady.store(true, std::memory_order_release);
			{
				std::scoped_lock lock(_telemetryMutex);
				_configurationState = loaded == 0 ? "profiles_without_valid_sdf_resource" : "profiles_ready";
			}
		} catch (const std::exception& a_error) {
			L->error("Face SDF resources unavailable; keeping Native rendering: {}", a_error.what());
			_resourcesReady.store(false, std::memory_order_release);
			_consumerReady.store(false, std::memory_order_release);
			_consumerConstants = nullptr;
			_consumerConstantsOff = nullptr;
		}
	}

	bool FaceSDF::IsCandidatePass(RE::BSRenderPass* a_pass, const Profile& a_profile, RE::Actor*& a_actor) noexcept
	{
		return MatchesExplicitProfile(a_pass, a_profile, a_actor);
	}

	void FaceSDF::PrepareImmediateDraw(RE::BSRenderPass* a_pass, void* a_context, bool a_begin) noexcept
	{
		auto* self = static_cast<FaceSDF*>(a_context);
		if (!self)
			return;
		if (!a_begin) {
			if (t_selectionOverflow != 0) {
				--t_selectionOverflow;
				return;
			}
			if (t_selectionDepth == 0 || t_selection.pass != a_pass)
				return;
			t_selection = std::move(t_selectionStack[--t_selectionDepth]);
			t_selectionStack[t_selectionDepth] = {};
			return;
		}
		if (t_selectionDepth == t_selectionStack.size()) {
			++t_selectionOverflow;
			t_selection = {};
			return;
		}
		t_selectionStack[t_selectionDepth++] = std::move(t_selection);
		t_selection = { .owner = self, .pass = a_pass };
		if (!self->IsEnabled() || !self->_resourcesReady.load(std::memory_order_acquire))
			return;
		const auto profiles = self->_profiles.load(std::memory_order_acquire);
		if (!profiles)
			return;
		for (const auto& profile : profiles->values) {
			RE::Actor* actor = nullptr;
			if (!profile.raceFormId || !profile.srv || !IsCandidatePass(a_pass, profile, actor))
				continue;
			auto& constants = t_selection.constants;
			std::copy(profile.forward.begin(), profile.forward.end(), constants.begin());
			std::copy(profile.right.begin(), profile.right.end(), constants.begin() + 4);
			std::copy(profile.up.begin(), profile.up.end(), constants.begin() + 8);
			std::copy(profile.uvScaleBias.begin(), profile.uvScaleBias.end(), constants.begin() + 12);
			constants[16] = 1.0f;
			constants[17] = profile.softness;
			constants[18] = profile.strength;
			t_selection.texture = profile.srv;
			t_selection.eligible = true;
			self->_eligibleDraws.fetch_add(1, std::memory_order_relaxed);
			return;
		}
		self->_fallbackDraws.fetch_add(1, std::memory_order_relaxed);
	}

	void FaceSDF::BindDraw(ID3D11DeviceContext* a_context) noexcept
	{
		if (!a_context || !_drawConstants)
			return;
		const bool eligible = t_selection.owner == this && t_selection.pass == engine::GetCurrentImmediatePrepassPass() &&
		                      t_selection.eligible && IsEnabled() && _resourcesReady.load(std::memory_order_acquire);
		auto* constants = _drawConstantsOff.get();
		if (eligible) {
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (SUCCEEDED(a_context->Map(_drawConstants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
				std::memcpy(mapped.pData, t_selection.constants.data(), sizeof(t_selection.constants));
				a_context->Unmap(_drawConstants.get(), 0);
				constants = _drawConstants.get();
			}
		}
		if (!constants)
			return;
		const auto stage = engine::GetActiveShaderInjectionStage();
		engine::BindInjectionConstantBuffers(a_context, render::kFaceSdfProfileConstantsSlot.first, 1, &constants);
		if (stage == engine::ShaderStage::kPixel) {
			auto* texture = eligible ? t_selection.texture.get() : nullptr;
			engine::BindInjectionShaderResources(a_context, render::kFaceSdfProfileTextureSlot.first, 1, &texture);
			auto* sampler = render::GetLinearClampSampler();
			engine::BindInjectionSamplers(a_context, render::kLinearClampSamplerSlot.first, 1, &sampler);
		}
	}

	void FaceSDF::BindLighting(ID3D11DeviceContext* a_context) noexcept
	{
		if (!a_context || !_consumerReady.load(std::memory_order_acquire) || !_consumerConstants)
			return;
		const bool valid = IsEnabled() && _resourcesReady.load(std::memory_order_acquire) &&
		                   engine::ValidateParallaxFaceTransportConsumerView(a_context, nullptr, nullptr);
		auto* buffer = valid ? _consumerConstants.get() : _consumerConstantsOff.get();
		if (!buffer)
			return;
		engine::BindInjectionConstantBuffers(a_context, render::kFaceSdfConsumerConstantsSlot.first, 1, &buffer);
	}

	bool FaceSDF::ValidateInjectionPrerequisites(std::string& a_error)
	{
		if (!_registrationsReady.load(std::memory_order_acquire)) {
			a_error = "Face SDF routes and RT6 contributor are unavailable";
			return false;
		}
		return true;
	}

	void FaceSDF::CollectTelemetry(telemetry::Sink& a_sink) const
	{
		const auto profiles = _profiles.load(std::memory_order_acquire);
		std::string configuration;
		{
			std::scoped_lock lock(_telemetryMutex);
			configuration = _configurationState;
		}
		a_sink.Field("profiles", profiles ? profiles->values.size() : 0)
			.Field("resources_ready", _resourcesReady.load(std::memory_order_relaxed))
			.Field("consumer_ready", _consumerReady.load(std::memory_order_relaxed))
			.Field("eligible_draws", _eligibleDraws.load(std::memory_order_relaxed))
			.Field("fallback_draws", _fallbackDraws.load(std::memory_order_relaxed))
			.Field("configuration", configuration);
	}

	namespace
	{
		struct AutoRegister
		{
			AutoRegister() { FeatureManager::Get().Register(FaceSDF::GetSingleton()); }
		};
		static AutoRegister g_autoRegister;
	}
}
