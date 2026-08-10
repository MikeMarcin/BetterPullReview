#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP

#include "bpr/core/agent_session.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bpr::core {
	/// \brief The supplement schema every agent receives a version of.
	///
	/// \details Version two replaces the partial file/defect vocabulary with
	/// exact receipt, defect-history, selection, and session records. A reader
	/// must refuse an unsupported version rather than guessing at field meaning.
	inline constexpr std::uint32_t agent_supplement_schema_version = 2U;

	inline constexpr std::size_t maximum_selected_notes         = 64U;
	inline constexpr std::size_t maximum_selected_drafts        = 64U;
	inline constexpr std::size_t maximum_note_bytes             = 16U * 1024U;
	inline constexpr std::size_t maximum_draft_bytes            = 16U * 1024U;
	inline constexpr std::size_t maximum_supplement_paths       = 4'096U;
	inline constexpr std::size_t maximum_supplement_defects     = 4'096U;
	inline constexpr std::size_t maximum_semantic_segments      = 4'096U;
	inline constexpr std::size_t maximum_semantic_groups        = 4'096U;
	inline constexpr std::size_t maximum_pull_request_url_bytes = 16U * 1024U;
	inline constexpr std::size_t maximum_selected_source_bytes  = 256U * 1024U;

	/// \brief One file's exact local review state.
	struct supplement_file_state
	{
		file_identity    File;
		file_material_id Material;
		std::string      Path;
		file_read_state  Read               = file_read_state::unread;
		bool             ChangedSinceReview = false;
		bool             Accepted           = false;
		bool             Excluded           = false;

		auto operator<=>(const supplement_file_state&) const = default;
	};

	/// \brief One complete private defect identity and retained history.
	struct supplement_defect
	{
		file_identity          File;
		review_conversation_id Conversation;
		defect_history         History;

		auto operator<=>(const supplement_defect&) const = default;
	};

	/// \brief Confidence attached to the selected semantic mapping.
	enum class supplement_mapping_confidence : std::uint8_t {
		exact,
		similar,
		approximate,
		unavailable,
	};

	/// \brief The semantic context the reviewer explicitly permits.
	///
	/// \details These are domain identities and source text, never measured
	/// coordinates, Qt indexes, ribbon paths, or live scroll state.
	struct supplement_semantic_context
	{
		file_identity                 File;
		semantic_selection            Selection;
		supplement_mapping_confidence Confidence
			= supplement_mapping_confidence::unavailable;
		std::vector<alignment_segment> Segments;
		std::vector<change_group>      Groups;
		std::optional<std::string>     SelectedSourceText;

		auto operator<=>(const supplement_semantic_context&) const = default;
	};

	/// \brief A private note the reviewer explicitly chose to include.
	struct supplement_note
	{
		std::optional<file_identity>          File;
		std::optional<file_comparison_digest> Comparison;
		std::string                           Text;

		auto operator<=>(const supplement_note&) const = default;
	};

	/// \brief An unpublished draft the reviewer explicitly chose to include.
	struct supplement_draft
	{
		std::optional<source_line_ref> Anchor;
		std::string                    Text;

		auto operator<=>(const supplement_draft&) const = default;
	};

	/// \brief What the reviewer approved for one versioned supplement.
	///
	/// \details The request contains no store and no implicit note or draft
	/// source. Empty selections therefore stay empty by construction.
	struct agent_supplement_request
	{
		agent_session                              Session;
		std::string                                PullRequestUrl;
		std::optional<git_commit_oid>              PreviousUpload;
		std::optional<git_commit_oid>              LastReviewedHead;
		std::vector<supplement_file_state>         Files;
		std::vector<supplement_defect>             Defects;
		std::optional<supplement_semantic_context> Selection;
		std::vector<supplement_note>               SelectedNotes;
		std::vector<supplement_draft>              SelectedDrafts;

		auto operator<=>(const agent_supplement_request&) const = default;
	};

	/// \brief The local, revision-pinned state BetterPullReview contributes.
	///
	/// \details The agent obtains GitHub state and credentials independently.
	/// This value carries exact local identities and only explicitly admitted
	/// private prose. Optional values remain optional through serialization so
	/// absent never collapses into an empty string or empty collection.
	struct agent_context_supplement
	{
		std::uint32_t SchemaVersion
			= agent_supplement_schema_version;
		agent_session                              Session;
		std::string                                PullRequestUrl;
		std::optional<git_commit_oid>              PreviousUpload;
		std::optional<git_commit_oid>              LastReviewedHead;
		std::vector<supplement_file_state>         Files;
		std::vector<supplement_defect>             Defects;
		std::optional<supplement_semantic_context> Selection;
		std::vector<supplement_note>               Notes;
		std::vector<supplement_draft>              Drafts;

		auto operator<=>(const agent_context_supplement&) const = default;
	};

	/// \brief Builds one exact bounded supplement, or refuses it whole.
	[[nodiscard]] auto build_agent_supplement(
		const agent_supplement_request& aRequest)
		-> std::optional<agent_context_supplement>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP
