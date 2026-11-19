#include "bpr/core/prompt_features.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		constexpr auto Features = std::array{
			prompt_feature{ .Name = writing_style_feature, .Editable = true, .Hook = writing_hook::writing_style },
		};
	}

	auto prompt_features() noexcept -> std::span<const prompt_feature> { return Features; }

	auto find_prompt_feature(const std::string_view aName) noexcept -> const prompt_feature*
	{
		const auto found = std::ranges::find(Features, aName, &prompt_feature::Name);
		return found == Features.end() ? nullptr : &*found;
	}

	auto feature_of_hook(const writing_hook aHook) noexcept -> const prompt_feature*
	{
		const auto found = std::ranges::find(Features, std::optional{ aHook }, &prompt_feature::Hook);
		return found == Features.end() ? nullptr : &*found;
	}

	auto referenced_features(const std::span<const std::string_view> aTexts) -> std::vector<std::string>
	{
		auto names = std::vector<std::string>{ };
		for (const auto text : aTexts) {
			for (const auto& name : find_feature_references(text)) {
				if (find_prompt_feature(name) != nullptr && std::ranges::find(names, name) == names.end()) {
					names.push_back(name);
				}
			}
		}
		return names;
	}
}
