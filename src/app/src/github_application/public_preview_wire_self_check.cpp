#include "bpr/app/github_application.hpp"
#include "github_application/public_preview_wire_transport.hpp"

#include "github_read_contract.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <variant>

// The isolated dry run behind the live public-preview candidate's
// single-attempt claim. Tests-only behavior; a tests-off build answers false.
namespace bpr::app {
	auto public_preview_single_attempt_self_check() noexcept -> bool
	{
#if defined(BPR_BUILD_TESTING)
		// [[BPR-0316]]. Exercise the real wrapper without a network: a typed
		// failure stops retries, and successful pages share one request cap.
		class stub_wire_transport final
			: public adapters::github::detail::github_wire_transport_port
		{
		public:
			explicit stub_wire_transport(const bool aFail) noexcept
				: Fail{ aFail }
			{
			}

			[[nodiscard]] auto perform(
				const adapters::github::detail::github_wire_request& aRequest,
				std::stop_token                                      aStop)
				-> adapters::github::detail::github_wire_completion override
			{
				static_cast<void>(aStop);
				++Forwarded;
				if (!Fail) {
					return adapters::github::detail::github_wire_success{
						.Correlation = aRequest.Correlation,
						.Response    = { .Status = 200 },
					};
				}
				return adapters::github::detail::github_wire_error{
					.Correlation = aRequest.Correlation,
					.Error       = application::github_provider_error{
						.Failure
						= application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			}

			[[nodiscard]] auto perform_capability_probe(
				const adapters::github::detail::
					github_capability_probe_wire_request& aRequest,
				std::stop_token                           aStop)
				-> adapters::github::detail::
					github_capability_probe_wire_completion override
			{
				static_cast<void>(aStop);
				return adapters::github::detail::
					github_capability_probe_wire_cancelled{
						.Correlation = aRequest.Correlation,
					};
			}

			[[nodiscard]] auto forwarded() const noexcept -> std::size_t
			{
				return Forwarded;
			}

		private:
			bool        Fail;
			std::size_t Forwarded = 0U;
		};

		try {
			auto inner    = std::make_shared<stub_wire_transport>(true);
			auto requests = std::make_shared<std::atomic<std::size_t>>(0U);
			auto probes   = std::make_shared<std::atomic<std::size_t>>(0U);
			auto log      = std::make_shared<anonymous_wire_operation_log>();
			auto counting = counting_anonymous_wire_transport{ inner,
															   requests,
															   probes,
															   log };
			auto request  = adapters::github::detail::github_wire_request{ };
			request.Correlation.Operation
				= adapters::github::detail::read_operation::get_pull_request;
			request.Correlation.Page = 1U;

			const auto first = counting.perform(request, std::stop_token{ });
			const auto forwardedFirst
				= std::holds_alternative<
					  adapters::github::detail::github_wire_error>(first)
				&& inner->forwarded() == 1U
				&& requests->load(std::memory_order_relaxed) == 1U
				&& log->records().size() == 1U
				&& log->records().front().Outcome == "error_network";

			const auto second = counting.perform(request, std::stop_token{ });
			const auto refusedSecond
				= std::holds_alternative<
					  adapters::github::detail::github_wire_cancelled>(second)
				&& inner->forwarded() == 1U
				&& requests->load(std::memory_order_relaxed) == 1U
				&& log->records().size() == 1U;

			// An all-successful page stream must stop after fifteen forwards,
			// even if the factory creates another wrapper over the same budget.
			auto successful   = std::make_shared<stub_wire_transport>(false);
			auto pageRequests = std::make_shared<std::atomic<std::size_t>>(0U);
			auto pageLog      = std::make_shared<anonymous_wire_operation_log>();
			auto pages        = counting_anonymous_wire_transport{
				successful, pageRequests, probes, pageLog
			};
			for (auto page = std::uint32_t{ 1 }; page <= 15U; ++page) {
				request.Correlation.Page = page;
				if (!std::holds_alternative<adapters::github::detail::github_wire_success>(
						pages.perform(request, std::stop_token{ }))) {
					return false;
				}
			}
			auto replacement = counting_anonymous_wire_transport{
				successful, pageRequests, probes, pageLog
			};
			request.Correlation.Page = 16U;
			const auto refusedPage   = replacement.perform(request, std::stop_token{ });
			const auto refusedRetry  = pages.perform(request, std::stop_token{ });
			const auto pagesBounded  = forwardedFirst && refusedSecond
				&& std::holds_alternative<adapters::github::detail::github_wire_cancelled>(refusedPage)
				&& std::holds_alternative<adapters::github::detail::github_wire_cancelled>(refusedRetry)
				&& successful->forwarded() == 15U
				&& pageRequests->load(std::memory_order_relaxed) == 15U
				&& pageLog->records().size() == 15U;
			if (!pagesBounded) {
				return false;
			}

			// Current one-page production sequence: ten content/status reads,
			// timeline, the recorded and branch-tip merge bases, the conditional
			// ancestry check, then fresh PR confirmation (BPR-1560/BPR-1572).
			// Keep these expectations separate from reserve_request's ceiling.
			using adapters::github::detail::read_operation;
			constexpr auto sequence = std::array{
				read_operation::get_repository,
				read_operation::get_pull_request,
				read_operation::list_pull_request_files,
				read_operation::list_pull_request_commits,
				read_operation::list_reviews,
				read_operation::list_review_comments,
				read_operation::list_issue_comments,
				read_operation::search_pull_requests,
				read_operation::list_checks,
				read_operation::list_commit_statuses,
				read_operation::list_pull_request_timeline,
				read_operation::get_merge_base,
				read_operation::get_merge_base,
				read_operation::get_merge_base,
				read_operation::get_pull_request,
			};
			auto sequenceRequests    = std::make_shared<std::atomic<std::size_t>>(0U);
			auto sequenceLog         = std::make_shared<anonymous_wire_operation_log>();
			auto sequenceInner       = std::make_shared<stub_wire_transport>(false);
			auto sequenceTransport   = counting_anonymous_wire_transport{ sequenceInner, sequenceRequests, probes, sequenceLog };
			request.Correlation.Page = 1U;
			for (const auto& operation : sequence) {
				request.Correlation.Operation = operation;
				if (!std::holds_alternative<adapters::github::detail::github_wire_success>(sequenceTransport.perform(request, std::stop_token{ }))) {
					return false;
				}
			}
			auto sequenceReplacement = counting_anonymous_wire_transport{ sequenceInner, sequenceRequests, probes, sequenceLog };
			return sequenceInner->forwarded() == 15U && sequenceRequests->load(std::memory_order_relaxed) == 15U
				&& sequenceLog->records().size() == 15U
				&& std::holds_alternative<adapters::github::detail::github_wire_cancelled>(sequenceReplacement.perform(request, std::stop_token{ }));
		} catch (...) {
			return false;
		}
#else
		return false;
#endif
	}
}
