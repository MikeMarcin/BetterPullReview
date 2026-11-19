#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKFLOW_ACTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKFLOW_ACTION_HPP

#include "bpr/core/local_draft_publication.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/workflow_configuration.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2032]], [[BPR-2033]], [[BPR-2034]], RFC-BPR-0112. A local
/// draft's selected workflow and its actions: the bounded request BPR writes
/// for a companion, the answer it admits, the honest state of each check,
/// and the interactive handoff of an effectful action with its recovery.
///
/// \details Pure. ACTIONS-01 as settled on 2026-11-10: BPR starts no
/// companion program. A command action is handed off: BPR writes the
/// request, names the exact argument array, and the author runs it in their
/// own terminal or harness, whose approval owns what it does. BPR reads the
/// answer back and labels it reported, never observed. An agent action runs
/// in BPR's own tool-free writing lane and only proposes. Nothing here holds
/// an enterprise rule: a check is met only by the companion's own findings
/// on the current candidate, configuration and upstream revision, and any
/// limitation it reports (a changed upstream rule, a missing helper, no dry
/// run, a review marker only the upstream tool can write) leaves it unmet.
/// A publication attempt has exactly one publisher; an external one is
/// associated only after GitHub is read, and nothing is replayed.
namespace bpr::core {
	struct local_draft;

	inline constexpr std::string_view workflow_request_format   = "bpr.workflow-request";
	inline constexpr std::string_view workflow_response_format  = "bpr.workflow-response";
	inline constexpr std::uint32_t    workflow_exchange_version = 1U;
	/// The evidence flow's external-result admission ceiling, reused (RFC-BPR-0112).
	inline constexpr std::size_t maximum_workflow_response_bytes = 16U * 1024U * 1024U;
	inline constexpr std::size_t maximum_workflow_items          = 64U;
	inline constexpr std::size_t maximum_workflow_message_bytes  = 16U * 1024U;
	inline constexpr std::size_t maximum_workflow_answer_bytes   = 16U * 1024U;

	/// \brief One action's state on the draft.
	enum class workflow_action_state : std::uint8_t {
		/// Never invoked, or reconciled and ready again.
		idle,
		/// The request is written and the author holds the command (or the
		/// agent is asked); no answer yet.
		handed_off,
		/// The companion asked the author something; Continue sends the answer.
		awaiting_input,
		/// The companion answered with its result.
		reported,
		/// The companion reported that it failed.
		failed,
		/// The companion reported it was stopped before it finished.
		interrupted,
		/// The companion does not know what happened, or its answer was lost.
		uncertain,
	};
	[[nodiscard]] auto stable_name(workflow_action_state aState) noexcept -> std::string_view;

	enum class workflow_finding_state : std::uint8_t {
		met,
		unmet,
		unknown,
	};
	[[nodiscard]] auto stable_name(workflow_finding_state aState) noexcept -> std::string_view;
	struct workflow_finding
	{
		std::string            Check;
		workflow_finding_state State = workflow_finding_state::unknown;
		std::string            Detail;

		auto operator<=>(const workflow_finding&) const = default;
	};

	/// \brief What a companion says it could not do honestly.
	enum class workflow_limitation_kind : std::uint8_t {
		/// The upstream tool's rules changed since the preparation it checked.
		upstream_changed,
		/// A helper the companion needs is not installed or not callable.
		missing_helper,
		/// The upstream tool has no dry run, so it cannot be used as a check.
		no_dry_run,
		/// A marker (such as a genuine self-review) only the upstream tool can write.
		review_marker,
		/// Any other step the companion cannot decompose.
		unsupported,
	};
	[[nodiscard]] auto stable_name(workflow_limitation_kind aKind) noexcept -> std::string_view;
	[[nodiscard]] auto describe(workflow_limitation_kind aKind) noexcept -> std::string_view;
	struct workflow_limitation
	{
		workflow_limitation_kind Kind = workflow_limitation_kind::unsupported;
		std::string              Detail;

		auto operator<=>(const workflow_limitation&) const = default;
	};

	/// \brief The upstream tool a companion consulted, as it reported it.
	struct workflow_upstream
	{
		std::string Name;
		std::string Fingerprint;

		auto operator<=>(const workflow_upstream&) const = default;
	};

	enum class workflow_reference_kind : std::uint8_t {
		pull_request,
		ticket,
		visual,
		checklist,
	};
	[[nodiscard]] auto stable_name(workflow_reference_kind aKind) noexcept -> std::string_view;
	enum class workflow_reference_state : std::uint8_t {
		created,
		updated,
		failed,
		/// The companion does not know whether it exists.
		unknown,
	};
	[[nodiscard]] auto stable_name(workflow_reference_state aState) noexcept -> std::string_view;
	/// \brief An external object a companion reported. Reported, not observed.
	struct workflow_reference
	{
		workflow_reference_kind  Kind  = workflow_reference_kind::ticket;
		workflow_reference_state State = workflow_reference_state::unknown;
		std::string              Id;
		std::string              Url;
		std::string              Detail;

