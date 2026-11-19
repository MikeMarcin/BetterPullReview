#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_FEATURES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_FEATURES_HPP

#include "bpr/core/review_preparation.hpp"
#include "bpr/core/skill_references.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2264]], ahead of RFC-BPR-0129. Features: named, built-in
/// add-ons a prompt or a skill names as `/bpr:<name>`.
///
/// \details The owner, 2026-11-18: "there should be mostly immutable
/// features but we can have some editable features using the prompt library
/// to edit them." Every Feature is immutable unless this registry marks it
/// editable; an editable Feature's text is one of the profile's prompt
/// overrides (`Hook`), edited in Settings › Prompts. `writing-style` is the
/// first Feature; [[BPR-2270]] `comment-labels` is the second. A reference stays in the text as
/// written; the Feature's text rides after the request's own instructions,
/// once however often it is named. Only the person's own text (a template,
/// an override, a skill they installed) can name one: values substituted
/// from pull request data are never read for references.
namespace bpr::core {
	/// \brief The Writing style Feature's name.
	inline constexpr std::string_view writing_style_feature = "writing-style";
	/// \brief [[BPR-2270]]. The Comment labels Feature's name.
	/// \details Owner, 2026-11-18: "Maybe the comment tag is another editable
	/// feature defaulted to our conventional comments".
	inline constexpr std::string_view comment_labels_feature = "comment-labels";

	/// \brief One Feature.
	struct prompt_feature
	{
		std::string_view Name;
		/// The person may change its text in Settings › Prompts.
		bool Editable = false;
		/// Where an editable Feature's text is kept: one of the profile's
		/// prompt overrides. None for an immutable Feature.
		std::optional<writing_hook> Hook;
		/// [[BPR-2270]]. A blank override (`writing_blank`, not empty) turns
		/// the Feature off, so nothing is sent for it; empty is the shipped
		/// text. Owner, 2026-11-18: "could be a user just blanks that out
		/// because they want all plain comments".
		bool BlankTurnsOff = false;
	};

	/// \brief [[BPR-2270]]. What a Feature that `BlankTurnsOff` keeps when the
	/// person clears its text: blank, so it is not the empty override that
	/// means the shipped text.
	inline constexpr std::string_view blank_feature_override = "\n";

	/// \brief Every Feature, in the order the Prompts library lists them.
	[[nodiscard]] auto prompt_features() noexcept -> std::span<const prompt_feature>;
	/// \brief The Feature a name (without `bpr:`) names, or none.
	[[nodiscard]] auto find_prompt_feature(std::string_view aName) noexcept -> const prompt_feature*;
	/// \brief The Feature whose text a prompt override keeps, or none.
	[[nodiscard]] auto feature_of_hook(writing_hook aHook) noexcept -> const prompt_feature*;
	/// \brief The Features the texts name, each once, in first-reference
	/// order. A `/bpr:<name>` that no Feature answers is left out.
	[[nodiscard]] auto referenced_features(std::span<const std::string_view> aTexts) -> std::vector<std::string>;
}

#endif
