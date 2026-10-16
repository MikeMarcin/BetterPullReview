#include "github_anonymous_read_admission.hpp"
#include "github_provider.hpp"
#include "github_read_contract.hpp"

#include <QByteArray>
#include <QCryptographicHash>

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

	class controlled_executor final : public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			++PostCalls;
			Tasks.push_back(std::move(aTask));
		}

		[[nodiscard]] auto pending() const noexcept -> std::size_t
		{
			return Tasks.size();
		}

		void run_pending()
		{
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
			}
		}

		std::size_t PostCalls = 0U;

	private:
		std::deque<std::function<void()>> Tasks;
	};

	/// \brief Records every request a transport is asked to perform.
	///
	/// \details It answers each one with a network failure, because what this
	/// contract needs is not a snapshot but the set of requests that reached a
	/// transport at all: their operations and, above all, their headers.
	class recording_wire_transport final : public github_wire_transport_port
	{
	public:
		[[nodiscard]] auto perform(const github_wire_request& aRequest,
								   std::stop_token) -> github_wire_completion override
		{
			Requests.push_back(aRequest);
			return github_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = {
					.Failure           = application::github_provider_failure::network,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			};
		}

		std::vector<github_wire_request> Requests;
	};

	class recording_transport_factory final
		: public github_wire_transport_factory_port
	{
	public:
		[[nodiscard]] auto
		make_bootstrap(const validated_github_host_profile&,
					   core::github_session_request_context,
					   application::github_credential_material) noexcept
			-> github_wire_transport_factory_result override
		{
			++BootstrapCalls;
			return Transport;
		}

		[[nodiscard]] auto make_account(
			const validated_github_host_profile&,
			application::github_credential_retrieve_success) noexcept
			-> github_wire_transport_factory_result override
		{
			++AccountCalls;
			return Transport;
		}

		[[nodiscard]] auto make_anonymous(
			const validated_github_host_profile&       aProfile,
			const core::github_session_request_context aContext) noexcept
			-> github_wire_transport_factory_result override
		{
			++AnonymousCalls;
			LastAnonymousProfile = aProfile.profile();
			LastAnonymousContext = aContext;
			return Transport;
		}

		std::shared_ptr<github_wire_transport_port>         Transport;
		std::size_t                                         BootstrapCalls = 0U;
		std::size_t                                         AccountCalls   = 0U;
		std::size_t                                         AnonymousCalls = 0U;
		std::optional<core::host_profile_id>                LastAnonymousProfile;
		std::optional<core::github_session_request_context> LastAnonymousContext;
	};

	[[nodiscard]] auto test_profile(const bool aGithubCom)
		-> validated_github_host_profile
	{
		const auto profile = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = aGithubCom ? core::host_profile_id{ 11U }
										 : core::host_profile_id{ 12U },
			.Host           = aGithubCom ? core::git_host_id{ 22U }
										 : core::git_host_id{ 23U },
			.Revision       = application::github_profile_revision{ 3U },
			.Server         = aGithubCom
				? application::github_server_contract::github_com_2022_11_28
				: application::github_server_contract::ghes_3_18_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = aGithubCom ? "github.com" : "ghe.example.invalid",
			.RestRoot       = aGithubCom ? "https://api.github.com"
										 : "https://ghe.example.invalid/api/v3",
			.GraphqlRoot    = aGithubCom ? "https://api.github.com/graphql"
										 : "https://ghe.example.invalid/api/graphql",
		});
		expect(profile.has_value(), "anonymous read test profile validates");
		return *profile;
	}

	[[nodiscard]] auto profile_authority_for(
		const validated_github_host_profile& aProfile)
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

	[[nodiscard]] auto session_context(
		const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 201U },
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

	[[nodiscard]] auto viewer_request_for(
		const validated_github_host_profile& aProfile)
		-> application::github_viewer_request
	{
		return {
			.Context = {
				.Request            = core::request_id{ 101U },
				.ApplicationSession = core::application_session_id{ 102U },
				.Profile            = aProfile.profile(),
				.IdentityGeneration = core::github_identity_generation{ 103U },
			},
			.Authority = profile_authority_for(aProfile),
			.Attempt   = application::github_provider_attempt{ 1U },
		};
	}

	[[nodiscard]] auto cache_entry_for(
		const validated_github_host_profile& aProfile)
		-> application::github_capability_cache_entry
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
		expect(application::valid(observation), "anonymous observation is valid");
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

	[[nodiscard]] auto pull_request_for(
		const validated_github_host_profile& aProfile)
		-> application::github_pull_request_provider_request
	{
		return {
			.Context = {
				.Request            = core::request_id{ 201U },
				.ApplicationSession = core::application_session_id{ 102U },
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
				.IdentityGeneration  = core::github_identity_generation{ 103U },
				.Generation          = core::request_generation{ 9U },
			},
			.ProfileAuthority   = profile_authority_for(aProfile),
			.Viewer             = viewer_record(),
			.CapabilityEvidence = cache_entry_for(aProfile),
			// No lease. An anonymous read has no account, so it has nothing to
			// present here; requiring one would force the host to fabricate an
			// account identity in order to read nothing that belongs to it.
			.AccountTransport = { },
			.Attempt          = application::github_provider_attempt{ 1U },
		};
	}

	struct anonymous_fixture
	{
		explicit anonymous_fixture(const bool aGithubCom)
			: Profile{ test_profile(aGithubCom) }
			, Transport{ std::make_shared<recording_wire_transport>() }
			, Factory{ std::make_shared<recording_transport_factory>() }
		{
			Factory->Transport = Transport;
			auto provider      = make_github_provider(
				std::span<const validated_github_host_profile>{ &Profile, 1U },
				Effects,
				Factory,
				github_provider_read_authority::anonymous_public);
			expect(provider.has_value(), "anonymous provider constructs");
			Provider = std::move(*provider);
		}

		validated_github_host_profile                Profile;
		controlled_executor                          Effects;
		std::shared_ptr<recording_wire_transport>    Transport;
		std::shared_ptr<recording_transport_factory> Factory;
		std::unique_ptr<github_provider>             Provider;
	};

	[[nodiscard]] auto carries_authorization(const github_wire_request& aRequest)
		-> bool
	{
		return std::ranges::any_of(
			aRequest.Wire.Headers, [](const http_header& aHeader) {
				return aHeader.Name.size() == std::string_view{ "Authorization" }.size()
					&& std::ranges::equal(
						   aHeader.Name,
						   std::string_view{ "Authorization" },
						   [](const char aLeft, const char aRight) {
							   return std::tolower(static_cast<unsigned char>(aLeft))
								   == std::tolower(static_cast<unsigned char>(aRight));
						   });
			});
	}

	void test_the_admission_table_is_exhaustive_and_distinct()
	{
		auto eligible = std::size_t{ 0U };
		for (const auto operation : all_read_operations) {
			const auto onGithub = admit_anonymous_read(
				operation,
				application::github_server_contract::github_com_2022_11_28);
			const auto onEnterprise = admit_anonymous_read(
				operation,
				application::github_server_contract::ghes_3_18_2022_11_28);
			expect(!onEnterprise.has_value()
					   && onEnterprise.error()
						   == anonymous_read_refusal::server_contract_unsupported,
				   "every operation is refused on a GHES contract");
			if (operation == read_operation::get_viewer_identity
				|| operation == read_operation::search_personal_pull_requests
				|| operation == read_operation::list_requested_reviewers) {
				expect(!onGithub.has_value()
						   && onGithub.error()
							   == anonymous_read_refusal::
								   identity_requires_credential,
					   "viewer identity and personal discovery are refused as identity operations");
				continue;
			}
			if (operation == read_operation::list_review_threads
				|| operation == read_operation::list_review_requests
				|| operation == read_operation::get_merge_assessment
				|| operation == read_operation::list_branch_rules
				|| operation == read_operation::get_branch_protection) {
				expect(!onGithub.has_value()
						   && onGithub.error()
							   == anonymous_read_refusal::graphql_surface_unavailable,
					   "unsupported anonymous surfaces are explicitly refused");
				continue;
			}
			expect(onGithub.has_value(), "an eligible REST read is admitted");
			++eligible;
		}
		expect(eligible == 12U && all_read_operations.size() == 20U,
			   "twelve of twenty operations admit anonymous REST reads");
		expect(anonymous_read_operations.size() + 3U == eligible
				   && std::ranges::all_of(anonymous_read_operations,
										  anonymous_read_eligible),
			   "the baseline snapshot plus independent timeline, status and merge-base reads are eligible");

		// The two refusals must be distinguishable outward, not merely inward.
		expect(provider_failure_for(
				   anonymous_read_refusal::identity_requires_credential)
				   != provider_failure_for(
					   anonymous_read_refusal::graphql_surface_unavailable),
			   "the two refusals project to distinct provider failures");
		expect(provider_failure_for(
				   anonymous_read_refusal::server_contract_unsupported)
				   != provider_failure_for(
					   anonymous_read_refusal::graphql_surface_unavailable),
			   "the contract refusal is distinct from the GraphQL refusal");
	}

	void test_no_eligible_read_compiles_an_authorization_header()
	{
		for (const auto operation : all_read_operations) {
			if (!anonymous_read_eligible(operation)) {
				continue;
			}
			semantic_read_request semantic{
				.Server            = server_contract::github_com,
				.Operation         = operation,
				.Owner             = "octo-org",
				.Repository        = "review-repository",
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = { },
				.Page              = 1U,
				.GraphqlPage       = std::nullopt,
			};
			switch (operation) {
			case read_operation::get_pull_request:
			case read_operation::list_pull_request_files:
			case read_operation::list_pull_request_commits:
			case read_operation::list_reviews:
			case read_operation::list_issue_comments:
			case read_operation::list_pull_request_timeline:
			case read_operation::list_review_comments:
				semantic.PullRequestNumber = 17U;
				break;
			case read_operation::list_commit_statuses:
			case read_operation::list_checks:
				semantic.HeadOid = "1111111111111111111111111111111111111111";
				break;
			case read_operation::get_merge_base:
				semantic.BaseOid = "2222222222222222222222222222222222222222";
				semantic.HeadOid = "1111111111111111111111111111111111111111";
				break;
			case read_operation::search_pull_requests:
				semantic.SearchTerms = "is:open";
				break;
			case read_operation::search_personal_pull_requests:
				semantic.SearchTerms = "author:octo-reviewer";
				break;
			case read_operation::list_requested_reviewers:
				semantic.PullRequestNumber = 17U;
				break;
			case read_operation::list_review_requests:
			case read_operation::get_merge_assessment:
			case read_operation::list_branch_rules:
			case read_operation::get_branch_protection:
			case read_operation::get_viewer_identity:
			case read_operation::get_repository:
			case read_operation::list_review_threads:
				break;
			}
			if (operation == read_operation::list_review_comments) {
				const auto route = compile_read_request(semantic);
				expect(route.has_value()
						   && route->Path
							   == "/repos/octo-org/review-repository/pulls/17/comments"
						   && route->Method == http_method::get
						   && route->Body.empty(),
					   "the inline review-comment read is a REST GET on the "
					   "pull-request comments path");
			}
			const auto wire = compile_read_request(semantic);
			expect(wire.has_value(), "an eligible read compiles");
			expect(!carries_authorization(github_wire_request{ .Wire = *wire }),
				   "a compiled read carries no authorization header");
		}
	}

	void test_viewer_identity_is_refused_with_no_effect()
	{
		anonymous_fixture fixture{ true };
		auto              completions
			= std::vector<application::github_viewer_completion>{ };
		fixture.Provider->observe_viewer(
			application::github_viewer_submission{
				.Request        = viewer_request_for(fixture.Profile),
				.Authentication = { },
			},
			[&completions](auto aCompletion) {
				completions.push_back(std::move(aCompletion));
			});
		expect(completions.size() == 1U, "the viewer refusal is delivered once");
		const auto* error = std::get_if<application::github_viewer_error>(
			&completions.front());
		expect(error != nullptr
				   && error->Error.Failure
					   == provider_failure_for(
						   anonymous_read_refusal::identity_requires_credential),
			   "viewer identity is refused with its own diagnostic");
		expect(fixture.Effects.PostCalls == 0U
				   && fixture.Effects.pending() == 0U
				   && fixture.Factory->AnonymousCalls == 0U
				   && fixture.Factory->AccountCalls == 0U
				   && fixture.Factory->BootstrapCalls == 0U
				   && fixture.Transport->Requests.empty(),
			   "the viewer refusal costs no transport and no request");
	}

	void test_the_capability_probe_is_refused_with_no_effect()
	{
		anonymous_fixture fixture{ true };
		auto              completions
			= std::vector<application::github_capability_completion>{ };
		fixture.Provider->probe_capabilities(
			application::github_capability_request{
				.Context          = session_context(fixture.Profile),
				.ProfileAuthority = profile_authority_for(fixture.Profile),
				.Viewer           = viewer_record(),
				.AccountTransport = { },
				.Attempt          = application::github_provider_attempt{ 1U },
			},
			[&completions](auto aCompletion) {
				completions.push_back(std::move(aCompletion));
			});
		expect(completions.size() == 1U, "the probe refusal is delivered once");
		const auto* error = std::get_if<application::github_capability_error>(
			&completions.front());
		expect(error != nullptr
				   && error->Error.Failure
					   == provider_failure_for(
						   anonymous_read_refusal::graphql_surface_unavailable),
			   "the GraphQL probe is refused as a GraphQL surface");
		expect(fixture.Effects.PostCalls == 0U
				   && fixture.Factory->AnonymousCalls == 0U
				   && fixture.Transport->Requests.empty(),
			   "the probe refusal costs no transport and no request");
	}

	void test_an_eligible_read_routes_through_the_anonymous_transport()
	{
		anonymous_fixture fixture{ true };
		const auto        request = pull_request_for(fixture.Profile);
		expect(request.CapabilityEvidence.ReadShape
				   == application::github_read_shape{ },
			   "an anonymous read shape is all false");

		auto completions
			= std::vector<application::github_pull_request_completion>{ };
		fixture.Provider->fetch_pull_request(
			request, [&completions](auto aCompletion) {
				completions.push_back(std::move(aCompletion));
			});
		fixture.Effects.run_pending();
		expect(completions.size() == 1U, "the read completes exactly once");

		expect(fixture.Factory->AnonymousCalls == 1U
				   && fixture.Factory->AccountCalls == 0U
				   && fixture.Factory->BootstrapCalls == 0U,
			   "an anonymous read asks only for an anonymous transport");
		expect(fixture.Factory->LastAnonymousProfile
					   == std::optional{ fixture.Profile.profile() }
				   && fixture.Factory->LastAnonymousContext.has_value()
				   && fixture.Factory->LastAnonymousContext->ApplicationSession
					   == request.Context.ApplicationSession
				   && fixture.Factory->LastAnonymousContext->IdentityGeneration
					   == request.Context.IdentityGeneration
				   && fixture.Factory->LastAnonymousContext->Profile
					   == request.Context.Profile,
			   "the anonymous transport is bound to the request's session");

		expect(!fixture.Transport->Requests.empty(),
			   "an eligible read reaches the anonymous transport");
		for (const auto& reached : fixture.Transport->Requests) {
			expect(anonymous_read_eligible(reached.Correlation.Operation),
				   "only eligible operations reach the anonymous transport");
			expect(!carries_authorization(reached),
				   "no request reaching the anonymous transport is authorized");
		}
	}

	void test_a_graphql_shape_is_refused_before_construction()
	{
		anonymous_fixture fixture{ true };
		auto              request                          = pull_request_for(fixture.Profile);
		request.CapabilityEvidence.ReadShape.ReviewThreads = true;

		auto completions
			= std::vector<application::github_pull_request_completion>{ };
		fixture.Provider->fetch_pull_request(
			request, [&completions](auto aCompletion) {
				completions.push_back(std::move(aCompletion));
			});
		expect(completions.size() == 1U, "the shape refusal is delivered once");
		expect(fixture.Effects.PostCalls == 0U
				   && fixture.Factory->AnonymousCalls == 0U
				   && fixture.Transport->Requests.empty(),
			   "a GraphQL-shaped anonymous read costs no request");
	}

	void test_a_ghes_profile_is_refused_with_zero_requests()
	{
		anonymous_fixture fixture{ false };

		auto reads = std::vector<application::github_pull_request_completion>{ };
		fixture.Provider->fetch_pull_request(
			pull_request_for(fixture.Profile),
			[&reads](auto aCompletion) { reads.push_back(std::move(aCompletion)); });
		expect(reads.size() == 1U, "the GHES refusal is delivered once");
		const auto* readError = std::get_if<application::github_pull_request_error>(
			&reads.front());
		expect(readError != nullptr
				   && readError->Error.Failure
					   == provider_failure_for(
						   anonymous_read_refusal::server_contract_unsupported),
			   "a GHES anonymous read is refused by contract");

		auto viewers = std::vector<application::github_viewer_completion>{ };
		fixture.Provider->observe_viewer(
			application::github_viewer_submission{
				.Request        = viewer_request_for(fixture.Profile),
				.Authentication = { },
			},
			[&viewers](auto aCompletion) { viewers.push_back(std::move(aCompletion)); });
		expect(viewers.size() == 1U, "the GHES viewer refusal is delivered once");
		const auto* viewerError = std::get_if<application::github_viewer_error>(
			&viewers.front());
		expect(viewerError != nullptr
				   && viewerError->Error.Failure
					   == provider_failure_for(
						   anonymous_read_refusal::server_contract_unsupported),
			   "a GHES viewer read is refused by contract, not by identity");

		expect(fixture.Effects.PostCalls == 0U
				   && fixture.Factory->AnonymousCalls == 0U
				   && fixture.Factory->AccountCalls == 0U
				   && fixture.Factory->BootstrapCalls == 0U
				   && fixture.Transport->Requests.empty(),
			   "a GHES anonymous profile sends zero requests");
	}

	void test_the_authenticated_provider_is_unchanged()
	{
		const auto profile   = test_profile(true);
		auto       effects   = controlled_executor{ };
		auto       transport = std::make_shared<recording_wire_transport>();
		auto       factory   = std::make_shared<recording_transport_factory>();
		factory->Transport   = transport;
		auto provider        = make_github_provider(
			std::span<const validated_github_host_profile>{ &profile, 1U },
			effects,
			factory);
		expect(provider.has_value(), "the default provider constructs");

		auto material = application::github_credential_material::create(
			std::array<std::byte, 4U>{ std::byte{ 'a' }, std::byte{ 'b' },
									   std::byte{ 'c' }, std::byte{ 'd' } });
		expect(material.has_value(), "bootstrap material constructs");
		auto completions
			= std::vector<application::github_viewer_completion>{ };
		(*provider)->observe_viewer(
			application::github_viewer_submission{
				.Request        = viewer_request_for(profile),
				.Authentication = application::github_bootstrap_credential{
					.Credential = std::move(*material),
				},
			},
			[&completions](auto aCompletion) {
				completions.push_back(std::move(aCompletion));
			});
		effects.run_pending();
		expect(completions.size() == 1U, "the default viewer completes once");
		// The point is not what the fake answers but which door it was asked
		// through: the default authority still bootstraps, and never reaches
		// for the anonymous factory entry.
		expect(factory->BootstrapCalls == 1U && factory->AnonymousCalls == 0U
				   && transport->Requests.size() == 1U,
			   "the default authority is untouched by the anonymous selection");
	}

	// --- RFC-BPR-0017: promoted captures are bound to the recorded probe ----

	[[nodiscard]] auto read_bytes(const std::filesystem::path& aPath)
		-> std::string
	{
		std::ifstream stream{ aPath, std::ios::binary };
		expect(stream.good(), "a bound fixture or capture file is readable");
		return std::string{ std::istreambuf_iterator<char>{ stream },
							std::istreambuf_iterator<char>{ } };
	}

	[[nodiscard]] auto digest_of(const std::string& aBytes) -> std::string
	{
		const auto hash = QCryptographicHash::hash(
			QByteArray::fromStdString(aBytes),
			QCryptographicHash::Sha256);
		return hash.toHex().toStdString();
	}

	struct fixture_json_options : glz::opts
	{
		bool validate_skipped             = true;
		bool validate_trailing_whitespace = true;
		bool escape_control_characters    = true;
		constexpr fixture_json_options()
		{
			null_terminated       = false;
			error_on_missing_keys = true;
			error_on_unknown_keys = false;
		}
	};
	struct promoted_json
	{
		std::string PromotedFile;
		std::string BodySha256;
		std::string CaptureRelativeFile;
		std::string CaptureFileSha256;
		std::string SourceKind;
		struct glaze
		{
			static constexpr auto value = glz::object("promoted_file", &promoted_json::PromotedFile, "body_sha256", &promoted_json::BodySha256, "capture_relative_file", &promoted_json::CaptureRelativeFile, "capture_file_sha256", &promoted_json::CaptureFileSha256, "source_kind", &promoted_json::SourceKind);
		};
	};
	struct synthetic_json
	{
		std::string File;
		std::string Sha256;
		std::string Purpose;
		std::string SourceKind;
		struct glaze
		{
			static constexpr auto value = glz::object("file", &synthetic_json::File, "sha256", &synthetic_json::Sha256, "purpose", &synthetic_json::Purpose, "source_kind", &synthetic_json::SourceKind);
		};
	};
	struct provenance_json
	{
		std::vector<promoted_json>  Promoted;
		std::vector<synthetic_json> Synthetic;
		struct glaze
		{
			static constexpr auto value = glz::object("promoted", &provenance_json::Promoted, "synthetic", &provenance_json::Synthetic);
		};
	};
	struct capture_json
	{
		std::string Body;
		std::string SchemaName;
		struct glaze
		{
			static constexpr auto value = glz::object("body", &capture_json::Body, "schema_name", &capture_json::SchemaName);
		};
	};

	/// The promoted fixtures are copies of real captures, so their claim to
	/// be real is only worth what binds them to the capture. Each promoted
	/// body is bound twice: to the digest the probe review recorded for that
	/// body, and to the capture file it was taken from, which is itself bound
	/// by its own file digest. A promoted body edited to make a test pass
	/// fails here rather than quietly becoming its own authority.
	void test_promoted_captures_match_their_recorded_digests()
	{
		const auto fixtureRoot = std::filesystem::path{
			BPR_RFC0017_ANONYMOUS_FIXTURE_ROOT
		};
		const auto captureRoot = std::filesystem::path{
			BPR_RFC0017_ANONYMOUS_CAPTURE_ROOT
		};
		provenance_json provenance;
		expect(!glz::read<fixture_json_options{ }>(provenance, read_bytes(fixtureRoot / "provenance.json")), "promoted provenance has its required typed JSON fields");
		const auto& promoted = provenance.Promoted;
		expect(promoted.size() == 6,
			   "every capture the probe review recorded is promoted");
		auto live = std::size_t{ 0U };
		for (const auto& entry : promoted) {
			const auto promotedBytes = read_bytes(
				fixtureRoot
				/ entry.PromotedFile);
			expect(digest_of(promotedBytes)
					   == entry.BodySha256,
				   "a promoted body hashes to the digest the review recorded");

			const auto captureBytes = read_bytes(
				captureRoot
				/ entry.CaptureRelativeFile);
			expect(digest_of(captureBytes)
					   == entry.CaptureFileSha256,
				   "the capture the body was promoted from is itself unchanged");
			capture_json capture;
			expect(!glz::read<fixture_json_options{ }>(capture, captureBytes), "promoted capture has its required typed JSON fields");
			expect(capture.Body == promotedBytes,
				   "the promoted copy is the capture's body byte for byte");
			expect(capture.SchemaName
					   == "bpr.rfc0017.anonymous_capture",
				   "the capture is a probe capture rather than an arbitrary file");
			if (entry.SourceKind == "live") {
				++live;
			}
		}
		expect(live == 5U,
			   "five promoted bodies are live service captures and the sixth "
			   "says it is derived");

		const auto& synthetic = provenance.Synthetic;
		expect(!synthetic.empty(),
			   "the hostile cases the single-comment capture cannot express "
			   "are listed");
		for (const auto& entry : synthetic) {
			expect(entry.SourceKind
					   == "synthetic",
				   "a hand-written fixture is named synthetic, never live");
			expect(!entry.Purpose.empty(),
				   "a synthetic fixture states the case it exists for");
			expect(digest_of(read_bytes(
					   fixtureRoot
					   / entry.File))
					   == entry.Sha256,
				   "a synthetic fixture matches its recorded digest");
		}
	}
} // namespace

auto main() -> int
{
	test_the_admission_table_is_exhaustive_and_distinct();
	test_no_eligible_read_compiles_an_authorization_header();
	test_viewer_identity_is_refused_with_no_effect();
	test_the_capability_probe_is_refused_with_no_effect();
	test_an_eligible_read_routes_through_the_anonymous_transport();
	test_a_graphql_shape_is_refused_before_construction();
	test_a_ghes_profile_is_refused_with_zero_requests();
	test_the_authenticated_provider_is_unchanged();
	test_promoted_captures_match_their_recorded_digests();
	std::cout << "github-anonymous-read:contract:pass\n";
	return EXIT_SUCCESS;
}