		auto operator<=>(const workflow_reference&) const = default;
	};

	/// \brief One action's record on the draft: what was last asked and what came back.
	struct workflow_action_record
	{
		std::string           Action;
		workflow_action_state State = workflow_action_state::idle;
		/// The current invocation; 0 before the first.
		std::uint64_t Invocation = 0;
		/// What the invocation was made on.
		std::uint64_t Candidate = 0;
		std::string   Revision;
		std::uint64_t TitleRevision       = 0;
		std::uint64_t DescriptionRevision = 0;
		/// For a publication action: the accepted plan's digest handed off.
		std::string PlanDigest;
		/// The author's answer the invocation carried, and the question the
		/// companion asked.
		std::string Answer;
		std::string Question;
		/// The companion's own words.
		std::string                      Message;
		std::vector<workflow_finding>    Findings;
		std::vector<workflow_limitation> Limitations;
		std::optional<workflow_upstream> Upstream;
		std::vector<workflow_reference>  References;
		/// Proposals its answers recorded.
		std::vector<std::uint64_t> Proposals;
		/// The author reconciled an unsettled answer (read GitHub, or checked
		/// themselves) and may hand it off again.
		bool         Reconciled = false;
		std::int64_t At         = 0;

		auto operator<=>(const workflow_action_record&) const = default;
	};

	/// \brief Who publishes the draft's pull request: one per attempt.
	enum class workflow_publisher : std::uint8_t {
		built_in,
		external,
	};
	[[nodiscard]] auto stable_name(workflow_publisher aPublisher) noexcept -> std::string_view;

	/// \brief The draft's selected workflow, pinned at the revision the author accepted.
	struct draft_workflow
	{
		workflow_configuration              Workflow;
		std::string                         Revision;
		std::vector<workflow_action_record> Actions;
		std::uint64_t                       NextInvocation = 1;
		/// External only when the workflow has a publish action and the author chose it.
		workflow_publisher Publisher = workflow_publisher::built_in;

		auto operator<=>(const draft_workflow&) const = default;
	};
	[[nodiscard]] auto valid(const draft_workflow& aWorkflow) -> bool;
	[[nodiscard]] auto encode_draft_workflow(const draft_workflow& aWorkflow) -> std::string;
	[[nodiscard]] auto decode_draft_workflow(std::string_view aText) -> std::optional<draft_workflow>;
	[[nodiscard]] auto find_action(const draft_workflow& aWorkflow, std::string_view aAction) -> const workflow_action*;
	[[nodiscard]] auto find_record(const draft_workflow& aWorkflow, std::string_view aAction) -> const workflow_action_record*;

