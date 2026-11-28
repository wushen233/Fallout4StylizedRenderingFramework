#pragma once

#include "Feature.h"
#include "Render/PrepassExtraTargets.h"
#include "ShaderDefines.h"

#include <DirectXMath.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <winrt/base.h>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace RE
{
	class Actor;
	class BSRenderPass;
}

namespace cs::features
{
	class FaceSDF final : public ShaderFeature<face_sdf_shader::kShaderDefines>
	{
	public:
		static FaceSDF* GetSingleton();
		std::string_view GetName() const override { return "FaceSDF"; }
		std::string_view GetDisplayName() const override { return "Directional Face SDF"; }
		std::string GetCategory() const override { return FeatureCategories::kMaterials; }
		std::string GetFeatureSummary() const override { return "Opt-in directional face SDF shading for explicitly matched opaque Face materials."; }
		bool DefaultEnabled() const override { return false; }
		bool Configure(const toml::table&, std::string&) override;
		void Load() override;
		void OnDataLoaded() override;
		void OnD3D11Ready(IDXGIAdapter*, ID3D11Device*) override;
		bool ProducesTelemetry() const override { return true; }
		void CollectTelemetry(telemetry::Sink&) const override;

	private:
		struct Profile;
		struct ProfileSet;
		struct DrawConstants;
		static void PrepareImmediateDraw(RE::BSRenderPass*, void*, bool) noexcept;
		static bool IsCandidatePass(RE::BSRenderPass*, const Profile&, RE::Actor*&) noexcept;
		void BindDraw(ID3D11DeviceContext*) noexcept;
		void BindLighting(ID3D11DeviceContext*) noexcept;
		bool ValidateInjectionPrerequisites(std::string&) override;

		std::atomic<std::shared_ptr<const ProfileSet>> _profiles;
		std::atomic<std::uint64_t> _eligibleDraws{ 0 };
		std::atomic<std::uint64_t> _fallbackDraws{ 0 };
		std::atomic_bool _resourcesReady{ false };
		std::atomic_bool _registrationsReady{ false };
		winrt::com_ptr<ID3D11Buffer> _drawConstants;
		winrt::com_ptr<ID3D11Buffer> _drawConstantsOff;
		winrt::com_ptr<ID3D11Buffer> _consumerConstants;
		winrt::com_ptr<ID3D11Buffer> _consumerConstantsOff;
		std::atomic_bool _consumerReady{ false };
		mutable std::mutex _telemetryMutex;
		std::string _configurationState{ "no_explicit_profiles" };
	};
}
