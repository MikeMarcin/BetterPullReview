#include "bpr/application/writing_hooks.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "writing_hooks_data.hpp"
namespace bpr::application {
	static_assert(detail::WritingDefaults.size() == core::writing_hook_count, "every hook ships a default");
	auto default_writing_hook(core::writing_hook aHook) -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aHook);
		return index < detail::WritingDefaults.size() ? detail::WritingDefaults[index] : std::string_view{ };
	}
	auto writing_hook_version_of(core::writing_hook aHook) noexcept -> std::string_view { return aHook == core::writing_hook::feedback ? review_feedback_hook_version : writing_hook_version; }
	auto writing_application_contract() -> std::string_view { return detail::WritingContract; }
	auto effective_writing_hook(const core::writing_hook_settings& aSettings, core::writing_hook aHook) -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aHook);
		if (index >= aSettings.Overrides.size() || !core::valid(aSettings)) {
			return { };
		}
		return aSettings.Overrides[index].empty() ? default_writing_hook(aHook) : aSettings.Overrides[index];
	}
	auto workshop_feedback_guidance(const core::writing_hook_settings& aSettings, review_label_helper aHelper) -> std::string_view
	{
		return aHelper == review_label_helper::on ? effective_writing_hook(aSettings, core::writing_hook::feedback) : std::string_view{ };
	}
}
