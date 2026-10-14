#include "bpr/application/diff_application_host.hpp"
#include "product_diff.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The signed-out Git authority course. The read course next door proves what
// public preview never asks; this one proves what it does reach: a real epoch
// handoff whose authority is the anonymous one, a credential-free smart-Git
// lease bound to it, a cache scope no account shares, and a rendered
// selected-file diff at the end of it. The counters stay part of the evidence,
// because an anonymous authority that reached a vault would still be wrong.
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

	// The credential-free smart-Git transport composition supplies for public
	// preview. It answers the protocol without ever being handed a credential,
	// and records every context it was asked for so a test can prove the
	// authority the lease carried rather than assume it.
	class fake_anonymous_transport final : public application::git_smart_transport_port
	{
	public:
		auto revoke() noexcept -> void override { Revoked = true; }

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

		[[nodiscard]] auto discover(
			const application::git_smart_discovery_request&,
			std::stop_token)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response> override
		{
			return application::git_smart_discovery_response{ };
		}

		[[nodiscard]] auto ls_refs(const application::git_smart_ls_refs_request&,
								   std::stop_token)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response> override
		{
			return application::git_smart_ls_refs_response{ };
		}

		[[nodiscard]] auto fetch(const application::git_smart_fetch_request&,
								 application::git_smart_wire_consumer&,
								 std::stop_token)
			-> application::git_smart_transport_result<void> override
		{
			return { };
		}

		bool Revoked = false;
	};

	class fake_anonymous_smart_git final
		: public application::git_smart_transport_factory_port
	{
	public:
		[[nodiscard]] auto acquire(
			const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<
				application::git_smart_transport> override
		{
			Contexts.push_back(aContext);
			auto transport = std::make_shared<fake_anonymous_transport>();
			Issued.push_back(transport);
			return transport;
		}

		std::vector<application::git_smart_request_context>    Contexts;
		std::vector<std::shared_ptr<fake_anonymous_transport>> Issued;
	};

	struct fixture
	{
		explicit fixture(const bool aComposeAnonymousAuthority = true,
						 const bool aComposeAnonymousSmartGit  = true)
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
				aComposeAnonymousAuthority ? &Anonymous : nullptr,
				aComposeAnonymousSmartGit ? &SmartGit : nullptr);
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
		fake_anonymous_smart_git                              SmartGit;
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

	// The diff lane, copied from the diff host course so the anonymous
	// authority is carried into a real rendering rather than described. Only
	// the authority changes here; every other fact is the sibling course.
	namespace diff_lane {
		template <typename Identity>
		[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
		{
			std::array<std::uint8_t, 32U> bytes{ };
			bytes[0]  = aSeed;
			bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
			return Identity{ bytes };
		}

		[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
			-> core::git_blob_oid
		{
			std::array<std::uint8_t, 32U> bytes{ };
			for (std::size_t index = 0U; index < 20U; ++index) {
				bytes[index] = static_cast<std::uint8_t>(aSeed + index);
			}
			return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
		}

		[[nodiscard]] auto bytes(const std::string_view aText)
			-> std::vector<std::uint8_t>
		{
			return { aText.begin(), aText.end() };
		}

		[[nodiscard]] auto comparison_key(
			const core::diff_options& aOptions  = { },
			const std::uint8_t        aFileSeed = 31U,
			const bool                aHasLeft  = true,
			const bool                aHasRight = true) -> core::file_comparison_key
		{
			const auto digest = core::options_digest(aOptions);
			expect(digest.has_value(), "test options normalize");
			const auto scope       = opaque<core::pull_request_authority_scope_id>(11U);
			const auto incarnation = opaque<core::epoch_catalog_incarnation_id>(12U);
			const auto authority   = core::pull_request_authority{
				.Profile         = core::host_profile_id{ 10U },
				.ProfileRevision = core::github_profile_revision{ 20U },
				.Host            = core::git_host_id{ 30U },
				.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
				.Account         = core::github_account_id{ 40U },
				.Viewer          = core::github_viewer_actor_id{ 50U },
				.Repository      = core::repository_id{ 101U },
				.Cache           = opaque<core::repository_cache_scope_id>(14U),
				.PullRequest     = core::pull_request_id{ 202U },
				.Number          = core::pull_request_number{ 17U },
				.Scope           = scope,
			};
			return core::file_comparison_key{
				.Authority   = authority,
				.Repository  = authority.Repository,
				.PullRequest = authority.PullRequest,
				.LeftAnchor  = core::comparison_anchor_identity{
					.Authority   = scope,
					.Incarnation = incarnation,
					.Kind        = core::comparison_anchor_kind::epoch_base,
					.Epoch       = core::epoch_ordinal{ 1 },
				},
				.RightAnchor = core::comparison_anchor_identity{
					.Authority   = scope,
					.Incarnation = incarnation,
					.Kind        = core::comparison_anchor_kind::epoch_head,
					.Epoch       = core::epoch_ordinal{ 2 },
				},
				.LeftBlob  = aHasLeft ? std::optional{ blob(21U) } : std::nullopt,
				.RightBlob = aHasRight ? std::optional{ blob(51U) } : std::nullopt,
				.File      = opaque<core::file_identity>(aFileSeed),
				.Options   = *digest,
				.Algorithm = core::supported_diff_algorithm_revision,
			};
		}

		class fake_blobs final : public application::diff_blob_port
		{
		public:
			[[nodiscard]] auto materialize(
				const application::diff_blob_request& aRequest,
				const std::stop_token                 aStop)
				-> application::diff_blob_observation override
			{
				Requests.push_back(aRequest);
				StopObserved.push_back(aStop.stop_requested());
				if (Throw) {
					throw std::runtime_error{ "blob failure" };
				}
				auto observation = application::diff_blob_observation{
					.Context = aRequest.Context,
					.Side    = aRequest.Side,
					.Blob    = aRequest.Blob,
					.Status  = application::diff_blob_status::available,
				};
				if (DeclaredBlobBytes.has_value()
					&& *DeclaredBlobBytes > aRequest.MaximumBytes) {
					RefusedBeforeAllocation = true;
					observation.Status      = application::diff_blob_status::materialization_limit;
					observation.Bytes.clear();
					return observation;
				}
				observation.Bytes = aRequest.Side == core::diff_side::left ? LeftBytes
																		   : RightBytes;
				if (Status.has_value()
					&& (!StatusSide.has_value() || *StatusSide == aRequest.Side)) {
					observation.Status = *Status;
					if (observation.Status != application::diff_blob_status::available) {
						observation.Bytes.clear();
					}
				}
				if (MismatchContext) {
					observation.Context.Generation = core::request_generation{ 999U };
				}
				if (MismatchSide) {
					observation.Side = aRequest.Side == core::diff_side::left
						? core::diff_side::right
						: core::diff_side::left;
				}
				if (MismatchBlob) {
					observation.Blob = blob(99U);
				}
				return observation;
			}

			void reset_behavior()
			{
				Status.reset();
				StatusSide.reset();
				Throw           = false;
				MismatchContext = false;
				MismatchSide    = false;
				MismatchBlob    = false;
				DeclaredBlobBytes.reset();
				RefusedBeforeAllocation = false;
				LeftBytes               = bytes("left\ncommon\n");
				RightBytes              = bytes("right\ncommon\n");
			}

			std::vector<application::diff_blob_request>  Requests;
			std::vector<bool>                            StopObserved;
			std::vector<std::uint8_t>                    LeftBytes  = bytes("left\ncommon\n");
			std::vector<std::uint8_t>                    RightBytes = bytes("right\ncommon\n");
			std::optional<application::diff_blob_status> Status;
			std::optional<core::diff_side>               StatusSide;
			bool                                         Throw           = false;
			bool                                         MismatchContext = false;
			bool                                         MismatchSide    = false;
			bool                                         MismatchBlob    = false;
			std::optional<std::uint64_t>                 DeclaredBlobBytes;
			bool                                         RefusedBeforeAllocation = false;
		};

		class recording_projection final : public application::diff_projection_port
		{
		public:
			struct result_record
			{
				core::diff_request_context Context;
				core::computed_file_diff   Result;
				bool                       CacheHit = false;
			};

			void publish(const application::diff_publication& aPublication) override
			{
				if (Throw) {
					throw std::runtime_error{ "projection failure" };
				}
				begin_delivery();
				expect(aPublication.Result != nullptr, "success has immutable result");
				Results.push_back(result_record{
					.Context  = aPublication.Context,
					.Result   = *aPublication.Result,
					.CacheHit = aPublication.CacheHit,
				});
				run_callback(OnPublish);
				end_delivery();
			}

			void fail(const application::diff_failure_publication& aFailure) override
			{
				if (Throw) {
					throw std::runtime_error{ "projection failure" };
				}
				begin_delivery();
				Failures.push_back(aFailure);
				run_callback(OnFail);
				end_delivery();
			}

			void invalidate(
				const core::pull_request_authority& aAuthority) override
			{
				if (Throw) {
					throw std::runtime_error{ "projection failure" };
				}
				begin_delivery();
				Invalidated.push_back(aAuthority);
				run_callback(OnInvalidate);
				end_delivery();
			}

			void begin_delivery()
			{
				NestedDelivery = NestedDelivery || Delivering;
				Delivering     = true;
			}

			void end_delivery()
			{
				Delivering = false;
			}

			static void run_callback(std::function<void()>& aCallback)
			{
				auto current = std::move(aCallback);
				aCallback    = { };
				if (current) {
					current();
				}
			}

			std::vector<result_record>                         Results;
			std::vector<application::diff_failure_publication> Failures;
			std::vector<core::pull_request_authority>          Invalidated;
			bool                                               Throw          = false;
			bool                                               Delivering     = false;
			bool                                               NestedDelivery = false;
			std::function<void()>                              OnPublish;
			std::function<void()>                              OnFail;
			std::function<void()>                              OnInvalidate;
		};

		struct fixture
		{
			explicit fixture(
				const application::diff_host_limits aLimits        = { },
				const core::diff_compute_limits     aComputeLimits = { })
				: Host{ std::make_unique<application::diff_application_host>(
					  core::diff_state{
						  .ApplicationSession = core::application_session_id{ 71U },
					  },
					  Blobs,
					  Effects,
					  Decisions,
					  Projections,
					  bpr::tests::product_line_matcher(),
					  aLimits,
					  aComputeLimits) }
			{
			}

			~fixture()
			{
				Host->shutdown();
				Effects.drain();
				Decisions.drain();
			}

			void request(const core::file_comparison_key& aKey,
						 const core::diff_options&        aOptions = { })
			{
				Host->request(aKey, aOptions);
			}

			void run_effect_and_decision()
			{
				expect(Effects.pending() == 1U, "one effect is pending");
				Effects.run_next();
				expect(Decisions.pending() == 1U,
					   "effect posts one serialized terminal");
				Decisions.run_next();
			}

			fake_blobs                                          Blobs;
			application::manual_executor                        Effects;
			application::manual_executor                        Decisions;
			recording_projection                                Projections;
			std::unique_ptr<application::diff_application_host> Host;
		};
	}

	[[nodiscard]] auto snapshot_for(
		const application::github_pull_request_provider_request& aRequest,
		const core::github_account_id                            aAccount = { },
		const application::github_actor_record&                  aViewer  = { })
		-> application::github_pull_request_snapshot
	{
		return {
			.Profile    = aRequest.Context.Profile,
			.Host       = aRequest.Context.Host,
			.Account    = aAccount,
			.Viewer     = aViewer,
			.Snapshot   = core::provider_snapshot_id{ 91U },
			.Repository = {
				.Repository    = core::repository_id{ 71U },
				.Node          = application::github_node_id{ "repository-node" },
				.Owner         = aRequest.Context.Repository.Owner,
				.Name          = aRequest.Context.Repository.Name,
				.DefaultBranch = "main",
			},
			.PullRequest = {
				.PullRequest = core::pull_request_id{ 81U },
				.Repository  = core::repository_id{ 71U },
				.Number      = aRequest.Context.Number,
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
				.Capabilities    = aRequest.CapabilityEvidence.Capabilities,
				.Revision        = aRequest.CapabilityEvidence.Revision,
				.ReadShape       = aRequest.CapabilityEvidence.ReadShape,
				.ContractVersion = 1U,
			},
			.RateLimits = anonymous_read_rate_limits(),
		};
	}

	// Answers the signed-out read the host issued. The snapshot names no
	// reader, which is what the credential-free lane observed.
	auto complete_signed_out_read(fixture& aFixture) -> void
	{
		expect(!aFixture.Anonymous.Reads.empty(), "a signed-out read is pending");
		const auto request    = aFixture.Anonymous.Reads.back().Request;
		auto       completion = aFixture.Anonymous.Reads.back().Completion;
		completion(application::github_pull_request_success{
			.Request  = request,
			.Snapshot = snapshot_for(request),
		});
		aFixture.drain();
	}

	// The same pull request read by an account. Only the reader changes, so
	// the comparison against the signed-out handoff is a comparison of
	// authorities rather than of two unrelated fixtures.
	auto complete_authenticated_read(fixture& aFixture) -> void
	{
		expect(!aFixture.Provider.PullRequests.empty(),
			   "an authenticated read is pending");
		const auto request    = aFixture.Provider.PullRequests.back().Request;
		auto       completion = aFixture.Provider.PullRequests.back().Completion;
		auto       observed   = snapshot_for(request,
											 request.Context.Account,
											 application::github_actor_record{
												 .Actor = request.Viewer.Node,
												 .Login = request.Viewer.Login,
											 });
		completion(application::github_pull_request_success{
			.Request  = request,
			.Snapshot = observed,
		});
		aFixture.drain();
	}

	auto test_the_signed_out_handoff_names_the_anonymous_authority() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		complete_signed_out_read(test);

		expect(!test.Host->epoch_handoff_unavailable(),
			   "a composed anonymous Git authority makes the handoff available");
		expect(!test.Host->epoch_handoff_delivery_failed(),
			   "the anonymous handoff is delivered rather than failed");
		expect(test.Epochs.Accepted.size() == 1U,
			   "the signed-out mode hands the epoch authority exactly one handoff");

		const auto& handoff = test.Epochs.Accepted.front();
		expect(handoff.Mode == core::github_access_mode::public_preview,
			   "the handoff carries the mode it was read in");
		expect(handoff.Account.value() == 0U && handoff.Viewer.value() == 0U,
			   "the anonymous authority names no account and no viewer");
		expect(!handoff.CurrentViewerReview.has_value(),
			   "a signed-out handoff carries no current viewer review");
		expect(application::valid(handoff),
			   "the anonymous handoff satisfies its own contract");
		expect(handoff.SmartGit != nullptr && !handoff.SmartGit->revoked(),
			   "the handoff carries a live credential-free smart-Git lease");

		const auto& binding = handoff.SmartGit->binding();
		expect(binding.Mode == core::github_access_mode::public_preview
				   && binding.Account.value() == 0U,
			   "the lease binding carries the anonymous authority, not an account's");
		expect(application::valid(binding) && application::agrees(binding, handoff),
			   "the binding is valid and agrees with the handoff it was minted for");

		expect(test.Vault.touched() == 0U,
			   "the anonymous Git authority performs no vault work");
		expect(test.Provider.Viewers.empty(),
			   "the anonymous Git authority issues no /user request");
		expect(test.Provider.Capabilities.empty() && test.Provider.PullRequests.empty(),
			   "no account-course provider request is issued at all");
	}

	auto test_null_git_ports_keep_the_handoff_typed_unavailable() -> void
	{
		fixture test{ true, false };
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		complete_signed_out_read(test);

		expect(test.Host->accepted_snapshot().has_value(),
			   "the signed-out snapshot is still accepted");
		expect(test.Epochs.Accepted.empty(),
			   "an uncomposed anonymous Git authority hands the epoch host nothing");
		expect(test.Host->epoch_handoff_unavailable(),
			   "the handoff reports the typed unavailable state");
		expect(!test.Host->epoch_handoff_delivery_failed(),
			   "sending nothing is not a delivery failure");
		expect(test.SmartGit.Contexts.empty(),
			   "no transport is acquired when no anonymous Git authority is composed");
		expect(test.Vault.touched() == 0U,
			   "the unavailable path performs no vault work either");
	}

	auto test_the_authenticated_authority_is_a_different_one() -> void
	{
		fixture signedOut;
		activate_signed_out(signedOut, core::host_profile_id{ 11U });
		open_pull_request(signedOut, 7U);
		complete_signed_out_read(signedOut);

		fixture authenticated;
		activate_authenticated(
			authenticated, core::host_profile_id{ 11U }, core::github_account_id{ 51U });
		open_pull_request(authenticated, 7U);
		complete_authenticated_read(authenticated);

		expect(signedOut.Epochs.Accepted.size() == 1U
				   && authenticated.Epochs.Accepted.size() == 1U,
			   "both modes hand the epoch authority one handoff for the same pull request");
		const auto& anonymous = signedOut.Epochs.Accepted.front();
		const auto& account   = authenticated.Epochs.Accepted.front();
		expect(anonymous.Profile == account.Profile
				   && anonymous.Host == account.Host
				   && anonymous.Repository == account.Repository
				   && anonymous.PullRequest == account.PullRequest,
			   "the two handoffs name the same pull request");
		expect(account.Mode == core::github_access_mode::authenticated
				   && account.Account.value() != 0U && account.Viewer.value() != 0U,
			   "the authenticated handoff names an exact reader");
		expect(anonymous.Mode != account.Mode
				   && anonymous.Account != account.Account
				   && anonymous.Viewer != account.Viewer,
			   "the same pull request read in two modes is two authorities");
		expect(application::authority_binding(anonymous)
				   != application::authority_binding(account),
			   "the smart-Git binding differs with the mode");
		expect(account.SmartGit == nullptr,
			   "the account lease is not a smart-Git factory in this fixture");
	}

	auto test_a_mode_switch_invalidates_the_anonymous_epoch_authority() -> void
	{
		fixture test;
		activate_signed_out(test, core::host_profile_id{ 11U });
		open_pull_request(test, 7U);
		complete_signed_out_read(test);
		expect(test.Epochs.Accepted.size() == 1U, "the anonymous handoff is delivered");
		expect(test.Epochs.Invalidated.empty(),
			   "nothing is invalidated before the mode changes");

		test.Host->enroll_profile(core::host_profile_id{ 11U }, credential());
		test.drain();

		expect(test.Epochs.Invalidated.size() == 1U,
			   "leaving public preview invalidates exactly one epoch authority");
		const auto& invalidated = test.Epochs.Invalidated.front();
		expect(invalidated.Mode == core::github_access_mode::public_preview,
			   "the invalidation names the anonymous authority that was active");
		expect(invalidated.Account.value() == 0U && invalidated.Viewer.value() == 0U,
			   "the invalidated authority carries no reader");
		expect(application::valid(invalidated),
			   "the anonymous invalidation satisfies its contract");
		expect(test.Epochs.Accepted.front().SmartGit->revoked(),
			   "the credential-free lease is revoked with the authority");
		expect(!test.Host->accepted_snapshot().has_value()
				   && !test.Host->epoch_handoff_delivery_failed(),
			   "the cleared host holds no snapshot and reports no delivery failure");
		expect(test.Epochs.Accepted.size() == 1U,
			   "leaving the mode delivers no second handoff");
	}

	// The mode-scoped authority the epoch catalog mints for each mode. The two
	// differ in the discriminant and in every scope derived from it, which is
	// exactly what must keep a signed-out read out of an account's cache.
	[[nodiscard]] auto mode_scoped_key(const core::github_access_mode aMode)
		-> core::file_comparison_key
	{
		auto       key       = diff_lane::comparison_key();
		const auto anonymous = aMode == core::github_access_mode::public_preview;
		auto&      authority = key.Authority;
		authority.Mode       = aMode;
		authority.Account    = anonymous ? core::github_account_id{ }
										 : core::github_account_id{ 40U };
		authority.Viewer     = anonymous ? core::github_viewer_actor_id{ }
										 : core::github_viewer_actor_id{ 50U };
		authority.Cache      = diff_lane::opaque<core::repository_cache_scope_id>(
			anonymous ? 114U : 14U);
		authority.Scope = diff_lane::opaque<core::pull_request_authority_scope_id>(
			anonymous ? 111U : 11U);
		key.LeftAnchor.Authority  = authority.Scope;
		key.RightAnchor.Authority = authority.Scope;
		expect(core::valid(authority), "the mode-scoped authority is valid");
		return key;
	}

	auto test_the_anonymous_authority_renders_a_selected_file_diff() -> void
	{
		diff_lane::fixture test;
		const auto         anonymous = mode_scoped_key(core::github_access_mode::public_preview);
		test.request(anonymous);
		test.run_effect_and_decision();

		expect(test.Projections.Results.size() == 1U,
			   "the anonymous authority publishes one rendered result");
		const auto& rendered = test.Projections.Results.back();
		expect(!rendered.CacheHit, "the first anonymous render is a miss");
		expect(rendered.Context.Key.Authority.Mode
				   == core::github_access_mode::public_preview,
			   "the rendered result is keyed to the anonymous authority");
		expect(rendered.Result.LeftDocument.has_value()
				   && rendered.Result.RightDocument.has_value(),
			   "both sides of the selected file are decoded");
		expect(rendered.Result.LeftDocument != rendered.Result.RightDocument,
			   "the rendered diff carries the change the two sides differ by");
		const auto anonymousCalls = test.Blobs.Requests.size();
		expect(anonymousCalls == 2U,
			   "the anonymous render materializes exactly the two selected sides");
		for (const auto& request : test.Blobs.Requests) {
			expect(request.Context.Key.Authority.Mode
					   == core::github_access_mode::public_preview,
				   "every blob is materialized under the anonymous authority");
			expect(request.Context.Key.Authority.Account.value() == 0U,
				   "no blob is materialized under an account");
		}

		const auto account = mode_scoped_key(core::github_access_mode::authenticated);
		expect(account.Authority.Cache != anonymous.Authority.Cache
				   && account.Authority.Scope != anonymous.Authority.Scope,
			   "the two modes name different cache and authority scopes");
		test.request(account);
		expect(test.Effects.pending() == 1U,
			   "the authenticated read of the same file cannot hit the anonymous entry");
		test.run_effect_and_decision();
		expect(test.Projections.Results.size() == 2U
				   && !test.Projections.Results.back().CacheHit
				   && test.Host->cache_entries() == 2U
				   && test.Blobs.Requests.size() == anonymousCalls + 2U,
			   "each mode renders and caches under its own authority");
	}
}

auto main() -> int
{
	test_the_signed_out_handoff_names_the_anonymous_authority();
	test_null_git_ports_keep_the_handoff_typed_unavailable();
	test_the_authenticated_authority_is_a_different_one();
	test_a_mode_switch_invalidates_the_anonymous_epoch_authority();
	test_the_anonymous_authority_renders_a_selected_file_diff();
	std::cout << "application anonymous diff authority tests passed\n";
	return EXIT_SUCCESS;
}
