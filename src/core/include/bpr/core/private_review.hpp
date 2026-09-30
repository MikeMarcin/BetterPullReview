#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PRIVATE_REVIEW_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PRIVATE_REVIEW_HPP

#include "bpr/core/agent_session.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/private_thread_lineage.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/review_ledger.hpp"

#include <array>
#include <expected>
#include <limits>
#include <span>
#include <variant>

namespace bpr::core {
	using agent_run_id                   = strong_id<struct agent_run_id_tag>;
	using review_composition_id          = strong_id<struct review_composition_id_tag>;
	using review_composition_revision_id = strong_id<struct review_composition_revision_id_tag>;
	using review_publication_lineage_id  = strong_id<struct review_publication_lineage_id_tag>;
	using private_ledger_incarnation     = fixed_identity_256<struct private_ledger_incarnation_tag>;

	inline constexpr std::size_t maximum_private_sources = 256U;

	/// \brief Durable namespace; a replacement ledger cannot reuse old references.
	struct private_review_scope
	{
		bpr_profile_id             Profile;
		private_ledger_incarnation Incarnation;
		git_host_id                Host;
		github_account_id          Account;
		repository_id              Repository;
		pull_request_id            PullRequest;
		auto                       operator<=>(const private_review_scope&) const = default;
	};

	/// \brief Durable composition correlation carried by the one existing draft authority.
	struct review_composition_link
	{
		private_review_scope           Scope;
		review_composition_id          Composition;
		review_composition_revision_id Revision;
		private_body_digest            BodyDigest;
		auto                           operator<=>(const review_composition_link&) const = default;
	};

	/// \brief Exact retained revision facts, never a restored runtime permission.
	struct private_review_binding
	{
		private_review_scope                  Scope;
		github_access_mode                    Access = github_access_mode::authenticated;
		host_authority_fingerprint            HostAuthority;
		agent_revision_binding                Revision;
		comparison_anchor_identity            LeftAnchor;
		comparison_anchor_identity            RightAnchor;
		std::optional<file_comparison_digest> Comparison;
		auto                                  operator<=>(const private_review_binding&) const = default;
	};

	struct private_pr_subject
	{
		auto operator<=>(const private_pr_subject&) const = default;
	};
	struct private_file_subject
	{
		file_identity          File;
		file_comparison_digest Comparison;
		auto                   operator<=>(const private_file_subject&) const = default;
	};
	struct private_group_subject
	{
		file_comparison_digest Comparison;
		change_group_id        Group;
		auto                   operator<=>(const private_group_subject&) const = default;
	};
	struct private_segment_subject
	{
		file_comparison_digest Comparison;
		alignment_segment_id   Segment;
		auto                   operator<=>(const private_segment_subject&) const = default;
	};
	struct private_move_subject
	{
		file_comparison_digest Comparison;
		move_relation_id       Move;
		auto                   operator<=>(const private_move_subject&) const = default;
	};
	/// \brief Finding identity belongs to a run, not its reusable session.
	struct private_finding_reference
	{
		agent_run_id        Run;
		std::uint32_t       Ordinal = 0;
		private_body_digest Digest;
		auto                operator<=>(const private_finding_reference&) const = default;
	};
	using private_thread_subject = std::variant<std::monostate, private_pr_subject, private_file_subject,
												source_line_ref, source_boundary_ref, source_span_ref, private_group_subject,
												private_segment_subject, private_move_subject, review_conversation_id, private_finding_reference>;

	/// \brief Migration retains only facts the legacy row actually recorded.
	struct legacy_retained_subject
	{
		private_review_scope                Scope;
		std::optional<private_file_subject> Attachment;
		auto                                operator<=>(const legacy_retained_subject&) const = default;
	};
	struct exact_private_subject
	{
		private_review_binding Binding;
		private_thread_subject Subject;
		auto                   operator<=>(const exact_private_subject&) const = default;
	};
	using private_thread_binding = std::variant<legacy_retained_subject, exact_private_subject>;

	struct private_reviewer_actor
	{
		std::optional<github_account_id> Account;
		auto                             operator<=>(const private_reviewer_actor&) const = default;
	};
	struct private_agent_actor
	{
		agent_run_id Run;
		auto         operator<=>(const private_agent_actor&) const = default;
	};
	struct private_legacy_unknown_actor
	{
		auto operator<=>(const private_legacy_unknown_actor&) const = default;
	};
	using private_review_actor = std::variant<private_legacy_unknown_actor, private_reviewer_actor, private_agent_actor>;
	/// \brief The shared provenance class of a pull-request thread actor: the
	/// reviewer is human, an admitted run is adapter-observed and a migrated
	/// row that never recorded its author is unknown.
	[[nodiscard]] auto provenance_of(const private_review_actor& aActor) noexcept -> authored_provenance;

	enum class private_subject_availability : std::uint8_t { current,
															 stale,
															 unplaceable };
	[[nodiscard]] auto stable_name(private_subject_availability aValue) noexcept -> std::string_view;
	[[nodiscard]] auto valid_private_scope(const private_review_scope& aScope) noexcept -> bool;
	[[nodiscard]] auto valid_private_binding(const private_review_binding& aBinding) noexcept -> bool;
	[[nodiscard]] auto scope_of(const private_thread_binding& aBinding) noexcept -> const private_review_scope&;
	/// \brief Validates diff subjects only against their immutable semantic index.
	/// Shared/finding existence is a separate store observation; IDs alone grant no publication target.
	[[nodiscard]] auto valid_private_subject(const exact_private_subject& aSubject, const diff_semantic_index* aIndex = nullptr) noexcept -> bool;
	/// \brief Observes availability without changing subject or lifecycle.
	[[nodiscard]] auto observe_private_subject(const private_thread_binding& aOriginal,
											   const private_review_binding& aCurrent, bool aExactSubjectAvailable) noexcept -> private_subject_availability;

