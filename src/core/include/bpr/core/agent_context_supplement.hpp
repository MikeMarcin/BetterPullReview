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
		application_session_id                     ApplicationSession;
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
		application_session_id                     ApplicationSession;
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

	/// \brief Whether one named session may carry the reviewer's private prose.
	///
	/// \details Both default to false, and the opt-in names the session it is
	/// for. That identity is what keeps it from being turned on once and
	/// forgotten: a new session has a new `agent_session_id`, so an opt-in
	/// made for the previous conversation admits nothing into the next one.
	/// The alternative homes were both worse — a profile field would follow
	/// the tool across every review, and a setting would follow the reviewer
	/// across every session, which is precisely the "turned on in March,
	/// leaking in September" shape this avoids.
	///
	/// Notes and drafts are separately opt-in because they are separately
	/// private: a reviewer may well want the agent to see the comment they are
	/// drafting without also handing it the notes they took about the author.
	struct agent_private_context_optin
	{
		agent_session_id Session;
		bool             Notes  = false;
		bool             Drafts = false;

		auto operator<=>(const agent_private_context_optin&) const = default;
	};

	/// \brief The private prose one session is allowed to carry.
	///
	/// \details Produced only by `admit_private_context`. The type holds
	/// exactly the two collections and nothing that could describe them, so
	/// there is no "the reviewer declined, here is a summary anyway" field.
	struct agent_private_context
	{
		std::vector<supplement_note>  Notes;
		std::vector<supplement_draft> Drafts;

		auto operator<=>(const agent_private_context&) const = default;
	};

	/// \brief Admits the reviewer's private prose into one session, or none of it.
	///
	/// \details The gate is the session's own opt-in, checked by identity. An
	/// opt-in belonging to a different session admits nothing rather than
	/// nearly everything: a caller that paired the wrong session with the
	/// wrong opt-in is a caller that should not be sending private prose.
	///
	/// \returns Exactly the supplied notes and drafts whose flag is set for
	/// this session, and empty collections otherwise. Never a subset chosen
	/// here, and never a placeholder.
	[[nodiscard]] auto admit_private_context(
		const agent_session&                 aSession,
		const agent_private_context_optin&   aOptIn,
		const std::vector<supplement_note>&  aNotes,
		const std::vector<supplement_draft>& aDrafts) -> agent_private_context;

	/// \brief Builds one supplement carrying only the prose this session opted into.
	///
	/// \details The same build as the single-argument overload in every other
	/// respect — the same identities, receipts, defect history, and semantic
	/// selection, validated the same way and refused whole the same way — with
	/// the reviewer's selected notes and drafts passed through
	/// `admit_private_context` first. A session that has not opted in carries
	/// zero notes and zero drafts even when the request named some and the
	/// review holds more, and the rest of the supplement is unchanged: the
	/// private prose is dropped, not the context.
	///
	/// An opt-in naming a different session refuses the whole build, as every
	/// other malformed request here does.
	[[nodiscard]] auto build_agent_supplement(
		const agent_supplement_request&    aRequest,
		const agent_private_context_optin& aOptIn)
		-> std::optional<agent_context_supplement>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP
