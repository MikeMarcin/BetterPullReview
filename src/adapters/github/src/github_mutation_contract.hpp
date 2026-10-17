#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_HPP

#include "github_read_contract.hpp"

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/review_mutation_recovery_port.hpp"
#include "bpr/application/review_publication_capability_cache.hpp"
#include "bpr/application/review_submission_contract.hpp"
#include "bpr/application/staged_review_contract.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	inline constexpr std::size_t maximum_review_mutation_body_bytes     = 64U * 1024U;
	inline constexpr std::size_t maximum_review_mutation_response_bytes = 256U * 1024U;
	/// \brief [[BPR-1538]]. One page of 100 reviews or review comments, as
	/// GitHub answers it. A review comment carries its user, links, reactions
	/// and the diff hunk up to its line: 3 to 4 KiB and about 62 JSON values
	/// each on MikeMarcin/BPR-Test#70, 380 KB for a page of 100, and more where
	/// a hunk is long. The ordinary 256 KiB and 4,096-value bounds refused a
	/// page from about 66 comments, so a review with comments on a busy pull
	/// request stayed unknown on every Check status. Under the native
	/// transport's 4 MiB response ceiling and, with its headers, under the
	/// 4 MiB a `gh api --include` answer may print.
	inline constexpr std::size_t maximum_review_mutation_list_response_bytes = 3U * 1024U * 1024U;
	/// \brief [[BPR-1538]]. The JSON values one such page may hold.
	inline constexpr std::size_t maximum_review_mutation_list_values         = 32U * 1024U;
	inline constexpr std::size_t maximum_review_mutation_idempotency_records = 1'024U;

	enum class review_mutation_operation : std::uint8_t {
		observe_pull_request,
		observe_viewer,
		observe_review_comments,
		create_review_comment,
		create_review,
		observe_reviews,
		/// [[BPR-0657]]. GraphQL: a reply under an existing thread, and a
		/// thread resolved or unresolved.
		reply_review_thread,
		resolve_review_thread,
		unresolve_review_thread,
		/// [[BPR-0879]]. GraphQL: which pull request a thread belongs to,
		/// asked before anything is written through that thread.
		observe_review_thread,
		/// [[BPR-0896]]. GraphQL: the comments of the thread a reply landed
		/// in, observed for reconciliation.
		observe_review_thread_comments,
		staged_list_reviews,
		staged_read_review,
		staged_list_comments,
		staged_create_pending,
		staged_update_comment,
		staged_update_summary,
		staged_submit_comment,
		staged_discard_pending,
		staged_update_description,
		staged_list_issue_comments,
		staged_create_issue_comment,
		staged_update_issue_comment,
		/// [[BPR-1368]]. GraphQL: a tour stop edited while its review is
		/// pending, where the REST edit answers 404.
		staged_update_pending_comment,
		/// [[BPR-1560]]. REST compare: the merge base GitHub numbers a LEFT
		/// line in now, asked before anything on a LEFT line is published.
		observe_merge_base,
		/// [[BPR-1572]]. REST compare of the recorded base and the expected
		/// head, asked when the base branch names another merge base.
		observe_recorded_merge_base,
		/// [[BPR-1572]]. REST compare of the recorded base and the base
		/// branch: whether the branch only moved forward from it.
		observe_base_ancestry,
	};

	/// \brief [[BPR-1018]]. What a transport demands of one operation's
	/// exchange with the server.
	///
	/// \details Derived from the operation, never from its path or method, so
	/// an operation added to `review_mutation_operation` has no exchange until
	/// it is classified here, and a transport refuses it until then.
	struct review_mutation_exchange
	{
		http_method Method = http_method::get;
		/// Sent to the GraphQL root and accounted to the `graphql` rate-limit
		/// resource; otherwise a REST resource accounted to `core`.
		bool Graphql = false;
		/// The one status that is success. GitHub answers a new review
		/// comment 201 Created, and everything else here 200 OK.
		int SuccessfulStatus = 200;
		/// The POST may leave only once the caller's transmission witness has
		/// admitted it. Exactly the writes the sessions keep a recovery record
		/// for: nothing else consults a witness, whether or not one is given.
		bool Witnessed = false;

		auto operator<=>(const review_mutation_exchange&) const = default;
	};

	/// \brief [[BPR-1018]]. The exchange of one operation; none for a value
	/// outside the enumeration.
	[[nodiscard]] auto review_mutation_exchange_for(
		review_mutation_operation aOperation) noexcept
		-> std::optional<review_mutation_exchange>;
	/// \brief [[BPR-1538]]. The largest response body one operation's
	/// exchange may carry: a page of the review list or of the review comment
	/// collection may carry `maximum_review_mutation_list_response_bytes`,
	/// everything else `maximum_review_mutation_response_bytes`.
	[[nodiscard]] auto maximum_review_mutation_wire_response_bytes(
		review_mutation_operation aOperation) noexcept -> std::size_t;

	enum class review_mutation_contract_failure : std::uint8_t {
		invalid_request,
		unexpected_method,
		unexpected_path,
		unexpected_headers,
		unexpected_body,
		invalid_status,
		redirect,
		malformed_response,
		identity_mismatch,
		response_limit,
		target_unavailable,
		/// [[BPR-1540]]. A paged list continues past the pages it may read.
		collection_limit,
	};

	/// \brief Carries one whole-review summary without a source-line anchor.
	struct review_submission_request
	{
		application::review_mutation_target_binding  Target;
		application::review_mutation_acting_identity ActingIdentity;
		core::review_disposition                     Disposition = core::review_disposition::comment;
		std::string                                  Body;
		/// [[BPR-1345]]. The pending drafts the review carries, sent as its
		/// `comments`; none keeps a summary-only review's exact bytes.
		std::vector<core::review_submission_comment> Comments = { };

		auto operator<=>(const review_submission_request&) const = default;
	};

	struct review_mutation_wire_request
	{
		using semantic_type = std::variant<
			application::review_mutation_target_binding,
			application::review_mutation_acting_identity,
			application::review_mutation_request,
			review_submission_request,
			application::staged_review_request,
			application::review_thread_resolution_request>;

		semantic_type                       Semantic;
		application::github_server_contract Server    = application::github_server_contract::github_com_2022_11_28;
		review_mutation_operation           Operation = review_mutation_operation::observe_pull_request;
		http_method                         Method    = http_method::get;
		std::string                         Path;
		std::vector<http_header>            Headers;
		std::string                         Body;

		auto operator==(const review_mutation_wire_request&) const -> bool = default;
	};

	struct review_mutation_wire_response
	{
		int                      Status = 0;
		std::vector<http_header> Headers;
		std::string              Body;

		auto operator==(const review_mutation_wire_response&) const -> bool = default;
	};

	inline constexpr std::uint32_t off_hunk_classifier_revision = 2U;
	/// \brief A pinned complete non-creation contract selected by the adapter.
	/// The owner-approved recorded contract is restricted to the designated PR.
	/// Synthetic contracts remain explicit fixture inputs, never production defaults.
	struct off_hunk_unsupported_contract
	{
		application::github_server_contract Server             = application::github_server_contract::github_com_2022_11_28;
		review_mutation_operation           Operation          = review_mutation_operation::create_review_comment;
		std::string                         ApiRevision        = "2022-11-28";
		std::uint32_t                       ClassifierRevision = off_hunk_classifier_revision;
		std::string                         Resource;
		std::string                         Field;
		std::string                         Code;
		std::string                         Message;
		bool                                RecordedDesignatedScope                                 = false;
		auto                                operator<=>(const off_hunk_unsupported_contract&) const = default;
	};

	/// \brief Matches only the fully received exact structured non-creation response.
	[[nodiscard]] auto definitive_off_hunk_unsupported(
		const review_mutation_wire_response& aResponse,
		const review_mutation_wire_request&  aRequest,
		const off_hunk_unsupported_contract& aContract) -> bool;

	struct review_mutation_target_observation
	{
		application::review_mutation_target_binding Target;
		/// [[BPR-1572]]. The commit count GitHub reports for the pull request
		/// (`commits`), when the answer carried it.
		std::optional<std::uint64_t> Commits;

		auto operator<=>(const review_mutation_target_observation&) const = default;
	};

	struct review_mutation_viewer_observation
	{
		application::review_mutation_acting_identity Identity;

		auto operator<=>(const review_mutation_viewer_observation&) const = default;
	};

	struct review_mutation_comment_observation
	{
		application::review_mutation_receipt         Receipt;
		std::uint64_t                                DatabaseId = 0U;
		std::string                                  NodeId;
		application::review_mutation_anchor_binding  Anchor;
		std::string                                  Body;
		application::review_mutation_acting_identity Author;
		application::review_remote_comment_subject   RemoteSubject = application::review_remote_comment_subject::line;

		auto operator<=>(const review_mutation_comment_observation&) const = default;
	};

	/// \brief Retains a submitted review identity separately from comment receipts.
	struct review_submission_observation
	{
		std::uint64_t                                ReviewDatabaseId = 0U;
		std::string                                  ReviewNodeId;
		application::review_mutation_target_binding  Target;
		application::review_mutation_acting_identity Author;
		core::review_disposition                     Disposition = core::review_disposition::comment;
		std::string                                  Body;
		std::string                                  SubmittedAt;

		auto operator<=>(const review_submission_observation&) const = default;
	};

	/// \brief A complete bounded review baseline and only exact submitted candidates.
	struct review_submission_reconciliation
	{
		std::vector<application::review_database_id> ReviewIds;
		std::vector<review_submission_observation>   ExactReviews;
	};
	/// \brief [[BPR-0657]]. The exact thread state the server answered with.
	struct review_thread_resolution_observation
	{
		application::review_thread_resolution_receipt Receipt;

		auto operator<=>(const review_thread_resolution_observation&) const = default;
	};

	/// \brief [[BPR-0879]]. The thread the server agrees is in this pull request.
	///
	/// \details Only produced when the observed thread's pull request is the
	/// designated one; any other answer is an identity mismatch, so a caller
	/// holding this value knows the write it is about to make lands in the
	/// pull request the reviewer is looking at.
	struct review_thread_target_observation
	{
		std::string Thread;
		bool        Resolved = false;

		auto operator<=>(const review_thread_target_observation&) const = default;
	};

	struct review_mutation_server_reconciliation
	{
		std::vector<application::review_comment_projection> Projection;
		std::vector<application::review_mutation_receipt>   ExactReceipts;

		auto operator<=>(const review_mutation_server_reconciliation&) const = default;
	};

	using review_mutation_request_result = std::expected<
		review_mutation_wire_request,
		review_mutation_contract_failure>;
	template <typename TValue>
	using review_mutation_parse_result = std::expected<
		TValue,
		review_mutation_contract_failure>;

	/// \brief Compiles only the finite exact-identity staged review catalog:
	/// REST, and the one fixed GraphQL pending stop edit ([[BPR-1368]]).
	[[nodiscard]] auto compile_staged_review_request(const application::staged_review_request& aRequest)
		-> review_mutation_request_result;
	/// \brief Validates authority, returned IDs/URLs, raw bodies and exact bounded pagination.
	[[nodiscard]] auto parse_staged_review_response(const review_mutation_wire_response&      aResponse,
													const application::staged_review_request& aExpected)
		-> review_mutation_parse_result<application::staged_review_observation>;

	/// \brief Compiles the bounded ordinary REST review-list observation.
	[[nodiscard]] auto compile_review_submission_observation_request(const review_submission_request& aRequest, application::github_server_contract aServer = application::github_server_contract::github_com_2022_11_28) -> review_mutation_request_result;
	[[nodiscard]] auto compile_target_observation_request(
		const application::review_mutation_target_binding& aTarget,
		application::github_server_contract                aServer = application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
	[[nodiscard]] auto compile_viewer_observation_request(
		const application::review_mutation_acting_identity& aIdentity,
		application::github_server_contract                 aServer = application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
	[[nodiscard]] auto compile_review_comment_request(
		const application::review_mutation_request& aRequest,
		application::github_server_contract         aServer = application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
	/// \brief Compiles an exact-head REST review with an explicit submission event.
	[[nodiscard]] auto compile_review_submission_request(
		const review_submission_request&    aRequest,
		application::github_server_contract aServer = application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
	/// \brief [[BPR-0657]]. Compiles the GraphQL mutation that resolves or
	/// unresolves one thread; refused for an unbound or malformed request.
	[[nodiscard]] auto compile_thread_resolution_request(
		const application::review_thread_resolution_request& aRequest,
		application::github_server_contract                  aServer
		= application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
	/// \brief [[BPR-0879]]. Compiles the read that names a thread's pull request.
	///
	/// \details Shares every guard the mutation has — it is the mutation's own
	/// compiled request with a read document in place of the write — so a
	/// request the contract would refuse to write is also refused to observe.
	[[nodiscard]] auto compile_thread_target_observation_request(
		const application::review_thread_resolution_request& aRequest,
		application::github_server_contract                  aServer
		= application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
	/// \brief [[BPR-0879]]. Accepts only a thread inside the designated pull request.
	[[nodiscard]] auto parse_thread_target_observation(
		const review_mutation_wire_response&                 aResponse,
		const application::review_thread_resolution_request& aExpected)
		-> review_mutation_parse_result<review_thread_target_observation>;
	/// \brief [[BPR-0657]]. Accepts only the exact thread in the state asked for.
	[[nodiscard]] auto parse_thread_resolution_response(
		const review_mutation_wire_response&                 aResponse,
		const application::review_thread_resolution_request& aExpected)
		-> review_mutation_parse_result<review_thread_resolution_observation>;
	/// \brief Compiles the reconciliation read for one comment mutation.
	///
	/// \details A new root comment is reconciled through the REST comment
	/// list. [[BPR-0896]]: a reply is written by GraphQL, so it is reconciled
	/// through the thread it landed in, by the reserved ObserveThreadComments
	/// document at the GraphQL root.
	[[nodiscard]] auto compile_review_comment_observation_request(
		const application::review_mutation_request& aRequest,
		application::github_server_contract         aServer = application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
	/// \brief [[BPR-1367]]. The pull request's review comment collection is
	/// read at most this many pages of 100. A longer collection is refused,
	/// not read in part.
	inline constexpr std::uint32_t maximum_review_comment_collection_pages = 10U;
	/// \brief Page `aPage` (one-based, at most
	/// `maximum_review_comment_collection_pages`) of the pull request's review
	/// comment collection.
	[[nodiscard]] auto compile_review_comment_collection_request(
		const application::review_mutation_target_binding& aTarget,
		application::github_server_contract                aServer,
		std::uint32_t                                      aPage = 1U)
		-> review_mutation_request_result;
	/// \brief Checks wire consistency with the supplied semantic request and server.
	///
	/// \details Recompiles and compares the complete canonical request. This
	/// detects wire changes that disagree with the compiler; it cannot detect
	/// a defect shared with that compiler or establish request provenance.
	/// Independent method/path/document expectations belong to contract tests
	/// and keyed fixture transports ([[BPR-0904]]).
	[[nodiscard]] auto validate_outbound_review_mutation_request(
		const review_mutation_wire_request& aRequest)
		-> std::expected<void, review_mutation_contract_failure>;
	/// \brief Validates stable PR identity for read-only recovery, ignoring revision movement.
	[[nodiscard]] auto parse_review_submission_recovery_target(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aExpected) -> review_mutation_parse_result<void>;

	/// \brief [[BPR-1572]]. Which pair a merge-base observation compares:
	/// the base branch and the expected head (`observe_merge_base`), the
	/// recorded base and the expected head (`observe_recorded_merge_base`), or
	/// the recorded base and the base branch tip the first answer resolved
	/// (`observe_base_ancestry`, whose merge base is the recorded base exactly
	/// when that tip descends from it). The ancestry request is compiled from
	/// `merge_base_ancestry_binding`, whose expected head is that tip.
	enum class merge_base_observation_basis : std::uint8_t {
		base_branch,
		recorded_base,
		base_ancestry,
	};

	/// \brief [[BPR-1572]]. What one compare answer named: the merge base,
	/// the base commit it was computed against (the base branch's tip for
	/// `base_branch`), and `ahead_by` when the answer carried it.
	struct merge_base_observation
	{
		core::git_commit_oid         MergeBase;
		core::git_commit_oid         BaseCommit;
		std::optional<std::uint64_t> AheadBy;

		auto operator<=>(const merge_base_observation&) const = default;
	};

	/// \brief [[BPR-1572]]. The binding the ancestry observation is compiled
	/// and parsed with: the target, its expected head replaced by the base
	/// branch tip `aTip` the base-branch answer resolved, so the pair is
	/// exactly the recorded base and that commit.
	[[nodiscard]] auto merge_base_ancestry_binding(
		const application::review_mutation_target_binding& aTarget,
		const core::git_commit_oid&                        aTip) -> application::review_mutation_target_binding;

	/// \brief [[BPR-1560]]. Compiles GitHub's compare of the pair `aBasis`
	/// names, on the page with no file list.
	[[nodiscard]] auto compile_merge_base_observation_request(
		const application::review_mutation_target_binding& aTarget,
		application::github_server_contract                aServer,
		merge_base_observation_basis                       aBasis = merge_base_observation_basis::base_branch)
		-> review_mutation_request_result;

	/// \brief [[BPR-1560]]. What GitHub's compare named for the pair `aBasis`
	/// names; the answer must echo that pair's head, name a full commit of the
	/// head's format for both commits, a non-negative `ahead_by` if any, and
	/// for a pair from the recorded base, the recorded base as base commit.
	[[nodiscard]] auto parse_merge_base_observation(
		const review_mutation_wire_response&               aResponse,
		const application::review_mutation_target_binding& aExpected,
		merge_base_observation_basis                       aBasis = merge_base_observation_basis::base_branch)
		-> review_mutation_parse_result<merge_base_observation>;

	/// \brief [[BPR-1572]]. Whether GitHub still numbers LEFT lines in the
	/// target's comparison base. The base branch's merge base naming it
	/// settles it. Otherwise GitHub compares a pull request from the merge
	/// base of its recorded base and head while the base branch only moves
	/// forward from that base, so the lines are kept only when the recorded
	/// base's merge base is the comparison base, its `ahead_by` is the commit
	/// count GitHub reports for the pull request (`aReportedCommits`, read
	/// before and again after these answers), and the recorded base is an
	/// ancestor of the tip the base branch resolved. The caller confirms the
	/// pull request, its recorded base and head unchanged after the answers.
	[[nodiscard]] auto left_lines_numbered_in_comparison_base(
		const application::review_mutation_target_binding& aTarget,
		const merge_base_observation&                      aBranch,
		const std::optional<merge_base_observation>&       aRecorded,
		const std::optional<merge_base_observation>&       aAncestry,
		const std::optional<std::uint64_t>&                aReportedCommits) noexcept -> bool;

	[[nodiscard]] auto parse_target_observation(
		const review_mutation_wire_response&               aResponse,
		const application::review_mutation_target_binding& aExpected)
		-> review_mutation_parse_result<review_mutation_target_observation>;
	[[nodiscard]] auto parse_viewer_observation(
		const review_mutation_wire_response&                aResponse,
		const application::review_mutation_acting_identity& aExpected)
		-> review_mutation_parse_result<review_mutation_viewer_observation>;
	[[nodiscard]] auto parse_review_comment_response(
		const review_mutation_wire_response&        aResponse,
		const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_comment_observation>;
	/// \brief Accepts only the exact submitted review, author, summary and target.
	[[nodiscard]] auto parse_review_submission_response(
		const review_mutation_wire_response& aResponse,
		const review_submission_request&     aExpected)
		-> review_mutation_parse_result<review_submission_observation>;
	/// \brief Reduces a reconciliation answer to every comment's projection
	/// and the exact receipts among them.
	///
	/// \details Dispatches on the request: the REST comment list for a root
	/// comment, the ObserveThreadComments envelope for a reply
	/// ([[BPR-0896]]). In the thread answer a comment that is not ours is
	/// free to be outdated, bot-authored or without a database identity; it
	/// is projected and simply not an exact receipt.
	[[nodiscard]] auto parse_review_comment_observations(
		const review_mutation_wire_response&        aResponse,
		const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_server_reconciliation>;

	/// \brief [[BPR-1345]] [[BPR-1367]]. One comment of the pull request's
	/// comment collection, with the review it belongs to. Lines are
	/// zero-based.
	///
	/// \details GitHub's list of one review's comments
	/// (`/reviews/{id}/comments`) carries no `line`, `side`, `start_line`,
	/// `start_side` or `subject_type`, pending or submitted (probed on
	/// MikeMarcin/BPR-Test#44 and #45). The collection carries them for every
	/// submitted comment, so a submitted review's lines are read here. It
	/// lists no comment of a pending review, not even to its author (#45).
	struct review_comment_collection_entry
	{
		std::uint64_t                Id = 0U;
		std::string                  NodeId;
		std::string                  WebUrl;
		std::string                  PullRequestUrl;
		std::uint64_t                Review = 0U;
		std::string                  Path;
		core::diff_side              Side = core::diff_side::right;
		std::optional<std::uint32_t> Line;
		std::optional<std::uint32_t> StartLine;
		/// The side a range starts on. GitHub allows a range from one side
		/// to the other.
		std::optional<core::diff_side> StartSide;
		std::string                    Body;
		std::string                    CommitId;
		/// [[BPR-1538]]. Where the comment was written: the commit, line and
		/// first line it was created on. GitHub keeps them when a push moves
		/// the comment or leaves it outdated, as `commit_id` and `line` are not.
		std::string                  OriginalCommitId;
		std::optional<std::uint32_t> OriginalLine;
		std::optional<std::uint32_t> OriginalStartLine;
		std::uint64_t                Author = 0U;
		std::string                  AuthorNodeId;
		std::string                  Login;
		/// A comment on a line or range, not on the file.
		bool OnLine = false;
		bool Reply  = false;

		auto operator==(const review_comment_collection_entry&) const -> bool = default;
	};
	/// \brief [[BPR-1367]]. One page of the collection and whether GitHub
	/// links a next one.
	struct review_comment_collection_page
	{
		std::vector<review_comment_collection_entry> Comments;
		bool                                         HasNext = false;
	};
	/// \brief [[BPR-1345]] [[BPR-1367]]. Reads page `aPage` of the pull
	/// request's review comments. A `Link` header must name only pages of
	/// this collection on the expected API origin; `next` must be `aPage + 1`.
	/// Other people's comments are read as structure only: outdated, on a
	/// file, or a range across sides.
	[[nodiscard]] auto parse_review_comment_collection(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aTarget, std::uint32_t aPage = 1U)
		-> review_mutation_parse_result<review_comment_collection_page>;
	/// \brief [[BPR-1367]]. The comments of review `aExpected.Review` among
	/// `aCollection`, as a tour's comments, each held to what a tour line
	/// comment is: mine, on the expected head, on a line, not a reply, at its
	/// own pull request URL and web anchor.
	[[nodiscard]] auto staged_review_comments(const std::vector<review_comment_collection_entry>& aCollection, const application::staged_review_request& aExpected)
		-> review_mutation_parse_result<std::vector<application::staged_review_comment>>;

	/// \brief Rejects incomplete pagination and retains every baseline review ID.
	[[nodiscard]] auto parse_review_submission_observations(const review_mutation_wire_response& aResponse, const review_submission_request& aExpected) -> review_mutation_parse_result<review_submission_reconciliation>;

	struct github_mutation_wire_request
	{
		application::github_credential_operation_context Context;
		review_mutation_wire_request                     Wire;

		auto operator==(const github_mutation_wire_request&) const -> bool = default;
	};

	struct github_mutation_wire_success
	{
		review_mutation_wire_response Response;

		auto operator==(const github_mutation_wire_success&) const -> bool = default;
	};

	enum class github_mutation_transmission : std::uint8_t {
		definitely_not_sent,
		indeterminate,
	};

	struct github_mutation_wire_error
	{
		application::review_mutation_failure Failure = application::review_mutation_failure::unavailable;
		github_mutation_transmission         Transmission
			= github_mutation_transmission::definitely_not_sent;
		std::optional<application::github_provider_failure> ProviderFailure;
		std::uint16_t                                       HttpStatus             = 0;
		application::diagnostic_transport_failure_source    TransportFailureSource = application::diagnostic_transport_failure_source::none;

		auto operator==(const github_mutation_wire_error&) const -> bool = default;
	};

	struct github_mutation_wire_cancelled
	{
		github_mutation_transmission Transmission
			= github_mutation_transmission::definitely_not_sent;

		auto operator==(const github_mutation_wire_cancelled&) const -> bool = default;
	};

	using github_mutation_transmission_witness = std::function<bool()>;

	using github_mutation_wire_completion = std::variant<
		github_mutation_wire_success,
		github_mutation_wire_error,
		github_mutation_wire_cancelled>;

	/// \brief Observation only: fixed vocabulary and numeric status; never response prose or mutation authority.
	[[nodiscard]] auto review_comment_diagnostic(const review_mutation_wire_request& aRequest, const github_mutation_wire_completion& aCompletion) noexcept -> application::diagnostic_event;

	class github_mutation_wire_transport;
	using github_mutation_operation = std::expected<std::shared_ptr<github_mutation_wire_transport>, application::review_mutation_failure>;

	class github_mutation_wire_transport
	{
	public:
		github_mutation_wire_transport()                                      = default;
		github_mutation_wire_transport(const github_mutation_wire_transport&) = delete;
		auto operator=(const github_mutation_wire_transport&)
			-> github_mutation_wire_transport&                           = delete;
		github_mutation_wire_transport(github_mutation_wire_transport&&) = delete;
		auto operator=(github_mutation_wire_transport&&)
			-> github_mutation_wire_transport&    = delete;
		virtual ~github_mutation_wire_transport() = default;

		/// \brief Holds one immutable credential through admission, transmission and reconciliation.
		[[nodiscard]] virtual auto begin_mutation_operation(std::stop_token) -> github_mutation_operation { return std::shared_ptr<github_mutation_wire_transport>{ }; }

		[[nodiscard]] virtual auto perform_review_mutation(
			const github_mutation_wire_request&  aRequest,
			std::stop_token                      aStop,
			github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion = 0;
	};

	/// \brief Restores a session's token-free wire after one serialized operation.
	class github_mutation_operation_scope final
	{
	public:
		github_mutation_operation_scope(github_mutation_wire_transport*& aSlot, std::stop_token aStop)
			: Slot{ aSlot }
			, Original{ aSlot }
			, Owner{ aSlot ? aSlot->begin_mutation_operation(aStop) : github_mutation_operation{ std::unexpected{ application::review_mutation_failure::unavailable } } }
		{
			if (Owner && *Owner) {
				Slot = Owner->get();
			}
		}
		github_mutation_operation_scope(const github_mutation_operation_scope&)                    = delete;
		auto operator=(const github_mutation_operation_scope&) -> github_mutation_operation_scope& = delete;
		~github_mutation_operation_scope() { Slot = Original; }
		[[nodiscard]] auto valid() const noexcept -> bool { return Owner.has_value(); }
		[[nodiscard]] auto failure() const noexcept -> application::review_mutation_failure { return Owner.error(); }

	private:
		github_mutation_wire_transport*& Slot;
		github_mutation_wire_transport*  Original;
		github_mutation_operation        Owner;
	};

	struct live_review_mutation_configuration
	{
		application::github_credential_operation_context CredentialContext;
		application::review_mutation_target_binding      Target;
		application::review_mutation_acting_identity     ActingIdentity;

		auto operator<=>(const live_review_mutation_configuration&) const = default;
	};

	[[nodiscard]] auto valid(
		const live_review_mutation_configuration& aConfiguration) noexcept -> bool;

	/// \brief Selects the RFC-0022 r3 recorded contract only for its exact live identity.
	/// Each classification additionally revalidates the recorded immutable patch
	/// against the sealed source endpoint, blob, path and selected line.
	[[nodiscard]] auto recorded_off_hunk_contract(const live_review_mutation_configuration& aConfiguration)
		-> std::optional<off_hunk_unsupported_contract>;

	/// \brief Implements the production provider protocol without naming its adapter.
	///
	/// \details The concrete provider delegates to this separately linked object
	/// so hermetic tests can exercise exact production protocol logic without
	/// making the live provider reachable from a test executable.
	class review_mutation_session final
	{
	public:
		review_mutation_session(
			github_mutation_wire_transport&        aTransport,
			live_review_mutation_configuration     aConfiguration,
			application::review_mutation_transport aLease,
			std::unique_ptr<application::review_mutation_recovery_port>
														  aRecovery,
			std::optional<off_hunk_unsupported_contract>  aUnsupportedContract = std::nullopt,
			application::review_publication_lineage_port* aLineage = nullptr, std::shared_ptr<application::diagnostic_log_port> aLog = { }) noexcept;

		[[nodiscard]] auto observe_target(std::stop_token aStop = { })
			-> std::expected<core::review_mutation_target, application::review_mutation_failure>;
		[[nodiscard]] auto submit(
			const application::review_mutation_request& aRequest,
			std::stop_token                             aStop = { })
			-> std::expected<application::review_mutation_receipt, application::review_mutation_failure>;
		[[nodiscard]] auto submit_and_reconcile(
			const application::review_mutation_request& aRequest,
			std::stop_token                             aStop = { })
			-> application::review_mutation_submission;
		/// \brief Reads an existing logical journal and server identity; has no admission/POST branch.
		[[nodiscard]] auto reconcile_publication(const application::review_mutation_request&, std::stop_token = { }) -> application::review_mutation_submission;
		void               repair_pending_publications(std::stop_token = { });
		/// \brief [[BPR-0657]]. Resolves or unresolves one thread through the
		/// leased transport; idempotent on the server, so no recovery record.
		[[nodiscard]] auto resolve_thread(
			const application::review_thread_resolution_request& aRequest,
			std::stop_token                                      aStop = { })
			-> std::expected<application::review_thread_resolution_receipt, application::review_mutation_failure>;

	private:
		[[nodiscard]] auto finish_lineage(const application::review_mutation_request&, application::review_mutation_submission, std::stop_token) -> application::review_mutation_submission;
		[[nodiscard]] auto submit_publication(const application::review_mutation_request& aRequest, std::stop_token aStop) -> application::review_mutation_submission;
		[[nodiscard]] auto recover_publication(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord, std::stop_token aStop) -> application::review_mutation_submission;
		[[nodiscard]] auto execute_publication_child(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord, std::stop_token aStop) -> application::review_mutation_submission;
		/// \brief Viewer, then target and head. [[BPR-1560]]: with `aLeftLine`,
		/// last the merge base GitHub numbers a LEFT line in, which must still
		/// be the one the comment was written against.
		[[nodiscard]] auto observe_exact(std::stop_token aStop, bool aLeftLine = false)
			-> std::expected<void, application::review_mutation_failure>;
		/// \brief [[BPR-0879]]. Refuses a thread outside the designated pull
		/// request before anything is written through it.
		[[nodiscard]] auto observe_thread_target(
			const application::review_thread_resolution_request& aRequest,
			std::stop_token                                      aStop)
			-> std::expected<
				review_thread_target_observation,
				application::review_mutation_failure>;
		[[nodiscard]] auto reconcile(
			const application::review_mutation_request& aRequest,
			std::stop_token                             aStop)
			-> std::expected<
				review_mutation_server_reconciliation,
				application::review_mutation_failure>;
		[[nodiscard]] auto perform(
			review_mutation_wire_request         aRequest,
			std::stop_token                      aStop,
			github_mutation_transmission_witness aWitness = { })
			-> std::expected<review_mutation_wire_response, application::review_mutation_failure>;

		github_mutation_wire_transport*                             Transport = nullptr;
		live_review_mutation_configuration                          Configuration;
		application::review_mutation_transport                      Lease;
		std::unique_ptr<application::review_mutation_recovery_port> Recovery;
		std::mutex                                                  Mutex;
		std::optional<off_hunk_unsupported_contract>                UnsupportedContract;
		application::review_publication_capability_cache            PublicationCapabilities;
		application::review_publication_lineage_port*               Lineage = nullptr;
		std::shared_ptr<application::diagnostic_log_port>           Log;
	};
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_HPP
