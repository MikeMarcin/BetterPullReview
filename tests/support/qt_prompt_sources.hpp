#ifndef BPR_TESTS_SUPPORT_QT_PROMPT_SOURCES_HPP
#define BPR_TESTS_SUPPORT_QT_PROMPT_SOURCES_HPP

#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/quick_ask.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-2181]] (AC13, Prompts review B1). What a window composition
/// hands the Agent tab and the tour chats: offers decided outside the chat,
/// on what it shows and the pull request's facts. A Qt test with no
/// composition installs one of these, deciding as the composition does.
namespace bpr::tests {
	/// \brief The Agent tab's offers from `aMacros`, the pull request's facts
	/// read from the tab's suggestion context as the composition reads them.
	inline void install_prompt_source(qt::agent_tab_view& aTab, std::vector<core::prompt_macro> aMacros, core::quick_ask_facts aReview = { })
	{
		aTab.set_prompt_source([&aTab, macros = std::move(aMacros), review = std::move(aReview)](const core::prompt_chat_view& aView) {
			auto       facts         = review;
			const auto context       = aTab.suggestion_context_now();
			facts.Base.PullRequest   = context.PullRequest;
			facts.Base.Repository    = context.Repository;
			facts.Base.Draft         = context.Draft;
			facts.Base.AuthorIsMe    = context.AuthorIsMe;
			facts.Base.ChecksFailing = context.ChecksFailing;
			return core::decide_chat_prompts(macros, core::chat_prompt_facts(facts, aView));
		});
	}

	/// \brief A tour chat's offers from `aMacros`.
	inline void install_prompt_source(qt::tour_side_chat& aChat, std::vector<core::prompt_macro> aMacros, core::quick_ask_facts aReview = { })
	{
		aChat.set_prompt_source([macros = std::move(aMacros), review = std::move(aReview)](const core::prompt_chat_view& aView) {
			return core::decide_chat_prompts(macros, core::chat_prompt_facts(review, aView));
		});
	}

	/// \brief The shipped set, placed, as a window gives it before the
	/// profile's own.
	[[nodiscard]] inline auto shipped_placed_prompts() -> std::vector<core::prompt_macro>
	{
		return core::placed_prompt_macros(core::merge_prompt_macros(core::shipped_prompt_macros(), { }));
	}
} // namespace bpr::tests

#endif // BPR_TESTS_SUPPORT_QT_PROMPT_SOURCES_HPP
