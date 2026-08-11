#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PROVIDER_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PROVIDER_HPP

#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_mutation_port.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <span>

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
	};

	using github_wire_transport_factory_result = std::expected<std::shared_ptr<github_wire_transport_port>,
															   application::github_provider_error>;

	class github_wire_transport_factory_port
	{
	public:
		github_wire_transport_factory_port()          = default;
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
	};

	enum class github_provider_configuration_failure : std::uint8_t {
		no_profiles,
		invalid_profile,
		duplicate_profile,
		allocation_failure,
	};

	class github_provider final : public application::github_provider_port
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

		void shutdown() noexcept;

	private:
		std::unique_ptr<implementation> Implementation;
	};

	using github_provider_factory_result = std::expected<std::unique_ptr<github_provider>,
														 github_provider_configuration_failure>;

	[[nodiscard]] auto
	make_github_provider(std::span<const validated_github_host_profile> aProfiles,
						 application::effect_executor&                  aEffects,
						 std::shared_ptr<github_wire_transport_factory_port>
							 aTransportFactory) noexcept
		-> github_provider_factory_result;

	[[nodiscard]] auto make_github_provider(
		std::span<const validated_github_host_profile* const> aProfiles,
		application::effect_executor&                         aEffects,
		std::shared_ptr<github_wire_transport_factory_port>
			aTransportFactory) noexcept -> github_provider_factory_result;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_PROVIDER_HPP
