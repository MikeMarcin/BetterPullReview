#include "bpr/application/github_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

// The signed-out host course. Every test here proves a negative as well as a
// positive: what public preview does, and what it never asks anyone for. The
// counters are therefore part of the evidence rather than instrumentation, and
// a test that only checked the accepted session would pass while the host
// quietly consulted a vault it must never touch.
namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	struct fake_lease final : application::github_account_transport_lease
	{
	};

	class counting_provider final : public application::github_provider_port
	{
	public:
		struct profile_call
		{
			application::github_profile_request            Request;
			application::github_profile_completion_handler Completion;
		};

		struct viewer_call
		{
			application::github_viewer_request            Request;
			application::github_viewer_completion_handler Completion;
		};

		struct capability_call
		{
			application::github_capability_request            Request;
			application::github_capability_completion_handler Completion;
		};

		struct pull_request_call
		{
			application::github_pull_request_provider_request   Request;
			application::github_pull_request_completion_handler Completion;
		};

		void resolve_profile(
			application::github_profile_request            aRequest,
			application::github_profile_completion_handler aCompletion) override
		{
			Profiles.push_back({ aRequest, std::move(aCompletion) });
		}

		void cancel_profile(application::github_profile_request aRequest) override
		{
			CancelledProfiles.push_back(aRequest);
		}

		void observe_viewer(
			application::github_viewer_submission         aSubmission,
			application::github_viewer_completion_handler aCompletion) override
		{
			Viewers.push_back({ aSubmission.Request, std::move(aCompletion) });
		}

		void cancel_viewer(application::github_viewer_request aRequest) override
		{
			CancelledViewers.push_back(aRequest);
		}

		void probe_capabilities(
			application::github_capability_request            aRequest,
			application::github_capability_completion_handler aCompletion) override
		{
			Capabilities.push_back({ aRequest, std::move(aCompletion) });
		}

		void cancel_capabilities(
			application::github_capability_request aRequest) override
		{
			CancelledCapabilities.push_back(aRequest);
		}

		void fetch_pull_request(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion) override
		{
			PullRequests.push_back({ std::move(aRequest), std::move(aCompletion) });
		}

		void cancel_pull_request(
			application::github_pull_request_provider_request aRequest) override
		{
			CancelledPullRequests.push_back(std::move(aRequest));
		}

		std::vector<profile_call>                           Profiles;
		std::vector<viewer_call>                            Viewers;
		std::vector<capability_call>                        Capabilities;
		std::vector<pull_request_call>                      PullRequests;
		std::vector<application::github_profile_request>    CancelledProfiles;
		std::vector<application::github_viewer_request>     CancelledViewers;
		std::vector<application::github_capability_request> CancelledCapabilities;
		std::vector<application::github_pull_request_provider_request>
			CancelledPullRequests;
	};

	// The credential-free read authority. It answers pull-request reads only,
	// which is the whole of what the mode can ask for.
	class anonymous_read_authority final
		: public application::github_pull_request_port
	{
	public:
		struct call
		{
			application::github_pull_request_provider_request   Request;
			application::github_pull_request_completion_handler Completion;
		};

		void fetch_pull_request(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion) override
		{
			Reads.push_back({ std::move(aRequest), std::move(aCompletion) });
		}

		void cancel_pull_request(
			application::github_pull_request_provider_request aRequest) override
		{
			Cancelled.push_back(std::move(aRequest));
		}

		std::vector<call>                                              Reads;
		std::vector<application::github_pull_request_provider_request> Cancelled;
	};

	class counting_vault final : public application::github_credential_vault_port
	{
	public:
		void store(application::github_credential_enrollment aEnrollment,
				   application::github_credential_store_completion_handler
					   aCompletion) override
		{
			++Stores;
			StoreContexts.push_back(aEnrollment.Context);
			StoreCompletions.push_back(std::move(aCompletion));
		}

		void retrieve(application::github_credential_operation_context aContext,
					  application::github_credential_retrieve_completion_handler
						  aCompletion) override
		{
			++Retrieves;
			RetrieveContexts.push_back(aContext);
			RetrieveCompletions.push_back(std::move(aCompletion));
		}

		void delete_credential(
			application::github_credential_operation_context         aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			++Deletes;
			aCompletion(application::github_credential_delete_success{
				.Context     = aContext,
				.Disposition = application::github_credential_delete_disposition::deleted,
			});
		}

		void store_refresh(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override
		{
			++Stores;
			const auto context = aEnrollment.Context;
			aEnrollment.Credential.reset();
			aCompletion(application::github_credential_store_error{
				.Context = context,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		void retrieve_refresh(
			application::github_credential_operation_context aContext,
			application::github_credential_retrieve_completion_handler
				aCompletion) override
		{
			++Retrieves;
			aCompletion(application::github_credential_retrieve_error{
				.Context = aContext,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		void delete_refresh(
			application::github_credential_operation_context         aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			++Deletes;
			aCompletion(application::github_credential_delete_error{
				.Context = aContext,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		void cancel(
			application::github_credential_operation_context aContext) override
		{
			++Cancellations;
			CancelledContexts.push_back(aContext);
		}

		[[nodiscard]] auto touched() const noexcept -> std::size_t
		{
			return Stores + Retrieves + Deletes + Cancellations;
		}

		std::size_t                                                   Stores        = 0U;
		std::size_t                                                   Retrieves     = 0U;
		std::size_t                                                   Deletes       = 0U;
		std::size_t                                                   Cancellations = 0U;
		std::vector<application::github_credential_operation_context> StoreContexts;
		std::vector<application::github_credential_operation_context>
			RetrieveContexts;
		std::vector<application::github_credential_operation_context>
			CancelledContexts;
		std::vector<application::github_credential_store_completion_handler>
			StoreCompletions;
		std::vector<application::github_credential_retrieve_completion_handler>
			RetrieveCompletions;
	};

	class recording_projections final : public application::github_projection_port
	{
	public:
		void publish(
			const application::github_presentation_update& aUpdate) override
		{
			Updates.push_back(aUpdate);
		}

		[[nodiscard]] auto last_session() const
			-> std::optional<application::github_session_projection>
		{
			for (auto index = Updates.size(); index > 0U; --index) {
				const auto& update = Updates[index - 1U];
				if (update.Session.has_value()) {
					return update.Session;
				}
			}
			return std::nullopt;
		}

		std::vector<application::github_presentation_update> Updates;
	};

	class recording_epochs final : public application::github_epoch_handoff_port
	{
	public:
		void accept(application::accepted_pull_request_handoff aHandoff) override
		{
			Accepted.push_back(std::move(aHandoff));
		}

		void invalidate(
			application::pull_request_authority_invalidation aInvalidation) override
		{
			Invalidated.push_back(aInvalidation);
		}

		std::vector<application::accepted_pull_request_handoff>       Accepted;
		std::vector<application::pull_request_authority_invalidation> Invalidated;
	};

	[[nodiscard]] auto initial_state() -> core::github_state
	{
		core::github_state state;
		state.ApplicationSession = core::application_session_id{ 41U };
		return state;
	}

	[[nodiscard]] auto dotcom_profile(const core::host_profile_id aProfile)
		-> application::github_profile_authority
	{
		return {
			.Profile               = aProfile,
			.ProfileRevision       = application::github_profile_revision{ 31U },
			.Host                  = core::git_host_id{ 21U },
			.Hostname              = application::github_canonical_hostname{ "github.com" },
			.ServerContract        = application::github_server_contract::github_com_2022_11_28,
			.Authentication        = application::github_authentication_transport::native_https,
			.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
		};
	}

	[[nodiscard]] auto enterprise_profile(const core::host_profile_id aProfile)
		-> application::github_profile_authority
	{
		auto result           = dotcom_profile(aProfile);
		result.Hostname       = application::github_canonical_hostname{ "ghe.example.invalid" };
		result.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		return result;
	}

	[[nodiscard]] auto viewer(const core::github_account_id aAccount
							  = core::github_account_id{ 51U })
		-> application::github_viewer_record
	{
		return {
			.Account     = aAccount,
			.Node        = application::github_actor_id{ "viewer-node" },
			.Login       = "reviewer",
			.DisplayName = "Review Engineer",
		};
	}

	[[nodiscard]] auto rate_limit(
		const application::github_rate_limit_resource aResource)
		-> application::github_rate_limit_record
	{
		return {
			.Resource         = aResource,
			.Limit            = 5'000U,
			.Remaining        = 4'999U,
			.ResetUnixSeconds = 2'000'000'000U,
		};
	}

	[[nodiscard]] auto rate_limits_for(
		const application::github_rate_limit_resource aResource)
		-> application::github_rate_limit_state
	{
		application::github_rate_limit_state result;
		expect(application::observe(result, rate_limit(aResource)),
			   "single-resource rate fixture is valid");
		return result;
	}

	[[nodiscard]] auto anonymous_read_rate_limits()
		-> application::github_rate_limit_state
	{
		auto result      = rate_limits_for(application::github_rate_limit_resource::core);
		auto search      = rate_limit(application::github_rate_limit_resource::search);
		search.Limit     = 30U;
		search.Remaining = 29U;
		expect(application::observe(result, search), "search rate fixture is valid");
		return result;
	}

	[[nodiscard]] auto capability_facts()
		-> std::array<application::github_capability_fact,
					  application::github_capability_fact_count>
	{
		using capability = core::github_capability;
		using fact       = application::github_capability_fact;
		using source     = application::github_capability_evidence_source;
		using state      = core::github_capability_state;
		return {
			fact{ capability::review_threads, state::unsupported, source::observed_contract },
			fact{ capability::viewed_file_mutations, state::unsupported, source::observed_contract },
			fact{ capability::resolve_thread, state::unsupported, source::observed_contract },
			fact{ capability::unresolve_thread, state::unsupported, source::observed_contract },
		};
	}

	[[nodiscard]] auto schema_facts()
		-> std::array<application::github_schema_capability_fact,
					  application::github_schema_capability_fact_count>
	{
		using capability = application::github_schema_capability;
		using fact       = application::github_schema_capability_fact;
		using source     = application::github_capability_evidence_source;
		using state      = core::github_capability_state;
		return {
			fact{ capability::review_threads_read, state::unsupported, source::observed_contract },
			fact{ capability::viewed_file_state_read, state::unsupported, source::observed_contract },
			fact{ capability::mark_file_as_viewed, state::unsupported, source::observed_contract },
			fact{ capability::unmark_file_as_viewed, state::unsupported, source::observed_contract },
			fact{ capability::resolve_permission, state::unsupported, source::observed_contract },
			fact{ capability::resolve_review_thread, state::unsupported, source::observed_contract },
			fact{ capability::unresolve_permission, state::unsupported, source::observed_contract },
			fact{ capability::unresolve_review_thread, state::unsupported, source::observed_contract },
		};
	}

	[[nodiscard]] auto credential() -> application::github_credential_material
	{
		const std::array bytes{ std::byte{ 0x01 }, std::byte{ 0x02 } };
		auto             result = application::github_credential_material::create(bytes);
		expect(result.has_value(), "credential fixture allocates");
		return std::move(*result);
	}

	[[nodiscard]] auto locator() -> core::repository_locator
	{
		return { .Owner = "octo", .Name = "review" };
	}

	struct fixture
	{
		explicit fixture(const bool aComposeAnonymousAuthority = true)
		{
			Host = std::make_unique<application::github_application_host>(
				initial_state(),
				Provider,
				Provider,
				Provider,
				Provider,
				Vault,
				Bindings,
				Lane,
				Projections,
				application::github_credential_shutdown_policy::retain,
				&Epochs,
				aComposeAnonymousAuthority ? &Anonymous : nullptr);
		}

		void drain()
		{
			while (Lane.pending() != 0U) {
				Lane.run_next();
			}
		}

		[[nodiscard]] auto signed_out_requests() const noexcept -> std::size_t
		{
			return Provider.Viewers.size() + Provider.Capabilities.size()
				+ Provider.PullRequests.size();
		}

		counting_provider                                     Provider;
		anonymous_read_authority                              Anonymous;
		counting_vault                                        Vault;
		application::manual_executor                          Lane;
		recording_projections                                 Projections;
		recording_epochs                                      Epochs;
		application::github_credential_binding_catalog        Bindings;
		std::unique_ptr<application::github_application_host> Host;
	};

	// Drives one signed-out activation to its terminal. The only completion it
	// answers is the profile resolution, because that is the only question the
	// mode is allowed to ask.
	auto activate_signed_out(fixture&                    aFixture,
							 const core::host_profile_id aProfile,
							 const bool                  aEnterprise = false) -> void
	{
		const auto before = aFixture.Provider.Profiles.size();
		aFixture.Host->activate_public_preview(aProfile);
		if (aFixture.Provider.Profiles.size() == before) {
			return;
		}
		const auto request    = aFixture.Provider.Profiles.back().Request;
		auto       completion = aFixture.Provider.Profiles.back().Completion;
		completion(application::github_profile_success{
			.Request   = request,
			.Authority = aEnterprise ? enterprise_profile(aProfile)
									 : dotcom_profile(aProfile),
		});
		aFixture.drain();
	}

	// Drives one authenticated activation to an accepted session, so the tests
	// below compare the signed-out course against the real one rather than
	// against a description of it. Every step here is a question public
	// preview must never ask: a bootstrap /user, a vault store, a vault
	// retrieve, an account-authenticated /user, and a capability probe.
	auto activate_authenticated(fixture&                      aFixture,
								const core::host_profile_id   aProfile,
								const core::github_account_id aAccount)
		-> std::shared_ptr<fake_lease>
	{
		aFixture.Host->enroll_profile(aProfile, credential());
		const auto profileRequest = aFixture.Provider.Profiles.back().Request;
		auto       profileDone    = aFixture.Provider.Profiles.back().Completion;
		profileDone(application::github_profile_success{
			.Request   = profileRequest,
			.Authority = dotcom_profile(aProfile),
		});
		aFixture.drain();

		const auto bootstrapRequest = aFixture.Provider.Viewers.back().Request;
		auto       bootstrapDone    = aFixture.Provider.Viewers.back().Completion;
		bootstrapDone(application::github_viewer_success{
			.Request          = bootstrapRequest,
			.Viewer           = viewer(aAccount),
			.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
			.AccountTransport = { },
		});
		aFixture.drain();

		const auto storeContext = aFixture.Vault.StoreContexts.back();
		auto       storeDone    = aFixture.Vault.StoreCompletions.back();
		storeDone(application::github_credential_store_success{
			.Context     = storeContext,
			.Disposition = application::github_credential_store_disposition::stored,
		});
		aFixture.drain();

		const auto retrieveContext = aFixture.Vault.RetrieveContexts.back();
		auto       retrieveDone    = aFixture.Vault.RetrieveCompletions.back();
		retrieveDone(application::github_credential_retrieve_success{
			.Context    = retrieveContext,
			.Credential = credential(),
		});
		aFixture.drain();

		auto       transport      = std::make_shared<fake_lease>();
		const auto accountRequest = aFixture.Provider.Viewers.back().Request;
		auto       accountDone    = aFixture.Provider.Viewers.back().Completion;
		accountDone(application::github_viewer_success{
			.Request          = accountRequest,
			.Viewer           = viewer(aAccount),
			.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
			.AccountTransport = transport,
		});
		aFixture.drain();

		const auto capabilityRequest = aFixture.Provider.Capabilities.back().Request;
		auto       capabilityDone    = aFixture.Provider.Capabilities.back().Completion;
		capabilityDone(application::github_capability_success{
			.Request     = capabilityRequest,
			.Facts       = capability_facts(),
			.SchemaFacts = schema_facts(),
			.RateLimits  = rate_limits_for(application::github_rate_limit_resource::graphql),
		});
		aFixture.drain();
		return transport;
	}

	auto open_pull_request(fixture& aFixture, const std::uint32_t aNumber) -> void
	{
		aFixture.Host->dispatch(core::open_pull_request_intent{
			.Repository = locator(),
			.Number     = core::pull_request_number{ aNumber },
		});
		aFixture.drain();
	}

	// The whole of what an invalidation must clear, read from the host's public
	// surface. An account switch and a mode switch must produce the same
	// answer here; the tests compare the two rather than restating either.
	struct cleared_state
	{
		bool        Session          = false;
		bool        Snapshot         = false;
		bool        MutationLease    = false;
		bool        RateLimits       = false;
		std::size_t CapabilityCache  = 0U;
		bool        CoreAccount      = false;
		bool        CoreHost         = false;
		bool        CorePullRequest  = false;
		bool        CoreSnapshot     = false;
		bool        EpochUnavailable = false;

		auto operator<=>(const cleared_state&) const = default;
	};

	[[nodiscard]] auto observed_clearing(const fixture& aFixture) -> cleared_state
	{
		const auto& state = aFixture.Host->state();
		return {
			.Session          = !aFixture.Host->accepted_session().has_value(),
			.Snapshot         = !aFixture.Host->accepted_snapshot().has_value(),
			.MutationLease    = aFixture.Host->accepted_review_mutation_transport() == nullptr,
			.RateLimits       = aFixture.Host->rate_limits() == application::github_rate_limit_state{ },
			.CapabilityCache  = aFixture.Host->capability_cache_size(),
			.CoreAccount      = !state.Account.has_value(),
			.CoreHost         = !state.Host.has_value(),
			.CorePullRequest  = !state.PullRequest.has_value(),
			.CoreSnapshot     = !state.AcceptedSnapshot.has_value(),
			.EpochUnavailable = aFixture.Host->epoch_handoff_unavailable(),
		};
	}

	auto test_signed_out_activation_asks_nothing_of_the_account_course() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });

		expect(test.Vault.touched() == 0U,
			   "a signed-out activation performs no vault lookup, store, delete, or cancellation");
		expect(test.Provider.Viewers.empty(),
			   "a signed-out activation issues no /user request");
		expect(test.Provider.Capabilities.empty(),
			   "a signed-out activation probes no capability");
		expect(test.Host->capability_cache_size() == 0U,
			   "a signed-out activation neither reads nor commits the capability cache");
		expect(test.Provider.Profiles.size() == 1U,
			   "a signed-out activation resolves exactly one profile authority");

		const auto& session = test.Host->accepted_session();
		expect(session.has_value(), "the signed-out session is accepted");
		expect(session->Mode == core::github_access_mode::public_preview,
			   "the accepted session is typed public preview");
		expect(application::anonymous(session->Viewer),
			   "the accepted session carries no viewer identity at all");
		expect(application::reads_nothing(session->CapabilityEvidence.ReadShape),
			   "the accepted read shape claims nothing");
		expect(session->CapabilityEvidence.ContractVersion == 1U,
			   "the synthesised evidence carries the contract version");
		expect(session->CapabilityEvidence.Capabilities
				   == core::anonymous_capabilities(),
			   "every capability is unsupported by policy rather than unknown");
		expect(test.Host->access_mode() == core::github_access_mode::public_preview,
			   "the host reports the mode it activated in");
		expect(!test.Host->public_preview_refusal().has_value(),
			   "an accepted activation records no refusal");

		const auto observations = core::anonymous_observations();
		expect(observations.Viewer == core::anonymous_viewer_identity::unavailable
				   && observations.ThreadResolution
					   == core::anonymous_thread_resolution::unavailable
				   && observations.ViewedFile
					   == core::anonymous_viewed_file_state::unavailable
				   && observations.Actionability
					   == core::anonymous_actionability::unavailable,
			   "viewer, resolution, viewed-file, and actionability observations are typed unavailable");

		open_pull_request(test, 7U);
		expect(test.Vault.touched() == 0U,
			   "a signed-out open performs no vault work either");
		expect(test.Provider.Viewers.empty() && test.Provider.PullRequests.empty(),
			   "a signed-out open reaches the authenticated provider for nothing");
		expect(test.Anonymous.Reads.size() == 1U,
			   "a signed-out open reaches the credential-free authority exactly once");
		expect(test.Anonymous.Reads.front().Request.Mode
				   == core::github_access_mode::public_preview,
			   "the read names the mode it was issued in");
		expect(test.Anonymous.Reads.front().Request.AccountTransport == nullptr,
			   "the read carries no account transport");
	}

	auto test_enterprise_and_uncomposed_authority_refuse_typed() -> void
	{
		{
			fixture test;
			activate_signed_out(test, core::host_profile_id{ 11U }, true);
			expect(test.Vault.touched() == 0U,
				   "an enterprise refusal performs no vault work");
			expect(test.signed_out_requests() == 0U,
				   "an enterprise refusal issues no viewer, capability, or read request");
			expect(test.Anonymous.Reads.empty(),
				   "an enterprise refusal reaches the credential-free authority for nothing");
			expect(!test.Host->accepted_session().has_value(),
				   "an enterprise host accepts no signed-out session");
			const auto& refusal = test.Host->public_preview_refusal();
			expect(refusal.has_value(), "the enterprise refusal is recorded");
			expect(refusal->Cause
					   == application::github_public_preview_refusal_cause::policy_refused,
				   "the enterprise refusal is the core policy's");
			expect(refusal->Policy.has_value()
					   && refusal->Policy->Reason
						   == core::anonymous_refusal_reason::ghes_unavailable,
				   "the enterprise refusal names the enterprise reason");
			expect(core::anonymous_refusal::SignInOffered,
				   "every refusal offers sign-in");
		}
		{
			fixture test{ false };
			activate_signed_out(test, core::host_profile_id{ 11U });
			expect(test.Provider.Profiles.empty(),
				   "an uncomposed authority refuses before any profile is resolved");
			expect(test.signed_out_requests() == 0U && test.Vault.touched() == 0U,
				   "an uncomposed authority refuses with zero requests of any kind");
			expect(test.Host->state().IdentityGeneration
					   == core::github_identity_generation{ },
				   "an uncomposed authority abandons no identity generation");
			const auto& refusal = test.Host->public_preview_refusal();
			expect(refusal.has_value()
					   && refusal->Cause
						   == application::github_public_preview_refusal_cause::
							   public_preview_unavailable
					   && !refusal->Policy.has_value(),
				   "the uncomposed refusal is public_preview_unavailable and carries no policy");
		}
	}

	auto test_the_signed_out_scope_is_stable_and_never_an_account() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		expect(test.Anonymous.Reads.size() == 1U, "the first open issues one read");
		const auto first = test.Anonymous.Reads.front().Request;

		auto cancelled = test.Anonymous.Reads.front().Completion;
		cancelled(application::github_pull_request_cancelled{ .Request = first });
		test.drain();

		open_pull_request(test, 8U);
		expect(test.Anonymous.Reads.size() == 2U, "the second open issues one read");
		const auto second = test.Anonymous.Reads.back().Request;

		expect(first.ProfileAuthority.Profile == second.ProfileAuthority.Profile
				   && first.ProfileAuthority.ProfileRevision
					   == second.ProfileAuthority.ProfileRevision
				   && first.ProfileAuthority.Host == second.ProfileAuthority.Host
				   && first.Mode == second.Mode
				   && first.Context.IdentityGeneration
					   == second.Context.IdentityGeneration,
			   "the signed-out scope is stable across two opens");
		expect(first.Viewer.Account.value() == 0U
				   && second.Viewer.Account.value() == 0U,
			   "the signed-out scope carries no account");

		fixture account;
		static_cast<void>(
			activate_authenticated(account, core::host_profile_id{ 11U },
								   core::github_account_id{ 51U }));
		const auto& authenticated = account.Host->accepted_session();
		expect(authenticated.has_value()
				   && authenticated->Mode == core::github_access_mode::authenticated,
			   "the authenticated session is accepted in its own mode");
		expect(*authenticated != *test.Host->accepted_session(),
			   "no account session equals the signed-out one");
		expect(authenticated->Viewer.Account.value() != 0U,
			   "the authenticated session names an exact account");
	}

	auto test_a_second_open_and_a_cancellation_stay_bounded() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		open_pull_request(test, 8U);
		expect(test.Anonymous.Reads.size() == 1U,
			   "a concurrent second open is refused rather than issued");
		expect(test.Anonymous.Cancelled.empty(),
			   "a refused second open cancels nothing");

		const auto request    = test.Anonymous.Reads.front().Request;
		auto       completion = test.Anonymous.Reads.front().Completion;
		completion(application::github_pull_request_cancelled{ .Request = request });
		test.drain();
		expect(!test.Host->accepted_snapshot().has_value(),
			   "a cancelled read accepts nothing");
		expect(test.Vault.touched() == 0U, "a cancelled read touches no vault");

		open_pull_request(test, 8U);
		expect(test.Anonymous.Reads.size() == 2U,
			   "the next open proceeds once the first has terminated");
	}

	auto test_a_stale_completion_after_a_switch_is_rejected() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		const auto request    = test.Anonymous.Reads.front().Request;
		auto       completion = test.Anonymous.Reads.front().Completion;

		static_cast<void>(
			activate_authenticated(test, core::host_profile_id{ 11U },
								   core::github_account_id{ 51U }));
		expect(test.Anonymous.Cancelled.size() == 1U,
			   "the switch cancels the in-flight signed-out read exactly once");

		const auto acceptedBefore = test.Host->accepted_snapshot().has_value();
		completion(application::github_pull_request_cancelled{ .Request = request });
		test.drain();
		expect(test.Host->accepted_snapshot().has_value() == acceptedBefore,
			   "the stale completion accepts nothing");
		expect(test.Anonymous.Reads.size() == 1U,
			   "the stale completion issues no further signed-out read");
	}

	auto test_a_rate_refusal_is_typed_terminal_and_offers_sign_in() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		const auto request    = test.Anonymous.Reads.front().Request;
		auto       completion = test.Anonymous.Reads.front().Completion;

		auto budget             = rate_limit(application::github_rate_limit_resource::core);
		budget.Limit            = 60U;
		budget.Remaining        = 0U;
		budget.ResetUnixSeconds = 1'900'000'000U;
		completion(application::github_pull_request_error{
			.Request = request,
			.Error   = {
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = rate_limits_for(application::github_rate_limit_resource::core),
				.RateLimitCause    = budget,
			},
		});
		test.drain();

		expect(test.Anonymous.Reads.size() == 1U,
			   "a rate refusal is terminal: exactly one attempt is made");
		const auto& refusal = test.Host->public_preview_refusal();
		expect(refusal.has_value()
				   && refusal->Cause
					   == application::github_public_preview_refusal_cause::policy_refused,
			   "the rate refusal is recorded as a policy refusal");
		expect(refusal->Policy.has_value()
				   && refusal->Policy->Reason
					   == core::anonymous_refusal_reason::rate_limited,
			   "the rate refusal names the budget reason, not a network or authentication fault");
		expect(refusal->Policy->Budget.has_value()
				   && refusal->Policy->Budget->Limit == 60U
				   && refusal->Policy->Budget->Reset == 1'900'000'000U,
			   "the rate refusal carries the limit and the reset the server named");
		expect(core::well_formed(*refusal->Policy)
				   && core::terminal(*refusal->Policy),
			   "the rate refusal is well formed and terminal");
		expect(!test.Host->accepted_snapshot().has_value(),
			   "a rate refusal accepts nothing");
		expect(test.Vault.touched() == 0U,
			   "a rate refusal never reaches for a credential");
	}

	auto test_shutdown_cancels_an_inflight_signed_out_read() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		const auto request    = test.Anonymous.Reads.front().Request;
		auto       completion = test.Anonymous.Reads.front().Completion;

		test.Host->shutdown();
		expect(test.Host->is_closing(), "shutdown closes ingress");
		expect(test.Anonymous.Cancelled.size() == 1U,
			   "shutdown cancels the in-flight signed-out read exactly once");
		expect(test.Provider.CancelledPullRequests.empty(),
			   "shutdown cancels nothing on the authenticated provider");

		completion(application::github_pull_request_cancelled{ .Request = request });
		test.drain();
		expect(!test.Host->accepted_snapshot().has_value(),
			   "a late completion after shutdown accepts nothing");
		expect(test.Vault.touched() == 0U,
			   "a signed-out shutdown performs no vault cleanup");

		test.Host.reset();
		expect(test.Lane.pending() == 0U,
			   "a destroyed signed-out host leaves no posted work");
	}

	auto test_mode_switching_clears_what_an_account_switch_clears() -> void
	{
		cleared_state accountSwitch;
		{
			fixture test;
			static_cast<void>(
				activate_authenticated(test, core::host_profile_id{ 11U },
									   core::github_account_id{ 51U }));
			expect(test.Host->accepted_session().has_value(),
				   "the first account session is accepted");
			test.Host->dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			test.drain();
			accountSwitch = observed_clearing(test);
			expect(accountSwitch.Session && accountSwitch.Snapshot
					   && accountSwitch.MutationLease && accountSwitch.RateLimits
					   && accountSwitch.CapabilityCache == 0U,
				   "an account switch clears the accepted identity");
		}

		fixture test;
		auto    previousGeneration = test.Host->state().IdentityGeneration;
		for (auto round = 0U; round < 3U; ++round) {
			activate_signed_out(test, core::host_profile_id{ 11U });
			expect(test.Host->access_mode()
					   == core::github_access_mode::public_preview,
				   "the signed-out round enters public preview");
			expect(test.Host->state().IdentityGeneration.value()
					   > previousGeneration.value(),
				   "entering public preview advances the identity generation");
			previousGeneration = test.Host->state().IdentityGeneration;

			test.Host->dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			test.drain();
			const auto observed = observed_clearing(test);
			expect(observed == accountSwitch,
				   "leaving public preview clears exactly what an account switch clears");
			expect(test.Host->access_mode()
					   == core::github_access_mode::authenticated,
				   "leaving public preview leaves the authenticated mode behind");
			expect(test.Host->state().IdentityGeneration.value()
					   > previousGeneration.value(),
				   "leaving public preview advances the identity generation");
			previousGeneration = test.Host->state().IdentityGeneration;
			expect(!test.Host->public_preview_refusal().has_value(),
				   "a switch clears any earlier refusal");
			expect(test.Vault.Retrieves == 0U && test.Vault.Stores == 0U,
				   "no signed-out round reaches the vault across repeated switching");
		}
	}

	auto test_every_refused_intent_is_typed_before_a_request() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		const auto readsBefore = test.Anonymous.Reads.size();

		constexpr std::array refused{
			core::anonymous_read_intent::read_review_threads,
			core::anonymous_read_intent::read_viewer_identity,
			core::anonymous_read_intent::resolve_review_thread,
			core::anonymous_read_intent::unresolve_review_thread,
			core::anonymous_read_intent::set_viewed_file_state,
			core::anonymous_read_intent::publish_review_comment,
			core::anonymous_read_intent::submit_review,
			core::anonymous_read_intent::read_private_repository,
			core::anonymous_read_intent::select_enterprise_host,
			core::anonymous_read_intent::react_to_comment,
		};
		for (const auto intent : refused) {
			const auto  decision = test.Host->public_preview_legality(intent);
			const auto* refusal
				= std::get_if<core::anonymous_intent_refused>(&decision);
			expect(refusal != nullptr,
				   "a mutation, triage, queue, viewed, or resolution intent is refused signed out");
			expect(core::well_formed(refusal->Refusal),
				   "the refusal carries exactly its reason's facts");
		}
		expect(test.Anonymous.Reads.size() == readsBefore
				   && test.signed_out_requests() == 0U && test.Vault.touched() == 0U,
			   "refusing an intent issues no request of any kind");
		expect(test.Host->accepted_review_mutation_transport() == nullptr,
			   "no mutation transport exists signed out");

		for (const auto intent : core::anonymous_read_intent_catalog) {
			const auto authenticated
				= core::legality(core::github_access_mode::authenticated, intent);
			expect(std::holds_alternative<core::anonymous_intent_legal>(authenticated),
				   "this policy governs public preview alone");
		}
	}

	auto test_the_epoch_authority_is_unavailable_rather_than_failed() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		const auto request    = test.Anonymous.Reads.front().Request;
		auto       completion = test.Anonymous.Reads.front().Completion;

		application::github_pull_request_snapshot observed{
			.Profile    = request.Context.Profile,
			.Host       = request.Context.Host,
			.Account    = { },
			.Viewer     = { },
			.Snapshot   = core::provider_snapshot_id{ 91U },
			.Repository = {
				.Repository    = core::repository_id{ 71U },
				.Node          = application::github_node_id{ "repository-node" },
				.Owner         = request.Context.Repository.Owner,
				.Name          = request.Context.Repository.Name,
				.DefaultBranch = "main",
			},
			.PullRequest = {
				.PullRequest = core::pull_request_id{ 81U },
				.Repository  = core::repository_id{ 71U },
				.Number      = request.Context.Number,
				.Node        = application::github_node_id{ "pull-request-node" },
				.Title       = "Signed-out read",
				.Body        = "Bounded.",
				.State       = application::github_pull_request_state::open,
				.Draft       = false,
				.Author      = std::nullopt,
				.BaseOid     = application::commit_oid{ "1111111111111111111111111111111111111111" },
				.HeadOid     = application::commit_oid{ "2222222222222222222222222222222222222222" },
				.BaseRef     = "main",
				.HeadRef     = "preview",
			},
			.CapabilityEvidence = {
				.Capabilities    = request.CapabilityEvidence.Capabilities,
				.Revision        = request.CapabilityEvidence.Revision,
				.ReadShape       = request.CapabilityEvidence.ReadShape,
				.ContractVersion = 1U,
			},
			.RateLimits = anonymous_read_rate_limits(),
		};
		completion(application::github_pull_request_success{
			.Request  = request,
			.Snapshot = observed,
		});
		test.drain();

		const auto& accepted = test.Host->accepted_snapshot();
		expect(accepted.has_value(), "the signed-out snapshot is accepted");
		expect(accepted->Mode == core::github_access_mode::public_preview,
			   "the host stamps the mode it issued the read in");
		expect(accepted->Account.value() == 0U
				   && accepted->Viewer == application::github_actor_record{ },
			   "the accepted snapshot carries no reader");
		expect(test.Epochs.Accepted.empty(),
			   "the signed-out mode hands the epoch authority nothing");
		expect(test.Host->epoch_handoff_unavailable(),
			   "the epoch handoff reports the unavailable state");
		expect(!test.Host->epoch_handoff_delivery_failed(),
			   "sending nothing signed out is not a delivery failure");
		expect(test.Host->accepted_review_mutation_transport() == nullptr,
			   "an accepted signed-out snapshot acquires no mutation lease");
		expect(test.Vault.touched() == 0U,
			   "accepting a signed-out snapshot performs no vault work");
	}
}

auto main() -> int
{
	test_signed_out_activation_asks_nothing_of_the_account_course();
	test_enterprise_and_uncomposed_authority_refuse_typed();
	test_the_signed_out_scope_is_stable_and_never_an_account();
	test_a_second_open_and_a_cancellation_stay_bounded();
	test_a_stale_completion_after_a_switch_is_rejected();
	test_a_rate_refusal_is_typed_terminal_and_offers_sign_in();
	test_shutdown_cancels_an_inflight_signed_out_read();
	test_mode_switching_clears_what_an_account_switch_clears();
	test_every_refused_intent_is_typed_before_a_request();
	test_the_epoch_authority_is_unavailable_rather_than_failed();
	std::cout << "application anonymous read host tests passed\n";
	return EXIT_SUCCESS;
}
