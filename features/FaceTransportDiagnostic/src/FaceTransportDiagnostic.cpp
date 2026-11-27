#include "FaceTransportDiagnostic.h"

#include "Menu/Section.h"
#include "Menu/SettingsEdit.h"
#include "Render/FeatureShaderBindings.h"
#include "Render/ShaderInjection.h"
#include "Render/SharedData.h"

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

	engine::ShaderDefineOptions FaceTransportDiagnostic::GetShaderDefineOptions(engine::ShaderInjectionTarget a_target) const
	{
		if (_current.EnableRGBA8Payload &&
			(a_target == engine::ShaderInjectionTarget::kDeferredPrepass || a_target == engine::ShaderInjectionTarget::kBsdfLight))
			return { { "FACE_TRANSPORT_RGBA8", "1" } };
		return {};
	}

	void FaceTransportDiagnostic::Load()
	{
		if (!cs::engine::RegisterFeatureShaderBindings(*this, cs::engine::AttachParallaxFaceTransportTarget)) {
			FailLoad("Face transport shader contribution registration failed.");
			return;
		}
		_target = cs::engine::RegisterParallaxFaceTransportTarget(
			*this,
			[this] { return IsInjectionOperational() && _current.EnableRGBA8Payload; },
			_current.EnableRGBA8Payload);
		if (!_target) {
			FailLoad("Shared RT6 target registration failed.");
			return;
		}
		_registrationsReady.store(true, std::memory_order_release);
	}

	bool FaceTransportDiagnostic::ValidateInjectionPrerequisites(std::string& a_error)
	{
		if (cs::render::IsSharedDataReady())
			return true;
		a_error = "the shared shader substrate is unavailable";
		return false;
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
