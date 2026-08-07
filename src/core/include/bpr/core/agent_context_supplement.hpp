#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP

#include "bpr/core/agent_session.hpp"
#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bpr::core {
	/// \brief The supplement schema every agent receives a version of.
	///
	/// \details Carried in the supplement itself so a tool reading one can tell
	/// which shape it got. Bumped when a field's meaning changes, not when one
	/// is added.
	inline constexpr std::uint32_t agent_supplement_schema_version = 1U;

	/// \brief The most private notes one supplement may carry.
	inline constexpr std::size_t maximum_selected_notes = 64U;
	/// \brief The longest single note a supplement may carry.
	inline constexpr std::size_t maximum_note_bytes = 16U * 1024U;
	/// \brief The most changed-since-review paths one supplement may carry.
	inline constexpr std::size_t maximum_supplement_paths = 4'096U;

	/// \brief One local defect the reviewer recorded, as the agent sees it.
	///
	/// \details The state and where it is, not what was said about it. A defect
	/// comment is review content the agent can read from GitHub itself; what it
	/// cannot read is that this reviewer marked it unresolved locally.
	struct supplement_defect
	{
		file_identity File;
		std::uint32_t Line = 0U;
		std::string   State;

		auto operator<=>(const supplement_defect&) const = default;
	};

	/// \brief A private note the reviewer explicitly chose to include.
	///
	/// \details Reachable only through `selected_notes` on the request. There
	/// is no default that produces one.
	struct supplement_note
	{
		std::string Text;

		auto operator<=>(const supplement_note&) const = default;
	};

	/// \brief What the reviewer asked to be included beyond the defaults.
	///
	/// \details Explicit by construction. A supplement is built from a request,
	/// and a request that names no note produces a supplement with no note —
	/// there is no path that reads a note store, because this type holds none.
	struct agent_supplement_request
	{
		agent_revision_binding         Binding;
		std::optional<git_commit_oid>  LastReviewedHead;
		std::vector<file_identity>     ChangedSinceReview;
		std::vector<supplement_defect> Defects;
		/// The current selection, when the reviewer is looking at something.
		std::optional<file_identity> SelectedFile;
		/// **Only what the reviewer explicitly chose.** Empty is the default
		/// and the common case.
		std::vector<supplement_note> SelectedNotes;

		auto operator<=>(const agent_supplement_request&) const = default;
	};

	/// \brief The local state BetterPullReview contributes to an agent.
	///
	/// \details **There is no field here a credential could occupy**, and no
	/// field a private note could reach without having been named in the
	/// request. `Notes` is copied from `SelectedNotes` and from nowhere else,
	/// so a supplement built from a request that selected none carries none —
	/// not because a filter removed them, but because there was no source.
	///
	/// A redaction step would be worse in the way that matters: it is a place a
	/// mistake can live, and the mistake is silent. The supplement looks right
	/// to whoever wrote the code and wrong only to the reviewer whose
	/// unfinished thinking is now in a tool they meant to ask about a diff.
	///
	/// The agent retrieves pull request data through its own GitHub tools. This
	/// carries only what GitHub cannot answer.
	struct agent_context_supplement
	{
		std::uint32_t                  SchemaVersion = agent_supplement_schema_version;
		agent_revision_binding         Binding;
		std::optional<git_commit_oid>  LastReviewedHead;
		std::vector<file_identity>     ChangedSinceReview;
		std::vector<supplement_defect> Defects;
		std::optional<file_identity>   SelectedFile;
		std::vector<supplement_note>   Notes;

		auto operator<=>(const agent_context_supplement&) const = default;
	};

	/// \brief Builds the supplement one request describes.
	///
	/// \returns No value when the request exceeds a bound. A supplement is
	/// refused whole rather than truncated: a silently shortened list of
	/// changed files would tell an agent that a file it was not shown is
	/// unchanged.
	[[nodiscard]] auto build_agent_supplement(
		const agent_supplement_request& aRequest)
		-> std::optional<agent_context_supplement>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP
