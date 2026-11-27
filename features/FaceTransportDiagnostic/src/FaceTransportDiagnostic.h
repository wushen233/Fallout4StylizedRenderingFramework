#pragma once

#include "FaceTransportDiagnosticSettings.h"
#include "Feature.h"
#include "FeatureCategories.h"
#include "Render/PrepassExtraTargets.h"
#include "ShaderDefines.h"

#include <mutex>

namespace cs::features
{
	class FaceTransportDiagnostic : public ShaderFeature<face_transport_diagnostic_shader::kShaderDefines>
	{
	public:
		static FaceTransportDiagnostic* GetSingleton();
		std::string_view GetName() const override { return "FaceTransportDiagnostic"; }
		std::string_view GetDisplayName() const override { return "Face Transport Diagnostic"; }
		std::string GetCategory() const override { return FeatureCategories::kMaterials; }
		std::string GetFeatureSummary() const override { return "Synthetic RT6 transport validation only; does not change lighting."; }
		bool SupportsEnableToggle() const override { return false; }
		bool HasShaderDefine(engine::ShaderInjectionTarget a_target) const override;
		engine::ShaderDefineOptions GetShaderDefineOptions(engine::ShaderInjectionTarget a_target = engine::ShaderInjectionTarget::kCount) const override;
		bool Configure(const toml::table&, std::string&) override;
		void Load() override;
		bool ProducesTelemetry() const override { return true; }
		void CollectTelemetry(telemetry::Sink& a_sink) const override;
		void DrawSettings() override;
		using Settings = face_transport_diagnostic::Settings;

	private:
		settings::LiveSettingsAccess* GetLiveSettings() override { return &_live; }
		std::string_view GetRegistrationFailureMessage() const override { return "diagnostic shader contribution registration failed"; }
		bool ValidateInjectionPrerequisites(std::string&) override;
		void PublishTelemetrySnapshot(const engine::PrepassExtraTargetPublication& a_snapshot) noexcept;
		Settings _current{};
		settings::LiveSettings<face_transport_diagnostic::kSchema> _live;
		const Settings& _settings = _live.Get();
		std::optional<cs::engine::PrepassExtraTargetId> _target;
		std::atomic_bool _registrationSucceeded{ false };
		mutable std::mutex _telemetryMutex;
		engine::PrepassExtraTargetPublication _telemetrySnapshot{};
	};
}
