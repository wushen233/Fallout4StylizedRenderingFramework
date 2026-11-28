#pragma once

#include "Render/ShaderDefineProvider.h"

namespace cs::features::face_sdf_shader
{
	inline const engine::ShaderDefineDeclaration kShaderDefines{
		"FACE_SDF",
		{ engine::ShaderInjectionTarget::kDeferredPrepass, engine::ShaderInjectionTarget::kBsdfLight },
		{},
		engine::ShaderInjectionTarget::kDeferredPrepass
	};

	inline const engine::RegisterShaderContribution kRegisterShaderContribution{ "FaceSDF", kShaderDefines };
}
