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
	inline constexpr std::size_t maximum_review_mutation_body_bytes          = 64U * 1024U;
	inline constexpr std::size_t maximum_review_mutation_response_bytes      = 256U * 1024U;
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
	};

	/// \brief Carries one whole-review summary without a source-line anchor.
	struct review_submission_request
	{
		application::review_mutation_target_binding  Target;
		application::review_mutation_acting_identity ActingIdentity;
		core::review_disposition                     Disposition = core::review_disposition::comment;
		std::string                                  Body;

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

	/// \brief Compiles only the finite exact-identity staged review REST catalog.
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
	[[nodiscard]] auto compile_review_comment_collection_request(
		const application::review_mutation_target_binding& aTarget,
		application::github_server_contract                aServer)
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

		[[nodiscard]] virtual auto perform_review_mutation(
			const github_mutation_wire_request&  aRequest,
			std::stop_token                      aStop,
			github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion = 0;
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
		[[nodiscard]] auto observe_exact(std::stop_token aStop)
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
