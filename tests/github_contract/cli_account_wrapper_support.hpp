#ifndef BPR_TESTS_GITHUB_CONTRACT_CLI_ACCOUNT_WRAPPER_SUPPORT_HPP
#define BPR_TESTS_GITHUB_CONTRACT_CLI_ACCOUNT_WRAPPER_SUPPORT_HPP

// [[BPR-1572]]. Drives a publication session through the production GitHub
// CLI account wrapper (`make_cli_observed_account_transport_for_testing`):
// the wrapper checks the account with reads of its own before and after
// every exchange, and reads the pull request once more after its pre-write
// check. This inner transport answers those account reads and forwards every
// other exchange to a test's own mutation server.

#include "github_mutation_contract.hpp"
#include "github_provider.hpp"
#include "github_wire_transport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace bpr::tests::github_contract {
	class cli_account_inner final : public adapters::github::detail::github_wire_transport_port,
									public adapters::github::detail::github_mutation_wire_transport,
									public std::enable_shared_from_this<cli_account_inner>
	{
	public:
		cli_account_inner(adapters::github::detail::github_mutation_wire_transport& aServer, const std::uint64_t aAccount, std::string aLogin)
			: Server{ aServer }
			, Account{ aAccount }
			, Login{ std::move(aLogin) }
		{ }

		[[nodiscard]] auto strict_cli() const noexcept -> bool override { return true; }
		[[nodiscard]] auto begin_mutation_operation(std::stop_token) -> adapters::github::detail::github_mutation_operation override
		{
			return std::shared_ptr<adapters::github::detail::github_mutation_wire_transport>{ shared_from_this() };
		}
		[[nodiscard]] auto perform(const adapters::github::detail::github_wire_request& aRequest, std::stop_token) -> adapters::github::detail::github_wire_completion override
		{
			++AccountReads;
			if (OnAccountRead) {
				OnAccountRead(AccountReads);
			}
			return adapters::github::detail::github_wire_success{
				.Correlation = aRequest.Correlation,
				.Response    = {
					.Status       = 200,
					.Headers      = { { .Name = "Content-Type", .Value = "application/json; charset=utf-8" },
									  { .Name = "X-RateLimit-Limit", .Value = "5000" },
									  { .Name = "X-RateLimit-Remaining", .Value = "4999" },
									  { .Name = "X-RateLimit-Reset", .Value = "2000000000" },
									  { .Name = "X-RateLimit-Resource", .Value = "core" } },
					.Body         = R"({"id":)" + std::to_string(Account) + R"(,"login":")" + Login + R"(","name":"Fixture Reviewer","node_id":"U_fixture"})",
					.Completeness = adapters::github::detail::page_completeness::complete,
				},
			};
		}
		[[nodiscard]] auto perform_capability_probe(const adapters::github::detail::github_capability_probe_wire_request& aRequest, std::stop_token)
			-> adapters::github::detail::github_capability_probe_wire_completion override
		{
			return adapters::github::detail::github_capability_probe_wire_error{ .Correlation = aRequest.Correlation,
																				 .Error       = { .Failure = application::github_provider_failure::unavailable } };
		}
		[[nodiscard]] auto perform_review_mutation(const adapters::github::detail::github_mutation_wire_request& aRequest, std::stop_token aStop,
												   adapters::github::detail::github_mutation_transmission_witness aWitness)
			-> adapters::github::detail::github_mutation_wire_completion override
		{
			return Server.perform_review_mutation(aRequest, aStop, std::move(aWitness));
		}

		/// Called with the count of account reads so far, as each is answered.
		std::function<void(std::size_t)> OnAccountRead;
		std::size_t                      AccountReads = 0U;

	private:
		adapters::github::detail::github_mutation_wire_transport& Server;
		std::uint64_t                                             Account = 0U;
		std::string                                               Login;
	};

	/// The production wrapper over a `cli_account_inner` for `aContext`.
	struct cli_account_wrapper
	{
		std::shared_ptr<cli_account_inner>                                        Inner;
		std::shared_ptr<adapters::github::detail::github_mutation_wire_transport> Transport;
	};

	[[nodiscard]] inline auto make_cli_account_wrapper(adapters::github::detail::github_mutation_wire_transport& aServer,
													   const application::github_credential_operation_context& aContext, const std::string& aLogin) -> cli_account_wrapper
	{
		auto inner     = std::make_shared<cli_account_inner>(aServer, aContext.Authority.Account.value(), aLogin);
		auto transport = adapters::github::detail::make_cli_observed_account_transport_for_testing(inner, aContext, aLogin);
		return { .Inner = std::move(inner), .Transport = std::move(transport) };
	}
}

#endif // BPR_TESTS_GITHUB_CONTRACT_CLI_ACCOUNT_WRAPPER_SUPPORT_HPP
