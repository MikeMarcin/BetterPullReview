#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_HPP

#include "github_read_contract.hpp"

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/review_mutation_recovery_port.hpp"
#include "bpr/application/review_submission_contract.hpp"

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
	};

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
	/// \brief [[BPR-0657]]. Accepts only the exact thread in the state asked for.
	[[nodiscard]] auto parse_thread_resolution_response(
		const review_mutation_wire_response&                 aResponse,
		const application::review_thread_resolution_request& aExpected)
		-> review_mutation_parse_result<review_thread_resolution_observation>;
	[[nodiscard]] auto compile_review_comment_observation_request(
		const application::review_mutation_request& aRequest,
		application::github_server_contract         aServer = application::github_server_contract::github_com_2022_11_28)
		-> review_mutation_request_result;
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
				aRecovery) noexcept;

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
		/// \brief [[BPR-0657]]. Resolves or unresolves one thread through the
		/// leased transport; idempotent on the server, so no recovery record.
		[[nodiscard]] auto resolve_thread(
			const application::review_thread_resolution_request& aRequest,
			std::stop_token                                      aStop = { })
			-> std::expected<application::review_thread_resolution_receipt, application::review_mutation_failure>;

	private:
		[[nodiscard]] auto observe_exact(std::stop_token aStop)
			-> std::expected<void, application::review_mutation_failure>;
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
	};
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_MUTATION_CONTRACT_HPP
