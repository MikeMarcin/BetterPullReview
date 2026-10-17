#include "../support/capability_error_content_canaries.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "github_mutation_contract.hpp"
#include "github_provider.hpp"
#include "github_publication_contract.hpp"
#include "github_viewer_named_blob_read.hpp"
#include "github_wire_request_validation.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	[[noreturn]] void fail(const std::string_view aMessage)
	{
		std::cerr << "FAILED: " << aMessage << '\n';
		std::exit(EXIT_FAILURE);
	}

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			fail(aMessage);
		}
	}

	template <typename Alternative, typename Completion>
	[[nodiscard]] auto require_one(const std::vector<Completion>& aCompletions,
								   const std::string_view         aMessage)
		-> const Alternative&
	{
		if (aCompletions.size() != 1U) {
			fail(aMessage);
		}
		const auto* value = std::get_if<Alternative>(&aCompletions.front());
		if (value == nullptr) {
			fail(aMessage);
		}
		return *value;
	}

	class controlled_executor final : public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			++PostCalls;
			if (ThrowOnPost) {
				if (RetainTaskOnThrow) {
					Tasks.push_back(std::move(aTask));
				}
				throw std::runtime_error{ "post failure" };
			}
			Tasks.push_back(std::move(aTask));
		}

		[[nodiscard]] auto pending() const noexcept -> std::size_t
		{
			return Tasks.size();
		}

		void run_next()
		{
			expect(!Tasks.empty(), "executor has a pending task");
			auto task = std::move(Tasks.front());
			Tasks.pop_front();
			task();
		}

		bool        ThrowOnPost       = false;
		bool        RetainTaskOnThrow = false;
		std::size_t PostCalls         = 0U;

	private:
		std::deque<std::function<void()>> Tasks;
	};

	class fake_git_transport : public application::git_smart_transport_port
	{
	public:
		void revoke() noexcept override { Revoked = true; }
		bool revoked() const noexcept override { return Revoked; }
		auto read_exact_blob(const application::git_smart_exact_blob_request& aRequest, std::stop_token)
			-> application::git_smart_transport_result<std::vector<std::byte>> override
		{
			++Calls;
			BlobRequest = aRequest;
			if (Hook) {
				Hook();
			}
			return BlobResult;
		}
		auto discover(const application::git_smart_discovery_request&, std::stop_token) -> application::git_smart_transport_result<application::git_smart_discovery_response> override
		{
			++Calls;
			if (Hook) {
				Hook();
			}
			return application::git_smart_discovery_response{ };
		}
		auto ls_refs(const application::git_smart_ls_refs_request&, std::stop_token) -> application::git_smart_transport_result<application::git_smart_ls_refs_response> override
		{
			++Calls;
			if (Hook) {
				Hook();
			}
			return application::git_smart_ls_refs_response{ };
		}
		auto fetch(const application::git_smart_fetch_request&, application::git_smart_wire_consumer&, std::stop_token) -> application::git_smart_transport_result<void> override
		{
			++Calls;
			if (Hook) {
				Hook();
			}
			return { };
		}
		bool                                                            Revoked = false;
		unsigned                                                        Calls   = 0;
		std::function<void()>                                           Hook;
		std::optional<application::git_smart_exact_blob_request>        BlobRequest;
		application::git_smart_transport_result<std::vector<std::byte>> BlobResult = std::vector{ std::byte{ 'x' } };
	};

	/// [[BPR-1370]]. A Git transport whose GraphQL answers name their account.
	class viewer_naming_git_transport final : public fake_git_transport, public viewer_named_blob_reader
	{
	public:
		auto read_exact_blobs_naming_viewer(const application::git_smart_exact_blobs_request& aRequest, std::stop_token)
			-> application::git_smart_transport_result<viewer_named_blobs> override
		{
			++NamedCalls;
			NamedRequest = aRequest;
			if (NamedHook) {
				NamedHook();
			}
			if (!NamedFailure) {
				return std::unexpected{ NamedFailure.error() };
			}
			auto answer   = viewer_named_blobs{ };
			answer.Viewer = Viewer;
			answer.Blobs.assign(aRequest.ExpectedBlobs.size(), NamedBlob);
			return answer;
		}
		unsigned                                                    NamedCalls = 0U;
		std::optional<application::git_smart_exact_blobs_request>   NamedRequest;
		std::function<void()>                                       NamedHook;
		std::expected<void, application::git_smart_transport_error> NamedFailure;
		std::optional<std::vector<std::byte>>                       NamedBlob = std::vector{ std::byte{ 't' } };
		github_answering_account                                    Viewer{ .Account = core::github_account_id{ 1001U }, .Login = "octo-reviewer" };
	};
	class fake_wire_transport final : public github_wire_transport_port,
									  public github_mutation_wire_transport,
									  public github_publication_wire_transport,
									  public application::git_smart_transport_factory_port
	{
	public:
		/// [[BPR-1177]]. A publication exchange: its credential context is
		/// recorded and nothing is answered.
		auto perform_publication(const github_publication_wire_call& aCall, std::stop_token) -> github_publication_wire_completion override
		{
			PublicationContexts.push_back(aCall.Context);
			return github_publication_wire_error{ };
		}
		std::vector<application::github_credential_operation_context> PublicationContexts;
		auto                                                          acquire(const application::git_smart_request_context& aContext) -> application::git_smart_transport_result<application::git_smart_transport> override
		{
			GitContext = aContext;
			return Git;
		}
		auto                                                  supports_sparse_commit_trees() const noexcept -> bool override { return Sparse; }
		bool                                                  Sparse = false;
		std::optional<application::git_smart_request_context> GitContext;
		std::shared_ptr<fake_git_transport>                   Git = std::make_shared<fake_git_transport>();
		using read_handler                                        = std::function<github_wire_completion(
			const github_wire_request&, std::stop_token)>;
		using capability_handler                                  = std::function<github_capability_probe_wire_completion(
			const github_capability_probe_wire_request&, std::stop_token)>;

		[[nodiscard]] auto perform(const github_wire_request& aRequest,
								   const std::stop_token      aStop)
			-> github_wire_completion override
		{
			ReadRequests.push_back(aRequest);
			ReadStopRequested.push_back(aStop.stop_requested());
			if (ReadHandler) {
				return ReadHandler(aRequest, aStop);
			}
			return github_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = {
					.Failure           = application::github_provider_failure::unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			};
		}

		[[nodiscard]] auto perform_capability_probe(
			const github_capability_probe_wire_request& aRequest,
			const std::stop_token                       aStop)
			-> github_capability_probe_wire_completion override
		{
			CapabilityRequests.push_back(aRequest);
			CapabilityStopRequested.push_back(aStop.stop_requested());
			if (CapabilityHandler) {
				return CapabilityHandler(aRequest, aStop);
			}
			return github_capability_probe_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = {
					.Failure           = application::github_provider_failure::unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			};
		}

		auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			if (aRequest.Wire.Method == http_method::post && (!aWitness || !aWitness())) {
				return github_mutation_wire_error{ };
			}
			++MutationCalls;
			if (MutationHook) {
				MutationHook();
			}
			return github_mutation_wire_success{ };
		}
		unsigned              MutationCalls = 0;
		std::function<void()> MutationHook;

		read_handler                                      ReadHandler;
		capability_handler                                CapabilityHandler;
		std::vector<github_wire_request>                  ReadRequests;
		std::vector<bool>                                 ReadStopRequested;
		std::vector<github_capability_probe_wire_request> CapabilityRequests;
		std::vector<bool>                                 CapabilityStopRequested;
	};

	class fake_transport_factory final : public github_wire_transport_factory_port
	{
	public:
		[[nodiscard]] auto
		make_bootstrap(const validated_github_host_profile&       aProfile,
					   const core::github_session_request_context aContext,
					   application::github_credential_material    aCredential) noexcept
			-> github_wire_transport_factory_result override
		{
			++BootstrapCalls;
			LastBootstrapProfile         = aProfile.profile();
			LastBootstrapContext         = aContext;
			LastBootstrapCredentialBytes = aCredential.size();
			if (BootstrapTransport == nullptr) {
				return std::unexpected{ application::github_provider_error{
					.Failure           = application::github_provider_failure::unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				} };
			}
			return BootstrapTransport;
		}

		[[nodiscard]] auto make_account(
			const validated_github_host_profile&            aProfile,
			application::github_credential_retrieve_success aCredential) noexcept
			-> github_wire_transport_factory_result override
		{
			++AccountCalls;
			LastAccountProfile         = aProfile.profile();
			LastAccountContext         = aCredential.Context;
			LastAccountCredentialBytes = aCredential.Credential.size();
			if (AccountTransport == nullptr) {
				return std::unexpected{ application::github_provider_error{
					.Failure           = application::github_provider_failure::unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				} };
			}
			return AccountTransport;
		}

		bool        CliEnabled = false;
		std::size_t CliCalls   = 0U;
		auto        supports_cli() const noexcept -> bool override { return CliEnabled; }
		auto        make_cli(const validated_github_host_profile&, core::github_session_request_context) noexcept -> github_wire_transport_factory_result override
		{
			++CliCalls;
			return AccountTransport;
		}
		std::shared_ptr<github_wire_transport_port>         BootstrapTransport;
		std::shared_ptr<github_wire_transport_port>         AccountTransport;
		std::size_t                                         BootstrapCalls = 0U;
		std::size_t                                         AccountCalls   = 0U;
		std::optional<core::host_profile_id>                LastBootstrapProfile;
		std::optional<core::github_session_request_context> LastBootstrapContext;
		std::size_t                                         LastBootstrapCredentialBytes = 0U;
		std::optional<core::host_profile_id>                LastAccountProfile;
		std::optional<application::github_credential_operation_context>
					LastAccountContext;
		std::size_t LastAccountCredentialBytes = 0U;
	};

	[[nodiscard]] auto
	test_profile(const application::github_authentication_transport aAuthentication)
		-> validated_github_host_profile
	{
		const auto profile = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 22U },
			.Revision       = application::github_profile_revision{ 3U },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = aAuthentication,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		});
		expect(profile.has_value(), "provider test profile validates");
		return *profile;
	}

	[[nodiscard]] auto
	test_enterprise_profile(
		const application::github_authentication_transport aAuthentication,
		const core::host_profile_id                        aProfileIdentity = core::host_profile_id{ 12U })
		-> validated_github_host_profile
	{
		const auto profile = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = aProfileIdentity,
			.Host           = core::git_host_id{ 23U },
			.Revision       = application::github_profile_revision{ 4U },
			.Server         = application::github_server_contract::ghes_3_18_2022_11_28,
			.Authentication = aAuthentication,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::managed,
			.Hostname       = "ghe.example.invalid",
			.RestRoot       = "https://ghe.example.invalid/api/v3",
			.GraphqlRoot    = "https://ghe.example.invalid/api/graphql",
		});
		expect(profile.has_value(), "enterprise provider test profile validates");
		return *profile;
	}

	[[nodiscard]] auto
	profile_authority_for(const validated_github_host_profile& aProfile)
		-> application::github_profile_authority
	{
		return {
			.Profile               = aProfile.profile(),
			.ProfileRevision       = aProfile.revision(),
			.Host                  = aProfile.host(),
			.Hostname              = application::github_canonical_hostname{ aProfile.hostname() },
			.ServerContract        = aProfile.server_contract(),
			.Authentication        = aProfile.authentication_transport(),
			.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
		};
	}

	[[nodiscard]] auto session_context(const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 101U },
			.ApplicationSession = core::application_session_id{ 102U },
			.Profile            = aProfile.profile(),
			.IdentityGeneration = core::github_identity_generation{ 103U },
		};
	}

	[[nodiscard]] auto viewer_record() -> application::github_viewer_record
	{
		return {
			.Account     = core::github_account_id{ 1001U },
			.Node        = application::github_actor_id{ "U_viewer" },
			.Login       = "octo-reviewer",
			.DisplayName = "Octo Reviewer",
		};
	}

	[[nodiscard]] auto
	viewer_request_for(const validated_github_host_profile& aProfile)
		-> application::github_viewer_request
	{
		return {
			.Context   = session_context(aProfile),
			.Authority = profile_authority_for(aProfile),
			.Attempt   = application::github_provider_attempt{ 1U },
		};
	}

	[[nodiscard]] auto credential_material()
		-> application::github_credential_material
	{
		constexpr std::string_view value      = "BPR0027ProviderFixtureValue";
		const auto                 characters = std::span<const char>{ value.data(), value.size() };
		auto                       material   = application::github_credential_material::create(
			std::as_bytes(characters));
		expect(material.has_value(), "provider test credential material");
		return std::move(*material);
	}

	[[nodiscard]] auto
	credential_context_for(const validated_github_host_profile&        aProfile,
						   const core::github_session_request_context& aContext,
						   const core::github_account_id               aAccount)
		-> application::github_credential_operation_context
	{
		return {
			.Request            = aContext.Request,
			.ApplicationSession = aContext.ApplicationSession,
			.Authority          = {
				.Profile         = aProfile.profile(),
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Hostname        = application::github_canonical_hostname{
					aProfile.hostname() },
				.Account        = aAccount,
				.ServerContract = aProfile.server_contract(),
			},
			.IdentityGeneration = aContext.IdentityGeneration,
		};
	}

	[[nodiscard]] auto response_headers() -> std::vector<http_header>
	{
		return {
			{
				.Name  = "Content-Type",
				.Value = "application/json; charset=utf-8",
			},
			{
				.Name  = "X-RateLimit-Limit",
				.Value = "5000",
			},
			{
				.Name  = "X-RateLimit-Remaining",
				.Value = "4999",
			},
			{
				.Name  = "X-RateLimit-Reset",
				.Value = "2000000000",
			},
			{
				.Name  = "X-RateLimit-Resource",
				.Value = "core",
			},
		};
	}

	[[nodiscard]] auto viewer_response() -> wire_response
	{
		return {
			.Status       = 200,
			.Headers      = response_headers(),
			.Body         = R"({"id":1001,"login":"octo-reviewer","name":"Octo Reviewer","node_id":"U_viewer"})",
			.Completeness = page_completeness::complete,
		};
	}

	[[nodiscard]] auto capability_fixture_root() -> std::filesystem::path
	{
#if defined(BPR_GITHUB_CAPABILITY_FIXTURE_ROOT)
		return BPR_GITHUB_CAPABILITY_FIXTURE_ROOT;
#else
		return std::filesystem::path{ __FILE__ }.parent_path().parent_path()
			/ "fixtures/github/capabilities/v1";
#endif
	}

	[[nodiscard]] auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		expect(input.good(), "provider capability fixture opens");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ },
		};
	}

	[[nodiscard]] auto supported_capability_response() -> capability_probe_response
	{
		auto response = capability_probe_response{
			.Status               = 200,
			.Authenticated        = true,
			.ContentType          = "application/json",
			.Body                 = read_file(capability_fixture_root() / "github-com/supported.json"),
			.RateLimitHeaders     = { },
			.RateLimitHeaderCount = 4U,
		};
		response.RateLimitHeaders[0U] = {
			.Name  = "X-RateLimit-Limit",
			.Value = "5000",
		};
		response.RateLimitHeaders[1U] = {
			.Name  = "X-RateLimit-Remaining",
			.Value = "4999",
		};
		response.RateLimitHeaders[2U] = {
			.Name  = "X-RateLimit-Reset",
			.Value = "2000000000",
		};
		response.RateLimitHeaders[3U] = {
			.Name  = "X-RateLimit-Resource",
			.Value = "graphql",
		};
		return response;
	}

	class recording_log final : public application::diagnostic_log_port
	{
	public:
		std::vector<application::diagnostic_event> Events;
		bool*                                      FailCompletionCopy = nullptr;
		auto                                       record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			if (FailCompletionCopy && aEvent.Outcome == application::diagnostic_outcome::started) {
				*FailCompletionCopy = true;
			}
			return application::diagnostic_log_status::available;
		}
		auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
	};

	struct fixture_json_options : glz::opts
	{
		// Glaze discovers these options by their required library member names.
		// NOLINTBEGIN(readability-identifier-naming)
		bool validate_skipped             = true;
		bool validate_trailing_whitespace = true;
		bool escape_control_characters    = true;
		// NOLINTEND(readability-identifier-naming)
		constexpr fixture_json_options()
		{
			null_terminated       = false;
			error_on_missing_keys = true;
			error_on_unknown_keys = false;
		}
	};
	auto capability_part_response(capability_probe_response aResponse, std::uint8_t aPart) -> capability_probe_response
	{
		using object = std::map<std::string, glz::raw_json, std::less<>>;
		object envelope;
		if (glz::read<fixture_json_options{ }>(envelope, aResponse.Body) || envelope.contains("errors")) {
			return aResponse;
		}
		const auto data = envelope.find("data");
		object     all;
		if (data == envelope.end() || glz::read<fixture_json_options{ }>(all, data->second.str)) {
			return aResponse;
		}
		object selected;
		for (const auto alias : capability_probe_part_aliases(aPart)) {
			if (const auto found = all.find(alias); found != all.end()) {
				selected.emplace(found->first, found->second);
			}
		}
		expect(!glz::write<fixture_json_options{ }>(selected, data->second.str), "selected capability fixture serializes");
		expect(!glz::write<fixture_json_options{ }>(envelope, aResponse.Body), "capability fixture envelope serializes");
		return aResponse;
	}

	struct provider_fixture
	{
		explicit provider_fixture(
			const application::github_authentication_transport aAuthentication, std::shared_ptr<application::diagnostic_log_port> aLog = { })
			: Profile{ test_profile(aAuthentication) }
			, Transport{ std::make_shared<fake_wire_transport>() }
			, Factory{ std::make_shared<fake_transport_factory>() }
		{
			Factory->BootstrapTransport = Transport;
			Factory->AccountTransport   = Transport;
			auto provider               = make_github_provider(
				std::span<const validated_github_host_profile>{ &Profile, 1U },
				Effects,
				Factory, github_provider_read_authority::authenticated, std::move(aLog));
			expect(provider.has_value(), "concrete provider constructs");
			Provider = std::move(*provider);
		}

		validated_github_host_profile           Profile;
		controlled_executor                     Effects;
		std::shared_ptr<fake_wire_transport>    Transport;
		std::shared_ptr<fake_transport_factory> Factory;
		std::unique_ptr<github_provider>        Provider;
	};

	[[nodiscard]] auto run_viewer(provider_fixture&                     aFixture,
								  application::github_viewer_submission aSubmission)
		-> application::github_viewer_completion
	{
		auto completions = std::vector<application::github_viewer_completion>{ };
		aFixture.Provider->observe_viewer(
			std::move(aSubmission),
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		expect(completions.empty() && aFixture.Effects.pending() == 1U,
			   "viewer work is queued once");
		aFixture.Effects.run_next();
		expect(completions.size() == 1U && aFixture.Effects.pending() == 0U,
			   "viewer work completes exactly once");
		return std::move(completions.front());
	}

	[[nodiscard]] auto
	run_capability(provider_fixture&                      aFixture,
				   application::github_capability_request aRequest)
		-> application::github_capability_completion
	{
		auto completions = std::vector<application::github_capability_completion>{ };
		aFixture.Provider->probe_capabilities(
			std::move(aRequest),
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		expect(completions.empty() && aFixture.Effects.pending() == 1U,
			   "capability work is queued once");
		aFixture.Effects.run_next();
		expect(completions.size() == 1U && aFixture.Effects.pending() == 0U,
			   "capability work completes exactly once");
		return std::move(completions.front());
	}

	[[nodiscard]] auto account_lease(provider_fixture& aFixture)
		-> application::github_account_transport
	{
		const auto request = viewer_request_for(aFixture.Profile);
		const auto context = credential_context_for(
			aFixture.Profile, request.Context, viewer_record().Account);
		aFixture.Transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				return github_wire_success{
					.Correlation = aWire.Correlation,
					.Response    = viewer_response(),
				};
			};
		auto        completion = run_viewer(aFixture,
											application::github_viewer_submission{
												.Request        = request,
												.Authentication = application::github_credential_retrieve_success{
													.Context    = context,
													.Credential = credential_material(),
												},
											});
		const auto* success    = std::get_if<application::github_viewer_success>(&completion);
		expect(success != nullptr && success->Request == request
				   && success->Viewer == viewer_record()
				   && success->AccountTransport != nullptr,
			   "retrieved-account viewer returns an exact account lease");
		expect(aFixture.Factory->AccountCalls == 1U
				   && aFixture.Factory->LastAccountProfile
					   == std::optional{ aFixture.Profile.profile() }
				   && aFixture.Factory->LastAccountContext == std::optional{ context }
				   && aFixture.Factory->LastAccountCredentialBytes != 0U,
			   "account factory receives the exact credential authority");
		return success->AccountTransport;
	}

	[[nodiscard]] auto
	capability_request_for(const validated_github_host_profile&  aProfile,
						   application::github_account_transport aLease)
		-> application::github_capability_request
	{
		return {
			.Context          = session_context(aProfile),
			.ProfileAuthority = profile_authority_for(aProfile),
			.Viewer           = viewer_record(),
			.AccountTransport = std::move(aLease),
			.Attempt          = application::github_provider_attempt{ 1U },
		};
	}

	[[nodiscard]] auto
	capability_request_for(const provider_fixture&               aFixture,
						   application::github_account_transport aLease)
		-> application::github_capability_request
	{
		return capability_request_for(aFixture.Profile, std::move(aLease));
	}

	[[nodiscard]] auto
	conclusive_observation(const validated_github_host_profile& aProfile)
		-> application::github_capability_probe_observation
	{
		auto observation = application::github_capability_probe_observation{
			.Authority = {
				.Profile               = aProfile.profile(),
				.ProfileRevision       = aProfile.revision(),
				.Host                  = aProfile.host(),
				.Account               = viewer_record().Account,
				.ServerContract        = aProfile.server_contract(),
				.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
			},
			.ObservedAt  = application::github_freshness_epoch{ 100U },
			.Facts       = { },
			.SchemaFacts = { },
		};
		for (std::size_t index = 0U; index < observation.Facts.size(); ++index) {
			observation.Facts[index] = {
				.Capability = application::canonical_github_capabilities[index],
				.State      = core::github_capability_state::unsupported,
				.Source     = application::github_capability_evidence_source::
					observed_contract,
			};
		}
		for (std::size_t index = 0U; index < observation.SchemaFacts.size(); ++index) {
			observation.SchemaFacts[index] = {
				.Capability = application::canonical_github_schema_capabilities[index],
				.State      = core::github_capability_state::unsupported,
				.Source     = application::github_capability_evidence_source::
					observed_contract,
			};
		}
		expect(application::valid(observation),
			   "baseline capability observation is valid");
		return observation;
	}

	[[nodiscard]] auto cache_entry_for(const validated_github_host_profile& aProfile)
		-> application::github_capability_cache_entry
	{
		const auto observation = conclusive_observation(aProfile);
		return {
			.Authority     = observation.Authority,
			.Facts         = observation.Facts,
			.SchemaFacts   = observation.SchemaFacts,
			.Capabilities  = application::projected_capabilities(observation),
			.ReadShape     = application::projected_read_shape(observation),
			.Revision      = core::capability_revision{ 44U },
			.ObservedAt    = observation.ObservedAt,
			.ExpiresBefore = application::github_freshness_epoch{ 400U },
		};
	}

	[[nodiscard]] auto
	pull_request_for(const validated_github_host_profile&  aProfile,
					 application::github_account_transport aLease)
		-> application::github_pull_request_provider_request
	{
		const auto session = session_context(aProfile);
		return {
			.Context = {
				.Request            = core::request_id{ 201U },
				.ApplicationSession = session.ApplicationSession,
				.Profile            = aProfile.profile(),
				.Host               = aProfile.host(),
				.Account            = viewer_record().Account,
				.Repository         = {
					.Owner = "octo-org",
					.Name  = "review-repository",
				},
				.Number              = core::pull_request_number{ 17U },
				.ExpectedRepository  = std::nullopt,
				.ExpectedPullRequest = std::nullopt,
				.Capabilities        = core::capability_revision{ 44U },
				.IdentityGeneration  = session.IdentityGeneration,
				.Generation          = core::request_generation{ 9U },
			},
			.ProfileAuthority   = profile_authority_for(aProfile),
			.Viewer             = viewer_record(),
			.CapabilityEvidence = cache_entry_for(aProfile),
			.AccountTransport   = std::move(aLease),
			.Attempt            = application::github_provider_attempt{ 1U },
		};
	}

	[[nodiscard]] auto pull_request_for(const provider_fixture&               aFixture,
										application::github_account_transport aLease)
		-> application::github_pull_request_provider_request
	{
		return pull_request_for(aFixture.Profile, std::move(aLease));
	}

	void test_profile_set_validation()
	{
		auto       effects = controlled_executor{ };
		auto       factory = std::make_shared<fake_transport_factory>();
		const auto empty   = make_github_provider(
			std::span<const validated_github_host_profile>{ }, effects, factory);
		expect(!empty.has_value()
				   && empty.error()
					   == github_provider_configuration_failure::no_profiles,
			   "provider rejects an empty profile set");

		const auto github = test_profile(
			application::github_authentication_transport::native_https);
		const auto enterpriseDuplicate = test_enterprise_profile(
			application::github_authentication_transport::native_https,
			github.profile());
		const std::array<validated_github_host_profile, 2U> duplicateProfiles{
			github,
			enterpriseDuplicate,
		};
		const auto duplicate = make_github_provider(duplicateProfiles, effects, factory);
		expect(
			!duplicate.has_value()
				&& duplicate.error()
					== github_provider_configuration_failure::duplicate_profile
				&& effects.PostCalls == 0U && factory->BootstrapCalls == 0U
				&& factory->AccountCalls == 0U,
			"provider rejects duplicate profile identities before any effect");
	}

	void test_profile_catalog_capacity()
	{
		auto catalog = github_profile_catalog{ };
		for (std::size_t index = 1U; index <= maximum_github_profile_catalog_entries; ++index) {
			const auto profile = test_enterprise_profile(application::github_authentication_transport::native_https,
														 core::host_profile_id{ index });
			expect(catalog.register_profile(profile).has_value(), "catalog admits every entry through exact capacity");
		}
		const auto duplicate = test_enterprise_profile(application::github_authentication_transport::native_https,
													   core::host_profile_id{ maximum_github_profile_catalog_entries });
		const auto retained  = catalog.find_profile(duplicate.profile());
		expect(catalog.register_profile(duplicate).has_value()
				   && catalog.find_profile(duplicate.profile()) == retained,
			   "exact duplicate at capacity preserves immutable entry");
		const auto overflow = test_enterprise_profile(application::github_authentication_transport::native_https,
													  core::host_profile_id{ maximum_github_profile_catalog_entries + 1U });
		const auto refused  = catalog.register_profile(overflow);
		expect(!refused && refused.error() == github_provider_configuration_failure::collection_limit
				   && catalog.find_profile(overflow.profile()) == nullptr
				   && catalog.find_profile(duplicate.profile()) == retained,
			   "capacity plus one is refused atomically without replacing existing authority");
	}

	void test_live_profile_catalog()
	{
		const auto github     = test_profile(application::github_authentication_transport::native_https);
		const auto enterprise = test_enterprise_profile(application::github_authentication_transport::native_https);
		auto       catalog    = std::make_shared<github_profile_catalog>();
		expect(catalog->register_profile(github).has_value(), "initial profile registers");
		const auto retained = catalog->find_profile(github.profile());
		expect(catalog->register_profile(github).has_value()
				   && catalog->find_profile(github.profile()) == retained,
			   "exact duplicate preserves immutable entry identity");
		const auto collision = test_enterprise_profile(application::github_authentication_transport::native_https, github.profile());
		const auto refused   = catalog->register_profile(collision);
		expect(!refused && refused.error() == github_provider_configuration_failure::duplicate_profile,
			   "same identity cannot replace authority");
		auto effects   = controlled_executor{ };
		auto factory   = std::make_shared<fake_transport_factory>();
		auto provider  = make_github_provider(catalog, effects, factory);
		auto anonymous = make_github_provider(catalog, effects, factory, github_provider_read_authority::anonymous_public);
		expect(provider.has_value() && anonymous.has_value(), "both providers share catalog");
		auto oldCompletions = std::vector<application::github_profile_completion>{ };
		auto newCompletions = std::vector<application::github_profile_completion>{ };
		(*provider)->resolve_profile({ .Context = session_context(github), .Attempt = application::github_provider_attempt{ 1U } },
									 [&](auto aCompletion) { oldCompletions.push_back(std::move(aCompletion)); });
		expect(effects.pending() == 1U, "old profile job remains pending during registration");
		expect(catalog->register_profile(enterprise).has_value(), "new profile registers live");
		(*anonymous)->resolve_profile({ .Context = session_context(enterprise), .Attempt = application::github_provider_attempt{ 1U } }, [&](auto aCompletion) { newCompletions.push_back(std::move(aCompletion)); });
		effects.run_next();
		effects.run_next();
		expect(require_one<application::github_profile_success>(oldCompletions, "old job completes").Authority == profile_authority_for(github)
				   && require_one<application::github_profile_success>(newCompletions, "new job completes").Authority == profile_authority_for(enterprise),
			   "registration preserves pending authority and exposes new authority to shared provider");
		(*provider)->shutdown();
		expect(catalog->register_profile(enterprise).has_value(), "one provider shutdown does not close shared catalog");
		catalog->shutdown();
		const auto closed = catalog->register_profile(github);
		expect(!closed && closed.error() == github_provider_configuration_failure::closing
				   && catalog->find_profile(github.profile()) == retained,
			   "closed catalog refuses registration while retained profile remains valid");
	}

	void test_multi_profile_routing_and_authority_rejection()
	{
		const auto github = test_profile(
			application::github_authentication_transport::native_https);
		const auto enterprise = test_enterprise_profile(
			application::github_authentication_transport::native_https);
		std::array<validated_github_host_profile, 2U> profiles{
			github,
			enterprise,
		};
		auto effects                = controlled_executor{ };
		auto transport              = std::make_shared<fake_wire_transport>();
		auto factory                = std::make_shared<fake_transport_factory>();
		factory->BootstrapTransport = transport;
		factory->AccountTransport   = transport;
		auto provider               = make_github_provider(profiles, effects, factory);
		expect(provider.has_value(), "multi-profile provider constructs");
		const auto replacement = test_enterprise_profile(
			application::github_authentication_transport::native_https,
			github.profile());
		profiles.front() = replacement;

		transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				return github_wire_success{
					.Correlation = aWire.Correlation,
					.Response    = viewer_response(),
				};
			};
		const auto verifyViewerRoute =
			[&](const validated_github_host_profile& aProfile,
				const server_contract                aServer) {
				const auto request     = viewer_request_for(aProfile);
				auto       completions = std::vector<application::github_viewer_completion>{ };
				(*provider)->observe_viewer(
					application::github_viewer_submission{
						.Request        = request,
						.Authentication = application::github_bootstrap_credential{
							.Credential = credential_material(),
						},
					},
					[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
				expect(completions.empty() && effects.pending() == 1U,
					   "selected viewer route is queued");
				effects.run_next();
				const auto& success = require_one<application::github_viewer_success>(
					completions, "selected viewer route succeeds once");
				const auto expected = compile_read_request(semantic_read_request{
					.Server            = aServer,
					.Operation         = read_operation::get_viewer_identity,
					.Owner             = { },
					.Repository        = { },
					.PullRequestNumber = 0U,
					.HeadOid           = { },
					.SearchTerms       = { },
					.Page              = 1U,
					.GraphqlPage       = std::nullopt,
				});
				expect(expected.has_value(), "selected viewer route compiles");
				const auto& wire = transport->ReadRequests.back();
				expect(
					success.Request == request && success.Viewer == viewer_record()
						&& factory->LastBootstrapProfile
							== std::optional{ aProfile.profile() }
						&& wire.Wire == *expected
						&& wire.Correlation.ProfileRevision == aProfile.revision()
						&& wire.Correlation.Host == aProfile.host(),
					"viewer job captures the exact selected immutable profile");
			};
		verifyViewerRoute(github, server_contract::github_com);
		verifyViewerRoute(enterprise, server_contract::ghes_3_18);

		const auto accountRequest = viewer_request_for(enterprise);
		const auto accountContext = credential_context_for(
			enterprise, accountRequest.Context, viewer_record().Account);
		auto accountCompletions = std::vector<application::github_viewer_completion>{ };
		(*provider)->observe_viewer(
			application::github_viewer_submission{
				.Request        = accountRequest,
				.Authentication = application::github_credential_retrieve_success{
					.Context    = accountContext,
					.Credential = credential_material(),
				},
			},
			[&accountCompletions](auto aCompletion) { accountCompletions.push_back(std::move(aCompletion)); });
		expect(accountCompletions.empty() && effects.pending() == 1U,
			   "enterprise account route is queued");
		effects.run_next();
		const auto& accountSuccess = require_one<application::github_viewer_success>(
			accountCompletions, "enterprise account route succeeds once");
		expect(
			accountSuccess.AccountTransport != nullptr
				&& factory->LastAccountProfile
					== std::optional{ enterprise.profile() },
			"account job captures the exact enterprise profile");
		const auto enterpriseLease = accountSuccess.AccountTransport;

		// [[BPR-0644]]. A stored credential whose /user answers another account
		// than the one it is bound to is refused as an authentication failure
		// that says why, and no account lease is issued for it.
		{
			const auto otherRequest = viewer_request_for(enterprise);
			const auto boundAccount = core::github_account_id{ viewer_record().Account.value() + 1U };
			auto       completions  = std::vector<application::github_viewer_completion>{ };
			(*provider)->observe_viewer(
				application::github_viewer_submission{
					.Request        = otherRequest,
					.Authentication = application::github_credential_retrieve_success{
						.Context    = credential_context_for(enterprise, otherRequest.Context, boundAccount),
						.Credential = credential_material(),
					},
				},
				[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
			effects.run_next();
			const auto& refused = require_one<application::github_viewer_error>(completions, "a viewer of another account is refused once");
			expect(refused.Error.Failure == application::github_provider_failure::authentication && refused.Error.AccountMismatch && application::valid(refused.Error),
				   "as an authentication failure that names the account mismatch");
			auto misfiled    = refused.Error;
			misfiled.Failure = application::github_provider_failure::network;
			expect(!application::valid(misfiled), "only an authentication failure may carry the account mismatch");
		}

		auto capabilityResponse = supported_capability_response();
		capabilityResponse.Body = read_file(
			capability_fixture_root() / "ghes-3.18/supported.json");
		transport->CapabilityHandler =
			[capabilityResponse](
				const github_capability_probe_wire_request& aWire,
				std::stop_token) {
				return github_capability_probe_wire_success{
					.Correlation = aWire.Correlation,
					.Response    = [&] { auto part = capability_part_response(capabilityResponse, aWire.Correlation.Part); if (aWire.Correlation.Part != 0U) { part.RateLimitHeaderCount = 0U; part.RateLimitHeaders = {}; } return part; }(),
				};
			};
		const auto capabilityRequest     = capability_request_for(enterprise, enterpriseLease);
		auto       capabilityCompletions = std::vector<application::github_capability_completion>{ };
		(*provider)->probe_capabilities(
			capabilityRequest,
			[&capabilityCompletions](auto aCompletion) { capabilityCompletions.push_back(std::move(aCompletion)); });
		expect(capabilityCompletions.empty() && effects.pending() == 1U,
			   "enterprise capability route is queued");
		effects.run_next();
		const auto& capabilitySuccess = require_one<application::github_capability_success>(
			capabilityCompletions,
			"enterprise capability route succeeds once");
		const auto expectedCapability = compile_capability_probe_request(enterprise, { }, capability_probe_part_count - 1U);
		expect(
			expectedCapability.has_value()
				&& capabilitySuccess.Request == capabilityRequest
				&& capabilitySuccess.RateLimits.Graphql.has_value()
				&& capabilitySuccess.RateLimits.Graphql->Remaining == 4999U
				&& transport->CapabilityRequests.back().Wire
					== *expectedCapability
				&& transport->CapabilityRequests.back().Correlation.Host
					== enterprise.host(),
			"capability job captures the exact enterprise profile");

		const auto pullRequest = pull_request_for(enterprise, enterpriseLease);
		transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				return github_wire_cancelled{
					.Correlation = aWire.Correlation,
				};
			};
		const auto readsBeforePull = transport->ReadRequests.size();
		auto       pullCompletions = std::vector<application::github_pull_request_completion>{ };
		(*provider)->fetch_pull_request(
			pullRequest,
			[&pullCompletions](auto aCompletion) { pullCompletions.push_back(std::move(aCompletion)); });
		expect(pullCompletions.empty() && effects.pending() == 1U,
			   "enterprise pull-request route is queued");
		effects.run_next();
		static_cast<void>(
			require_one<application::github_pull_request_cancelled>(
				pullCompletions,
				"enterprise pull-request transport cancellation is final"));
		const auto expectedRepository = compile_read_request(
			semantic_read_request{
				.Server            = server_contract::ghes_3_18,
				.Operation         = read_operation::get_repository,
				.Owner             = pullRequest.Context.Repository.Owner,
				.Repository        = pullRequest.Context.Repository.Name,
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = { },
				.Page              = 1U,
				.GraphqlPage       = std::nullopt,
			});
		expect(
			expectedRepository.has_value()
				&& transport->ReadRequests.size() == readsBeforePull + 1U
				&& transport->ReadRequests.back().Wire == *expectedRepository
				&& transport->ReadRequests.back().Correlation.Host
					== enterprise.host(),
			"pull-request job captures the exact enterprise profile");

		const auto effectsBeforeRejection = effects.PostCalls;
		const auto readsBeforeRejection   = transport->ReadRequests.size();
		const auto factoryBeforeRejection = factory->BootstrapCalls;
		auto       unknown                = viewer_request_for(github);
		unknown.Context.Profile           = core::host_profile_id{ 999U };
		unknown.Authority.Profile         = unknown.Context.Profile;
		auto completions                  = std::vector<application::github_viewer_completion>{ };
		(*provider)->observe_viewer(
			application::github_viewer_submission{
				.Request        = unknown,
				.Authentication = application::github_bootstrap_credential{
					.Credential = credential_material(),
				},
			},
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		const auto& unknownError = require_one<application::github_viewer_error>(
			completions, "unknown profile authority is rejected once");
		expect(
			unknownError.Error.Failure
					== application::github_provider_failure::unavailable
				&& effects.PostCalls == effectsBeforeRejection
				&& transport->ReadRequests.size() == readsBeforeRejection
				&& factory->BootstrapCalls == factoryBeforeRejection,
			"unknown profile authority cannot reach transport selection");

		auto mismatched                      = viewer_request_for(enterprise);
		mismatched.Authority.ProfileRevision = github.revision();
		completions.clear();
		(*provider)->observe_viewer(
			application::github_viewer_submission{
				.Request        = mismatched,
				.Authentication = application::github_bootstrap_credential{
					.Credential = credential_material(),
				},
			},
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		const auto& mismatchError = require_one<application::github_viewer_error>(
			completions, "mismatched profile authority is rejected once");
		expect(
			mismatchError.Error.Failure
					== application::github_provider_failure::unavailable
				&& effects.PostCalls == effectsBeforeRejection
				&& transport->ReadRequests.size() == readsBeforeRejection
				&& factory->BootstrapCalls == factoryBeforeRejection,
			"authority drift cannot cross-route to another configured profile");

		auto hostnameMismatch               = viewer_request_for(enterprise);
		hostnameMismatch.Authority.Hostname = application::github_canonical_hostname{ "ghe.other.invalid" };
		completions.clear();
		(*provider)->observe_viewer(
			application::github_viewer_submission{
				.Request        = hostnameMismatch,
				.Authentication = application::github_bootstrap_credential{
					.Credential = credential_material(),
				},
			},
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		const auto& hostnameError = require_one<application::github_viewer_error>(
			completions, "mismatched hostname authority is rejected once");
		expect(
			hostnameError.Error.Failure
					== application::github_provider_failure::unavailable
				&& effects.PostCalls == effectsBeforeRejection
				&& transport->ReadRequests.size() == readsBeforeRejection
				&& factory->BootstrapCalls == factoryBeforeRejection,
			"hostname authority drift cannot reach transport selection");
	}

	void test_multi_profile_cancellation_is_profile_scoped()
	{
		const auto github = test_profile(
			application::github_authentication_transport::native_https);
		const auto enterprise = test_enterprise_profile(
			application::github_authentication_transport::native_https);
		const std::array<validated_github_host_profile, 2U> profiles{
			github,
			enterprise,
		};
		auto effects  = controlled_executor{ };
		auto factory  = std::make_shared<fake_transport_factory>();
		auto provider = make_github_provider(profiles, effects, factory);
		expect(provider.has_value(), "cancellation multi-profile provider constructs");

		const auto githubRequest = application::github_profile_request{
			.Context = session_context(github),
			.Attempt = application::github_provider_attempt{ 1U },
		};
		const auto enterpriseRequest = application::github_profile_request{
			.Context = session_context(enterprise),
			.Attempt = application::github_provider_attempt{ 1U },
		};
		auto githubCompletions     = std::vector<application::github_profile_completion>{ };
		auto enterpriseCompletions = std::vector<application::github_profile_completion>{ };
		(*provider)->resolve_profile(
			githubRequest,
			[&githubCompletions](auto aCompletion) { githubCompletions.push_back(std::move(aCompletion)); });
		(*provider)->resolve_profile(
			enterpriseRequest,
			[&enterpriseCompletions](auto aCompletion) { enterpriseCompletions.push_back(std::move(aCompletion)); });
		expect(effects.pending() == 2U, "both profile jobs are queued");

		(*provider)->cancel_profile(githubRequest);
		effects.run_next();
		effects.run_next();
		const auto& cancelled = require_one<application::github_profile_cancelled>(
			githubCompletions, "selected profile cancellation is final once");
		const auto& success = require_one<application::github_profile_success>(
			enterpriseCompletions, "other profile remains routable");
		expect(
			cancelled.Request == githubRequest
				&& success.Request == enterpriseRequest
				&& success.Authority == profile_authority_for(enterprise)
				&& effects.pending() == 0U,
			"cancellation matches the complete profile request context");
	}

	void test_profile_resolution()
	{
		provider_fixture fixture{
			application::github_authentication_transport::native_https
		};
		const auto request = application::github_profile_request{
			.Context = session_context(fixture.Profile),
			.Attempt = application::github_provider_attempt{ 1U },
		};
		auto completions = std::vector<application::github_profile_completion>{ };
		fixture.Provider->resolve_profile(
			request,
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		expect(completions.empty() && fixture.Effects.pending() == 1U,
			   "profile resolution is queued");
		fixture.Effects.run_next();
		const auto& success = require_one<application::github_profile_success>(
			completions, "exact profile resolves once");
		expect(success.Request == request
				   && success.Authority == profile_authority_for(fixture.Profile),
			   "profile resolution returns exact immutable authority");

		auto mismatch            = request;
		mismatch.Context.Profile = core::host_profile_id{ 999U };
		completions.clear();
		fixture.Provider->resolve_profile(
			mismatch,
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		expect(fixture.Effects.pending() == 1U, "mismatch is still serialized");
		fixture.Effects.run_next();
		const auto& error = require_one<application::github_profile_error>(
			completions, "mismatched profile fails once");
		expect(error.Request == mismatch
				   && error.Error.Failure
					   == application::github_provider_failure::unavailable,
			   "profile mismatch cannot resolve another authority");
	}

	void test_bootstrap_viewer_has_no_account_lease()
	{
		provider_fixture fixture{
			application::github_authentication_transport::native_https
		};
		const auto request = viewer_request_for(fixture.Profile);
		fixture.Transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				return github_wire_success{
					.Correlation = aWire.Correlation,
					.Response    = viewer_response(),
				};
			};
		auto        completion = run_viewer(fixture,
											application::github_viewer_submission{
												.Request        = request,
												.Authentication = application::github_bootstrap_credential{
													.Credential = credential_material(),
												},
											});
		const auto* success    = std::get_if<application::github_viewer_success>(&completion);
		expect(success != nullptr && success->Request == request
				   && success->Viewer == viewer_record()
				   && success->AccountTransport == nullptr,
			   "bootstrap /user success cannot mint an account lease");
		expect(fixture.Factory->BootstrapCalls == 1U
				   && fixture.Factory->AccountCalls == 0U
				   && fixture.Factory->LastBootstrapProfile
					   == std::optional{ fixture.Profile.profile() }
				   && fixture.Factory->LastBootstrapContext
					   == std::optional{ request.Context }
				   && fixture.Factory->LastBootstrapCredentialBytes != 0U,
			   "bootstrap factory sees only the exact session submission");
		expect(fixture.Transport->ReadRequests.size() == 1U
				   && !fixture.Transport->ReadStopRequested.front(),
			   "bootstrap performs exactly one non-cancelled /user request");
		const auto expected = compile_read_request(semantic_read_request{
			.Server            = server_contract::github_com,
			.Operation         = read_operation::get_viewer_identity,
			.Owner             = { },
			.Repository        = { },
			.PullRequestNumber = 0U,
			.HeadOid           = { },
			.SearchTerms       = { },
			.Page              = 1U,
			.GraphqlPage       = std::nullopt,
		});
		expect(expected.has_value(), "canonical viewer request compiles");
		const auto& wire = fixture.Transport->ReadRequests.front();
		expect(wire.Wire == *expected && wire.Correlation == github_wire_correlation{
											 .Parent          = request.Context,
											 .ProfileRevision = request.Authority.ProfileRevision,
											 .Host            = request.Authority.Host,
											 .Operation       = read_operation::get_viewer_identity,
											 .Page            = 1U,
										 },
			   "bootstrap /user request is exact and typed");
	}

	void test_retrieved_account_must_match_observed_viewer()
	{
		provider_fixture fixture{
			application::github_authentication_transport::native_https
		};
		const auto request = viewer_request_for(fixture.Profile);
		const auto context = credential_context_for(
			fixture.Profile, request.Context, viewer_record().Account);
		fixture.Transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				auto response = viewer_response();
				response.Body = R"({"id":1002,"login":"other-user","name":"Other User","node_id":"U_other"})";
				return github_wire_success{
					.Correlation = aWire.Correlation,
					.Response    = std::move(response),
				};
			};
		auto        completion = run_viewer(fixture,
											application::github_viewer_submission{
												.Request        = request,
												.Authentication = application::github_credential_retrieve_success{
													.Context    = context,
													.Credential = credential_material(),
												},
											});
		const auto* error      = std::get_if<application::github_viewer_error>(&completion);
		expect(error != nullptr && error->Request == request
				   && error->Error.Failure
					   == application::github_provider_failure::authentication
				   && fixture.Factory->AccountCalls == 1U
				   && fixture.Transport->ReadRequests.size() == 1U,
			   "retrieved authority cannot lease a differently observed account");
	}

	void test_viewer_terminal_correlation()
	{
		provider_fixture fixture{
			application::github_authentication_transport::native_https
		};
		const auto request    = viewer_request_for(fixture.Profile);
		const auto submission = [&request] {
			return application::github_viewer_submission{
				.Request        = request,
				.Authentication = application::github_bootstrap_credential{
					.Credential = credential_material(),
				},
			};
		};

		fixture.Transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				auto correlation = aWire.Correlation;
				correlation.Host = core::git_host_id{ 999U };
				return github_wire_error{
					.Correlation = correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			};
		auto        completion = run_viewer(fixture, submission());
		const auto* wrongError = std::get_if<application::github_viewer_error>(&completion);
		expect(
			wrongError != nullptr
				&& wrongError->Error.Failure
					== application::github_provider_failure::malformed_response,
			"wrong viewer error correlation is rejected");

		fixture.Transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				auto correlation = aWire.Correlation;
				correlation.Page = 2U;
				return github_wire_cancelled{
					.Correlation = correlation,
				};
			};
		completion              = run_viewer(fixture, submission());
		const auto* wrongCancel = std::get_if<application::github_viewer_error>(&completion);
		expect(
			wrongCancel != nullptr
				&& wrongCancel->Error.Failure
					== application::github_provider_failure::malformed_response,
			"wrong viewer cancellation correlation is rejected");

		fixture.Transport->ReadHandler =
			[](const github_wire_request& aWire, std::stop_token) {
				return github_wire_cancelled{
					.Correlation = aWire.Correlation,
				};
			};
		completion = run_viewer(fixture, submission());
		expect(std::holds_alternative<application::github_viewer_cancelled>(
				   completion),
			   "exact viewer cancellation is terminal");
	}

	void verify_capability_terminal_contracts(
		provider_fixture&                             aFixture,
		const application::github_capability_request& aRequest)
	{
		auto requestDrift            = aRequest;
		requestDrift.Context.Request = core::request_id{ 999U };
		auto       rejected          = std::vector<application::github_capability_completion>{ };
		const auto postsBefore       = aFixture.Effects.PostCalls;
		aFixture.Provider->probe_capabilities(
			requestDrift,
			[&rejected](auto aCompletion) { rejected.push_back(std::move(aCompletion)); });
		const auto& bindingError = require_one<application::github_capability_error>(
			rejected, "capability lease request drift is rejected once");
		expect(bindingError.Error.Failure
					   == application::github_provider_failure::unavailable
				   && aFixture.Effects.PostCalls == postsBefore
				   && aFixture.Transport->CapabilityRequests.empty(),
			   "capability lease is bound to the exact credential request");

		const auto response = supported_capability_response();
		aFixture.Transport->CapabilityHandler =
			[response](const github_capability_probe_wire_request& aWire,
					   std::stop_token) {
				return github_capability_probe_wire_success{
					.Correlation = aWire.Correlation,
					.Response    = capability_part_response(response, aWire.Correlation.Part),
				};
			};
		auto        completion = run_capability(aFixture, aRequest);
		const auto* success    = std::get_if<application::github_capability_success>(&completion);
		expect(success != nullptr && success->Request == aRequest
				   && success->RateLimits.Graphql.has_value()
				   && success->RateLimits.Graphql->Limit == 5000U
				   && success->RateLimits.Graphql->Remaining == 4999U
				   && success->RateLimits.Graphql->ResetUnixSeconds == 2000000000U
				   && std::ranges::all_of(
					   success->Facts,
					   [](const auto& aFact) {
						   return aFact.State
							   == core::github_capability_state::supported;
					   })
				   && std::ranges::all_of(
					   success->SchemaFacts,
					   [](const auto& aFact) {
						   return aFact.State
							   == core::github_capability_state::supported;
					   }),
			   "exact capability success projects observed fixture evidence");
		expect(aFixture.Transport->CapabilityRequests.size() == capability_probe_part_count
				   && !aFixture.Transport->CapabilityStopRequested.front(),
			   "capability success performs nine serial transport calls");
		const auto expected = compile_capability_probe_request(aFixture.Profile);
		expect(expected.has_value(), "canonical provider probe compiles");
		expect(aFixture.Transport->CapabilityRequests.front() == github_capability_probe_wire_request{
				   .Correlation = {
					   .Session         = aRequest.Context,
					   .ProfileRevision = aRequest.ProfileAuthority.ProfileRevision,
					   .Host            = aRequest.ProfileAuthority.Host,
					   .ObservedAccount = aRequest.Viewer.Account,
				   },
				   .Wire = *expected,
			   },
			   "provider emits the exact capability wire request");

		auto structuredLimit   = response;
		structuredLimit.Status = 403;
		structuredLimit.Body   = R"({"errors":[{"extensions":{"code":"RATE_LIMITED"}}]})";
		aFixture.Transport->CapabilityHandler =
			[structuredLimit](const github_capability_probe_wire_request& aWire,
							  std::stop_token) {
				return github_capability_probe_wire_success{
					.Correlation = aWire.Correlation,
					.Response    = structuredLimit,
				};
			};
		completion          = run_capability(aFixture, aRequest);
		const auto* limited = std::get_if<application::github_capability_error>(&completion);
		expect(limited != nullptr
				   && limited->Error.Failure
					   == application::github_provider_failure::rate_limit
				   && limited->Error.RetryAfterSeconds
					   == std::optional<std::uint32_t>{ 60U }
				   && limited->Error.RateLimitCause.has_value()
				   && limited->Error.RateLimitCause->Resource
					   == application::github_rate_limit_resource::graphql
				   && limited->Error.RateLimitCause->SecondaryLimited
				   && limited->Error.RateLimits.Graphql
					   == limited->Error.RateLimitCause,
			   "structured capability throttling preserves exact GraphQL cause");

		auto completedHttpError                   = limited->Error;
		completedHttpError.HttpStatus             = 200U;
		completedHttpError.TransportFailureSource = application::diagnostic_transport_failure_source::http_response;
		expect(application::valid(completedHttpError), "completed HTTP 200 GraphQL throttle is a valid provider error");
		aFixture.Transport->CapabilityHandler =
			[completedHttpError](const github_capability_probe_wire_request& aWire, std::stop_token) {
				return github_capability_probe_wire_error{ .Correlation = aWire.Correlation, .Error = completedHttpError };
			};
		completion                 = run_capability(aFixture, aRequest);
		const auto* completedError = std::get_if<application::github_capability_error>(&completion);
		expect(completedError != nullptr && completedError->Error == completedHttpError,
			   "provider preserves completed HTTP 200 GraphQL rate error including source, retry and cause");
		completedHttpError.TransportFailureSource = application::diagnostic_transport_failure_source::none;
		expect(!application::valid(completedHttpError), "HTTP status without a response source is rejected by the provider contract");

		aFixture.Transport->CapabilityHandler =
			[](const github_capability_probe_wire_request& aWire, std::stop_token) {
				return github_capability_probe_wire_error{
					.Correlation = aWire.Correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			};
		completion             = run_capability(aFixture, aRequest);
		const auto* exactError = std::get_if<application::github_capability_error>(&completion);
		expect(exactError != nullptr
				   && exactError->Error.Failure
					   == application::github_provider_failure::permission,
			   "correctly correlated capability error is preserved");

		aFixture.Transport->CapabilityHandler =
			[](const github_capability_probe_wire_request& aWire, std::stop_token) {
				auto correlation = aWire.Correlation;
				correlation.Host = core::git_host_id{ 999U };
				return github_capability_probe_wire_error{
					.Correlation = correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			};
		completion             = run_capability(aFixture, aRequest);
		const auto* wrongError = std::get_if<application::github_capability_error>(&completion);
		expect(
			wrongError != nullptr
				&& wrongError->Error.Failure
					== application::github_provider_failure::malformed_response,
			"wrong capability error correlation is rejected");

		aFixture.Transport->CapabilityHandler =
			[](const github_capability_probe_wire_request& aWire, std::stop_token) {
				auto correlation            = aWire.Correlation;
				correlation.Session.Request = core::request_id{ 999U };
				return github_capability_probe_wire_cancelled{
					.Correlation = correlation,
				};
			};
		completion              = run_capability(aFixture, aRequest);
		const auto* wrongCancel = std::get_if<application::github_capability_error>(&completion);
		expect(
			wrongCancel != nullptr
				&& wrongCancel->Error.Failure
					== application::github_provider_failure::malformed_response,
			"wrong capability cancellation correlation is rejected");

		aFixture.Transport->CapabilityHandler =
			[](const github_capability_probe_wire_request& aWire, std::stop_token) {
				return github_capability_probe_wire_cancelled{
					.Correlation = aWire.Correlation,
				};
			};
		completion = run_capability(aFixture, aRequest);
		expect(std::holds_alternative<application::github_capability_cancelled>(
				   completion),
			   "exact capability cancellation is terminal");
	}

	void verify_fabricated_cache_rejection(
		provider_fixture&                            aFixture,
		const application::github_account_transport& aLease)
	{
		const auto readCallsBefore                                           = aFixture.Transport->ReadRequests.size();
		const auto postsBefore                                               = aFixture.Effects.PostCalls;
		auto       fabricatedCapabilities                                    = pull_request_for(aFixture, aLease);
		fabricatedCapabilities.CapabilityEvidence.Capabilities.ReviewThreads = core::github_capability_state::supported;
		auto pullCompletions                                                 = std::vector<application::github_pull_request_completion>{ };
		aFixture.Provider->fetch_pull_request(
			fabricatedCapabilities,
			[&pullCompletions](auto aValue) { pullCompletions.push_back(std::move(aValue)); });
		const auto& fabricatedCapabilityError = require_one<application::github_pull_request_error>(
			pullCompletions,
			"fabricated aggregate capability is rejected once");
		expect(fabricatedCapabilityError.Error.Failure
					   == application::github_provider_failure::unavailable
				   && aFixture.Effects.PostCalls == postsBefore
				   && aFixture.Transport->ReadRequests.size() == readCallsBefore,
			   "fabricated capability evidence causes no provider effect");

		auto fabricatedShape                                       = pull_request_for(aFixture, aLease);
		fabricatedShape.CapabilityEvidence.ReadShape.ReviewThreads = true;
		pullCompletions.clear();
		aFixture.Provider->fetch_pull_request(
			fabricatedShape,
			[&pullCompletions](auto aValue) { pullCompletions.push_back(std::move(aValue)); });
		const auto& fabricatedShapeError = require_one<application::github_pull_request_error>(
			pullCompletions, "fabricated read shape is rejected once");
		expect(fabricatedShapeError.Error.Failure
					   == application::github_provider_failure::unavailable
				   && aFixture.Effects.PostCalls == postsBefore
				   && aFixture.Transport->ReadRequests.size() == readCallsBefore,
			   "fabricated read shape causes no provider effect");
	}

	void verify_no_internal_retry(
		provider_fixture&                             aFixture,
		const application::github_capability_request& aRequest)
	{
		aFixture.Transport->CapabilityHandler =
			[](const github_capability_probe_wire_request& aWire, std::stop_token) {
				return github_capability_probe_wire_error{
					.Correlation = aWire.Correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			};
		const auto callsBeforeRetry = aFixture.Transport->CapabilityRequests.size();
		auto       firstAttempt     = aRequest;
		firstAttempt.Attempt        = application::github_provider_attempt{ 1U };
		auto        completion      = run_capability(aFixture, firstAttempt);
		const auto* firstError      = std::get_if<application::github_capability_error>(&completion);
		expect(firstError != nullptr
				   && firstError->Error.Failure
					   == application::github_provider_failure::network
				   && aFixture.Transport->CapabilityRequests.size()
					   == callsBeforeRetry + 1U
				   && aFixture.Effects.pending() == 0U,
			   "first submitted attempt performs exactly one wire call");

		auto secondAttempt      = aRequest;
		secondAttempt.Attempt   = application::github_provider_attempt{ 2U };
		completion              = run_capability(aFixture, secondAttempt);
		const auto* secondError = std::get_if<application::github_capability_error>(&completion);
		expect(secondError != nullptr
				   && secondError->Error.Failure
					   == application::github_provider_failure::network
				   && aFixture.Transport->CapabilityRequests.size()
					   == callsBeforeRetry + 2U
				   && aFixture.Effects.pending() == 0U,
			   "provider never retries beyond explicitly submitted attempts");
	}

	template <typename TTerminal>
	struct throwing_completion_copy
	{
		bool* Fail;
		int*  Terminals;
		throwing_completion_copy(bool& aFail, int& aTerminals)
			: Fail{ &aFail }
			, Terminals{ &aTerminals }
		{ }
		throwing_completion_copy(const throwing_completion_copy& aOther)
			: Fail{ aOther.Fail }
			, Terminals{ aOther.Terminals }
		{
			if (*Fail) {
				throw std::bad_alloc{ };
			}
		}
		void operator()(TTerminal) const { ++*Terminals; }
	};
	void test_logging_wrapper_failure_preserves_terminal()
	{
		const auto       log = std::make_shared<recording_log>();
		provider_fixture fixture{ application::github_authentication_transport::native_https, log };
		bool             fail      = false;
		int              terminals = 0;
		log->FailCompletionCopy    = &fail;
		application::github_viewer_completion_handler viewer{ throwing_completion_copy<application::github_viewer_completion>{ fail, terminals } };
		fixture.Provider->observe_viewer({ }, std::move(viewer));
		expect(terminals == 1 && fixture.Effects.pending() == 0U, "logging copy failure preserves original viewer refusal terminal");
		fail = false;
		application::github_capability_completion_handler capability{ throwing_completion_copy<application::github_capability_completion>{ fail, terminals } };
		fixture.Provider->probe_capabilities({ }, std::move(capability));
		expect(terminals == 2 && fixture.Effects.pending() == 0U, "logging copy failure preserves original capability refusal terminal");
	}
	auto test_sanitized_provider_logging(const bpr::tests::capability_error_content_canary* aCanary = nullptr) -> bool
	{
		const auto       log = std::make_shared<recording_log>();
		provider_fixture fixture{ application::github_authentication_transport::native_https, log };
		const auto       request             = capability_request_for(fixture, account_lease(fixture));
		fixture.Transport->CapabilityHandler = [aCanary](const github_capability_probe_wire_request& aWire, std::stop_token) {
			auto response      = supported_capability_response();
			response.Body      = aCanary ? aCanary->Body : R"({"errors":[{"message":"Unknown schema field newField","extensions":{"code":"UNFAMILIAR_SCHEMA_CODE"}}]})";
			const auto secret  = bpr::tests::capability_error_canary_credential;
			const auto summary = sanitize_capability_error(response.Body, std::as_bytes(std::span{ secret }));
			return github_capability_probe_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response), .Summary = summary };
		};
		const auto completion = run_capability(fixture, request);
		expect(std::holds_alternative<application::github_capability_error>(completion), "logging preserves capability failure");
		expect(log->Events.size() >= 5U, "viewer and capability outcomes logged");
		const auto& detail = log->Events[log->Events.size() - 2U];
		expect(detail.Operation == application::diagnostic_operation::capability && detail.Outcome == application::diagnostic_outcome::response_received && detail.HttpStatus == 200 && detail.Request == request.Context.Request.value(), "provider forwards sanitized detail with request and status");
		if (!aCanary) {
			expect(detail.Summary.text().find("Unknown schema field") != std::string_view::npos && detail.Summary.text().find("unclassified-code") != std::string_view::npos && detail.Summary.text().find("newField") == std::string_view::npos, "provider retains unknown-error explanation and explicit content omission");
		}
		expect(detail.Summary.text().find(bpr::tests::capability_error_canary_credential) == std::string_view::npos, "provider never records the active credential");
		expect(log->Events.back().Outcome == application::diagnostic_outcome::failed, "failure terminal remains explicit");
		return !aCanary || detail.Summary.text().find(aCanary->Marker) == std::string_view::npos;
	}
	void test_pull_request_attempt_diagnostic_propagation()
	{
		const auto       log = std::make_shared<recording_log>();
		provider_fixture fixture{ application::github_authentication_transport::native_https, log };
		const auto       request       = pull_request_for(fixture, account_lease(fixture));
		fixture.Transport->ReadHandler = [](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
			return github_wire_error{
				.Correlation = aWire.Correlation,
				.Error       = { .Failure = application::github_provider_failure::network },
			};
		};
		auto completions = std::vector<application::github_pull_request_completion>{ };
		fixture.Provider->fetch_pull_request(request, [&](auto aResult) { completions.push_back(std::move(aResult)); });
		fixture.Effects.run_next();
		const auto& error = require_one<application::github_pull_request_error>(completions, "network load fails once");
		expect(error.Error.Failure == application::github_provider_failure::network
				   && error.LastReadOperation == application::diagnostic_paged_operation::get_repository
				   && !error.ValidationError,
			   "provider terminal carries last attempted page despite absent HTTP response");
		expect(std::ranges::none_of(log->Events, [](const auto& aEvent) {
				   return aEvent.Operation == application::diagnostic_operation::pull_request
					   && aEvent.Outcome == application::diagnostic_outcome::response_received;
			   }),
			   "transport failure invents no response_received event");
		expect(std::ranges::any_of(log->Events, [](const auto& aEvent) {
				   return aEvent.Operation == application::diagnostic_operation::pull_request
					   && aEvent.Outcome == application::diagnostic_outcome::started
					   && aEvent.Detail == application::diagnostic_detail::verbose
					   && aEvent.PagedOperation == application::diagnostic_paged_operation::get_repository;
			   }),
			   "attempt retains bounded Verbose operation metadata without an HTTP response");
	}

	void test_actor_conflict_diagnostic_propagation()
	{
		for (const std::string_view login : { "OCTO-REVIEWER", "other-login" }) {
			const auto       log = std::make_shared<recording_log>();
			provider_fixture fixture{ application::github_authentication_transport::native_https, log };
			const auto       request       = pull_request_for(fixture, account_lease(fixture));
			fixture.Transport->ReadHandler = [login](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
				if (aWire.Wire.Operation == read_operation::get_merge_assessment
					|| aWire.Wire.Operation == read_operation::list_review_requests
					|| aWire.Wire.Operation == read_operation::list_requested_reviewers
					|| aWire.Wire.Operation == read_operation::list_branch_rules
					|| aWire.Wire.Operation == read_operation::get_branch_protection) {
					expect(validate_outbound_read_request(aWire.Wire).has_value(), "optional diagnostic fixture request is canonical");
					return github_wire_error{ .Correlation = aWire.Correlation,
											  .Error       = { .Failure = application::github_provider_failure::unsupported_capability } };
				}
				if (aWire.Wire.Operation == read_operation::list_commit_statuses) {
					auto response = viewer_response();
					response.Body = "[]";
					return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
				}
				const auto filename = [&]() -> std::string_view {
					switch (aWire.Wire.Operation) {
					case read_operation::get_repository:            return "repository";
					case read_operation::get_pull_request:          return "pull_request";
					case read_operation::list_pull_request_files:   return "files";
					case read_operation::list_pull_request_commits: return "commits";
					case read_operation::list_reviews:              return "reviews";
					case read_operation::list_issue_comments:       return "issue_comments";
					case read_operation::list_checks:               return "checks";
					case read_operation::search_pull_requests:      return "search";
					default:                                        fail("bounded provider fixture operation");
					}
				}();
				auto response = viewer_response();
				response.Body = read_file(capability_fixture_root().parent_path().parent_path() / "contracts/v1/github-com/responses" / (std::string{ filename } + ".json"));
				if (aWire.Wire.Operation == read_operation::search_pull_requests) {
					response.Headers.back().Value = "search";
				}
				if (aWire.Wire.Operation == read_operation::list_reviews) {
					for (const auto& [from, to] : std::array<std::pair<std::string_view, std::string_view>, 2U>{ { { "U_fixture_viewer", "U_viewer" }, { "octo-reviewer", login } } }) {
						const auto position = response.Body.find(from);
						expect(position != std::string::npos, "recorded review has synthetic actor fields");
						response.Body.replace(position, from.size(), to);
					}
				}
				return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
			};
			auto completions = std::vector<application::github_pull_request_completion>{ };
			fixture.Provider->fetch_pull_request(request, [&](auto aResult) { completions.push_back(std::move(aResult)); });
			fixture.Effects.run_next();
			const auto& error = require_one<application::github_pull_request_error>(completions, "actor conflict fails atomically once");
			expect(error.RelationshipDetail == application::github_read_validation_error::relationship_actor_login,
				   "provider preserves actor refusal");
			const auto detail = std::string{ "actor_conflict=viewer,review," } + (login == "OCTO-REVIEWER" ? "ascii_case_only" : "different");
			expect(std::ranges::count_if(log->Events, [&](const auto& aEvent) {
					   return aEvent.Summary.text() == detail && aEvent.PagedOperation == application::diagnostic_paged_operation::list_reviews
						   && aEvent.Outcome == application::diagnostic_outcome::failed;
				   }) == 1,
				   "provider records one fixed-origin conflict diagnostic without login values");
		}
	}

	/// [[BPR-1560]], [[BPR-1572]]. The provider says why a snapshot has no
	/// merge base: a read that failed (a window may keep the merge base it
	/// had) differs from answers proving there is no single one (the pull
	/// request moved between the snapshot and the read confirming it, or the
	/// base branch names another merge base while GitHub's own answers do not
	/// show it still compares from the recorded base), which a window must
	/// never override with an earlier answer. A base branch that only moved
	/// forward from the recorded base, as a stacked pull request's does when
	/// it takes some of the head's commits, leaves GitHub comparing from the
	/// recorded base's merge base, which is accepted.
	void test_merge_base_failure_and_refusal_are_distinct()
	{
		enum class course : std::uint8_t { agree,
										   tip_disagrees,
										   moved,
										   compare_fails,
										   stacked_forward,
										   stacked_rewritten,
										   stacked_count_differs,
										   stacked_ancestry_fails,
										   stacked_moved };
		const auto mergeBase = std::string(40, 'a');
		const auto otherBase = std::string(40, 'c');
		const auto branchTip = std::string(40, 'b');
		const auto recorded  = std::string(40, '1');
		for (const auto which : { course::agree, course::tip_disagrees, course::moved, course::compare_fails, course::stacked_forward, course::stacked_rewritten,
								  course::stacked_count_differs, course::stacked_ancestry_fails, course::stacked_moved }) {
			provider_fixture fixture{ application::github_authentication_transport::native_https };
			const auto       request      = pull_request_for(fixture, account_lease(fixture));
			auto             pullRequests = 0U;
			auto             compares     = 0U;
			auto             ancestries   = 0U;
			const auto       stacked      = which == course::stacked_forward || which == course::stacked_rewritten || which == course::stacked_count_differs
				|| which == course::stacked_ancestry_fails || which == course::stacked_moved;
			fixture.Transport->ReadHandler = [&](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
				if (aWire.Wire.Operation == read_operation::get_merge_base) {
					++compares;
					expect(validate_outbound_read_request(aWire.Wire).has_value(), "the compare request is canonical");
					auto       response = viewer_response();
					const auto byBranch = !aWire.Wire.Semantic.BranchRef.empty();
					// The recorded base against the branch tip GitHub resolved:
					// whether the base branch only moved forward from it.
					const auto ancestry = !byBranch && aWire.Wire.Semantic.HeadOid == branchTip;
					ancestries += ancestry ? 1U : 0U;
					if (which == course::compare_fails || (ancestry && which == course::stacked_ancestry_fails)) {
						response.Status = 500;
						response.Body   = R"({"message":"fixture compare unavailable"})";
						return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
					}
					const auto asked = byBranch ? aWire.Wire.Semantic.BranchRef : aWire.Wire.Semantic.BaseOid;
					auto       named = mergeBase;
					auto       ahead = std::string{ };
					if (byBranch && (which == course::tip_disagrees || stacked)) {
						named = otherBase;
						ahead = R"("ahead_by":1,)";
					} else if (ancestry) {
						named = which == course::stacked_rewritten ? std::string(40, 'e') : recorded;
						ahead = R"("ahead_by":2,)";
					} else if (stacked) {
						// The pull request reports one commit (pull_request.json).
						ahead = which == course::stacked_count_differs ? R"("ahead_by":2,)" : R"("ahead_by":1,)";
					}
					response.Body = R"({"url":"https://api.github.com/repos/)";
					response.Body.append(aWire.Wire.Semantic.Owner).append("/").append(aWire.Wire.Semantic.Repository).append("/compare/").append(asked).append("...");
					response.Body.append(aWire.Wire.Semantic.HeadOid).append(R"(","status":"ahead",)").append(ahead).append(R"("base_commit":{"sha":")");
					response.Body.append(byBranch ? branchTip : aWire.Wire.Semantic.BaseOid).append(R"("},"merge_base_commit":{"sha":")");
					response.Body.append(named).append(R"("},"commits":[],"files":[]})");
					return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
				}
				const auto filename = [&]() -> std::optional<std::string_view> {
					switch (aWire.Wire.Operation) {
					case read_operation::get_repository:            return "repository";
					case read_operation::get_pull_request:          return "pull_request";
					case read_operation::list_pull_request_files:   return "files";
					case read_operation::list_pull_request_commits: return "commits";
					case read_operation::list_reviews:              return "reviews";
					case read_operation::list_issue_comments:       return "issue_comments";
					case read_operation::list_checks:               return "checks";
					case read_operation::search_pull_requests:      return "search";
					default:                                        return std::nullopt;
					}
				}();
				if (!filename) {
					return github_wire_error{ .Correlation = aWire.Correlation, .Error = { .Failure = application::github_provider_failure::unsupported_capability } };
				}
				auto response = viewer_response();
				response.Body = read_file(capability_fixture_root().parent_path().parent_path() / "contracts/v1/github-com/responses" / (std::string{ *filename } + ".json"));
				if (aWire.Wire.Operation == read_operation::search_pull_requests) {
					response.Headers.back().Value = "search";
				}
				if (aWire.Wire.Operation == read_operation::get_pull_request && ++pullRequests == 2U && (which == course::moved || which == course::stacked_moved)) {
					const auto position = response.Body.find(recorded);
					expect(position != std::string::npos, "the recorded pull request names its base");
					response.Body.replace(position, 40U, std::string(40, 'd'));
				}
				return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
			};
			auto completions = std::vector<application::github_pull_request_completion>{ };
			fixture.Provider->fetch_pull_request(request, [&](auto aResult) { completions.push_back(std::move(aResult)); });
			fixture.Effects.run_next();
			const auto& success  = require_one<application::github_pull_request_success>(completions, "the read completes with or without a merge base");
			const auto& snapshot = success.Snapshot;
			const auto  refused  = snapshot.MergeBaseAvailability == application::github_merge_base_availability::refused && !snapshot.MergeBaseOid;
			switch (which) {
			case course::agree:
				expect(snapshot.MergeBaseAvailability == application::github_merge_base_availability::available && snapshot.MergeBaseOid
						   && snapshot.MergeBaseOid->value() == mergeBase && snapshot.MergeBaseTipOid && snapshot.MergeBaseTipOid->value() == branchTip
						   && compares == 2U && ancestries == 0U && pullRequests == 2U,
					   "the recorded base and the base branch name one merge base, and the pull request is unchanged");
				break;
			case course::tip_disagrees:
				expect(refused && compares == 2U && ancestries == 0U,
					   "the base branch naming another merge base, with no commit count from GitHub to say which it compares from, is a refusal");
				break;
			case course::moved:
				expect(refused && pullRequests == 2U, "the pull request moving under the read is a refusal, not a failure");
				break;
			case course::compare_fails:
				expect(snapshot.MergeBaseAvailability == application::github_merge_base_availability::failed && !snapshot.MergeBaseOid,
					   "a compare that fails is a failure a window may outlive");
				break;
			case course::stacked_forward:
				expect(snapshot.MergeBaseAvailability == application::github_merge_base_availability::available && snapshot.MergeBaseOid
						   && snapshot.MergeBaseOid->value() == mergeBase && snapshot.MergeBaseTipOid && snapshot.MergeBaseTipOid->value() == recorded
						   && compares == 3U && ancestries == 1U && pullRequests == 2U,
					   "a base branch that only moved forward from the recorded base leaves GitHub comparing from the recorded base's merge base");
				break;
			case course::stacked_rewritten:
				expect(refused && compares == 3U && ancestries == 1U,
					   "a base branch that no longer contains the recorded base is about to replace it: refused");
				break;
			case course::stacked_count_differs:
				expect(refused && compares == 2U && ancestries == 0U,
					   "GitHub's commit count for the pull request not being the recorded base's is a refusal, asked nothing more");
				break;
			case course::stacked_ancestry_fails:
				expect(snapshot.MergeBaseAvailability == application::github_merge_base_availability::failed && !snapshot.MergeBaseOid && ancestries == 1U,
					   "the ancestry read failing is a failure, not an acceptance");
				break;
			case course::stacked_moved:
				expect(refused && compares == 3U && pullRequests == 2U, "the pull request moving under a stacked read is a refusal");
				break;
			}
		}
	}

	void test_capability_refusal_detail_propagation()
	{
		provider_fixture fixture{ application::github_authentication_transport::native_https };
		const auto       request = capability_request_for(fixture, account_lease(fixture));
		using diagnostic         = application::github_capability_diagnostic;
		for (const auto detail : { diagnostic::transport_content_encoding, static_cast<diagnostic>(255U) }) {
			fixture.Transport->CapabilityHandler = [detail](const github_capability_probe_wire_request& aWire, std::stop_token) {
				return github_capability_probe_wire_error{
					.Correlation      = aWire.Correlation,
					.Error            = { .Failure = application::github_provider_failure::malformed_response },
					.CapabilityDetail = detail
				};
			};
			const auto  completion = run_capability(fixture, request);
			const auto* error      = std::get_if<application::github_capability_error>(&completion);
			expect(error != nullptr && error->Error.Failure == application::github_provider_failure::malformed_response
					   && error->CapabilityDetail == application::bounded_capability_diagnostic(detail),
				   "provider preserves only bounded capability detail without changing failure policy");
		}
		for (const auto detail : { diagnostic::graphql_code_forbidden, diagnostic::provider_rate_tuple, diagnostic::provider_retry_metadata, diagnostic::provider_correlation }) {
			fixture.Transport->CapabilityHandler = [detail](const github_capability_probe_wire_request& aWire, std::stop_token) {
				auto response    = supported_capability_response();
				auto correlation = aWire.Correlation;
				if (detail == diagnostic::graphql_code_forbidden) {
					response.Body = R"({"errors":[{"extensions":{"code":"FORBIDDEN"},"message":"fixture-secret-marker"}]})";
				}
				if (detail == diagnostic::provider_rate_tuple) {
					response.RateLimitHeaderCount = 0U;
				}
				if (detail == diagnostic::provider_retry_metadata) {
					response.RateLimitHeaders[4]  = { .Name = "Retry-After", .Value = "7" };
					response.RateLimitHeaderCount = 5U;
				}
				if (detail == diagnostic::provider_correlation) {
					correlation.Host = core::git_host_id{ 999U };
				}
				return github_capability_probe_wire_success{ .Correlation = correlation, .Response = std::move(response) };
			};
			const auto  completion = run_capability(fixture, request);
			const auto* error      = std::get_if<application::github_capability_error>(&completion);
			expect(error != nullptr && error->CapabilityDetail == detail
					   && error->Error.Failure == application::github_provider_failure::malformed_response,
				   "provider discriminates normalization and metadata or correlation refusal");
		}
	}

	void test_account_capability_cache_and_retry_contracts()
	{
		provider_fixture fixture{
			application::github_authentication_transport::native_https
		};
		const auto lease   = account_lease(fixture);
		const auto request = capability_request_for(fixture, lease);
		verify_capability_terminal_contracts(fixture, request);
		verify_fabricated_cache_rejection(fixture, lease);
		verify_no_internal_retry(fixture, request);
	}

	void test_capability_batch_failure_boundaries()
	{
		enum class failure { envelope,
							 part,
							 correlation,
							 cancelled,
							 exhausted,
							 retry_after };
		// This matrix changes only the failing part. Prepare the nine exact
		// canonical responses once; every attempt still receives its own copy.
		// This avoids repeated fixture I/O and splitting while preserving every
		// physical request, refusal and full retry in the matrix.
		const auto canonical = supported_capability_response();
		auto       parts     = std::array<capability_probe_response, 9U>{ };
		for (auto index = std::size_t{ 0U }; index < parts.size(); ++index) {
			parts[index] = capability_part_response(canonical, static_cast<std::uint8_t>(index));
		}
		for (const auto mode : { failure::envelope, failure::part, failure::correlation, failure::cancelled, failure::exhausted, failure::retry_after }) {
			for (const auto stopPart : std::array<std::uint8_t, 3>{ 0U, 4U, 8U }) {
				provider_fixture fixture{ application::github_authentication_transport::native_https };
				const auto       request             = capability_request_for(fixture, account_lease(fixture));
				std::uint8_t     next                = 0U;
				fixture.Transport->CapabilityHandler = [&](const github_capability_probe_wire_request& aWire, std::stop_token) {
					expect(aWire.Correlation.Part == next && aWire.Wire.Part == next && aWire.Correlation.Session == request.Context, "parts remain ordered in the exact logical request");
					++next;
					auto response    = parts.at(aWire.Correlation.Part);
					auto correlation = aWire.Correlation;
					if (aWire.Correlation.Part == stopPart) {
						switch (mode) {
						case failure::envelope:    response.Body = R"({"errors":[{"type":"FORBIDDEN"}]})"; break;
						case failure::part:        response = parts.at((stopPart + 1U) % parts.size()); break;
						case failure::correlation: correlation.Part = static_cast<std::uint8_t>((stopPart + 1U) % 9U); break;
						case failure::cancelled:   fixture.Provider->cancel_capabilities(request); break;
						case failure::exhausted:   response.RateLimitHeaders[1].Value = "0"; break;
						case failure::retry_after:
							response.RateLimitHeaders[4]  = { .Name = "Retry-After", .Value = "7" };
							response.RateLimitHeaderCount = 5U;
							break;
						}
					}
					return github_capability_probe_wire_success{ .Correlation = correlation, .Response = std::move(response) };
				};
				const auto result = run_capability(fixture, request);
				expect(next == stopPart + 1U, "first refused or cancelled part stops additional physical requests");
				if (mode == failure::cancelled) {
					expect(std::holds_alternative<application::github_capability_cancelled>(result), "mid-batch cancellation has one terminal without partial facts");
				} else if (mode == failure::exhausted && stopPart == 8U) {
					expect(std::holds_alternative<application::github_capability_success>(result), "last-part exhaustion permits already complete facts");
				} else {
					const auto* error = std::get_if<application::github_capability_error>(&result);
					expect(error != nullptr, "refused partial batch never promotes facts");
					if (mode == failure::exhausted) {
						expect(error->Error.Failure == application::github_provider_failure::rate_limit, "exhaustion uses existing rate-limit policy");
					}
				}
				next                                 = 0U;
				fixture.Transport->CapabilityHandler = [&](const github_capability_probe_wire_request& aWire, std::stop_token) {
					expect(aWire.Correlation.Part == next++, "fresh attempt restarts canonical plan without stale partials");
					return github_capability_probe_wire_success{ .Correlation = aWire.Correlation, .Response = parts.at(aWire.Correlation.Part) };
				};
				auto retry    = request;
				retry.Attempt = application::github_provider_attempt{ static_cast<std::uint8_t>(request.Attempt.value() + 1U) };
				expect(std::holds_alternative<application::github_capability_success>(run_capability(fixture, retry)) && next == 9U, "fresh full attempt succeeds after refusal");
			}
		}
	}

	void test_cancellation_finality()
	{
		provider_fixture fixture{
			application::github_authentication_transport::native_https
		};
		const auto lease       = account_lease(fixture);
		auto       request     = capability_request_for(fixture, lease);
		request.Attempt        = application::github_provider_attempt{ 3U };
		const auto callsBefore = fixture.Transport->CapabilityRequests.size();
		auto       completions = std::vector<application::github_capability_completion>{ };
		fixture.Provider->probe_capabilities(
			request,
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		fixture.Provider->cancel_capabilities(request);
		fixture.Provider->cancel_capabilities(request);
		fixture.Effects.run_next();
		const auto& cancelled = require_one<application::github_capability_cancelled>(
			completions, "pre-execution cancellation is final exactly once");
		fixture.Provider->cancel_capabilities(request);
		expect(cancelled.Request == request && completions.size() == 1U
				   && fixture.Transport->CapabilityRequests.size() == callsBefore
				   && fixture.Effects.pending() == 0U,
			   "cancelled work cannot perform transport effects or redeliver");

		auto lateRequest    = request;
		lateRequest.Attempt = application::github_provider_attempt{ 2U };
		bool stopObserved   = false;
		fixture.Transport->CapabilityHandler =
			[&fixture, lateRequest, &stopObserved](
				const github_capability_probe_wire_request& aWire,
				const std::stop_token                       aStop) {
				fixture.Provider->cancel_capabilities(lateRequest);
				stopObserved = aStop.stop_requested();
				return github_capability_probe_wire_error{
					.Correlation = aWire.Correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			};
		auto completion = run_capability(fixture, lateRequest);
		expect(
			stopObserved
				&& std::holds_alternative<application::github_capability_cancelled>(
					completion),
			"cancellation won during transport remains the only terminal");
	}

	void test_exception_containment_is_exactly_once()
	{
		{
			provider_fixture fixture{
				application::github_authentication_transport::native_https
			};
			fixture.Effects.ThrowOnPost       = true;
			fixture.Effects.RetainTaskOnThrow = true;
			const auto request                = application::github_profile_request{
				.Context = session_context(fixture.Profile),
				.Attempt = application::github_provider_attempt{ 1U },
			};
			auto completions = std::vector<application::github_profile_completion>{ };
			fixture.Provider->resolve_profile(
				request,
				[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
			const auto error = require_one<application::github_profile_error>(
				completions, "post throw is contained exactly once");
			expect(fixture.Effects.pending() == 1U,
				   "throwing post retained its worker task");
			fixture.Effects.run_next();
			fixture.Provider->cancel_profile(request);
			fixture.Provider->shutdown();
			expect(
				error.Request == request
					&& error.Error.Failure
						== application::github_provider_failure::response_limit
					&& fixture.Effects.PostCalls == 1U
					&& fixture.Effects.pending() == 0U && completions.size() == 1U,
				"post failure preserves the original completion delivery");
		}

		{
			provider_fixture fixture{
				application::github_authentication_transport::native_https
			};
			fixture.Effects.ThrowOnPost       = true;
			fixture.Effects.RetainTaskOnThrow = true;
			const auto request                = viewer_request_for(fixture.Profile);
			auto       completions            = std::vector<application::github_viewer_completion>{ };
			fixture.Provider->observe_viewer(
				application::github_viewer_submission{
					.Request        = request,
					.Authentication = application::github_bootstrap_credential{
						.Credential = credential_material(),
					},
				},
				[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
			const auto& error = require_one<application::github_viewer_error>(
				completions, "viewer post throw is contained exactly once");
			expect(
				fixture.Effects.pending() == 1U
					&& fixture.Factory->BootstrapCalls == 0U
					&& fixture.Factory->AccountCalls == 0U
					&& fixture.Transport->ReadRequests.empty(),
				"retained viewer task is stopped before any credential transport");
			fixture.Effects.run_next();
			expect(
				error.Request == request
					&& error.Error.Failure
						== application::github_provider_failure::response_limit
					&& fixture.Effects.pending() == 0U && completions.size() == 1U
					&& fixture.Factory->BootstrapCalls == 0U
					&& fixture.Transport->ReadRequests.empty(),
				"retained viewer task cannot perform effects or redeliver");
		}

		{
			provider_fixture fixture{
				application::github_authentication_transport::native_https
			};
			fixture.Transport->ReadHandler =
				[](const github_wire_request&,
				   std::stop_token) -> github_wire_completion { throw std::runtime_error{ "worker failure" }; };
			const auto request     = viewer_request_for(fixture.Profile);
			auto       completions = std::vector<application::github_viewer_completion>{ };
			fixture.Provider->observe_viewer(
				application::github_viewer_submission{
					.Request        = request,
					.Authentication = application::github_bootstrap_credential{
						.Credential = credential_material(),
					},
				},
				[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
			expect(completions.empty() && fixture.Effects.pending() == 1U,
				   "throwing worker is queued");
			bool escaped = false;
			try {
				fixture.Effects.run_next();
			} catch (...) {
				escaped = true;
			}
			const auto& error = require_one<application::github_viewer_error>(
				completions, "worker throw is contained exactly once");
			fixture.Provider->cancel_viewer(request);
			fixture.Provider->shutdown();
			expect(
				!escaped && error.Request == request
					&& error.Error.Failure
						== application::github_provider_failure::response_limit
					&& fixture.Transport->ReadRequests.size() == 1U
					&& fixture.Effects.pending() == 0U && completions.size() == 1U,
				"worker exception cannot escape or redeliver");
		}
	}

	void test_cli_observed_account_lease()
	{
		for (const bool postflight : { false, true }) {
			provider_fixture fixture{ application::github_authentication_transport::github_cli };
			fixture.Factory->CliEnabled    = true;
			bool changed                   = false;
			fixture.Transport->ReadHandler = [&](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
				auto response = viewer_response();
				if (changed) {
					response.Body = R"({"id":1002,"login":"other","name":"Other User","node_id":"U_other"})";
				}
				return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
			};
			auto        viewer  = run_viewer(fixture, { .Request = viewer_request_for(fixture.Profile), .Authentication = std::monostate{ } });
			const auto* success = std::get_if<application::github_viewer_success>(&viewer);
			expect(success != nullptr && success->AccountTransport != nullptr && fixture.Factory->CliCalls == 1U
					   && fixture.Factory->AccountCalls == 0U && fixture.Factory->BootstrapCalls == 0U,
				   "CLI viewer creates a credential-free observed lease");
			fixture.Transport->CapabilityHandler = [&](const github_capability_probe_wire_request& aWire, std::stop_token) {
				return github_capability_probe_wire_success{ .Correlation = aWire.Correlation, .Response = capability_part_response(supported_capability_response(), aWire.Correlation.Part) };
			};
			const auto request = capability_request_for(fixture, success->AccountTransport);
			expect(std::holds_alternative<application::github_capability_success>(run_capability(fixture, request)), "CLI observed lease completes capability reads");
			expect(fixture.Transport->ReadRequests.size() == 19U, "each of nine capability requests has account preflight and postflight");
			const auto readRequest         = pull_request_for(fixture, success->AccountTransport);
			const auto viewerHandler       = fixture.Transport->ReadHandler;
			const auto viewerCorrelation   = fixture.Transport->ReadRequests.front().Correlation;
			fixture.Transport->ReadHandler = [&](const github_wire_request& aWire, std::stop_token aStop) -> github_wire_completion {
				expect(validate_github_wire_request(fixture.Profile, aWire).has_value(), "CLI PR read and private viewer checks pass production wire validation");
				if (aWire.Correlation.Operation == read_operation::get_viewer_identity) {
					expect(aWire.Correlation == viewerCorrelation, "private viewer check retains original session correlation");
					return viewerHandler(aWire, aStop);
				}
				if (aWire.Correlation.Operation == read_operation::get_repository) {
					auto response = viewer_response();
					response.Body = R"({"id":2001,"node_id":"R_repository","name":"review-repository","full_name":"octo-org/review-repository","owner":{"id":1001,"login":"octo-org","node_id":"O_organization"},"default_branch":"main","private":false,"visibility":"public","html_url":"https://github.com/octo-org/review-repository","archive_url":"https://api.github.com/repos/octo-org/review-repository/{archive_format}{/ref}"})";
					return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
				}
				return github_wire_cancelled{ .Correlation = aWire.Correlation };
			};
			std::vector<application::github_pull_request_completion> reads;
			std::vector<application::github_read_progress>           progress;
			fixture.Provider->fetch_pull_request_with_progress(readRequest, [&](auto aResult) { reads.push_back(std::move(aResult)); }, [&](const auto& aContext, const auto aProgress) {
					expect(aContext == readRequest.Context && reads.empty(), "progress retains exact read identity before its terminal");
					progress.push_back(aProgress);
					throw std::runtime_error{"observational subscriber failure"}; });
			fixture.Effects.run_next();
			static_cast<void>(require_one<application::github_pull_request_cancelled>(reads, "CLI PR read accepts repository success then reaches next read"));
			expect(progress.size() == 3U && progress[0] == application::github_read_progress{ .Phase = application::github_read_phase::metadata, .PagesReceived = 0U, .Sequence = 1U }
					   && progress[1].PagesReceived == 1U && progress[1].Sequence == 2U
					   && progress[2].PagesReceived == 1U && progress[2].Sequence == 3U,
				   "provider emits monotone start/received progress without counting private viewer preflights or altering completion");
			expect(fixture.Transport->ReadRequests.size() == 24U, "repository read has preflight and postflight before next read preflight and dispatch");
			fixture.Transport->ReadHandler       = viewerHandler;
			const auto before                    = fixture.Transport->CapabilityRequests.size();
			changed                              = !postflight;
			fixture.Transport->CapabilityHandler = [&](const github_capability_probe_wire_request& aWire, std::stop_token) {
				changed = true;
				return github_capability_probe_wire_success{ .Correlation = aWire.Correlation, .Response = capability_part_response(supported_capability_response(), aWire.Correlation.Part) };
			};
			const auto  terminal = run_capability(fixture, request);
			const auto* error    = std::get_if<application::github_capability_error>(&terminal);
			expect(error != nullptr && error->Error.Failure == application::github_provider_failure::authentication, "observed preflight or postflight mismatch is typed authentication refusal");
			expect(fixture.Transport->CapabilityRequests.size() == before + (postflight ? 1U : 0U), "preflight refuses dispatch and postflight discards partial facts");
			changed          = false;
			const auto calls = fixture.Transport->ReadRequests.size();
			static_cast<void>(run_capability(fixture, request));
			expect(fixture.Transport->ReadRequests.size() == calls, "mismatched lease remains refused without further requests");
		}
	}

	void test_cli_git_lease_forwarding()
	{
		for (const bool postflight : { false, true }) {
			provider_fixture fixture{ application::github_authentication_transport::github_cli };
			fixture.Factory->CliEnabled    = true;
			bool changed                   = false;
			fixture.Transport->ReadHandler = [&](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
				auto response = viewer_response();
				if (changed) {
					response.Body = R"({"id":1001,"login":"renamed","name":"Changed","node_id":"U_other"})";
				}
				return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
			};
			auto  viewer  = run_viewer(fixture, { .Request = viewer_request_for(fixture.Profile), .Authentication = std::monostate{ } });
			auto& success = std::get<application::github_viewer_success>(viewer);
			auto* factory = dynamic_cast<application::git_smart_transport_factory_port*>(success.AccountTransport.get());
			expect(factory != nullptr, "CLI observed account forwards Git factory");
			expect(!factory->supports_sparse_commit_trees(), "account wrappers do not invent sparse capability");
			fixture.Transport->Sparse = true;
			expect(factory->supports_sparse_commit_trees(), "both account wrappers preserve the selected CLI sparse capability");
			application::git_smart_request_context context{ };
			context.Authority.Account = core::github_account_id{ 1001 };
			context.AcquisitionMode   = application::git_smart_acquisition_mode::sparse_commit_trees;
			const auto initial        = fixture.Transport->ReadRequests.size();
			auto       git            = factory->acquire(context);
			expect(git.has_value() && fixture.Transport->ReadRequests.size() == initial, "Git acquisition is side-effect free");
			expect(fixture.Transport->GitContext == context, "account factory forwards the complete sparse request unchanged");
			expect((*git)->discover({ context }, { }).has_value() && fixture.Transport->ReadRequests.size() == initial + 2, "Git lease performs pre/post account checks");
			expect((*git)->ls_refs({ context, { } }, { }).has_value() && fixture.Transport->ReadRequests.size() == initial + 4, "refs round retains account checks");
			const application::git_smart_exact_blob_request blob{
				.Context      = context,
				.ExpectedBlob = *core::parse_git_blob_oid("1111111111111111111111111111111111111111"),
				.MaximumBytes = 17U,
			};
			const auto bytes = (*git)->read_exact_blob(blob, { });
			expect(bytes && *bytes == *fixture.Transport->Git->BlobResult
					   && fixture.Transport->ReadRequests.size() == initial + 6,
				   "selected blob bytes traverse the account lease with pre/post identity checks");
			const auto& forwarded = fixture.Transport->Git->BlobRequest;
			expect(forwarded && forwarded->Context == blob.Context && forwarded->ExpectedBlob == blob.ExpectedBlob
					   && forwarded->MaximumBytes == blob.MaximumBytes,
				   "exact blob identity and byte bound survive the wrapper unchanged");
			changed                      = !postflight;
			fixture.Transport->Git->Hook = [&] { changed = true; };
			const auto dispatched        = fixture.Transport->Git->Calls;
			auto       denied            = (*git)->read_exact_blob(blob, { });
			expect(!denied && denied.error().Failure == application::git_smart_transport_failure::authentication && (*git)->revoked(), "pre/post login drift revokes Git lease");
			expect(fixture.Transport->Git->Calls == dispatched + (postflight ? 1U : 0U), "preflight forbids dispatch and postflight discards result");
			expect(!factory->supports_sparse_commit_trees(), "observed account mismatch revokes sparse capability");
			success.AccountTransport.reset();
			changed                 = false;
			const auto observations = fixture.Transport->ReadRequests.size();
			expect(!(*git)->discover({ context }, { }).has_value() && fixture.Transport->ReadRequests.size() == observations, "revoked Git lease cannot revive after account returns");
			expect(!(*git)->read_exact_blob(blob, { }) && fixture.Transport->ReadRequests.size() == observations,
				   "revoked exact blob lease cannot revive after account returns");
		}
	}

	/// [[BPR-1370]]. A text blob read through the CLI account lease is one
	/// GraphQL read whose answer names its account; the lease holds that
	/// account to its own exactly as it holds an observation, and only a blob
	/// GraphQL cannot carry takes the observed REST read.
	void test_cli_text_blob_reads_name_their_account()
	{
		using failure = application::git_smart_transport_failure;
		struct lease
		{
			lease()
				: Fixture{ application::github_authentication_transport::github_cli }
			{
				Fixture.Factory->CliEnabled    = true;
				Fixture.Transport->Git         = Git;
				Fixture.Transport->ReadHandler = [](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
					return github_wire_success{ .Correlation = aWire.Correlation, .Response = viewer_response() };
				};
				auto viewer               = run_viewer(Fixture, { .Request = viewer_request_for(Fixture.Profile), .Authentication = std::monostate{ } });
				Account                   = std::get<application::github_viewer_success>(viewer).AccountTransport;
				Factory                   = dynamic_cast<application::git_smart_transport_factory_port*>(Account.get());
				Context.Authority.Account = core::github_account_id{ 1001 };
				Fixture.Transport->Sparse = true;
				auto acquired             = Factory->acquire(Context);
				expect(acquired.has_value(), "the CLI lease acquires a viewer-naming Git transport");
				Transport = *acquired;
			}
			[[nodiscard]] auto observations() const -> std::size_t { return Fixture.Transport->ReadRequests.size(); }
			[[nodiscard]] auto blob(const std::size_t aMaximum = 17U) const -> application::git_smart_exact_blob_request
			{
				return { .Context = Context, .ExpectedBlob = *core::parse_git_blob_oid("1111111111111111111111111111111111111111"), .MaximumBytes = aMaximum };
			}
			provider_fixture                               Fixture;
			std::shared_ptr<viewer_naming_git_transport>   Git = std::make_shared<viewer_naming_git_transport>();
			application::github_account_transport          Account;
			application::git_smart_transport_factory_port* Factory = nullptr;
			application::git_smart_request_context         Context{ };
			application::git_smart_transport               Transport;
		};
		{
			lease      text;
			const auto before = text.observations();
			const auto bytes  = text.Transport->read_exact_blob(text.blob(), { });
			expect(bytes && *bytes == std::vector{ std::byte{ 't' } } && text.Git->NamedCalls == 1U && text.Git->Calls == 0U,
				   "a text blob is read once through the read that names its account, not the REST read");
			expect(text.observations() == before, "a text blob read makes no separate account observation (two before)");
			expect(text.Git->NamedRequest && text.Git->NamedRequest->Context == text.Context && text.Git->NamedRequest->ExpectedBlobs.size() == 1U
					   && text.Git->NamedRequest->ExpectedBlobs.front() == text.blob().ExpectedBlob && text.Git->NamedRequest->MaximumBytes == 17U,
				   "the text read keeps the exact context, blob identity and byte bound");
			static_cast<void>(text.Transport->read_exact_blob(text.blob(application::maximum_git_exact_blob_bytes), { }));
			expect(text.Git->NamedRequest->MaximumBytes == application::maximum_git_exact_blob_batch_bytes,
				   "a text read asks GraphQL for no more than a batch blob; a larger one takes the REST read");
			const application::git_smart_exact_blobs_request batch{ .Context = text.Context, .ExpectedBlobs = { text.blob().ExpectedBlob, text.blob().ExpectedBlob }, .MaximumBytes = 64U };
			const auto                                       read = text.Transport->read_exact_blobs(batch, { });
			expect(read && read->size() == 2U && (*read)[0] && text.Git->NamedCalls == 3U && text.observations() == before,
				   "a batch is one read that names its account, with no observation pair around it");

			// A blob GraphQL cannot carry, or a GraphQL failure, takes the observed REST read.
			text.Git->NamedBlob = std::nullopt;
			const auto binary   = text.Transport->read_exact_blob(text.blob(), { });
			expect(binary && *binary == std::vector{ std::byte{ 'x' } } && text.Git->Calls == 1U && text.observations() == before + 2U,
				   "a binary, truncated or oversized blob falls back to the REST read between the lease's observations");
			text.Git->NamedBlob    = std::vector{ std::byte{ 't' } };
			text.Git->NamedFailure = std::unexpected{ application::git_smart_transport_error{ failure::unavailable } };
			const auto unavailable = text.Transport->read_exact_blob(text.blob(), { });
			expect(unavailable && text.Git->Calls == 2U && text.observations() == before + 4U, "a failed GraphQL read falls back to the observed REST read");
			const auto batchFailure = text.Transport->read_exact_blobs(batch, { });
			expect(!batchFailure && batchFailure.error().Failure == failure::unavailable && text.observations() == before + 4U,
				   "a failed batch is refused without falling back; preparation reads none ahead");
			text.Git->NamedFailure = std::unexpected{ application::git_smart_transport_error{ failure::cancelled } };
			const auto cancelled   = text.Transport->read_exact_blob(text.blob(), { });
			expect(!cancelled && cancelled.error().Failure == failure::cancelled && text.Git->Calls == 2U && text.observations() == before + 4U,
				   "a cancelled text read does not fall back");
			text.Git->NamedFailure = { };
			std::stop_source stop;
			text.Git->NamedHook = [&] { stop.request_stop(); };
			const auto stopped  = text.Transport->read_exact_blob(text.blob(), stop.get_token());
			text.Git->NamedHook = nullptr;
			expect(!stopped && stopped.error().Failure == failure::cancelled && text.Git->Calls == 2U, "a text read stopped while in flight returns nothing");
			stop.request_stop();
			const auto calls = text.Git->NamedCalls;
			expect(!text.Transport->read_exact_blob(text.blob(), stop.get_token()) && text.Git->NamedCalls == calls, "a stopped request reads nothing");
		}
		for (const auto& [account, login] : { std::pair{ 1001U, "renamed" }, std::pair{ 1002U, "octo-reviewer" } }) {
			lease mismatch;
			mismatch.Git->Viewer = github_answering_account{ .Account = core::github_account_id{ account }, .Login = login };
			const auto before    = mismatch.observations();
			const auto refused   = mismatch.Transport->read_exact_blob(mismatch.blob(), { });
			expect(!refused && refused.error().Failure == failure::authentication && mismatch.Transport->revoked() && mismatch.Git->Calls == 0U,
				   "a text read whose answer names another account or login is refused, revoked and not retried over REST");
			expect(!mismatch.Factory->supports_sparse_commit_trees() && !mismatch.Factory->acquire(mismatch.Context).has_value(),
				   "the answer's mismatch refuses the whole account lease, as an observed mismatch does");
			mismatch.Git->Viewer = github_answering_account{ .Account = core::github_account_id{ 1001U }, .Login = "octo-reviewer" };
			const auto calls     = mismatch.Git->NamedCalls;
			expect(!mismatch.Transport->read_exact_blob(mismatch.blob(), { }) && !mismatch.Transport->read_exact_blobs({ .Context = mismatch.Context, .ExpectedBlobs = { mismatch.blob().ExpectedBlob }, .MaximumBytes = 8U }, { })
					   && mismatch.Git->NamedCalls == calls && mismatch.Git->Calls == 0U && mismatch.observations() == before,
				   "the refused lease reads nothing more after the account returns");
		}
		{
			lease batch;
			batch.Git->Viewer = github_answering_account{ .Account = core::github_account_id{ 1002U }, .Login = "other" };
			const auto read   = batch.Transport->read_exact_blobs({ .Context = batch.Context, .ExpectedBlobs = { batch.blob().ExpectedBlob }, .MaximumBytes = 8U }, { });
			expect(!read && read.error().Failure == failure::authentication && batch.Transport->revoked(), "a batch read by another account is refused and revokes the lease");
		}
	}

	void test_cli_exact_blob_cancellation()
	{
		for (const bool duringRead : { false, true }) {
			provider_fixture fixture{ application::github_authentication_transport::github_cli };
			fixture.Factory->CliEnabled    = true;
			fixture.Transport->ReadHandler = [](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
				return github_wire_success{ .Correlation = aWire.Correlation, .Response = viewer_response() };
			};
			auto                                   viewer  = run_viewer(fixture, { .Request = viewer_request_for(fixture.Profile), .Authentication = std::monostate{ } });
			auto&                                  success = std::get<application::github_viewer_success>(viewer);
			auto*                                  factory = dynamic_cast<application::git_smart_transport_factory_port*>(success.AccountTransport.get());
			application::git_smart_request_context context{ };
			context.Authority.Account = core::github_account_id{ 1001 };
			auto git                  = factory->acquire(context);
			expect(git.has_value(), "CLI exact blob lease is available");
			success.AccountTransport.reset();
			const application::git_smart_exact_blob_request blob{ .Context = context };
			fixture.Transport->Git->BlobResult = std::unexpected{ application::git_smart_transport_error{ application::git_smart_transport_failure::response_limit } };
			const auto limited                 = (*git)->read_exact_blob(blob, { });
			expect(!limited && limited.error().Failure == application::git_smart_transport_failure::response_limit,
				   "exact blob failure survives account wrapper without partial bytes");
			fixture.Transport->Git->BlobResult = std::vector{ std::byte{ 'x' } };
			std::stop_source stop;
			if (duringRead) {
				fixture.Transport->Git->Hook = [&] { stop.request_stop(); };
			} else {
				stop.request_stop();
			}
			const auto calls        = fixture.Transport->Git->Calls;
			const auto observations = fixture.Transport->ReadRequests.size();
			const auto cancelled    = (*git)->read_exact_blob(blob, stop.get_token());
			expect(!cancelled && cancelled.error().Failure == application::git_smart_transport_failure::cancelled,
				   "cancelled exact blob cannot publish bytes");
			expect(fixture.Transport->Git->Calls == calls + (duringRead ? 1U : 0U)
					   && fixture.Transport->ReadRequests.size() == observations + (duringRead ? 1U : 0U),
				   "cancellation prevents dispatch or suppresses postflight work");
		}
	}
	void test_cli_mutation_lease_account_checks()
	{
		for (bool postflight : { false, true }) {
			provider_fixture fixture{ application::github_authentication_transport::github_cli };
			fixture.Factory->CliEnabled    = true;
			bool changed                   = false;
			fixture.Transport->ReadHandler = [&](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
				auto response = viewer_response();
				if (changed) {
					response.Body = R"({"id":1002,"login":"other","name":"Other User","node_id":"U_other"})";
				}
				return github_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response) };
			};
			auto  viewer  = run_viewer(fixture, { .Request = viewer_request_for(fixture.Profile), .Authentication = std::monostate{ } });
			auto& success = std::get<application::github_viewer_success>(viewer);
			auto* factory = dynamic_cast<application::review_mutation_transport_factory_port*>(success.AccountTransport.get());
			expect(factory != nullptr, "CLI account exposes mutation factory");
			auto lease = std::dynamic_pointer_cast<github_review_mutation_transport_lease>(factory->acquire_review_mutation_transport());
			expect(lease && lease->mutation_transport(), "CLI lease exposes observed mutation transport");
			const application::review_mutation_acting_identity identity{ .Account = core::github_account_id{ 1001 }, .DatabaseId = 1001, .NodeId = "U_viewer", .Login = "viewer" };
			const github_mutation_wire_request                 request{ .Context = lease->credential_context(), .Wire = *compile_viewer_observation_request(identity) };
			const auto                                         before = fixture.Transport->ReadRequests.size();
			expect(std::holds_alternative<github_mutation_wire_success>(lease->mutation_transport()->perform_review_mutation(request, { }, [] { return true; })) && fixture.Transport->ReadRequests.size() == before + 2, "mutation dispatch has pre/post account checks");
			changed                         = !postflight;
			fixture.Transport->MutationHook = [&] { changed = true; };
			const auto  calls               = fixture.Transport->MutationCalls;
			const auto  result              = lease->mutation_transport()->perform_review_mutation(request, { }, [] { return true; });
			const auto* error               = std::get_if<github_mutation_wire_error>(&result);
			expect(error && error->Transmission == github_mutation_transmission::definitely_not_sent && fixture.Transport->MutationCalls == calls + (postflight ? 1U : 0U), "account mismatch refuses preflight or discards postflight GET without claiming transmission");
			changed          = false;
			const auto after = fixture.Transport->MutationCalls;
			static_cast<void>(lease->mutation_transport()->perform_review_mutation(request, { }, [] { return true; }));
			expect(fixture.Transport->MutationCalls == after, "mismatched lease cannot publish again after account returns");
		}
	}

	/// [[BPR-1177]]. A native account lease mints the publication catalog over
	/// its own wire, under exactly its own credential context, and that
	/// catalog refuses another account before a request exists.
	void test_account_lease_mints_publication_catalog()
	{
		provider_fixture fixture{ application::github_authentication_transport::native_https };
		const auto       lease   = account_lease(fixture);
		auto* const      factory = dynamic_cast<application::github_publication_session_factory_port*>(lease.get());
		expect(factory != nullptr, "the account lease can mint a publication catalog");
		const auto session = factory->acquire_publication_session();
		expect(session != nullptr, "a native wire carries publication");
		auto* const mutation = dynamic_cast<application::review_mutation_transport_factory_port*>(lease.get());
		const auto  bound    = std::dynamic_pointer_cast<github_review_mutation_transport_lease>(mutation->acquire_review_mutation_transport());
		expect(bound != nullptr, "the lease's own credential context");
		const auto& context = bound->credential_context();
		const auto  target  = application::github_publication_target{ .Host       = std::string{ context.Authority.Hostname.value() },
																	  .Server     = context.Authority.ServerContract,
																	  .Account    = context.Authority.Account,
																	  .Login      = "viewer",
																	  .Owner      = "octo-org",
																	  .Repository = "review-repository" };
		static_cast<void>(session->observe_repository(target, { }));
		expect(fixture.Transport->PublicationContexts.size() == 1U && fixture.Transport->PublicationContexts.front() == context,
			   "the catalog speaks under exactly the account's credential context");
		auto other    = target;
		other.Account = core::github_account_id{ context.Authority.Account.value() + 1U };
		static_cast<void>(session->observe_repository(other, { }));
		expect(fixture.Transport->PublicationContexts.size() == 1U, "another account is refused before a request exists");
	}

	void test_cli_is_categorically_unavailable()
	{
		provider_fixture fixture{
			application::github_authentication_transport::github_cli
		};
		const auto request     = viewer_request_for(fixture.Profile);
		auto       completions = std::vector<application::github_viewer_completion>{ };
		fixture.Provider->observe_viewer(
			application::github_viewer_submission{
				.Request        = request,
				.Authentication = std::monostate{ },
			},
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		const auto& error = require_one<application::github_viewer_error>(
			completions, "CLI viewer is unavailable exactly once");
		expect(error.Request == request
				   && error.Error.Failure
					   == application::github_provider_failure::cli_unavailable
				   && fixture.Factory->BootstrapCalls == 0U
				   && fixture.Factory->AccountCalls == 0U
				   && fixture.Transport->ReadRequests.empty()
				   && fixture.Transport->CapabilityRequests.empty()
				   && fixture.Effects.PostCalls == 0U
				   && fixture.Effects.pending() == 0U,
			   "CLI path cannot start a process or credential transport");

		completions.clear();
		fixture.Provider->observe_viewer(
			application::github_viewer_submission{
				.Request        = request,
				.Authentication = application::github_bootstrap_credential{
					.Credential = credential_material(),
				},
			},
			[&completions](auto aCompletion) { completions.push_back(std::move(aCompletion)); });
		const auto& credentialError = require_one<application::github_viewer_error>(
			completions, "CLI credential submission is rejected once");
		expect(credentialError.Error.Failure
					   == application::github_provider_failure::unavailable
				   && fixture.Factory->BootstrapCalls == 0U
				   && fixture.Factory->AccountCalls == 0U
				   && fixture.Effects.PostCalls == 0U,
			   "CLI unavailability cannot be bypassed with credential material");
	}
	void test_window_read_authentication_provenance()
	{
		auto       fixture                 = provider_fixture{ application::github_authentication_transport::native_https };
		const auto lease                   = account_lease(fixture);
		auto       request                 = pull_request_for(fixture, lease);
		request.AuthenticationSession      = request.Context.ApplicationSession;
		request.Context.ApplicationSession = core::application_session_id{ 900U };
		fixture.Transport->ReadHandler     = [](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
			return github_wire_cancelled{ .Correlation = aWire.Correlation };
		};
		auto completions = std::vector<application::github_pull_request_completion>{ };
		fixture.Provider->fetch_pull_request(request, [&](auto aResult) { completions.push_back(std::move(aResult)); });
		expect(fixture.Effects.pending() == 1U, "distinct window read admitted under exact authentication provenance");
		fixture.Effects.run_next();
		expect(completions.size() == 1U && std::holds_alternative<application::github_pull_request_cancelled>(completions.front()), "window read reaches transport and terminates exactly once");
		const auto& wire = fixture.Transport->ReadRequests.back();
		expect(wire.Correlation.Parent == github_wire_parent_context{ request.Context }
				   && wire.Correlation.AuthenticationSession == request.AuthenticationSession
				   && std::get<application::github_pull_request_cancelled>(completions.front()).Request == request,
			   "wire and completion preserve original window context and explicit credential provenance");
		for (const auto variant : { 0, 1, 2, 3, 4 }) {
			auto forged = request;
			if (variant == 0) {
				forged.AuthenticationSession = core::application_session_id{ 999U };
			}
			if (variant == 1) {
				forged.AuthenticationSession = core::application_session_id{ };
			}
			if (variant == 2) {
				forged.AuthenticationSession.reset();
			}
			if (variant == 3) {
				forged.Context.IdentityGeneration = core::github_identity_generation{ 999U };
			}
			if (variant == 4) {
				forged.Context.Account = core::github_account_id{ 999U };
			}
			completions.clear();
			const auto calls = fixture.Transport->ReadRequests.size();
			fixture.Provider->fetch_pull_request(forged, [&](auto aResult) { completions.push_back(std::move(aResult)); });
			expect(completions.size() == 1U && std::holds_alternative<application::github_pull_request_error>(completions.front())
					   && fixture.Effects.pending() == 0U && fixture.Transport->ReadRequests.size() == calls,
				   "forged or missing credential provenance and foreign identity refuse before effects");
		}
	}

	/// [[BPR-0988]]. The provider's discovery entry: the exact account lease a
	/// pull request read uses, one worker job, exactly one terminal, and
	/// refusal before any effect for a foreign or missing authority.
	void test_personal_discovery_entry()
	{
		auto       fixture = provider_fixture{ application::github_authentication_transport::native_https };
		const auto lease   = account_lease(fixture);
		const auto base    = pull_request_for(fixture, lease);
		auto       request = application::personal_discovery_request{
			.Context = {
				.Request            = base.Context.Request,
				.ApplicationSession = base.Context.ApplicationSession,
				.Profile            = base.Context.Profile,
				.Host               = base.Context.Host,
				.Account            = base.Context.Account,
				.Repository         = base.Context.Repository,
				.Capabilities       = base.Context.Capabilities,
				.IdentityGeneration = base.Context.IdentityGeneration,
			},
			.ProfileAuthority   = base.ProfileAuthority,
			.Viewer             = base.Viewer,
			.CapabilityEvidence = base.CapabilityEvidence,
			.AccountTransport   = base.AccountTransport,
			.Reason             = application::personal_discovery_reason::authored,
		};
		fixture.Transport->ReadHandler = [](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
			expect(aWire.Correlation.Operation == read_operation::search_personal_pull_requests
					   && std::holds_alternative<core::repository_request_context>(aWire.Correlation.Parent),
				   "the provider issues the repository-scoped search first");
			return github_wire_cancelled{ .Correlation = aWire.Correlation };
		};
		auto       completions = std::vector<application::personal_discovery_completion>{ };
		const auto before      = fixture.Transport->ReadRequests.size();
		fixture.Provider->discover(request, [&](auto aResult) { completions.push_back(std::move(aResult)); });
		expect(fixture.Effects.pending() == 1U && completions.empty(), "a discovery read is one posted job");
		fixture.Effects.run_next();
		expect(completions.size() == 1U && std::holds_alternative<application::personal_discovery_cancelled>(completions.front())
				   && std::get<application::personal_discovery_cancelled>(completions.front()).Request == request
				   && fixture.Transport->ReadRequests.size() == before + 1U,
			   "the read reaches the account transport and terminates exactly once");
		for (const auto variant : { 0, 1, 2, 3 }) {
			auto forged = request;
			if (variant == 0) {
				forged.Context.Account = core::github_account_id{ 999U };
			}
			if (variant == 1) {
				forged.Context.IdentityGeneration = core::github_identity_generation{ 999U };
			}
			if (variant == 2) {
				forged.AccountTransport = nullptr;
			}
			if (variant == 3) {
				forged.AuthenticationSession = core::application_session_id{ 999U };
			}
			completions.clear();
			const auto calls = fixture.Transport->ReadRequests.size();
			fixture.Provider->discover(forged, [&](auto aResult) { completions.push_back(std::move(aResult)); });
			expect(completions.size() == 1U && std::holds_alternative<application::personal_discovery_error>(completions.front())
					   && fixture.Effects.pending() == 0U && fixture.Transport->ReadRequests.size() == calls,
				   "a foreign account, generation, lease or credential provenance is refused before any effect");
		}
		completions.clear();
		fixture.Transport->ReadHandler = [](const github_wire_request& aWire, std::stop_token) -> github_wire_completion {
			return github_wire_cancelled{ .Correlation = aWire.Correlation };
		};
		fixture.Provider->discover(request, [&](auto aResult) { completions.push_back(std::move(aResult)); });
		fixture.Provider->cancel_discovery(request);
		fixture.Effects.run_next();
		expect(completions.size() == 1U && std::holds_alternative<application::personal_discovery_cancelled>(completions.front()),
			   "cancelling a queued discovery read still delivers exactly one terminal");
	}

}

int main(int aArgc, char** aArgv)
{
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--audit-content-policy") {
		static_cast<void>(test_sanitized_provider_logging());
		bool contentRetained = false;
		for (const auto& canary : bpr::tests::capability_error_content_canaries) {
			const bool retained = !test_sanitized_provider_logging(&canary);
			contentRetained     = retained || contentRetained;
			std::cout << "provider-content-case=" << canary.Name << " retained=" << retained << '\n';
		}
		std::cout << "provider-content-audit content_retained=" << contentRetained << '\n';
		return contentRetained ? EXIT_FAILURE : EXIT_SUCCESS;
	}
	test_window_read_authentication_provenance();
	test_personal_discovery_entry();
	static_cast<void>(test_sanitized_provider_logging());
	test_actor_conflict_diagnostic_propagation();
	test_pull_request_attempt_diagnostic_propagation();
	test_merge_base_failure_and_refusal_are_distinct();
	test_logging_wrapper_failure_preserves_terminal();
	test_profile_set_validation();
	test_live_profile_catalog();
	test_profile_catalog_capacity();
	test_multi_profile_routing_and_authority_rejection();
	test_multi_profile_cancellation_is_profile_scoped();
	test_profile_resolution();
	test_bootstrap_viewer_has_no_account_lease();
	test_retrieved_account_must_match_observed_viewer();
	test_viewer_terminal_correlation();
	test_account_capability_cache_and_retry_contracts();
	test_capability_refusal_detail_propagation();
	test_capability_batch_failure_boundaries();
	test_cancellation_finality();
	test_exception_containment_is_exactly_once();
	test_cli_is_categorically_unavailable();
	test_account_lease_mints_publication_catalog();
	test_cli_observed_account_lease();
	test_cli_text_blob_reads_name_their_account();
	test_cli_git_lease_forwarding();
	test_cli_exact_blob_cancellation();
	test_cli_mutation_lease_account_checks();
	return EXIT_SUCCESS;
}
