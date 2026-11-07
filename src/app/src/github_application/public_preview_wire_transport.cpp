#include "github_application/public_preview_wire_transport.hpp"

#include "github_read_contract.hpp"
#include "native_https_transport.hpp"

#include <chrono>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace bpr::app {
	namespace {
		// The enumerator's own name, so a failure is reported as the kind the
		// provider contract calls it rather than as a number a reader has to
		// look up.
		[[nodiscard]] auto anonymous_wire_failure_name(
			const application::github_provider_failure aFailure) noexcept
			-> std::string_view
		{
			using failure = application::github_provider_failure;
			switch (aFailure) {
			case failure::authentication:
				return "authentication";
			case failure::permission:
				return "permission";
			case failure::network:
				return "network";
			case failure::api_version:
				return "api_version";
			case failure::unsupported_capability:
				return "unsupported_capability";
			case failure::rate_limit:
				return "rate_limit";
			case failure::certificate:
				return "certificate";
			case failure::redirect:
				return "redirect";
			case failure::timeout:
				return "timeout";
			case failure::process_startup:
				return "process_startup";
			case failure::cli_unavailable:
				return "cli_unavailable";
			case failure::vault:
				return "vault";
			case failure::malformed_response:
				return "malformed_response";
			case failure::response_limit:
				return "response_limit";
			case failure::unavailable:
				return "unavailable";
			}
			return "unknown";
		}

		// The whole of what a completion is allowed to say on a card: a status
		// line, or a failure kind with the two numbers a failed read is
		// diagnosed from. No body, no URL, no header other than the
		// rate-limit triple the failure itself named.
		[[nodiscard]] auto anonymous_wire_outcome_text(
			const adapters::github::detail::github_wire_completion& aCompletion)
			-> std::string
		{
			if (const auto* const success = std::get_if<
					adapters::github::detail::github_wire_success>(&aCompletion);
				success != nullptr) {
				return "http_" + std::to_string(success->Response.Status);
			}
			const auto* const failure
				= std::get_if<adapters::github::detail::github_wire_error>(
					&aCompletion);
			if (failure == nullptr) {
				return "cancelled";
			}
			auto text = std::string{ "error_" };
			text += anonymous_wire_failure_name(failure->Error.Failure);
			if (failure->Error.RetryAfterSeconds.has_value()) {
				text += ".retry_after=";
				text += std::to_string(*failure->Error.RetryAfterSeconds);
			}
			if (failure->Error.RateLimitCause.has_value()) {
				text += ".rate=";
				text += std::to_string(failure->Error.RateLimitCause->Limit);
				text += '/';
				text += std::to_string(failure->Error.RateLimitCause->Remaining);
				text += '/';
				text += std::to_string(
					failure->Error.RateLimitCause->ResetUnixSeconds);
			}
			return text;
		}

		[[nodiscard]] auto shipped_anonymous_refusal() noexcept
			-> application::github_provider_error
		{
			return application::github_provider_error{
				.Failure = application::github_provider_failure::
					unsupported_capability,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			};
		}
	}

	counting_anonymous_pull_requests::counting_anonymous_pull_requests(
		application::github_pull_request_port& aInner) noexcept
		: Inner{ &aInner }
	{
	}

	void counting_anonymous_pull_requests::fetch_pull_request(
		application::github_pull_request_provider_request aRequest,
		application::github_pull_request_completion_handler
			aCompletion)
	{
		++Reads;
		Inner->fetch_pull_request(
			std::move(aRequest), std::move(aCompletion));
	}

	void counting_anonymous_pull_requests::cancel_pull_request(
		application::github_pull_request_provider_request aRequest)
	{
		Inner->cancel_pull_request(std::move(aRequest));
	}

	void counting_anonymous_pull_requests::fetch_pull_request_with_progress(
		application::github_pull_request_provider_request   aRequest,
		application::github_pull_request_completion_handler aCompletion,
		application::github_pull_request_progress_handler   aProgress)
	{
		++Reads;
		Inner->fetch_pull_request_with_progress(
			std::move(aRequest), std::move(aCompletion), std::move(aProgress));
	}

	auto counting_anonymous_pull_requests::reads() const noexcept -> std::size_t
	{
		return Reads;
	}

	void counting_anonymous_pull_requests::fetch_pull_request_with_observations(
		application::github_pull_request_provider_request   aRequest,
		application::github_pull_request_completion_handler aCompletion,
		application::github_pull_request_progress_handler   aProgress,
		application::github_pull_request_overview_handler   aOverview)
	{
		++Reads;
		Inner->fetch_pull_request_with_observations(std::move(aRequest), std::move(aCompletion), std::move(aProgress), std::move(aOverview));
	}

	void anonymous_wire_operation_log::record(
		std::string         aOperation,
		const std::uint32_t aPage,
		std::string         aOutcome,
		const std::uint64_t aElapsedMilliseconds)
	{
		const std::scoped_lock lock{ Mutex };
		Records.push_back(public_preview_wire_operation_record{
			.Operation           = std::move(aOperation),
			.Page                = aPage,
			.Outcome             = std::move(aOutcome),
			.ElapsedMilliseconds = aElapsedMilliseconds,
		});
	}

	void anonymous_wire_operation_log::latch_failure()
	{
		const std::scoped_lock lock{ Mutex };
		Failed = true;
	}

	auto anonymous_wire_operation_log::failed() const -> bool
	{
		const std::scoped_lock lock{ Mutex };
		return Failed;
	}

	auto anonymous_wire_operation_log::records() const
		-> std::vector<public_preview_wire_operation_record>
	{
		const std::scoped_lock lock{ Mutex };
		return Records;
	}

	counting_anonymous_wire_transport::counting_anonymous_wire_transport(
		std::shared_ptr<
			adapters::github::detail::github_wire_transport_port>
													  aInner,
		std::shared_ptr<std::atomic<std::size_t>>     aRequests,
		std::shared_ptr<std::atomic<std::size_t>>     aCapabilityProbes,
		std::shared_ptr<anonymous_wire_operation_log> aLog) noexcept
		: Inner{ std::move(aInner) }
		, Requests{ std::move(aRequests) }
		, CapabilityProbes{ std::move(aCapabilityProbes) }
		, Log{ std::move(aLog) }
	{
	}

	void counting_anonymous_wire_transport::set_insights_channel(std::shared_ptr<adapters::github::detail::github_insights_channel> aChannel)
	{
		github_wire_transport_port::set_insights_channel(aChannel);
		Inner->set_insights_channel(std::move(aChannel));
	}

	auto counting_anonymous_wire_transport::perform(
		const adapters::github::detail::github_wire_request& aRequest,
		std::stop_token                                      aStop)
		-> adapters::github::detail::github_wire_completion
	{
		// One attempt. Once a request for this read has come back a
		// failure, every later one is answered here -- as the
		// cancellation the host already knows how to end a read on --
		// rather than forwarded, so the retry costs nothing outside
		// the process. A refused request is neither counted nor
		// recorded, because the count is what reached the wire.
		// Pagination shares this fixed diagnostic budget. Fifteen slots
		// cover the single-page read, timeline and worst-case merge-base
		// audit/confirmation, without allowing an unbounded page stream.
		if (Log->failed() || !reserve_request()) {
			return adapters::github::detail::github_wire_cancelled{
				.Correlation = aRequest.Correlation,
			};
		}
		const auto started    = std::chrono::steady_clock::now();
		auto       completion = Inner->perform(aRequest, aStop);
		const auto elapsed    = std::chrono::duration_cast<
			std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - started);
		Log->record(
			std::string{ adapters::github::detail::operation_name(
				aRequest.Correlation.Operation) },
			aRequest.Correlation.Page,
			anonymous_wire_outcome_text(completion),
			static_cast<std::uint64_t>(
				elapsed.count() < 0 ? 0 : elapsed.count()));
		if (std::holds_alternative<
				adapters::github::detail::github_wire_error>(
				completion)) {
			Log->latch_failure();
		}
		return completion;
	}

	auto counting_anonymous_wire_transport::perform_capability_probe(
		const adapters::github::detail::
			github_capability_probe_wire_request& aRequest,
		std::stop_token                           aStop)
		-> adapters::github::detail::
			github_capability_probe_wire_completion
	{
		CapabilityProbes->fetch_add(1U, std::memory_order_relaxed);
		return Inner->perform_capability_probe(
			aRequest, std::move(aStop));
	}

	auto counting_anonymous_wire_transport::reserve_request() noexcept -> bool
	{
		auto count = Requests->load(std::memory_order_relaxed);
		while (count < 15U) {
			if (Requests->compare_exchange_weak(
					count, count + 1U, std::memory_order_relaxed)) {
				return true;
			}
		}
		return false;
	}

	auto shipped_anonymous_wire_transport_factory::make_bootstrap(
		const adapters::github::validated_github_host_profile& aProfile,
		core::github_session_request_context                   aContext,
		application::github_credential_material                aCredential) noexcept
		-> adapters::github::detail::
			github_wire_transport_factory_result
	{
		static_cast<void>(aProfile);
		static_cast<void>(aContext);
		static_cast<void>(aCredential);
		CredentialedRefusals->fetch_add(1U, std::memory_order_relaxed);
		return std::unexpected{ shipped_anonymous_refusal() };
	}

	auto shipped_anonymous_wire_transport_factory::make_account(
		const adapters::github::validated_github_host_profile& aProfile,
		application::github_credential_retrieve_success        aCredential) noexcept
		-> adapters::github::detail::
			github_wire_transport_factory_result
	{
		static_cast<void>(aProfile);
		static_cast<void>(aCredential);
		CredentialedRefusals->fetch_add(1U, std::memory_order_relaxed);
		return std::unexpected{ shipped_anonymous_refusal() };
	}

	auto shipped_anonymous_wire_transport_factory::make_anonymous(
		const adapters::github::validated_github_host_profile& aProfile,
		core::github_session_request_context                   aContext) noexcept
		-> adapters::github::detail::
			github_wire_transport_factory_result
	{
		auto transport = adapters::github::detail::
			make_native_https_anonymous_read_transport(
				aProfile, std::move(aContext));
		if (!transport.has_value()) {
			return std::unexpected{ shipped_anonymous_refusal() };
		}
		try {
			return std::shared_ptr<adapters::github::detail::
									   github_wire_transport_port>{
				std::make_shared<counting_anonymous_wire_transport>(
					std::shared_ptr<adapters::github::detail::
										github_wire_transport_port>{
						std::move(*transport) },
					Requests,
					CapabilityProbes,
					Log)
			};
		} catch (...) {
			return std::unexpected{ shipped_anonymous_refusal() };
		}
	}

	auto shipped_anonymous_wire_transport_factory::requests() const noexcept -> std::size_t
	{
		return Requests->load(std::memory_order_relaxed);
	}

	auto shipped_anonymous_wire_transport_factory::capability_probes() const noexcept -> std::size_t
	{
		return CapabilityProbes->load(std::memory_order_relaxed);
	}

	auto shipped_anonymous_wire_transport_factory::credentialed_refusals() const noexcept
		-> std::size_t
	{
		return CredentialedRefusals->load(std::memory_order_relaxed);
	}

	auto shipped_anonymous_wire_transport_factory::operations() const
		-> std::vector<public_preview_wire_operation_record>
	{
		return Log->records();
	}

	anonymous_git_smart_transport_factory::anonymous_git_smart_transport_factory(
		const adapters::github::validated_github_host_profile&         aProfile,
		std::shared_ptr<application::git_smart_transport_factory_port> aRecorded) noexcept
		: Profile{ &aProfile }
		, Recorded{ std::move(aRecorded) }
	{
	}

	auto anonymous_git_smart_transport_factory::acquire(
		const application::git_smart_request_context& aContext)
		-> application::git_smart_transport_result<
			application::git_smart_transport>
	{
		Acquisitions.fetch_add(1U, std::memory_order_relaxed);
		// [[BPR-0715]]. Under a recorded fixture the signed-out lease
		// is the fixture's own credential-free transport, so a course
		// reaches an exact comparison over the real epoch path instead
		// of dialling out of the network-isolated run.
		if (Recorded != nullptr) {
			return Recorded->acquire(aContext);
		}
		// [[BPR-0663]]. The binding admits a lease only for the
		// identity generation the accepted revision was read in, so
		// the wire carries that generation rather than a constant of
		// its own; the request id is this factory's correlation and
		// nothing judges it.
		auto wire = adapters::github::detail::
			make_native_https_anonymous_smart_transport(
				*Profile,
				core::github_session_request_context{
					.Request            = core::request_id{ 917U },
					.ApplicationSession = aContext.ApplicationSession,
					.Profile            = Profile->profile(),
					.IdentityGeneration
					= aContext.Revision.Source.IdentityGeneration,
				});
		if (!wire.has_value()) {
			return std::unexpected{
				application::git_smart_transport_error{
					.Failure = application::
						git_smart_transport_failure::unavailable,
				}
			};
		}
		return adapters::github::detail::make_native_git_smart_transport(
			std::shared_ptr<
				adapters::github::detail::native_https_transport>{
				std::move(*wire) },
			aContext);
	}

	auto anonymous_git_smart_transport_factory::acquisitions() const noexcept -> std::size_t
	{
		return Acquisitions.load(std::memory_order_relaxed);
	}
}
