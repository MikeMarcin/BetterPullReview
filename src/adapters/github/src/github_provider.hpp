#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PROVIDER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PROVIDER_HPP

#include "github_anonymous_read_admission.hpp"
#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"
#include "native_https_transport.hpp"

#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/markdown_image_ports.hpp"
#include "bpr/application/personal_discovery_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_mutation_port.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace bpr::adapters::github::detail {
	class github_mutation_wire_transport;

	/// \brief Adapter-private view of an opaque review-mutation lease.
	class github_review_mutation_transport_lease
		: public application::review_mutation_transport_lease
	{
	public:
		~github_review_mutation_transport_lease() override = default;

		[[nodiscard]] virtual auto profile() const noexcept
			-> const application::github_profile_authority& = 0;
		[[nodiscard]] virtual auto credential_context() const noexcept
			-> const application::github_credential_operation_context& = 0;
		[[nodiscard]] virtual auto mutation_transport() noexcept
			-> github_mutation_wire_transport* = 0;
		/// \brief The retained wire implements fresh exact-account credential acquisition.
		[[nodiscard]] virtual auto strict_cli() const noexcept -> bool { return false; }
	};

	using github_wire_transport_factory_result = std::expected<std::shared_ptr<github_wire_transport_port>,
															   application::github_provider_error>;

	class github_wire_transport_factory_port
	{
	public:
		github_wire_transport_factory_port() = default;
		std::shared_ptr<application::insights_observer> Insights;
		virtual ~github_wire_transport_factory_port() = default;

		github_wire_transport_factory_port(
			const github_wire_transport_factory_port&) = delete;
		auto operator=(const github_wire_transport_factory_port&)
			-> github_wire_transport_factory_port&                               = delete;
		github_wire_transport_factory_port(github_wire_transport_factory_port&&) = delete;
		auto operator=(github_wire_transport_factory_port&&)
			-> github_wire_transport_factory_port& = delete;

		[[nodiscard]] virtual auto
		make_bootstrap(const validated_github_host_profile&    aProfile,
					   core::github_session_request_context    aContext,
					   application::github_credential_material aCredential) noexcept
			-> github_wire_transport_factory_result = 0;

		[[nodiscard]] virtual auto make_account(
			const validated_github_host_profile&            aProfile,
			application::github_credential_retrieve_success aCredential) noexcept
			-> github_wire_transport_factory_result = 0;

		/// Configured CLI supplies its own credential. The default remains unavailable.
		[[nodiscard]] virtual auto make_cli(
			const validated_github_host_profile& aProfile,
			core::github_session_request_context aContext) noexcept
			-> github_wire_transport_factory_result
		{
			static_cast<void>(aProfile);
			static_cast<void>(aContext);
			return std::unexpected{ application::github_provider_error{
				.Failure           = application::github_provider_failure::cli_unavailable,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			} };
		}

		[[nodiscard]] virtual auto supports_cli() const noexcept -> bool
		{
			return false;
		}

		/// \brief Builds a transport bound to an anonymous read authority.
		///
		/// \details Deliberately not pure. A factory that has no anonymous
		/// form should refuse rather than be forced to invent one, and the
		/// refusal is the safe default: a provider configured for anonymous
		/// reads against such a factory sends nothing. There is no credential
		/// parameter, so no caller can supply material through this entry.
		[[nodiscard]] virtual auto make_anonymous(
			const validated_github_host_profile& aProfile,
			core::github_session_request_context aContext) noexcept
			-> github_wire_transport_factory_result
		{
			static_cast<void>(aProfile);
			static_cast<void>(aContext);
			return std::unexpected{ application::github_provider_error{
				.Failure = application::github_provider_failure::
					unsupported_capability,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			} };
		}
	};

	class native_github_wire_transport_factory final
		: public github_wire_transport_factory_port
	{
	public:
		[[nodiscard]] auto
		make_bootstrap(const validated_github_host_profile&    aProfile,
					   core::github_session_request_context    aContext,
					   application::github_credential_material aCredential) noexcept
			-> github_wire_transport_factory_result override;

		[[nodiscard]] auto make_account(
			const validated_github_host_profile&            aProfile,
			application::github_credential_retrieve_success aCredential) noexcept
			-> github_wire_transport_factory_result override;

		[[nodiscard]] auto make_anonymous(
			const validated_github_host_profile& aProfile,
			core::github_session_request_context aContext) noexcept
			-> github_wire_transport_factory_result override;
	};

	enum class github_provider_configuration_failure : std::uint8_t {
		no_profiles,
		invalid_profile,
		duplicate_profile,
		allocation_failure,
		closing,
		collection_limit,
	};

	inline constexpr std::size_t maximum_github_profile_catalog_entries = 128U;

	/// \brief Shared, append-only catalog; retained entries survive registration and shutdown.
	class github_profile_catalog final
	{
	public:
		/// \brief Adds a validated profile; exact duplicates succeed without replacement.
		[[nodiscard]] auto register_profile(const validated_github_host_profile& aProfile) noexcept
			-> std::expected<void, github_provider_configuration_failure>;
		/// \brief Copies immutable ownership under the catalog lock.
		[[nodiscard]] auto find_profile(core::host_profile_id aIdentity) const noexcept
			-> std::shared_ptr<const validated_github_host_profile>;
		/// \brief Closes registration while preserving existing entries and provider jobs.
		void shutdown() noexcept;

	private:
		mutable std::mutex                                                Mutex;
		std::vector<std::shared_ptr<const validated_github_host_profile>> Profiles;
		bool                                                              Closing = false;
	};

	class github_provider final : public application::github_provider_port,
								  public application::personal_discovery_port,
								  public application::markdown_image_resolution_port,
								  public application::markdown_image_fetch_port
	{
	public:
		class implementation;

		explicit github_provider(
			std::unique_ptr<implementation> aImplementation) noexcept;
		~github_provider() override;

		github_provider(const github_provider&)                    = delete;
		auto operator=(const github_provider&) -> github_provider& = delete;
		github_provider(github_provider&&)                         = delete;
		auto operator=(github_provider&&) -> github_provider&      = delete;

		void resolve_profile(
			application::github_profile_request            aRequest,
			application::github_profile_completion_handler aCompletion) override;
		void cancel_profile(application::github_profile_request aRequest) override;

		void observe_viewer(
			application::github_viewer_submission         aSubmission,
			application::github_viewer_completion_handler aCompletion) override;
		void cancel_viewer(application::github_viewer_request aRequest) override;

		void probe_capabilities(
			application::github_capability_request            aRequest,
			application::github_capability_completion_handler aCompletion) override;
		void cancel_capabilities(
			application::github_capability_request aRequest) override;

		void fetch_pull_request(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion)
			override;
		void cancel_pull_request(
			application::github_pull_request_provider_request aRequest) override;
		void fetch_pull_request_with_progress(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion,
			application::github_pull_request_progress_handler   aProgress) override;
		void fetch_pull_request_with_observations(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion,
			application::github_pull_request_progress_handler   aProgress,
			application::github_pull_request_overview_handler   aOverview) override;

		/// \brief [[BPR-0988]]. One bounded personal discovery read, on the
		/// same exact account transport a pull request read uses.
		void discover(application::personal_discovery_request            aRequest,
					  application::personal_discovery_completion_handler aCompletion) override;
		void cancel_discovery(const application::personal_discovery_request& aRequest) override;
		/// \brief RFC-BPR-0084. Reads GitHub's rendering of the accepted bodies
		/// through the request's exact account transport; authenticated only.
		void resolve(application::markdown_image_resolution_request                         aRequest,
					 std::function<void(application::markdown_image_resolution_completion)> aCompletion) override;
		void cancel_resolution(const application::markdown_image_resolution_request& aRequest) override;
		/// \brief RFC-BPR-0084. Fetches one body image. A credentialed GHES hop
		/// runs on the provider's own serial lane through the request's exact
		/// native account transport; every other fetch runs on one of the
		/// configured image lanes through that lane's credential-free media
		/// transport for the profile. Unconfigured, every fetch fails.
		void fetch(application::markdown_image_fetch_request                     aRequest,
				   std::stop_token                                               aStop,
				   std::function<void(application::markdown_image_fetch_result)> aCompletion) override;
		/// \brief Performs one credential-free image fetch on its lane.
		using media_performer = std::function<application::markdown_image_fetch_result(const application::markdown_image_fetch_request&, std::stop_token)>;
		/// \brief Builds one lane's credential-free media performer for a profile.
		using media_transport_factory = std::function<media_performer(const validated_github_host_profile&)>;
		/// \brief RFC-BPR-0084. Gives image fetches their serial lanes, one media
		/// transport per lane and profile. Call once, before any fetch.
		void configure_markdown_images(std::vector<application::effect_executor*> aLanes, media_transport_factory aFactory);

		void shutdown() noexcept;

	private:
		std::unique_ptr<implementation> Implementation;
	};

	using github_provider_factory_result = std::expected<std::unique_ptr<github_provider>,
														 github_provider_configuration_failure>;

	/// \brief RFC-BPR-0084. The shipped media performers: one native HTTPS
	/// media transport per lane and profile.
	[[nodiscard]] auto make_native_markdown_image_media_factory() -> github_provider::media_transport_factory;

	[[nodiscard]] auto make_github_provider(
		std::shared_ptr<github_profile_catalog>             aProfiles,
		application::effect_executor&                       aEffects,
		std::shared_ptr<github_wire_transport_factory_port> aTransportFactory,
		github_provider_read_authority                      aReadAuthority = github_provider_read_authority::authenticated,
		std::shared_ptr<application::diagnostic_log_port>   aLog           = { }) noexcept
		-> github_provider_factory_result;

	[[nodiscard]] auto
	make_github_provider(std::span<const validated_github_host_profile> aProfiles,
						 application::effect_executor&                  aEffects,
						 std::shared_ptr<github_wire_transport_factory_port>
														aTransportFactory,
						 github_provider_read_authority aReadAuthority
						 = github_provider_read_authority::authenticated,
						 std::shared_ptr<application::diagnostic_log_port> aLog = { }) noexcept
		-> github_provider_factory_result;

	[[nodiscard]] auto make_github_provider(
		std::span<const validated_github_host_profile* const> aProfiles,
		application::effect_executor&                         aEffects,
		std::shared_ptr<github_wire_transport_factory_port>   aTransportFactory,
		github_provider_read_authority                        aReadAuthority
		= github_provider_read_authority::authenticated,
		std::shared_ptr<application::diagnostic_log_port> aLog = { }) noexcept
		-> github_provider_factory_result;

	class github_mutation_wire_transport;
	/// \brief [[BPR-1572]]. For tests: the production GitHub CLI account
	/// wrapper over `aInner`, bound to the account `aContext` names under
	/// `aLogin`, as an account lease installs it, so a session's publication
	/// can be driven through it. `aInner` answers the account reads and, as a
	/// mutation transport, everything else.
	[[nodiscard]] auto make_cli_observed_account_transport_for_testing(
		std::shared_ptr<github_wire_transport_port>             aInner,
		const application::github_credential_operation_context& aContext,
		std::string                                             aLogin) -> std::shared_ptr<github_mutation_wire_transport>;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PROVIDER_HPP
