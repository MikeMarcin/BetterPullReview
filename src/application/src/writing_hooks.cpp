#include "bpr/application/writing_hooks.hpp"
#include "writing_hooks_data.hpp"
namespace bpr::application {
	auto default_writing_hook(core::writing_hook aHook) -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aHook);
		return index < detail::WritingDefaults.size() ? detail::WritingDefaults[index] : std::string_view{ };
	}
	auto writing_application_contract() -> std::string_view { return detail::WritingContract; }
	auto effective_writing_hook(const core::writing_hook_settings& aSettings, core::writing_hook aHook) -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aHook);
		if (index >= aSettings.Overrides.size() || !core::valid(aSettings)) {
			return { };
		}
		return aSettings.Overrides[index].empty() ? default_writing_hook(aHook) : aSettings.Overrides[index];
	}
}