	/// \brief One compact header; bodies and history stay in bounded store observations.
	struct private_review_thread
	{
		private_review_thread_id Id;
		private_thread_binding   Binding;
		std::uint64_t            Version = 0;
		private_vertex_order     LastOrder;
		private_thread_lifecycle Lifecycle                                       = private_thread_lifecycle::open;
		auto                     operator<=>(const private_review_thread&) const = default;
	};
	/// \brief Exact source identity for a predecessor/reply within one review.
	struct private_revision_reference
	{
		private_review_scope            Scope;
		private_review_thread_id        Thread;
		private_review_turn_id          Turn;
		private_review_turn_revision_id Revision;
		private_vertex_order            Order;
		private_body_digest             Digest;
		auto                            operator<=>(const private_revision_reference&) const = default;
	};
	/// \brief Immutable authored value; edits return a successor rather than mutate it.
	struct private_review_revision
	{
		private_revision_reference                Identity;
		private_review_actor                      Actor;
		std::string                               Body;
		private_revision_completion               Completion = private_revision_completion::incomplete;
		std::optional<private_revision_reference> Predecessor;
		std::optional<private_revision_reference> ReplyTo;
		std::optional<private_derivation_kind>    EditDerivation;
		auto                                      operator<=>(const private_review_revision&) const = default;
	};
	/// \brief Already admitted run facts supplied by the durable authority.
	struct private_run_authorization
	{
		private_review_scope               Scope;
		agent_run_id                       Run;
		private_vertex_order               Order;
		private_review_binding             Binding;
		private_revision_completion        Completion   = private_revision_completion::incomplete;
		std::optional<private_body_digest> OutputDigest = { };
	};
	/// \brief Host-allocated identities and bounded body for a new immutable revision.
	struct private_revision_input
	{
		private_review_turn_id          Turn;
		private_review_turn_revision_id Revision;
		private_vertex_order            Order;
		private_review_actor            Actor;
		std::string                     Body;
		private_revision_completion     Completion = private_revision_completion::complete;
	};
	struct private_revision_commit
	{
		private_review_thread   Thread;
		private_review_revision Revision;
	};
	/// \brief Creates an exact thread and reviewer contribution as one local proposal.
	[[nodiscard]] auto create_private_thread(private_review_thread_id aId, const exact_private_subject& aSubject,
											 const private_revision_input& aInput, const diff_semantic_index* aIndex = nullptr)
		-> std::expected<private_revision_commit, private_review_refusal>;
	/// \brief Converts a legacy row byte-for-byte without inventing revision binding or actor.
	[[nodiscard]] auto migrate_private_thread(private_review_thread_id aId, const legacy_retained_subject& aSubject,
											  const private_revision_input& aInput) -> std::expected<private_revision_commit, private_review_refusal>;
	/// \brief Appends a new turn; replying requires one exact earlier source observation.
	[[nodiscard]] auto append_private_turn(const private_review_thread& aThread, std::uint64_t aExpectedVersion,
										   const private_revision_input& aInput, const private_review_revision* aReplyTo = nullptr,
										   const private_run_authorization* aRun = nullptr) -> std::expected<private_revision_commit, private_review_refusal>;
	/// \brief Appends a reviewer successor using the exact latest revision as CAS.
	/// Incomplete agent text requires explicit adoption; its derivation remains visible.
	[[nodiscard]] auto edit_private_revision(const private_review_thread& aThread, std::uint64_t aExpectedVersion,
											 const private_review_revision& aLatest, private_review_turn_revision_id aExpectedLatest,
											 const private_revision_input& aInput, bool aAdoptIncomplete = false)
		-> std::expected<private_revision_commit, private_review_refusal>;
	/// \brief Changes local visibility/resolution only; reopening an archive is explicit.
	[[nodiscard]] auto set_private_lifecycle(const private_review_thread& aThread, std::uint64_t aExpectedVersion,
											 private_thread_lifecycle aLifecycle) -> std::expected<private_review_thread, private_review_refusal>;
	[[nodiscard]] auto valid_private_revision(const private_review_revision& aRevision) noexcept -> bool;
	[[nodiscard]] auto eligible_private_revision(const private_review_revision& aRevision) noexcept -> bool;

	enum class private_request_lane : std::uint8_t { catalog,
													 transcript,
													 mutation,
													 agent_admission };
	/// \brief Constant-sized request identity; stale/duplicate terminals compare all fields.
	struct private_request_context
	{
		private_review_scope       Scope;
		application_session_id     Session;
		bpr_window_context         Window;
		github_identity_generation IdentityGeneration;
		request_id                 Request;
		request_generation         Generation;
		auto                       operator==(const private_request_context&) const -> bool = default;
	};
	struct private_review_state
	{
		private_review_scope                                  Scope;
		std::optional<private_review_thread_id>               SelectedThread;
		std::optional<private_review_turn_revision_id>        SelectedRevision;
		std::array<std::optional<private_request_context>, 4> Pending;
		std::array<std::uint64_t, 4>                          Generations{ };
	};
	/// \brief Starts a lane with an explicit identity; generation exhaustion refuses.
	[[nodiscard]] auto begin_private_request(private_review_state& aState, private_request_lane aLane,
											 private_request_context aContext) -> std::expected<private_request_context, private_review_refusal>;
	/// \brief Consumes one matching terminal in constant time, including scope/window identity.
	[[nodiscard]] auto finish_private_request(private_review_state& aState, private_request_lane aLane,
											  const private_request_context& aContext) noexcept -> bool;
}
#endif
