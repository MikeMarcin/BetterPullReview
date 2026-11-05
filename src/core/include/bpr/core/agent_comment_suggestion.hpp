#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_COMMENT_SUGGESTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_COMMENT_SUGGESTION_HPP

#include "bpr/core/feedback_convention.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1861]]. The Conventional Comments label an agent wrote at the
/// start of a comment it suggested, read into the comment editor's own label
/// model.
/// \details Owner, 2026-11-04: "Suggested comments from the agent should use
/// the conventional comments labels". The review skill asks the agent to open
/// each suggested comment with a header such as
/// `suggestion (non-blocking): subject`, but a model writes what it writes:
/// `**Suggestion:** subject`, `Issue(Blocking) : subject`, a header alone on
/// its first line with the subject below it. This reads those variants and
/// writes the header again through the editor's own serializer
/// (`decide_feedback_composition_edit`, the edit the label chip makes), so the
/// draft opens with the label in the chip in the catalog's lowercase spelling
/// and its decorations in the toggles, exactly as if the reviewer had chosen
/// them there.
///
/// It adds nothing the agent did not write. A missing label stays missing, an
/// unknown one (`nit:`, `bug:`) is left as the text it is and no alias is
/// read, and a header that cannot be rewritten without restructuring the body
/// (a subject that is a list or a fence) is left exactly as written. Pure,
/// bounded and linear: equal text gives equal bytes.
namespace bpr::core {
	/// \brief How a suggested comment's first line was read.
	enum class agent_comment_reading : std::uint8_t {
		/// No header at the start: the text is the draft as written.
		unlabelled,
		/// A header-shaped start whose label is not in the catalog. The text
		/// is kept as written and no label is set.
		unknown_label,
		/// A catalog label and its decorations, now written as the label
		/// control writes them.
		labelled,
		/// A catalog label began a header the serializer could not write
		/// without restructuring the body or exceeding its bound. The text is
		/// kept as written.
		kept_as_written,
	};

	[[nodiscard]] auto stable_name(agent_comment_reading aReading) noexcept -> std::string_view;

	/// \brief One suggested comment, ready to be a draft's body.
	struct agent_comment_suggestion
	{
		agent_comment_reading Reading = agent_comment_reading::unlabelled;
		/// Set only when `labelled`.
		std::optional<feedback_label> Label = std::nullopt;
		/// Which standard decorations the written header carries, read back
		/// from it. A label carries one weight, so of `blocking` and
		/// `non-blocking` the later one written stays, as the toggles do.
		bool Blocking    = false;
		bool NonBlocking = false;
		bool IfMinor     = false;
		/// Custom decorations, as written, in order, each once.
		std::vector<std::string> Custom;
		/// The draft's body: the canonical header and the agent's own subject
		/// and discussion when `labelled`, otherwise the text as given.
		std::string Body;

		auto operator==(const agent_comment_suggestion&) const -> bool = default;
	};

	/// \brief Read the label of one comment an agent suggested.
	/// \details Leading blank lines and the first line's leading spaces are
	/// passed over. The header may wrap its label, or the label and its
	/// decorations, or the whole header through the colon, in `**`, `__`, `*`
	/// or `_`. The label matches a catalog ID ASCII case-insensitively, with
	/// no alias, abbreviation or plural. Spaces may stand before the
	/// decoration list, inside it and before the colon. A standard decoration
	/// matches case-insensitively with `-`, `_` or a space between its words
	/// (`Non Blocking`, `if_minor`); any other decoration must be a token the
	/// editor can write and is kept as written. A header whose subject is
	/// empty takes its subject from the next non-blank line. Only the first
	/// `maximum_feedback_prefix_bytes` of the text are examined for a header.
	[[nodiscard]] auto read_agent_comment_suggestion(std::string_view aText) -> agent_comment_suggestion;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_COMMENT_SUGGESTION_HPP
