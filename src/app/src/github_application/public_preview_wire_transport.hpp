#ifndef BPR_APP_GITHUB_APPLICATION_PUBLIC_PREVIEW_WIRE_TRANSPORT_HPP
#define BPR_APP_GITHUB_APPLICATION_PUBLIC_PREVIEW_WIRE_TRANSPORT_HPP
#include "bpr/app/github_application.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "github_host_profile.hpp"
#include "github_provider.hpp"
#include "github_wire_transport.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

// The signed-out public-preview authority's wire: what the composition root
// counts and bounds about credential-free reads (BPR-0305, BPR-0316) and the
// credential-free Git smart lease. Private to the composition library.
namespace bpr::app {
	// Counts every credential-free pull-request read that reached the
	// anonymous authority. Composed whether or not the selection hands
	// that authority to the host, so an unavailable build reports a number
	// rather than an absence: zero is the claim that the mode's gate
	// refused before a request was built, and any other value is the gate
	// being bypassed.
	class counting_anonymous_pull_requests final
		: public application::github_pull_request_port
	{
	public:
		explicit counting_anonymous_pull_requests(
			application::github_pull_request_port& aInner) noexcept;

		void fetch_pull_request(
			application::github_pull_request_provider_request aRequest,
			application::github_pull_request_completion_handler
				aCompletion) override;

		void cancel_pull_request(
			application::github_pull_request_provider_request aRequest)
			override;

		void fetch_pull_request_with_progress(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion,
			application::github_pull_request_progress_handler   aProgress) override;

		[[nodiscard]] auto reads() const noexcept -> std::size_t;

		void fetch_pull_request_with_observations(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion,
			application::github_pull_request_progress_handler   aProgress,
			application::github_pull_request_overview_handler   aOverview) override;

	private:
		application::github_pull_request_port* Inner = nullptr;
		std::size_t                            Reads = 0U;
	};

	// [[BPR-0316]]. What the signed-out transport forwarded, in order,
	// and what came back for each request. The count below answers "how
	// much of the shared hourly budget did this read cost"; only this
	// answers "which operation failed, how, and how long it waited",
	// which is the question the first authorized read could not be asked.
	// Shared and lock-guarded rather than atomic, because the transport is
	// driven from a worker while the reporting thread copies the whole
	// record.
	class anonymous_wire_operation_log final
	{
	public:
		void record(
			std::string         aOperation,
			const std::uint32_t aPage,
			std::string         aOutcome,
			const std::uint64_t aElapsedMilliseconds);

		// Latched by the first request that came back a failure, and never
		// cleared. The host retries a `network` or `timeout` provider
		// failure up to `maximum_github_provider_attempts`, so a candidate
		// read allowed to fail costs three times what it was authorized
		// to; the transport answering the retry itself, without
		// forwarding it, is the whole of the single-attempt mechanism.
		void latch_failure();

		[[nodiscard]] auto failed() const -> bool;

		[[nodiscard]] auto records() const
			-> std::vector<public_preview_wire_operation_record>;

	private:
		mutable std::mutex                                Mutex;
		std::vector<public_preview_wire_operation_record> Records;
		bool                                              Failed = false;
	};

	// [[BPR-0316]]. Counts what the anonymous transport itself put on the
	// wire. `counting_anonymous_pull_requests` counts pull-request reads
	// offered to the authority; one such read is several REST operations,
	// and the number a live candidate has to report is the second one. The
	// counters are shared and atomic because the transport is driven from a
	// worker and read from the reporting thread.
	class counting_anonymous_wire_transport final
		: public adapters::github::detail::github_wire_transport_port
	{
	public:
		counting_anonymous_wire_transport(
			std::shared_ptr<
				adapters::github::detail::github_wire_transport_port>
														  aInner,
			std::shared_ptr<std::atomic<std::size_t>>     aRequests,
			std::shared_ptr<std::atomic<std::size_t>>     aCapabilityProbes,
			std::shared_ptr<anonymous_wire_operation_log> aLog) noexcept;

		void set_insights_channel(std::shared_ptr<adapters::github::detail::github_insights_channel> aChannel) override;

		[[nodiscard]] auto perform(
			const adapters::github::detail::github_wire_request& aRequest,
			std::stop_token                                      aStop)
			-> adapters::github::detail::github_wire_completion override;

		[[nodiscard]] auto perform_capability_probe(
			const adapters::github::detail::
				github_capability_probe_wire_request& aRequest,
			std::stop_token                           aStop)
			-> adapters::github::detail::
				github_capability_probe_wire_completion override;

	private:
		[[nodiscard]] auto reserve_request() noexcept -> bool;

		std::shared_ptr<
			adapters::github::detail::github_wire_transport_port>
													  Inner;
		std::shared_ptr<std::atomic<std::size_t>>     Requests;
		std::shared_ptr<std::atomic<std::size_t>>     CapabilityProbes;
		std::shared_ptr<anonymous_wire_operation_log> Log;
	};

