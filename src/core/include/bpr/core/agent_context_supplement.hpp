#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONTEXT_SUPPLEMENT_HPP

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
	/// The longest question one chat turn may carry.
	///
	/// \details The same ceiling as a note and a draft, and for the same
	/// reason: it is prose a person typed, and a message longer than the
	/// longest comment they could have written is not a question.
	inline constexpr std::size_t maximum_chat_message_bytes = 16U * 1024U;

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

	/// \brief One chat turn's typed question, keyed to the session it is for.
	///
	/// \details The session identity is the same gate `agent_private_context_optin`
	/// carries and is here for the same reason: a message is prose the reviewer
	/// typed into one conversation, and a build that paired it with a different
	/// session is a caller that should not be sending it. There is no field
	/// here a note, a draft, or a selection could occupy — the message is what
	/// was asked, not a second way to attach what the opt-in refused.
	///
	/// Absent is the ordinary state. Every launch that is not a chat turn omits
	/// this, and the supplement's message is then empty rather than a
	/// placeholder saying so.
	struct agent_chat_message
	{
		agent_session_id Session;
		std::string      Text;

		auto operator<=>(const agent_chat_message&) const = default;
	};

	/// \brief [[BPR-1091]]. The longest review-feedback guidance one
	/// workshop request may carry: the profile hook's own ceiling
	/// (`maximum_writing_instruction_bytes`).
	inline constexpr std::size_t maximum_feedback_guidance_bytes = 8U * 1024U;
	/// \brief The longest name of the shipped guidance version.
	inline constexpr std::size_t maximum_feedback_guidance_version_bytes = 64U;

	/// \brief Where the guidance a workshop request carries came from.
	enum class feedback_guidance_source : std::uint8_t {
		/// The shipped default of the version named beside it.
		shipped,
		/// The profile's own override, sent as written.
		profile_override,
	};
	[[nodiscard]] auto stable_name(feedback_guidance_source aSource) noexcept -> std::string_view;

	/// \brief [[BPR-1091]]. The review-feedback hook's effective text, sent
	/// with one comment-workshop request (RFC-BPR-0051 R34, R35).
	///
	/// \details The application fills it only for a workshop press and only
	/// while the profile's label helper is on; every other launch, and every
	/// press with the helper off, carries none, so its wire bytes are what they
	/// were before the hook existed. `Version` names the shipped default
	/// available to this invocation and `Source` says whether `Text` is that
	/// default or the profile's override, so the run records which guidance it
	/// was given. The text is instruction data for the agent's prose work; it
	/// grants no tool, publication or review authority.
	struct supplement_feedback_guidance
	{
		feedback_guidance_source Source = feedback_guidance_source::shipped;
		std::string              Version;
		std::string              Text;

		auto operator<=>(const supplement_feedback_guidance&) const = default;
	};

	/// \brief What the reviewer approved for one versioned supplement.
	///
	/// \details The request contains no store and no implicit note or draft
	/// source. Empty selections therefore stay empty by construction.
	///
	/// `Message` is the reviewer's typed question, engaged only when this
	/// build is for one chat turn and keyed to the session it was typed into.
	///
	/// `Operation` is which of the six comment-workshop operations this build
	/// is for. Engaged only when the build is a workshop request — which
	/// `workshop_operation_for` decides from the entry point, so that an
	/// operation cannot ride along on a review or a chat turn — and absent for
	/// every other launch. Absent means no operation was asked for, not that
	/// one was dropped.
	struct agent_supplement_request
	{
		application_session_id                     ApplicationSession;
		agent_session                              Session;
		std::string                                PullRequestUrl;
		std::optional<agent_chat_message>          Message;
		std::optional<git_commit_oid>              PreviousUpload;
		std::optional<git_commit_oid>              LastReviewedHead;
		std::vector<supplement_file_state>         Files;
		std::vector<supplement_defect>             Defects;
		std::optional<supplement_semantic_context> Selection;
		std::vector<supplement_note>               SelectedNotes;
		std::vector<supplement_draft>              SelectedDrafts;
		std::optional<workshop_operation>          Operation;
		/// [[BPR-1091]]. Engaged only beside an engaged `Operation`; a request
		/// that carries guidance without one is refused whole.
		std::optional<supplement_feedback_guidance> FeedbackGuidance;

		auto operator<=>(const agent_supplement_request&) const = default;
	};

	/// \brief The local, revision-pinned state BetterPullReview contributes.
	///
	/// \details The agent obtains GitHub state and credentials independently.
	/// This value carries exact local identities and only explicitly admitted
	/// private prose. Optional values remain optional through serialization so
	/// absent never collapses into an empty string or empty collection.
	///
	/// `Message` is what the reviewer typed for this turn, admitted by
	/// `build_agent_supplement` from a request whose `agent_chat_message`
	/// names this session. Empty means no question was asked - which is what
	/// every launch that is not a chat turn carries - rather than a question
	/// this build dropped.
	///
	/// `Operation` is the workshop operation the request named, carried
	/// through unchanged. A request naming an undeclared operation is refused
	/// whole rather than carried with the field cleared, because a supplement
	/// that quietly dropped the operation would ask the agent to revise a
	/// comment without saying how.
	struct agent_context_supplement
	{
		std::uint32_t SchemaVersion
			= agent_supplement_schema_version;
		application_session_id                     ApplicationSession;
		agent_session                              Session;
		std::string                                PullRequestUrl;
		std::string                                Message;
		std::optional<git_commit_oid>              PreviousUpload;
		std::optional<git_commit_oid>              LastReviewedHead;
		std::vector<supplement_file_state>         Files;
		std::vector<supplement_defect>             Defects;
		std::optional<supplement_semantic_context> Selection;
		std::vector<supplement_note>               Notes;
		std::vector<supplement_draft>              Drafts;
		std::optional<workshop_operation>          Operation;
		/// [[BPR-1091]]. The review-feedback guidance of a workshop request,
		/// carried through unchanged; absent for every other launch.
		std::optional<supplement_feedback_guidance> FeedbackGuidance;

		auto operator<=>(const agent_context_supplement&) const = default;
	};

	/// \brief Builds one exact bounded supplement, or refuses it whole.
	///
	/// \details A request carrying a message is refused whole when the message
	/// names another session or is longer than `maximum_chat_message_bytes`,
	/// exactly as an oversize note or an out-of-session opt-in is. Truncating
	/// it would send the agent half a question, and dropping it would send a
	/// turn the reviewer asked for with nothing in it.
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
