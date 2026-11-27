#pragma once

#include "Settings/SettingsSchema.h"

namespace cs::features::face_transport_diagnostic
{
	struct Settings
	{
		bool EnableRGBA8Payload = false;
	};

	inline constexpr settings::Schema kSchema{
		std::tuple{
			settings::Field{ "EnableRGBA8Payload", "Enable the synthetic Face transport payload; restart required.", &Settings::EnableRGBA8Payload, {}, settings::ApplyTiming::kNextLaunch } }
	};
}
