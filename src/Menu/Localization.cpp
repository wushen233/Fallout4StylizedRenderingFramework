// SPDX-License-Identifier: GPL-3.0-or-later
#include "Menu/Localization.h"

#include "Log.h"
#include "Settings/FeatureConfig.h"

namespace cs::localize
{
	const Catalog& Get()
	{
		static const Catalog catalog = [] {
			const auto root = feature_config::GetRoot();
			const auto requested = root["localization"]["language"].value_or(std::string("en"));
			const auto directory = std::filesystem::path(feature_config::kConfigPath).parent_path() / "Translations";
			auto loaded = Catalog::Load(directory, requested);
			auto* log = cs::log::Get("cs.menu");
			if (loaded.ActiveCode() != requested)
				log->warn("Menu language '{}' is unavailable; using English.", requested);
			log->info("Menu language: {}", loaded.ActiveCode());
			return loaded;
		}();
		return catalog;
	}
}
