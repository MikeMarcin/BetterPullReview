#include "github_anonymous_read_admission.hpp"
#include "github_host_profile.hpp"
#include "github_read_contract.hpp"
#include "native_https_transport.hpp"

#include "bpr/core/epoch_transition.hpp"

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>
#include <QtLogging>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <expected>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;
	using namespace std::chrono_literals;

	// The admission table is pure and constant-evaluable, so the whole verdict
	// set is settled at compile time rather than sampled at run time. A change
	// that widened the eligible set would fail here before any test ran.
	static_assert(!admit_anonymous_read(
					   read_operation::get_viewer_identity,
					   application::github_server_contract::github_com_2022_11_28)
					   .has_value());
	static_assert(!admit_anonymous_read(
					   read_operation::list_review_threads,
					   application::github_server_contract::github_com_2022_11_28)
					   .has_value());
	static_assert(admit_anonymous_read(
					  read_operation::get_repository,
					  application::github_server_contract::github_com_2022_11_28)
					  .has_value());
	static_assert(!admit_anonymous_read(
					   read_operation::get_repository,
					   application::github_server_contract::ghes_3_18_2022_11_28)
					   .has_value());

	class test_failure final : public std::runtime_error
	{
	public:
		explicit test_failure(const std::string_view aMessage)
			: std::runtime_error{ std::string{ aMessage } }
		{
		}
	};

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			throw test_failure{ aMessage };
		}
	}

	void discard_qt_message(QtMsgType, const QMessageLogContext&, const QString&)
	{
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	template <typename Oid>
	[[nodiscard]] constexpr auto oid(const std::uint8_t aSeed) -> Oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(
				aSeed + static_cast<std::uint8_t>(index));
		}
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto credential_material(const std::string_view aValue)
		-> application::github_credential_material
	{
		auto bytes = std::vector<std::byte>{ };
		bytes.reserve(aValue.size());
		for (const auto character : aValue) {
			bytes.push_back(
				static_cast<std::byte>(static_cast<unsigned char>(character)));
		}
		auto material = application::github_credential_material::create(bytes);
		expect(material.has_value(), "credential-material");
		return std::move(*material);
	}

	[[nodiscard]] auto profile_candidate(const bool aGithubCom)
		-> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 21U },
			.Revision       = application::github_profile_revision{ 31U },
			.Server         = aGithubCom
				? application::github_server_contract::github_com_2022_11_28
				: application::github_server_contract::ghes_3_18_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::managed,
			.Hostname       = aGithubCom ? "github.com" : "ghe.bpr.invalid",
			.RestRoot       = aGithubCom ? "https://api.github.com"
										 : "https://ghe.bpr.invalid/api/v3",
			.GraphqlRoot    = aGithubCom ? "https://api.github.com/graphql"
										 : "https://ghe.bpr.invalid/api/graphql",
		};
	}

	[[nodiscard]] auto session_context(
		const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 101U },
			.ApplicationSession = core::application_session_id{ 102U },
			.Profile            = aProfile.profile(),
			.IdentityGeneration = core::github_identity_generation{ 103U },
		};
	}

	[[nodiscard]] auto pull_request_context(
		const validated_github_host_profile& aProfile)
		-> core::pull_request_request_context
	{
		return {
			.Request            = core::request_id{ 101U },
			.ApplicationSession = core::application_session_id{ 102U },
			.Profile            = aProfile.profile(),
			.Host               = aProfile.host(),
			// Deliberately non-zero and unrelated to the binding. An anonymous
			// binding must ignore it rather than compare against it, and a
			// comparison against account zero would pass only by accident.
			.Account    = core::github_account_id{ 203U },
			.Repository = core::repository_locator{
				.Owner = "octo-org",
				.Name  = "review-repository",
			},
			.Number              = core::pull_request_number{ 17U },
			.ExpectedRepository  = core::repository_id{ 204U },
			.ExpectedPullRequest = core::pull_request_id{ 205U },
			.Capabilities        = core::capability_revision{ 206U },
			.IdentityGeneration  = core::github_identity_generation{ 103U },
			.Generation          = core::request_generation{ 208U },
		};
	}

	[[nodiscard]] auto semantic_for(const validated_github_host_profile& aProfile,
									const read_operation                 aOperation)
		-> semantic_read_request
	{
		const auto            context = pull_request_context(aProfile);
		const auto            server  = aProfile.server_contract()
				== application::github_server_contract::github_com_2022_11_28
			? server_contract::github_com
			: server_contract::ghes_3_18;
		semantic_read_request semantic{
			.Server            = server,
			.Operation         = aOperation,
			.Owner             = context.Repository.Owner,
			.Repository        = context.Repository.Name,
			.PullRequestNumber = 0U,
			.HeadOid           = { },
			.SearchTerms       = { },
			.Page              = 1U,
			.GraphqlPage       = std::nullopt,
		};
		switch (aOperation) {
		case read_operation::get_viewer_identity:
			semantic.Owner      = { };
			semantic.Repository = { };
			break;
		case read_operation::get_pull_request:
		case read_operation::list_pull_request_files:
		case read_operation::list_pull_request_commits:
		case read_operation::list_reviews:
		case read_operation::list_issue_comments:
		case read_operation::list_pull_request_timeline:
		case read_operation::list_review_comments:
		case read_operation::list_review_requests:
		case read_operation::get_merge_assessment:
			semantic.PullRequestNumber = context.Number.value();
			break;
		case read_operation::list_review_threads:
			semantic.PullRequestNumber = context.Number.value();
			semantic.GraphqlPage       = github_graphql_page_request{
				.Kind        = github_graphql_page_kind::initial,
				.Shape       = { .ReviewThreads = true },
				.AfterCursor = { },
				.ThreadId    = { },
			};
			break;
		case read_operation::list_commit_statuses:
		case read_operation::list_checks:
			semantic.HeadOid = "1111111111111111111111111111111111111111";
			break;
		case read_operation::search_pull_requests:
			semantic.SearchTerms = "is:open";
			break;
		case read_operation::search_personal_pull_requests:
			semantic.SearchTerms = "author:octo-reviewer";
			break;
		case read_operation::list_requested_reviewers:
			semantic.PullRequestNumber = context.Number.value();
			break;
		case read_operation::list_branch_rules:
		case read_operation::get_branch_protection:
			semantic.BranchRef = "main";
			break;
		case read_operation::get_repository:
			break;
		case read_operation::get_merge_base:
			semantic.BaseOid = "2222222222222222222222222222222222222222";
			semantic.HeadOid = "1111111111111111111111111111111111111111";
			break;
		case read_operation::resolve_body_images:
			semantic.PullRequestNumber = context.Number.value();
			semantic.Nodes             = { "PR_kwDOA" };
			break;
		case read_operation::get_team_membership:
			semantic.Organization = "octo-org";
			semantic.Team         = "reviewers";
			semantic.Login        = "octo-reviewer";
			break;
		}
		return semantic;
	}

	[[nodiscard]] auto wire_request_for(
		const validated_github_host_profile& aProfile,
		const read_operation                 aOperation) -> github_wire_request
	{
		const auto wire = compile_read_request(semantic_for(aProfile, aOperation));
		expect(wire.has_value(), "canonical-wire-request");
		return {
			.Correlation = github_wire_correlation{
				.Parent          = aOperation == read_operation::get_viewer_identity
					? github_wire_parent_context{ session_context(aProfile) }
					: github_wire_parent_context{ pull_request_context(aProfile) },
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Operation       = aOperation,
				.Page            = 1U,
			},
			.Wire = *wire,
		};
	}

	/// \brief Reports whether a request was refused before it was constructed.
	///
	/// \details A refused binding answers `unavailable` without dialling: the
	/// only path to that failure here is `claim_credential` returning false,
	/// ahead of request construction. A binding that admits the request instead
	/// reaches the proxy candidate list and fails against the closed loopback
	/// port, which reports the dial rather than the refusal. Discriminating on
	/// `unavailable` rather than on one particular dial failure keeps the
	/// distinction exact without making it depend on how fast a refused
	/// connection comes back.
	[[nodiscard]] auto refused_before_the_wire(
		native_https_transport& aTransport, const github_wire_request& aRequest)
		-> bool
	{
		const auto  completion = aTransport.perform(aRequest, std::stop_token{ });
		const auto* error      = std::get_if<github_wire_error>(&completion);
		return error != nullptr
			&& error->Error.Failure
			== application::github_provider_failure::unavailable;
	}

	[[nodiscard]] auto reached_the_wire(
		native_https_transport& aTransport, const github_wire_request& aRequest)
		-> bool
	{
		const auto  completion = aTransport.perform(aRequest, std::stop_token{ });
		const auto* error      = std::get_if<github_wire_error>(&completion);
		return error != nullptr
			&& error->Error.Failure
			!= application::github_provider_failure::unavailable;
	}

	[[nodiscard]] auto configuration() -> native_https_test_configuration
	{
		return {
			.ProxyHost    = "127.0.0.1",
			.ProxyPort    = 1U,
			.TotalTimeout = 2s,
		};
	}

	[[nodiscard]] auto anonymous_transport(
		const validated_github_host_profile& aProfile)
		-> std::unique_ptr<native_https_transport>
	{
		auto transport = make_native_https_anonymous_read_transport_for_test(
			aProfile, session_context(aProfile), configuration());
		expect(transport.has_value(), "anonymous-transport");
		return std::move(*transport);
	}

	void test_the_eligible_set_is_the_whole_match_table(
		const validated_github_host_profile& aProfile)
	{
		auto transport = anonymous_transport(aProfile);
		auto admitted  = std::size_t{ 0U };
		auto refused   = std::size_t{ 0U };
		for (const auto operation : all_read_operations) {
			const auto request = wire_request_for(aProfile, operation);
			if (anonymous_read_eligible(operation)) {
				expect(reached_the_wire(*transport, request),
					   "eligible-operation-refused");
				++admitted;
			} else {
				expect(refused_before_the_wire(*transport, request),
					   "ineligible-operation-admitted");
				++refused;
			}
		}
		// Every compiled operation has an explicit anonymous admission verdict.
		expect(admitted == 12U && refused == 10U
				   && all_read_operations.size() == 22U
				   && read_operation_catalog.size() == 10U,
			   "eligible-set-size");
	}

	void test_a_foreign_correlation_is_refused(
		const validated_github_host_profile& aProfile)
	{
		auto transport = anonymous_transport(aProfile);

		auto session = wire_request_for(aProfile, read_operation::get_repository);
		auto parent  = std::get<core::pull_request_request_context>(
			session.Correlation.Parent);
		auto foreignSession               = parent;
		foreignSession.ApplicationSession = core::application_session_id{ 999U };
		session.Correlation.Parent        = foreignSession;
		expect(refused_before_the_wire(*transport, session),
			   "foreign-session-admitted");

		auto generation = wire_request_for(
			aProfile, read_operation::get_repository);
		auto foreignGeneration = parent;
		foreignGeneration.IdentityGeneration
			= core::github_identity_generation{ 999U };
		generation.Correlation.Parent = foreignGeneration;
		expect(refused_before_the_wire(*transport, generation),
			   "foreign-generation-admitted");

		auto host               = wire_request_for(aProfile, read_operation::get_repository);
		auto foreignHost        = parent;
		foreignHost.Host        = core::git_host_id{ 999U };
		host.Correlation.Parent = foreignHost;
		expect(refused_before_the_wire(*transport, host), "foreign-host-admitted");

		// The account the binding never compares must still not become a way in
		// through some other door: a differing account on an otherwise exact
		// correlation is admitted, because an anonymous read has no account to
		// contradict. That is the deliberate shape, recorded so a later change
		// to it is visible.
		auto account               = wire_request_for(aProfile, read_operation::get_repository);
		auto otherAccount          = parent;
		otherAccount.Account       = core::github_account_id{ 777U };
		account.Correlation.Parent = otherAccount;
		expect(reached_the_wire(*transport, account), "account-compared");
	}

	void test_no_other_surface_accepts_the_binding(
		const validated_github_host_profile& aProfile)
	{
		auto transport = anonymous_transport(aProfile);

		const auto code = compile_device_code_request(
			server_contract::github_com, "Iv1.bpr0301contract");
		expect(code.has_value(), "device-code-compiled");
		const auto device = transport->perform_device_authorization(
			*code, std::stop_token{ });
		const auto* deviceError
			= std::get_if<native_https_device_authorization_error>(&device);
		expect(deviceError != nullptr
				   && deviceError->Error.Failure
					   == application::github_provider_failure::unavailable,
			   "device-overload-admitted");

		const auto probe = transport->perform_capability_probe(
			github_capability_probe_wire_request{ }, std::stop_token{ });
		expect(std::holds_alternative<github_capability_probe_wire_error>(probe),
			   "capability-probe-admitted");

		const auto mutation = transport->perform_review_mutation(
			github_mutation_wire_request{ }, std::stop_token{ }, { });
		const auto* mutationError = std::get_if<github_mutation_wire_error>(&mutation);
		expect(mutationError != nullptr
				   && mutationError->Transmission
					   == github_mutation_transmission::definitely_not_sent,
			   "mutation-admitted");
	}

	void test_the_smart_surface_refuses_the_binding(
		const validated_github_host_profile& aProfile)
	{
		auto owned = make_native_https_anonymous_read_transport_for_test(
			aProfile, session_context(aProfile), configuration());
		expect(owned.has_value(), "smart-anonymous-transport");
		std::shared_ptr<native_https_transport> shared{ std::move(*owned) };

		const auto baseRef = application::parse_git_base_branch_ref("refs/heads/main");
		expect(baseRef.has_value(), "smart-base-ref");
		const auto context = application::git_smart_request_context{
			.ApplicationSession = core::application_session_id{ 102U },
			.Authority          = core::pull_request_authority{
				.Profile         = aProfile.profile(),
				.ProfileRevision = core::github_profile_revision{ 31U },
				.Host            = aProfile.host(),
				.HostAuthority   = opaque<core::host_authority_fingerprint>(1U),
				.Account         = core::github_account_id{ 203U },
				.Viewer          = core::github_viewer_actor_id{ 204U },
				.Repository      = core::repository_id{ 205U },
				.Cache           = opaque<core::repository_cache_scope_id>(2U),
				.PullRequest     = core::pull_request_id{ 206U },
				.Number          = core::pull_request_number{ 17U },
				.Scope           = opaque<core::pull_request_authority_scope_id>(3U),
			},
			.Revision = core::accepted_pull_request_revision{
				.Source = core::pull_request_source_revision{
					.Observation           = opaque<core::epoch_source_observation_id>(9U),
					.Snapshot              = core::provider_snapshot_id{ 301U },
					.IdentityGeneration    = core::github_identity_generation{ 103U },
					.PullRequestGeneration = core::request_generation{ 208U },
				},
				.Origin     = core::pull_request_revision_origin::live,
				.BaseBranch = opaque<core::base_branch_id>(4U),
				.Base       = oid<core::git_commit_oid>(10U),
				.Head       = oid<core::git_commit_oid>(30U),
			},
			.Repository = core::repository_locator{
				.Owner = "octo-org",
				.Name  = "review-repository",
			},
			.BaseRef = *baseRef,
		};
		// Public smart HTTP is RFC-BPR-0017's separate question and belongs to
		// [[BPR-0317]]. Until it is answered the lease is refused typed, so no
		// anonymous Basic credential can be assembled from an empty span.
		const auto lease = make_native_git_smart_transport(shared, context);
		expect(!lease.has_value(), "smart-lease-acquired");
	}

	void test_the_contract_gate_precedes_the_transport(
		const validated_github_host_profile& aGithubCom,
		const validated_github_host_profile& aGhes)
	{
		const auto ghes = make_native_https_anonymous_read_transport(
			aGhes, session_context(aGhes));
		expect(!ghes.has_value()
				   && ghes.error()
					   == native_https_configuration_failure::
						   anonymous_read_contract_unsupported,
			   "ghes-production-admitted");

		const auto ghesForTest = make_native_https_anonymous_read_transport_for_test(
			aGhes, session_context(aGhes), configuration());
		expect(!ghesForTest.has_value()
				   && ghesForTest.error()
					   == native_https_configuration_failure::
						   anonymous_read_contract_unsupported,
			   "ghes-test-admitted");

		auto foreign          = session_context(aGithubCom);
		foreign.Profile       = core::host_profile_id{ 99U };
		const auto mismatched = make_native_https_anonymous_read_transport_for_test(
			aGithubCom, foreign, configuration());
		expect(!mismatched.has_value()
				   && mismatched.error()
					   == native_https_configuration_failure::
						   invalid_bootstrap_context,
			   "foreign-profile-admitted");
	}

	void test_absent_material_does_not_become_anonymous(
		const validated_github_host_profile& aProfile)
	{
		const auto context = application::github_credential_operation_context{
			.Request            = core::request_id{ 101U },
			.ApplicationSession = core::application_session_id{ 102U },
			.Authority          = application::github_credential_authority_key{
				.Profile         = aProfile.profile(),
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Hostname        = application::github_canonical_hostname{
					aProfile.hostname() },
				.Account        = core::github_account_id{ 203U },
				.ServerContract = aProfile.server_contract(),
			},
			.IdentityGeneration = core::github_identity_generation{ 103U },
		};
		// The canary. An account transport handed empty or malformed material
		// must keep failing the way it always has; degrading to the anonymous
		// authority would turn a revoked or missing credential into a silently
		// public session, which is the one way this feature could go wrong
		// without anybody naming it.
		// Absent material has no representation at all: the vault type refuses
		// to hold nothing. So the degradation this guards against cannot even
		// begin with an empty credential; it would have to begin with a
		// malformed one, which is what the loop below hands over.
		expect(!application::github_credential_material::create({ }).has_value(),
			   "empty-material-constructible");

		for (const auto& [material, message] :
			 std::initializer_list<std::pair<std::string_view, std::string_view>>{
				 { " ", "blank-material-degraded" },
				 { "not a token", "malformed-material-degraded" },
			 }) {
			const auto production = make_native_https_account_transport(
				aProfile,
				application::github_credential_retrieve_success{
					.Context    = context,
					.Credential = credential_material(material),
				});
			expect(!production.has_value()
					   && (production.error()
							   == native_https_configuration_failure::
								   invalid_account_credential
						   || production.error()
							   == native_https_configuration_failure::
								   invalid_credential_material),
				   message);
			const auto forTest = make_native_https_account_transport_for_test(
				aProfile,
				application::github_credential_retrieve_success{
					.Context    = context,
					.Credential = credential_material(material),
				},
				configuration());
			expect(!forTest.has_value(), message);
		}
	}

	void run_contract()
	{
		static_assert(!std::is_copy_constructible_v<native_https_transport>);
		static_assert(native_https_total_timeout == 30s);

		const auto githubCom = validate_github_host_profile(profile_candidate(true));
		expect(githubCom.has_value() && githubCom->hostname() == "github.com",
			   "github-com-profile");
		const auto ghes = validate_github_host_profile(profile_candidate(false));
		expect(ghes.has_value(), "ghes-profile");

		test_the_contract_gate_precedes_the_transport(*githubCom, *ghes);
		test_the_eligible_set_is_the_whole_match_table(*githubCom);
		test_a_foreign_correlation_is_refused(*githubCom);
		test_no_other_surface_accepts_the_binding(*githubCom);
		test_the_smart_surface_refuses_the_binding(*githubCom);
		test_absent_material_does_not_become_anonymous(*githubCom);
	}
} // namespace

auto main(int argument_count, char* arguments[]) -> int
{
	try {
		qInstallMessageHandler(discard_qt_message);
		QCoreApplication application{ argument_count, arguments };
		static_cast<void>(application);

		expect(argument_count == 2
				   && std::string_view{ arguments[1] } == "--contract",
			   "arguments");
		run_contract();
		std::cout << "native-https-anonymous-read:contract:pass\n";
		return EXIT_SUCCESS;
	} catch (const test_failure& failure) {
		std::cerr << "native-https-anonymous-read:fail:" << failure.what() << '\n';
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "native-https-anonymous-read:fail:internal\n";
		return EXIT_FAILURE;
	}
}
