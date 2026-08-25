#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_DEVICE_AUTHORIZATION_TRANSPORT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_DEVICE_AUTHORIZATION_TRANSPORT_HPP

#include "native_https_transport.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/port.hpp"

#include <expected>
#include <memory>
#include <string>

namespace bpr::adapters::github::detail {
	enum class device_authorization_transport_configuration_failure : std::uint8_t {
		no_transport,
		invalid_client_id,
		allocation_failure,
	};

	/// \brief Performs the device-authorization exchanges on a real transport.
	///
	/// \details The production implementation of [[BPR-0279]]'s transport port.
	/// It owns three exchanges and no policy: compile the exact bounded form
	/// request, hand it to `native_https_transport::perform_device_authorization`
	/// off the decision lane, parse the answer into the typed record the host
	/// expects, and post exactly one completion. The poll schedule, the bounds,
	/// the retry rule, and every terminal remain the host's.
	///
	/// The refresh exchange is the one [[BPR-0450]] added, and it is the one
	/// whose wire shapes are `documented_not_recorded` — see the note at the
	/// top of `github_device_authorization_contract.hpp`. The other two replay
	/// the recorded BPR-0276 courses.
	///
	/// Secret material never lives here longer than a call. The device code and
	/// the refresh token arrive as move-only `github_credential_material`, are
	/// read once to build the body, and are released with the request; the
	/// tokens an answer carries go straight into the completion. Nothing is
	/// retained between exchanges, and no failure carries a response byte.
	///
	/// Cancellation is advisory and does not suppress a terminal, exactly as
	/// the port promises: `cancel` requests the stop token the in-flight
	/// exchange already holds, and the exchange still reports once.
	class github_device_authorization_transport final
		: public application::device_authorization_transport_port
	{
	public:
		class implementation;

		explicit github_device_authorization_transport(
			std::unique_ptr<implementation> aImplementation) noexcept;
		~github_device_authorization_transport() override;

		void acquire_device_code(
			core::device_authorization_request_context  aContext,
			application::device_code_completion_handler aCompletion) override;
		void exchange_device_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override;
		void refresh_access_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override;
		void cancel(
			core::device_authorization_request_context aContext) override;

	private:
		std::unique_ptr<implementation> Implementation;
	};

	using github_device_authorization_transport_factory_result = std::expected<
		std::unique_ptr<github_device_authorization_transport>,
		device_authorization_transport_configuration_failure>;

	/// \brief Creates the production device-authorization transport.
	///
	/// \details The transport must be one built by
	/// `make_native_https_credential_free_transport`; nothing else satisfies
	/// the credential-free binding `perform_device_authorization` requires, so
	/// a credentialed transport cannot be handed in and silently work. The
	/// client identifier is validated here rather than at each exchange, so a
	/// malformed registration fails at composition instead of at the first
	/// request.
	[[nodiscard]] auto make_github_device_authorization_transport(
		std::shared_ptr<native_https_transport> aTransport,
		std::string                             aClientId,
		application::effect_executor&           aEffects) noexcept
		-> github_device_authorization_transport_factory_result;
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_DEVICE_AUTHORIZATION_TRANSPORT_HPP
