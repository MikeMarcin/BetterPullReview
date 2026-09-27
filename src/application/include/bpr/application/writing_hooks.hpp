#ifndef BPR_APPLICATION_WRITING_HOOKS_HPP
#define BPR_APPLICATION_WRITING_HOOKS_HPP
#include "bpr/core/review_preparation.hpp"
namespace bpr::application {
	inline constexpr std::string_view writing_hook_version = "review-preparation-v1";
	/// \brief Shipped instruction resource; no runtime file or repository lookup.
	[[nodiscard]] auto default_writing_hook(core::writing_hook aHook) -> std::string_view;
	[[nodiscard]] auto writing_application_contract() -> std::string_view;
	[[nodiscard]] auto effective_writing_hook(const core::writing_hook_settings& aSettings, core::writing_hook aHook) -> std::string_view;
}
#endif
