#include "github_publication_contract.hpp"

#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		using application::github_publication_error;
		using application::github_publication_failure;
		using application::github_publication_transmission;

		[[nodiscard]] auto error(const github_publication_failure aFailure, const github_publication_transmission aTransmission = github_publication_transmission::definitely_not_sent,
								 const std::uint16_t aStatus = 0U) -> std::unexpected<github_publication_error>
		{
			return std::unexpected{ github_publication_error{ .Failure = aFailure, .Transmission = aTransmission, .HttpStatus = aStatus } };
		}

		[[nodiscard]] auto transmission(const github_mutation_transmission aTransmission) noexcept -> github_publication_transmission
		{
			return aTransmission == github_mutation_transmission::definitely_not_sent ? github_publication_transmission::definitely_not_sent : github_publication_transmission::indeterminate;
		}

		/// A provider failure as the catalog reports it: a server's
		/// definitive refusal stays a refusal, anything that could still have
		/// taken effect stays unavailable.
		[[nodiscard]] auto map_failure(const github_publication_wire_error& aError) -> github_publication_error
		{
			const auto status = aError.Error.HttpStatus;
			auto       mapped = github_publication_error{ .Transmission = transmission(aError.Transmission), .HttpStatus = status };
			switch (aError.Error.Failure) {
			case application::github_provider_failure::rate_limit:
				mapped.Failure           = github_publication_failure::rate_limited;
				mapped.RetryAfterSeconds = aError.Error.RetryAfterSeconds;
				return mapped;
			case application::github_provider_failure::authentication:
			case application::github_provider_failure::permission:
			case application::github_provider_failure::unsupported_capability:
			case application::github_provider_failure::api_version:
				mapped.Failure = github_publication_failure::refused;
				return mapped;
			default:
				break;
			}
			// Other 4xx answers are the server's definitive refusal; 5xx and
			// transport failures may have taken effect.
			mapped.Failure = status >= 400U && status < 500U ? github_publication_failure::refused : github_publication_failure::unavailable;
			return mapped;
		}

		[[nodiscard]] auto contract_error(const publication_contract_failure aFailure, const bool aWrite) -> std::unexpected<github_publication_error>
		{
			// A malformed or mismatched answer to a write arrived after the
			// write reached the server.
			return error(aFailure == publication_contract_failure::invalid_request ? github_publication_failure::refused : github_publication_failure::mismatch,
						 aWrite && aFailure != publication_contract_failure::invalid_request ? github_publication_transmission::indeterminate : github_publication_transmission::definitely_not_sent);
		}
	}

	github_publication_session::github_publication_session(github_publication_wire_transport& aTransport, application::github_credential_operation_context aContext) noexcept
		: Transport{ &aTransport }
		, Context{ std::move(aContext) }
	{
	}

	auto github_publication_session::admits(const application::github_publication_target& aTarget) const noexcept -> bool
	{
		const auto& authority = Context.Authority;
		return application::valid(Context) && admitted_publication_server(aTarget.Server) && aTarget.Server == authority.ServerContract && aTarget.Account == authority.Account
			&& aTarget.Host == authority.Hostname.value();
	}

	auto github_publication_session::exchange(const application::github_publication_target& aTarget, publication_contract_result<publication_wire_request> aRequest, std::stop_token aStop)
		-> application::github_publication_result<publication_wire_response>
	{
		if (!admits(aTarget)) {
			return error(github_publication_failure::unsupported_host);
		}
		if (!aRequest) {
			return error(github_publication_failure::refused);
		}
		if (aStop.stop_requested()) {
			return error(github_publication_failure::cancelled);
		}
		const auto exchange   = publication_exchange_for(aRequest->Operation);
		auto       completion = Transport->perform_publication({ .Context = Context, .Wire = std::move(*aRequest) }, std::move(aStop));
		if (auto* success = std::get_if<github_publication_wire_success>(&completion)) {
			if (!exchange || (success->Response.Status != exchange->SuccessfulStatus && success->Response.Status != exchange->AdmittedStatus)) {
				return error(github_publication_failure::mismatch, exchange && exchange->Write ? github_publication_transmission::indeterminate : github_publication_transmission::definitely_not_sent,
							 static_cast<std::uint16_t>(success->Response.Status));
			}
			return std::move(success->Response);
		}
		if (const auto* failure = std::get_if<github_publication_wire_error>(&completion)) {
			return std::unexpected{ map_failure(*failure) };
		}
		return error(github_publication_failure::cancelled, transmission(std::get<github_publication_wire_cancelled>(completion).Transmission));
	}

	auto github_publication_session::observe_repository(const application::github_publication_target& aTarget, std::stop_token aStop)
		-> application::github_publication_result<application::github_publication_capability>
	{
		const auto response = exchange(aTarget, compile_observe_repository(aTarget), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		auto parsed = parse_repository(*response, aTarget);
		if (!parsed) {
			return contract_error(parsed.error(), false);
		}
		return std::move(*parsed);
	}

	auto github_publication_session::observe_branch(const application::github_publication_target& aTarget, const std::string& aBranch, std::stop_token aStop)
		-> application::github_publication_result<std::optional<core::git_commit_oid>>
	{
		const auto response = exchange(aTarget, compile_observe_branch(aTarget, aBranch), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		const auto parsed = parse_branch(*response, aBranch, core::git_hash_algorithm::sha1);
		if (!parsed) {
			return contract_error(parsed.error(), false);
		}
		return *parsed;
	}

	auto github_publication_session::observe_commit(const application::github_publication_target& aTarget, const core::git_commit_oid& aCommit, std::stop_token aStop)
		-> application::github_publication_result<std::optional<application::github_publication_commit>>
	{
		const auto response = exchange(aTarget, compile_observe_commit(aTarget, aCommit), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		auto parsed = parse_commit(*response, aCommit);
		if (!parsed) {
			return contract_error(parsed.error(), false);
		}
		return std::move(*parsed);
	}

	auto github_publication_session::observe_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aTree, std::stop_token aStop)
		-> application::github_publication_result<bool>
	{
		const auto response = exchange(aTarget, compile_observe_tree(aTarget, aTree), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		const auto parsed = parse_tree_presence(*response, aTree);
		if (!parsed) {
			return contract_error(parsed.error(), false);
		}
		return *parsed;
	}

	auto github_publication_session::observe_blob(const application::github_publication_target& aTarget, const core::git_entry_oid& aBlob, std::stop_token aStop)
		-> application::github_publication_result<bool>
	{
		const auto response = exchange(aTarget, compile_observe_blob(aTarget, aBlob), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		const auto parsed = parse_blob_presence(*response, aBlob);
		if (!parsed) {
			return contract_error(parsed.error(), false);
		}
		return *parsed;
	}

	auto github_publication_session::create_blob(const application::github_publication_target& aTarget, const std::span<const std::uint8_t> aBytes, const core::git_entry_oid& aExpected,
												 std::stop_token aStop) -> application::github_publication_result<void>
	{
		if (aBytes.size() > application::maximum_github_publication_blob_bytes) {
			return error(github_publication_failure::too_large);
		}
		// The bytes must be the named object before anything is sent. A refused
		// identity, collision detection included, is refused here too; the
		// publisher has already named a detection before calling ([[BPR-1450]]).
		const auto identity = core::git_blob_identity(aBytes, aExpected.algorithm());
		if (!identity || core::retag<core::git_entry_oid_kind>(*identity) != aExpected) {
			return error(github_publication_failure::refused);
		}
		const auto response = exchange(aTarget, compile_create_blob(aTarget, aBytes), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		const auto created = parse_created_sha(*response, aExpected.algorithm());
		if (!created) {
			return contract_error(created.error(), true);
		}
		if (core::git_entry_oid{ aExpected.algorithm(), *created } != aExpected) {
			return error(github_publication_failure::mismatch, github_publication_transmission::indeterminate, 201U);
		}
		return { };
	}

	auto github_publication_session::create_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges,
												 const std::vector<std::string>& aDeletions, const core::git_tree_oid& aExpected, std::stop_token aStop) -> application::github_publication_result<void>
	{
		const auto response = exchange(aTarget, compile_create_tree(aTarget, aBase, aChanges, aDeletions), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		const auto created = parse_created_sha(*response, aExpected.algorithm());
		if (!created) {
			return contract_error(created.error(), true);
		}
		if (core::git_tree_oid{ aExpected.algorithm(), *created } != aExpected) {
			return error(github_publication_failure::mismatch, github_publication_transmission::indeterminate, 201U);
		}
		return { };
	}

	auto github_publication_session::create_commit(const application::github_publication_target& aTarget, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected,
												   std::stop_token aStop) -> application::github_publication_result<void>
	{
		if (core::git_commit_identity(aCommit) != aExpected) {
			return error(github_publication_failure::refused);
		}
		const auto response = exchange(aTarget, compile_create_commit(aTarget, aCommit), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		if (const auto parsed = parse_created_commit(*response, aCommit, aExpected); !parsed) {
			return contract_error(parsed.error(), true);
		}
		return { };
	}

	auto github_publication_session::create_branch(const application::github_publication_target& aTarget, const std::string& aBranch, const core::git_commit_oid& aCommit, std::stop_token aStop)
		-> application::github_publication_result<application::github_branch_creation>
	{
		const auto response = exchange(aTarget, compile_create_branch(aTarget, aBranch, aCommit), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		const auto parsed = parse_created_branch(*response, aBranch, aCommit);
		if (!parsed) {
			// An unexplained 422 is the server's refusal, not an existing branch.
			if (parsed.error() == publication_contract_failure::unexpected_status) {
				return error(github_publication_failure::refused, github_publication_transmission::indeterminate, static_cast<std::uint16_t>(response->Status));
			}
			return contract_error(parsed.error(), true);
		}
		return *parsed;
	}

	auto github_publication_session::create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token aStop)
		-> application::github_publication_result<application::github_pull_request_observation>
	{
		const auto response = exchange(aTarget, compile_create_pull_request(aTarget, aDraft), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		if (response->Status == 422) {
			// The server refused, possibly because a pull request for this
			// head already exists; the caller reads before deciding.
			return error(github_publication_failure::refused, github_publication_transmission::indeterminate, 422U);
		}
		auto parsed = parse_pull_request(*response, core::git_hash_algorithm::sha1);
		if (!parsed || !*parsed) {
			return contract_error(parsed ? publication_contract_failure::malformed_response : parsed.error(), true);
		}
		return std::move(**parsed);
	}

	auto github_publication_session::find_pull_requests(const application::github_publication_target& aTarget, const std::string& aHead, const std::string& aBase, std::stop_token aStop)
		-> application::github_publication_result<std::vector<application::github_pull_request_observation>>
	{
		const auto response = exchange(aTarget, compile_find_pull_requests(aTarget, aHead, aBase), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		auto parsed = parse_pull_requests(*response, core::git_hash_algorithm::sha1);
		if (!parsed) {
			return contract_error(parsed.error(), false);
		}
		return std::move(*parsed);
	}

	auto github_publication_session::observe_pull_request(const application::github_publication_target& aTarget, const std::uint64_t aNumber, std::stop_token aStop)
		-> application::github_publication_result<std::optional<application::github_pull_request_observation>>
	{
		const auto response = exchange(aTarget, compile_observe_pull_request(aTarget, aNumber), std::move(aStop));
		if (!response) {
			return std::unexpected{ response.error() };
		}
		auto parsed = parse_pull_request(*response, core::git_hash_algorithm::sha1);
		if (!parsed) {
			return contract_error(parsed.error(), false);
		}
		return std::move(*parsed);
	}
}