	// [[BPR-0316]]'s live-candidate REST authority. Every transport it
	// issues comes from `make_native_https_anonymous_read_transport` --
	// the shipped entry, with system proxy discovery and system trust --
	// and it has no credentialed form at all: both credentialed entries
	// refuse and count the attempt, so a composition that reached for an
	// authorization header shows up as a number rather than as a header
	// nobody looked for. The GHES refusal is the shipped factory's, made
	// before a transport exists.
	class shipped_anonymous_wire_transport_factory final
		: public adapters::github::detail::github_wire_transport_factory_port
	{
	public:
		[[nodiscard]] auto make_bootstrap(
			const adapters::github::validated_github_host_profile& aProfile,
			core::github_session_request_context                   aContext,
			application::github_credential_material                aCredential) noexcept
			-> adapters::github::detail::
				github_wire_transport_factory_result override;

		[[nodiscard]] auto make_account(
			const adapters::github::validated_github_host_profile& aProfile,
			application::github_credential_retrieve_success        aCredential) noexcept
			-> adapters::github::detail::
				github_wire_transport_factory_result override;

		[[nodiscard]] auto make_anonymous(
			const adapters::github::validated_github_host_profile& aProfile,
			core::github_session_request_context                   aContext) noexcept
			-> adapters::github::detail::
				github_wire_transport_factory_result override;

		[[nodiscard]] auto requests() const noexcept -> std::size_t;

		[[nodiscard]] auto capability_probes() const noexcept -> std::size_t;

		[[nodiscard]] auto credentialed_refusals() const noexcept
			-> std::size_t;

		// What each forwarded request was for and how it ended, in the
		// order it was forwarded.
		[[nodiscard]] auto operations() const
			-> std::vector<public_preview_wire_operation_record>;

	private:
		std::shared_ptr<std::atomic<std::size_t>> Requests
			= std::make_shared<std::atomic<std::size_t>>(0U);
		std::shared_ptr<std::atomic<std::size_t>> CapabilityProbes
			= std::make_shared<std::atomic<std::size_t>>(0U);
		std::shared_ptr<std::atomic<std::size_t>> CredentialedRefusals
			= std::make_shared<std::atomic<std::size_t>>(0U);
		std::shared_ptr<anonymous_wire_operation_log> Log
			= std::make_shared<anonymous_wire_operation_log>();
	};

	// The credential-free Git smart authority. It takes no credential and
	// has no entry that could accept one: every lease it issues comes from
	// the shipped anonymous factory, which refuses an authenticated
	// authority and refuses an enterprise profile before a request exists.
	// Constructed in both profiles, so a shipped graph reaches the
	// production factory by construction rather than through a fixture.
	class anonymous_git_smart_transport_factory final
		: public application::git_smart_transport_factory_port
	{
	public:
		explicit anonymous_git_smart_transport_factory(
			const adapters::github::validated_github_host_profile&         aProfile,
			std::shared_ptr<application::git_smart_transport_factory_port> aRecorded = nullptr) noexcept;

		[[nodiscard]] auto acquire(
			const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<
				application::git_smart_transport> override;

		// [[BPR-0838]]. Epoch work, historical retention imports among it,
		// acquires on worker threads, and `status()` reads on the Qt thread.
		// The count is one independent value that shares no invariant with
		// another field, so relaxed ordering is enough, as for the counters
		// above. A reader that needs every import counted waits for the
		// imports to finish.
		[[nodiscard]] auto acquisitions() const noexcept -> std::size_t;

	private:
		const adapters::github::validated_github_host_profile*         Profile = nullptr;
		std::shared_ptr<application::git_smart_transport_factory_port> Recorded;
		std::atomic_size_t                                             Acquisitions = 0U;
	};
}
#endif
