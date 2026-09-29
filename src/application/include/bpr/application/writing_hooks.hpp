#ifndef BPR_APPLICATION_WRITING_HOOKS_HPP
#define BPR_APPLICATION_WRITING_HOOKS_HPP
#include "bpr/core/review_preparation.hpp"
#include <span>
#include <vector>
namespace bpr::application {
	struct writing_source
	{
		std::string                            Id;
		std::string                            Text;
		std::optional<core::tour_source_range> Source                                          = std::nullopt;
		auto                                   operator==(const writing_source&) const -> bool = default;
	};
	struct writing_note
	{
		std::string              Kind;
		std::string              Text;
		std::vector<std::string> SourceRefs;
	};
	struct writing_outline_stop
	{
		std::string              Title;
		std::string              Anchor;
		std::vector<std::string> Related;
		std::string              Reason;
		std::string              Question;
	};
	struct writing_result
	{
		bool                              Insufficient = false;
		std::string                       Replacement;
		std::string                       Reason;
		std::vector<writing_note>         Notes;
		std::vector<writing_outline_stop> Stops;
	};
	[[nodiscard]] auto build_writing_prompt(const core::writing_hook_settings& aSettings, const core::writing_request& aRequest, std::span<const writing_source> aSources) -> std::optional<std::string>;
	[[nodiscard]] auto decode_writing_result(std::string_view aJson, core::writing_hook aHook, std::span<const writing_source> aSources, bool aRequiresEvidence) -> std::optional<writing_result>;

	inline constexpr std::string_view writing_hook_version = "review-preparation-v1";
	/// \brief Shipped instruction resource; no runtime file or repository lookup.
	[[nodiscard]] auto default_writing_hook(core::writing_hook aHook) -> std::string_view;
	[[nodiscard]] auto writing_application_contract() -> std::string_view;
	[[nodiscard]] auto effective_writing_hook(const core::writing_hook_settings& aSettings, core::writing_hook aHook) -> std::string_view;
}
#endif