	enum class workflow_refusal : std::uint8_t {
		/// The draft changed since it was read.
		stale,
		/// The draft has no workflow, or no action with that ID.
		unknown,
		/// The action's state does not allow it (an answer already in, a
		/// question not asked, a handoff not yet reconciled).
		state,
		/// The answer is not this invocation's, or not for this draft.
		mismatched,
		/// An answer or argument out of its bounds.
		invalid,
		/// A publication is under way, uncertain or linked.
		publication_open,
		/// No current plan, or the digest is not the plan's.
		no_plan,
		/// A required check is not met.
		checks_unmet,
		/// The draft is not linked to a pull request yet.
		not_linked,
		/// The author chose the other publisher.
		other_publisher,
		/// Another publication action is handed off and unresolved.
		external_open,
	};
	[[nodiscard]] auto stable_name(workflow_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Selects a workflow for the draft, or clears it with none. Only
	/// this draft changes; its earlier action records go with the earlier
	/// selection. Refused while an external publication is unresolved.
	[[nodiscard]] auto select_workflow(const local_draft& aDraft, std::uint64_t aExpectedVersion, const std::optional<workflow_configuration>& aWorkflow, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;
	/// \brief Accepts the profile's newer revision of the selected workflow.
	/// Records stay; checks made under the earlier revision are no longer met.
	[[nodiscard]] auto accept_workflow_update(const local_draft& aDraft, std::uint64_t aExpectedVersion, const workflow_configuration& aWorkflow, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;
	/// \brief Chooses the publisher. External needs a publish action.
	[[nodiscard]] auto choose_publisher(const local_draft& aDraft, std::uint64_t aExpectedVersion, workflow_publisher aPublisher, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;

	/// \brief Starts an invocation of a preparation action (any but the two
	/// publication slots): a new invocation number bound to the latest
	/// candidate and the current field revisions. `aAnswer` is the author's
	/// answer to the companion's question, when continuing one. Only this
	/// action's record changes.
	[[nodiscard]] auto begin_action(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aAction, std::string aAnswer, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;

	/// \brief The exact request for the action's current invocation, as the
	/// file the companion reads. A publication action's request carries the
	/// recorded plan and, once linked, the pull request.
	[[nodiscard]] auto encode_workflow_request(const local_draft& aDraft, std::string_view aAction) -> std::optional<std::string>;

	/// \brief A companion's answer, read and bounded but not yet admitted.
	enum class workflow_outcome : std::uint8_t {
		proposal,
		findings,
		input_request,
		completed,
		failed,
		interrupted,
		uncertain,
	};
	[[nodiscard]] auto stable_name(workflow_outcome aOutcome) noexcept -> std::string_view;
	struct workflow_response
	{
		std::uint64_t                     Invocation = 0;
		std::string                       Draft;
		std::string                       Action;
		workflow_outcome                  Outcome = workflow_outcome::failed;
		std::string                       Message;
		std::vector<local_proposal_draft> Proposals;
		std::vector<workflow_finding>     Findings;
		std::string                       Question;
		std::optional<workflow_upstream>  Upstream;
		std::vector<workflow_limitation>  Limitations;
		std::vector<workflow_reference>   References;

		auto operator<=>(const workflow_response&) const = default;
	};
	enum class workflow_response_refusal : std::uint8_t {
		too_large,
		malformed,
		not_a_response,
		unsupported_version,
		/// A field or item list out of its bounds, or a proposal for a target a companion cannot propose.
		invalid,
	};
	[[nodiscard]] auto stable_name(workflow_response_refusal aRefusal) noexcept -> std::string_view;
	[[nodiscard]] auto describe(workflow_response_refusal aRefusal) noexcept -> std::string_view;
	[[nodiscard]] auto decode_workflow_response(std::string_view aBytes) -> std::expected<workflow_response, workflow_response_refusal>;

	/// \brief Admits an answer for the action's current invocation. Proposals
	/// are recorded against the field revisions the invocation read, so an
	/// edit the author made since leaves them stale, never applied. A
	/// publication answer only records what was reported: a pull request is
	/// associated by `link_external_publication` after GitHub is read.
	[[nodiscard]] auto admit_workflow_response(const local_draft& aDraft, std::uint64_t aExpectedVersion, const workflow_response& aResponse, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;
	/// \brief The author's own record that an answer was lost: the handoff
	/// becomes uncertain, to be reconciled before anything is handed off again.
	[[nodiscard]] auto mark_action_lost(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aAction, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;

	/// \brief Why a required check is not met.
	enum class check_gap : std::uint8_t {
		never_run,
		/// Asked, and not answered (or answered with a question).
		pending,
		/// It ran on an earlier candidate.
		earlier_candidate,
		/// It ran under an earlier revision of the workflow.
		earlier_configuration,
		/// A later answer reported another fingerprint for the same upstream tool.
		upstream_changed,
		/// A finding is unmet or unknown, or the companion reported failure.
		finding,
		/// The companion reported a limitation.
		limitation,
	};
	[[nodiscard]] auto stable_name(check_gap aGap) noexcept -> std::string_view;
	[[nodiscard]] auto describe(check_gap aGap) noexcept -> std::string_view;
	struct check_status
	{
		std::string              Action;
		std::string              Task;
		std::optional<check_gap> Gap;

		auto operator<=>(const check_status&) const = default;
	};
	/// \brief Each required check and why it is unmet, in the workflow's order.
	[[nodiscard]] auto required_checks(const local_draft& aDraft) -> std::vector<check_status>;

	/// \brief Hands the draft's recorded plan to the external publisher:
	/// the author chose external, the digest is the current plan's, nothing
	/// was attempted for it, every required check is met and no other
	/// publication handoff is unresolved.
	[[nodiscard]] auto hand_off_publication(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aAction, std::string_view aDigest, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;
	/// \brief Hands a follow-up (finish_publication) off once the pull
	/// request is linked. An earlier attempt that did not end reported must
	/// have been reconciled; its reported objects travel in the request so
	/// the companion does not make them twice.
	[[nodiscard]] auto hand_off_follow_up(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aAction, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;
	/// \brief Whether an external publication handoff is unresolved: the
	/// built-in route refuses while one is.
	[[nodiscard]] auto external_publication_open(const local_draft& aDraft) -> bool;
	/// \brief The read after an external publication: the pull request
	/// GitHub shows from the draft's branch into its target, or none. Found,
	/// the draft is linked to it as one BPR did not create; none, the
	/// handoff is reconciled and may be handed off again with a new
	/// confirmation. The link's head and base are compared with the plan.
	[[nodiscard]] auto reconcile_external_publication(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aAction, std::optional<local_publication_link> aFound,
													  std::int64_t aNow) -> std::expected<local_draft, workflow_refusal>;
	/// \brief The author's statement that they checked a follow-up's
	/// unsettled objects themselves (BPR cannot query a ticket system); it
	/// may be handed off again.
	[[nodiscard]] auto reconcile_follow_up(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string_view aAction, std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKFLOW_ACTION_HPP
