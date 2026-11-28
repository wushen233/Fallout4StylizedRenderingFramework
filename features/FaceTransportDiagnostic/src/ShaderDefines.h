#pragma once

#include "Render/ShaderDefineProvider.h"

namespace cs::features::face_transport_diagnostic_shader
{
	inline const engine::ShaderDefineDeclaration kShaderDefines{
		"FACE_TRANSPORT_DIAGNOSTIC", { engine::ShaderInjectionTarget::kDeferredPrepass, engine::ShaderInjectionTarget::kBsdfLight }
	};

	inline const engine::RegisterShaderContribution kRegisterShaderContribution{ "FaceTransportDiagnostic", kShaderDefines };
}
