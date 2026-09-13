#include "github_provider.hpp"
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

	class fake_wire_transport final : public github_wire_transport_port
	{
	public:
		using read_handler       = std::function<github_wire_completion(
			const github_wire_request&, std::stop_token)>;
		using capability_handler = std::function<github_capability_probe_wire_completion(
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
	void test_sanitized_provider_logging()
	{
		const auto       log = std::make_shared<recording_log>();
		provider_fixture fixture{ application::github_authentication_transport::native_https, log };
		const auto       request             = capability_request_for(fixture, account_lease(fixture));
		fixture.Transport->CapabilityHandler = [](const github_capability_probe_wire_request& aWire, std::stop_token) {
			auto response             = supported_capability_response();
			response.Body             = R"({"errors":[{"message":"Unknown schema field newField","extensions":{"code":"UNFAMILIAR_SCHEMA_CODE"}}]})";
			const std::string secret  = "runtime-only-fixture-secret";
			const auto        summary = sanitize_capability_error(response.Body, std::as_bytes(std::span{ secret }));
			return github_capability_probe_wire_success{ .Correlation = aWire.Correlation, .Response = std::move(response), .Summary = summary };
		};
		const auto completion = run_capability(fixture, request);
		expect(std::holds_alternative<application::github_capability_error>(completion), "logging preserves capability failure");
		expect(log->Events.size() >= 5U, "viewer and capability outcomes logged");
		const auto& detail = log->Events[log->Events.size() - 2U];
		expect(detail.Operation == application::diagnostic_operation::capability && detail.Outcome == application::diagnostic_outcome::response_received && detail.HttpStatus == 200 && detail.Request == request.Context.Request.value() && detail.Summary.text().find("UNFAMILIAR_SCHEMA_CODE") != std::string_view::npos, "provider forwards only sanitized useful detail with request and status");
		expect(log->Events.back().Outcome == application::diagnostic_outcome::failed, "failure terminal remains explicit");
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
		expect(std::ranges::none_of(log->Events, [](const auto& event) {
				   return event.Operation == application::diagnostic_operation::pull_request;
			   }),
			   "transport failure invents no response_received event");
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
		for (const auto mode : { failure::envelope, failure::part, failure::correlation, failure::cancelled, failure::exhausted, failure::retry_after }) {
			for (const auto stopPart : std::array<std::uint8_t, 3>{ 0U, 4U, 8U }) {
				provider_fixture fixture{ application::github_authentication_transport::native_https };
				const auto       request             = capability_request_for(fixture, account_lease(fixture));
				std::uint8_t     next                = 0U;
				fixture.Transport->CapabilityHandler = [&](const github_capability_probe_wire_request& aWire, std::stop_token) {
					expect(aWire.Correlation.Part == next && aWire.Wire.Part == next && aWire.Correlation.Session == request.Context, "parts remain ordered in the exact logical request");
					++next;
					auto response    = capability_part_response(supported_capability_response(), aWire.Correlation.Part);
					auto correlation = aWire.Correlation;
					if (aWire.Correlation.Part == stopPart) {
						switch (mode) {
						case failure::envelope:    response.Body = R"({"errors":[{"type":"FORBIDDEN"}]})"; break;
						case failure::part:        response = capability_part_response(supported_capability_response(), static_cast<std::uint8_t>((stopPart + 1U) % 9U)); break;
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
					return github_capability_probe_wire_success{ .Correlation = aWire.Correlation, .Response = capability_part_response(supported_capability_response(), aWire.Correlation.Part) };
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
			fixture.Provider->fetch_pull_request(readRequest, [&](auto aResult) { reads.push_back(std::move(aResult)); });
			fixture.Effects.run_next();
			static_cast<void>(require_one<application::github_pull_request_cancelled>(reads, "CLI PR read accepts repository success then reaches next read"));
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

}

int main()
{
	test_window_read_authentication_provenance();
	test_sanitized_provider_logging();
	test_pull_request_attempt_diagnostic_propagation();
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
	test_cli_observed_account_lease();
	return EXIT_SUCCESS;
}
