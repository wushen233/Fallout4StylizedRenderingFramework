// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <toml++/toml.hpp>

namespace cs::localize
{
	struct Language
	{
		std::string code;
		std::string name;
		std::map<std::string, std::string, std::less<>> strings;
	};

	// A process-lifetime catalog: the host copies page labels at registration.
	// Never replace the active table while a host callback may use its strings.
	class Catalog
	{
	public:
		[[nodiscard]] static bool ValidCode(std::string_view a_code) noexcept
		{
			return !a_code.empty() && a_code.size() <= 32 &&
			       std::ranges::all_of(a_code, [](unsigned char c) {
					   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
					          (c >= '0' && c <= '9') || c == '_' || c == '-';
				   });
		}

		[[nodiscard]] static Catalog Load(const std::filesystem::path& a_directory, std::string_view a_requested)
		{
			Catalog result;
			std::error_code error;
			for (std::filesystem::directory_iterator it(a_directory, error), end; !error && it != end; it.increment(error)) {
				try {
					const auto& path = it->path();
					if (path.extension() != ".toml" || !it->is_regular_file())
						continue;
					const auto code = path.stem().string();
					if (!ValidCode(code) || code == "en" || it->file_size() > 1024 * 1024)
						continue;
					std::ifstream input(path, std::ios::binary);
					if (!input)
						continue;
					const auto document = toml::parse(input);
					const auto name = document["language"].value<std::string>();
					const auto* strings = document["strings"].as_table();
					if (!name || name->empty() || name->find('\0') != std::string::npos || !strings)
						continue;
					Language language{ code, *name, {} };
					for (const auto& [key, node] : *strings) {
						const auto value = node.value<std::string>();
						if (value && !value->empty() && value->find('\0') == std::string::npos)
							language.strings.emplace(std::string(key.str()), *value);
					}
					if (!language.strings.empty())
						result._languages.push_back(std::move(language));
				} catch (const std::exception&) {
					// A damaged or unavailable translation must not prevent the menu from loading.
				}
			}
			std::ranges::sort(result._languages.begin() + 1, result._languages.end(), {}, &Language::code);
			for (std::size_t i = 0; i < result._languages.size(); ++i) {
				if (result._languages[i].code == a_requested) {
					result._active = i;
					break;
				}
			}
			return result;
		}

		[[nodiscard]] const std::vector<Language>& Languages() const noexcept { return _languages; }
		[[nodiscard]] const std::string& ActiveCode() const noexcept { return _languages[_active].code; }

		[[nodiscard]] const std::string* Find(std::string_view a_english) const noexcept
		{
			const auto& strings = _languages[_active].strings;
			const auto it = strings.find(a_english);
			return it != strings.end() ? &it->second : nullptr;
		}

		[[nodiscard]] std::string Translate(std::string_view a_english) const
		{
			const auto* translated = Find(a_english);
			return translated ? *translated : std::string(a_english);
		}

	private:
		std::vector<Language> _languages{ { "en", "English", {} } };
		std::size_t _active{};
	};

	// First used after core configuration is loaded, during host registration.
	[[nodiscard]] const Catalog& Get();

	// Display text only. Do not pass translated strings as printf/format strings.
	[[nodiscard]] inline const char* Text(const char* a_english)
	{
		if (!a_english)
			return nullptr;
		const auto* translated = Get().Find(a_english);
		return translated ? translated->c_str() : a_english;
	}

	[[nodiscard]] inline std::string Translate(std::string_view a_english)
	{
		return Get().Translate(a_english);
	}
}
