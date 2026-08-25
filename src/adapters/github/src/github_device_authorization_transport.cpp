#include "github_device_authorization_transport.hpp"

#include "github_device_authorization_contract.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {

		// Every way the wire can disappoint, reduced to the six the host
		// understands. A parse refusal is a malformed response and not a
		// network problem, because retrying it cannot change the answer.
		[[nodiscard]] auto host_failure(
			const device_authorization_contract_failure aFailure) noexcept
			-> application::device_authorization_transport_failure
		{
			using transport
				= application::device_authorization_transport_failure;
			switch (aFailure) {
			case device_authorization_contract_failure::response_limit:
				return transport::response_too_large;
			case device_authorization_contract_failure::
				unsupported_server_contract:
			case device_authorization_contract_failure::invalid_client_id:
			case device_authorization_contract_failure::invalid_device_code:
			case device_authorization_contract_failure::invalid_refresh_token:
			case device_authorization_contract_failure::unexpected_status:
			case device_authorization_contract_failure::unexpected_content_type:
			case device_authorization_contract_failure::malformed_response:
			case device_authorization_contract_failure::unexpected_field:
			case device_authorization_contract_failure::missing_field:
			case device_authorization_contract_failure::invalid_field_value:
			case device_authorization_contract_failure::unofficial_error_uri:
			case device_authorization_contract_failure::unknown_error_code:
			case device_authorization_contract_failure::
				off_authority_verification_uri:
				return transport::malformed_response;
			}
			return transport::malformed_response;
		}

		[[nodiscard]] auto host_failure(
			const application::github_provider_failure aFailure) noexcept
			-> application::device_authorization_transport_failure
		{
			using transport
				= application::device_authorization_transport_failure;
			switch (aFailure) {
			case application::github_provider_failure::certificate:
				return transport::certificate;
			case application::github_provider_failure::redirect:
				return transport::redirect;
			case application::github_provider_failure::timeout:
				return transport::timeout;
			case application::github_provider_failure::malformed_response:
				return transport::malformed_response;
			case application::github_provider_failure::response_limit:
				return transport::response_too_large;
			case application::github_provider_failure::authentication:
			case application::github_provider_failure::permission:
			case application::github_provider_failure::network:
			case application::github_provider_failure::api_version:
			case application::github_provider_failure::unsupported_capability:
			case application::github_provider_failure::rate_limit:
			case application::github_provider_failure::process_startup:
			case application::github_provider_failure::cli_unavailable:
			case application::github_provider_failure::vault:
			case application::github_provider_failure::unavailable:
				return transport::network;
			}
			return transport::network;
		}

		[[nodiscard]] auto as_text(
			const application::github_credential_material& aMaterial)
			-> std::string
		{
			const auto bytes = aMaterial.bytes();
			auto       text  = std::string{ };
			text.resize(bytes.size());
			std::ranges::transform(
				bytes,
				text.begin(),
				[](const std::byte aByte) {
					return static_cast<char>(aByte);
				});
			return text;
		}

		[[nodiscard]] auto as_material(const std::string& aText)
			-> std::optional<application::github_credential_material>
		{
			auto bytes = std::vector<std::byte>{ };
			bytes.resize(aText.size());
			std::ranges::transform(
				aText,
				bytes.begin(),
				[](const char aCharacter) {
					return static_cast<std::byte>(aCharacter);
				});
			auto material
				= application::github_credential_material::create(bytes);
			std::ranges::fill(bytes, std::byte{ 0 });
			if (!material.has_value()) {
				return std::nullopt;
			}
			return std::optional{ std::move(*material) };
		}

		[[nodiscard]] auto grant_to_material(const device_token_grant& aGrant)
			-> std::optional<application::device_token_material>
		{
			auto access = as_material(aGrant.AccessToken);
			if (!access.has_value()) {
				return std::nullopt;
			}
			auto refresh = std::optional<
				application::github_credential_material>{ };
			if (!aGrant.RefreshToken.empty()) {
				auto rotated = as_material(aGrant.RefreshToken);
				if (!rotated.has_value()) {
					return std::nullopt;
				}
				refresh = std::move(rotated);
			}
			return application::device_token_material{
				.AccessToken      = std::move(*access),
				.RefreshToken     = std::move(refresh),
				.ExpiresInSeconds = aGrant.ExpiresInSeconds,
				.RefreshTokenExpiresInSeconds
				= aGrant.RefreshTokenExpiresInSeconds,
			};
		}
		using device_exchange_answer = std::variant<
			wire_response,
			application::device_authorization_transport_error,
			application::device_authorization_transport_cancelled>;

		// The two answers that are not the parsed body pass straight through;
		// only a real response reaches a parser. Written once so the token
		// and refresh interpreters cannot disagree about what a cancellation
		// means.
		template <typename TCompletion>
		[[nodiscard]] auto forwarded(device_exchange_answer& aAnswer)
			-> std::optional<TCompletion>
		{
			if (const auto* const cancelled = std::get_if<
					application::device_authorization_transport_cancelled>(
					&aAnswer)) {
				return TCompletion{ *cancelled };
			}
			if (const auto* const error = std::get_if<
					application::device_authorization_transport_error>(
					&aAnswer)) {
				return TCompletion{ *error };
			}
			return std::nullopt;
		}

		[[nodiscard]] auto malformed(
			const core::device_authorization_request_context& aContext)
			-> application::device_authorization_transport_error
		{
			return application::device_authorization_transport_error{
				.Context = aContext,
				.Failure = application::
					device_authorization_transport_failure::malformed_response,
			};
		}

		[[nodiscard]] auto issued_from(
			const core::device_authorization_request_context& aContext,
			const device_token_grant&                         aGrant)
			-> application::device_token_completion
		{
			auto material = grant_to_material(aGrant);
			if (!material.has_value()) {
				return malformed(aContext);
			}
			return application::device_token_issued{
				.Context = aContext,
				.Grant   = std::move(*material),
			};
		}

		[[nodiscard]] auto interpret_token_answer(
			const core::device_authorization_request_context& aContext,
			device_exchange_answer                            aAnswer)
			-> application::device_token_completion
		{
			if (auto forward
				= forwarded<application::device_token_completion>(aAnswer)) {
				return std::move(*forward);
			}
			const auto parsed = parse_device_token_response(
				std::get<wire_response>(aAnswer));
			if (!parsed.has_value()) {
				return application::device_authorization_transport_error{
					.Context = aContext,
					.Failure = host_failure(parsed.error()),
				};
			}
			if (const auto* const delay
				= std::get_if<device_token_delay>(&*parsed)) {
				return application::device_token_deferred{
					.Context                 = aContext,
					.Reason                  = delay->Kind == device_token_delay_kind::slow_down
						? application::device_token_delay_reason::slow_down
						: application::device_token_delay_reason::
							  authorization_pending,
					.SlowDownIntervalSeconds = delay->SlowDownIntervalSeconds,
				};
			}
			if (const auto* const refusal
				= std::get_if<device_token_refusal>(&*parsed)) {
				return application::device_token_refused{
					.Context = aContext,
					.Reason  = refusal->Kind
							== device_token_refusal_kind::expired_token
						? application::device_token_refusal_reason::expired_token
						: application::device_token_refusal_reason::access_denied,
				};
			}
			return issued_from(aContext, std::get<device_token_grant>(*parsed));
		}

		[[nodiscard]] auto interpret_refresh_answer(
			const core::device_authorization_request_context& aContext,
			device_exchange_answer                            aAnswer)
			-> application::device_token_completion
		{
			if (auto forward
				= forwarded<application::device_token_completion>(aAnswer)) {
				return std::move(*forward);
			}
			const auto parsed = parse_device_refresh_response(
				std::get<wire_response>(aAnswer));
			if (!parsed.has_value()) {
				return application::device_authorization_transport_error{
					.Context = aContext,
					.Failure = host_failure(parsed.error()),
				};
			}
			// A renewal the server refuses is the host's `refresh_rejected`,
			// which it reaches through the expired-token refusal. There is no
			// deferred case, because nothing is pending.
			if (std::holds_alternative<device_refresh_refusal>(*parsed)) {
				return application::device_token_refused{
					.Context = aContext,
					.Reason  = application::device_token_refusal_reason::
						expired_token,
				};
			}
			return issued_from(aContext, std::get<device_token_grant>(*parsed));
		}

	} // namespace

	class github_device_authorization_transport::implementation final
	{
	public:
		implementation(
			std::shared_ptr<native_https_transport> aTransport,
			std::string                             aClientId,
			application::effect_executor&           aEffects) noexcept
			: Transport{ std::move(aTransport) }
			, ClientId{ std::move(aClientId) }
			, Effects{ &aEffects }
		{
		}

		// One stop source per live request context, so a cancel names the
		// exchange it means and cannot reach across to another flow's.
		[[nodiscard]] auto arm(
			const core::device_authorization_request_context& aContext)
			-> std::stop_token
		{
			const auto guard = std::scoped_lock{ Mutex };
			auto&      source
				= Sources[aContext.Request];
			source = std::stop_source{ };
			return source.get_token();
		}

		void disarm(const core::device_authorization_request_context& aContext)
		{
			const auto guard = std::scoped_lock{ Mutex };
			Sources.erase(aContext.Request);
		}

		void request_stop(
			const core::device_authorization_request_context& aContext)
		{
			const auto guard  = std::scoped_lock{ Mutex };
			const auto source = Sources.find(aContext.Request);
			if (source != Sources.end()) {
				source->second.request_stop();
			}
		}

		// The one shape all three exchanges share. `aCompile` builds the
		// bounded body, `aInterpret` turns a parsed answer into the host's
		// completion, and everything between them — the off-lane hop, the
		// stop token, the error mapping, the exactly-one terminal — is here
		// once rather than three times.
		template <typename TCompile, typename TInterpret, typename TCompletion>
		void submit(
			core::device_authorization_request_context aContext,
			TCompile                                   aCompile,
			TInterpret                                 aInterpret,
			TCompletion                                aCompletion)
		{
			auto request = aCompile();
			if (!request.has_value()) {
				aCompletion(
					application::device_authorization_transport_error{
						.Context = aContext,
						.Failure = host_failure(request.error()),
					});
				return;
			}

			const auto stop = arm(aContext);
			Effects->post(
				[this,
				 aContext,
				 wire = std::move(*request),
				 stop,
				 interpret  = std::move(aInterpret),
				 completion = std::move(aCompletion)]() mutable {
					completion(
						interpret(perform(aContext, wire, stop)));
					disarm(aContext);
				});
		}

		[[nodiscard]] auto perform(
			const core::device_authorization_request_context& aContext,
			const device_authorization_wire_request&          aRequest,
			std::stop_token                                   aStop)
			-> std::variant<wire_response, application::device_authorization_transport_error, application::device_authorization_transport_cancelled>
		{
			auto completion
				= Transport->perform_device_authorization(aRequest, aStop);
			if (std::holds_alternative<
					native_https_device_authorization_cancelled>(completion)) {
				return application::device_authorization_transport_cancelled{
					.Context = aContext,
				};
			}
			if (const auto* const error
				= std::get_if<native_https_device_authorization_error>(
					&completion)) {
				return application::device_authorization_transport_error{
					.Context = aContext,
					.Failure = host_failure(error->Error.Failure),
				};
			}
			return std::get<native_https_device_authorization_success>(
					   completion)
				.Response;
		}

		std::shared_ptr<native_https_transport>      Transport;
		std::string                                  ClientId;
		application::effect_executor*                Effects = nullptr;
		std::mutex                                   Mutex;
		std::map<core::request_id, std::stop_source> Sources;
	};

	github_device_authorization_transport::
		github_device_authorization_transport(
			std::unique_ptr<implementation> aImplementation) noexcept
		: Implementation{ std::move(aImplementation) }
	{
	}

	github_device_authorization_transport::
		~github_device_authorization_transport()
		= default;

	void github_device_authorization_transport::acquire_device_code(
		core::device_authorization_request_context  aContext,
		application::device_code_completion_handler aCompletion)
	{
		Implementation->submit(
			aContext,
			[this] {
				return compile_device_code_request(
					server_contract::github_com,
					Implementation->ClientId);
			},
			[aContext](auto aOutcome) -> application::device_code_completion {
				if (auto forward
					= forwarded<application::device_code_completion>(aOutcome)) {
					return std::move(*forward);
				}
				const auto parsed = parse_device_code_response(
					std::get<wire_response>(aOutcome));
				if (!parsed.has_value()) {
					return application::device_authorization_transport_error{
						.Context = aContext,
						.Failure = host_failure(parsed.error()),
					};
				}
				if (std::holds_alternative<device_code_refusal>(
						*parsed)) {
					return application::device_code_refused{
						.Context = aContext,
					};
				}
				const auto& grant
					= std::get<device_code_grant>(*parsed);
				auto code = as_material(grant.DeviceCode);
				if (!code.has_value()) {
					return malformed(aContext);
				}
				return application::device_code_acquired{
					.Context = aContext,
					.Grant   = application::device_code_acquisition{
						.DeviceCode       = std::move(*code),
						.UserCode         = grant.UserCode,
						.VerificationUri  = grant.VerificationUri,
						.ExpiresInSeconds = grant.ExpiresInSeconds,
						.IntervalSeconds  = grant.IntervalSeconds,
					},
				};
			},
			std::move(aCompletion));
	}

	void github_device_authorization_transport::exchange_device_token(
		application::device_token_exchange           aExchange,
		application::device_token_completion_handler aCompletion)
	{
		const auto context = aExchange.Context;
		auto       code    = as_text(aExchange.Code);
		Implementation->submit(
			context,
			[this, &code] {
				return compile_device_token_request(
					server_contract::github_com,
					Implementation->ClientId,
					code);
			},
			[context](auto aOutcome) -> application::device_token_completion {
				return interpret_token_answer(context, std::move(aOutcome));
			},
			std::move(aCompletion));
		std::ranges::fill(code, '\0');
	}

	void github_device_authorization_transport::refresh_access_token(
		application::device_token_exchange           aExchange,
		application::device_token_completion_handler aCompletion)
	{
		const auto context = aExchange.Context;
		auto       refresh = as_text(aExchange.Code);
		Implementation->submit(
			context,
			[this, &refresh] {
				return compile_device_refresh_request(
					server_contract::github_com,
					Implementation->ClientId,
					refresh);
			},
			[context](auto aOutcome) -> application::device_token_completion {
				return interpret_refresh_answer(context, std::move(aOutcome));
			},
			std::move(aCompletion));
		std::ranges::fill(refresh, '\0');
	}

	void github_device_authorization_transport::cancel(
		const core::device_authorization_request_context aContext)
	{
		Implementation->request_stop(aContext);
	}

	auto make_github_device_authorization_transport(
		std::shared_ptr<native_https_transport> aTransport,
		std::string                             aClientId,
		application::effect_executor&           aEffects) noexcept
		-> github_device_authorization_transport_factory_result
	{
		if (aTransport == nullptr) {
			return std::unexpected(
				device_authorization_transport_configuration_failure::
					no_transport);
		}
		if (!valid_device_client_id(aClientId)) {
			return std::unexpected(
				device_authorization_transport_configuration_failure::
					invalid_client_id);
		}
		auto owned = std::make_unique<
			github_device_authorization_transport::implementation>(
			std::move(aTransport),
			std::move(aClientId),
			aEffects);
		return std::make_unique<github_device_authorization_transport>(
			std::move(owned));
	}
} // namespace bpr::adapters::github::detail
