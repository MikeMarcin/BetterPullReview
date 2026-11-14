#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROMPT_WORDS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROMPT_WORDS_HPP

#include "bpr/core/prompt_macro.hpp"

#include <QString>

/// \file
/// \brief [[BPR-2179]] (docs/design/prompts-settings-v4/SPEC.md section 7).
/// The words for a prompt's conditions and surfaces, shared by the Ask menus
/// and the Prompts page so a greyed row and its editor say the same thing.
namespace bpr::qt {
	/// \brief A condition's name in the add menu and its row: "Thread is
	/// unresolved".
	[[nodiscard]] auto prompt_fact_name(core::prompt_fact aFact) -> QString;
	/// \brief The group the add menu lists it under: Pull request, File,
	/// Thread or Chat.
	[[nodiscard]] auto prompt_fact_group(core::prompt_fact aFact) -> QString;
	/// \brief Why a greyed Ask menu row cannot run yet, for its tooltip and
	/// accessible description; empty for a condition of the hides kind.
	[[nodiscard]] auto prompt_condition_reason(const core::prompt_condition& aCondition) -> QString;
	/// \brief The one line an open condition row says about being unmet.
	[[nodiscard]] auto prompt_condition_unmet_line(const core::prompt_condition& aCondition) -> QString;
	/// \brief "Pull request menu", "Thread chips", "Tour stop".
	[[nodiscard]] auto prompt_surface_name(core::prompt_surface aSurface) -> QString;
} // namespace bpr::qt

#endif
