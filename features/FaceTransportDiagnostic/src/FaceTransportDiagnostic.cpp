#include "FaceTransportDiagnostic.h"

#include "Menu/Section.h"
#include "Menu/SettingsEdit.h"
#include "Render/FaceTransportPublicationContract.h"
#include "Render/FeatureShaderBindings.h"
#include "Render/ShaderInjection.h"
#include "Render/SharedData.h"
#include "Telemetry/Telemetry.h"

namespace cs::features
{
	FaceTransportDiagnostic* FaceTransportDiagnostic::GetSingleton()
	{
		static FaceTransportDiagnostic instance;
		return &instance;
	}

	bool FaceTransportDiagnostic::Configure(const toml::table& a_config, std::string& a_error)
	{
		if (!_live.Configure(a_config, a_error))
			return false;
		_current = _live.Get();
		return true;
	}

	bool FaceTransportDiagnostic::HasShaderDefine(engine::ShaderInjectionTarget a_target) const
	{
		return engine::face_transport::GetRegistrationPlan(_current.EnableRGBA8Payload).shaderBindings &&
		       face_transport_diagnostic_shader::kShaderDefines.HasShaderDefine(a_target);
	}

	engine::ShaderDefineOptions FaceTransportDiagnostic::GetShaderDefineOptions(engine::ShaderInjectionTarget a_target) const
	{
		if (HasShaderDefine(a_target) &&
			(a_target == engine::ShaderInjectionTarget::kDeferredPrepass || a_target == engine::ShaderInjectionTarget::kBsdfLight))
			return { { "FACE_TRANSPORT_RGBA8", "1" } };
		return {};
	}

	void FaceTransportDiagnostic::Load()
	{
		const auto registration = engine::face_transport::GetRegistrationPlan(_current.EnableRGBA8Payload);
		if (!registration.shaderBindings && !registration.sharedTargetContributor) {
			// No routes are required while disabled; this is a completed no-op registration phase.
			_registrationsReady.store(true, std::memory_order_release);
			return;
		}
		if (registration.shaderBindings && !cs::engine::RegisterFeatureShaderBindings(*this, cs::engine::AttachParallaxFaceTransportTarget)) {
			FailLoad("Face transport shader contribution registration failed.");
			return;
		}
		if (registration.sharedTargetContributor) {
			_target = cs::engine::RegisterParallaxFaceTransportTarget(
				*this,
				[this] { return _current.EnableRGBA8Payload; },
				true,
				[this](const auto& a_snapshot) { PublishTelemetrySnapshot(a_snapshot); });
			if (!_target) {
				FailLoad("Shared RT6 target registration failed.");
				return;
			}
		}
		_registrationSucceeded.store(true, std::memory_order_release);
		_registrationsReady.store(true, std::memory_order_release);
	}

	bool FaceTransportDiagnostic::ValidateInjectionPrerequisites(std::string& a_error)
	{
		if (!_current.EnableRGBA8Payload)
			return true;
		if (cs::render::IsSharedDataReady())
			return true;
		a_error = "the shared shader substrate is unavailable";
		return false;
	}

	void FaceTransportDiagnostic::PublishTelemetrySnapshot(const engine::PrepassExtraTargetPublication& a_snapshot) noexcept
	{
		std::scoped_lock lock(_telemetryMutex);
		_telemetrySnapshot = a_snapshot;
	}

	void FaceTransportDiagnostic::CollectTelemetry(cs::telemetry::Sink& a_sink) const
	{
		engine::PrepassExtraTargetPublication snapshot;
		{
			std::scoped_lock lock(_telemetryMutex);
			snapshot = _telemetrySnapshot;
		}
		const bool configured = _current.EnableRGBA8Payload;
		const bool operational = configured && _registrationSucceeded.load(std::memory_order_acquire) && IsInjectionOperational();
		const auto formatName = [](DXGI_FORMAT a_format) -> std::string_view {
			switch (a_format) {
			case DXGI_FORMAT_R8_UNORM:
				return "R8_UNORM";
			case DXGI_FORMAT_R8G8B8A8_UNORM:
				return "R8G8B8A8_UNORM";
			default:
				return "UNKNOWN";
			}
		};
		const auto vetoName = [](engine::PrepassExtraTargetPublication::Veto a_veto) -> std::string_view {
			switch (a_veto) {
			case engine::PrepassExtraTargetPublication::Veto::kDiagnosticDisabled:
				return "diagnostic_disabled";
			case engine::PrepassExtraTargetPublication::Veto::kResourceUnavailable:
				return "resource_unavailable";
			case engine::PrepassExtraTargetPublication::Veto::kBindingNotAttempted:
				return "binding_not_attempted";
			case engine::PrepassExtraTargetPublication::Veto::kBindingNotConfirmed:
				return "binding_not_confirmed";
			case engine::PrepassExtraTargetPublication::Veto::kContextMismatch:
				return "context_mismatch";
			case engine::PrepassExtraTargetPublication::Veto::kGenerationMismatch:
				return "generation_mismatch";
			case engine::PrepassExtraTargetPublication::Veto::kPublicationFailed:
				return "publication_failed";
			case engine::PrepassExtraTargetPublication::Veto::kDiagnosticFormatUnavailable:
				return "rgba8_fallback";
			case engine::PrepassExtraTargetPublication::Veto::kUnverifiedViewAndLightAssociation:
				return "unverified_view_light_association";
			}
			return "unknown";
		};
		a_sink
			.Field("configured", configured)
			.Field("operational", operational)
			.Field("registration_required", configured)
			.Field("registration_succeeded", _registrationSucceeded.load(std::memory_order_acquire))
			.Field("registration_state", configured ? (_registrationSucceeded.load(std::memory_order_acquire) ? "registered" : "not_registered") : "not_required_disabled")
			.Field("requested_format", configured ? "R8G8B8A8_UNORM" : "R8_UNORM")
			.Field("actual_format", formatName(snapshot.actualFormat))
			.Field("resource_generation", snapshot.resourceGeneration)
			.Field("frame_serial", snapshot.frameSerial)
			.Field("publication_id", snapshot.publicationId)
			.Field("resource_ready", snapshot.resourceReady)
			.Field("binding_attempted", snapshot.bindingAttempted)
			.Field("binding_confirmed", snapshot.bindingConfirmed)
			.Field("srv_binding_confirmed", snapshot.srvBindingConfirmed)
			.Field("published", snapshot.published)
			.Field("consumer_eligible", false)
			.Field("veto_reason", vetoName(snapshot.veto))
			.Field("rgba8_allocation_fallback", snapshot.rgba8Fallback);
	}

	void FaceTransportDiagnostic::DrawSettings()
	{
		settings::SettingsEdit edit{ *this };
		const auto field = edit.Bind(_live);
		if (const ui::Section section{ "face-transport-diagnostic", "Transport Diagnostic (restart required)" }; section)
			field(&Settings::EnableRGBA8Payload);
	}

	namespace
	{
		struct AutoRegister
		{
			AutoRegister() { FeatureManager::Get().Register(FaceTransportDiagnostic::GetSingleton()); }
		};
		static AutoRegister _autoRegister;
	}
}
