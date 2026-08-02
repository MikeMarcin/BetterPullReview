#include "bpr/application/github_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Type>
	concept exposes_requests = requires(const Type& aValue) {
		aValue.Requests;
	};

	template <typename Type>
	concept exposes_accepted_session = requires(const Type& aValue) {
		aValue.AcceptedSession;
	};

	template <typename Type>
	concept exposes_accepted_snapshot = requires(const Type& aValue) {
		aValue.AcceptedSnapshot;
	};

	template <typename Type>
	concept exposes_viewer = requires(const Type& aValue) {
		aValue.Viewer;
	};

	template <typename Type>
	concept exposes_capability_evidence = requires(const Type& aValue) {
		aValue.CapabilityEvidence;
	};

	template <typename Type>
	concept exposes_account_transport = requires(const Type& aValue) {
		aValue.AccountTransport;
	};

	template <typename Type>
	concept exposes_credential = requires(const Type& aValue) {
		aValue.Credential;
	};

	template <typename Type>
	concept exposes_detail = requires(const Type& aValue) {
		aValue.Detail;
	};

	class fake_smart_transport final
		: public application::git_smart_transport_port
	{
	public:
		void revoke() noexcept override { Revoked = true; }

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
			return unavailable<application::git_smart_discovery_response>();
		}

		[[nodiscard]] auto ls_refs(
			const application::git_smart_ls_refs_request&,
			std::stop_token)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response> override
		{
			return unavailable<application::git_smart_ls_refs_response>();
		}

		[[nodiscard]] auto fetch(
			const application::git_smart_fetch_request&,
			application::git_smart_wire_consumer&,
			std::stop_token)
			-> application::git_smart_transport_result<void> override
		{
			return unavailable<void>();
		}

	private:
		template <typename Value>
		[[nodiscard]] auto unavailable() const
			-> application::git_smart_transport_result<Value>
		{
			return std::unexpected{ application::git_smart_transport_error{
				.Failure = Revoked
					? application::git_smart_transport_failure::cancelled
					: application::git_smart_transport_failure::unavailable,
			} };
		}

		bool Revoked = false;
	};

	struct fake_lease final
		: application::github_account_transport_lease,
		  application::git_smart_transport_factory_port
	{
		[[nodiscard]] auto acquire(
			const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<
				application::git_smart_transport> override
		{
			AcquiredContexts.push_back(aContext);
			LastTransport = std::make_shared<fake_smart_transport>();
			return LastTransport;
		}

		std::vector<application::git_smart_request_context> AcquiredContexts;
		std::shared_ptr<fake_smart_transport>               LastTransport;
	};

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	class fake_provider final : public application::github_provider_port
	{
	public:
		struct profile_call
		{
			application::github_profile_request            Request;
			application::github_profile_completion_handler Completion;
		};

		enum class viewer_authentication {
			cli,
			bootstrap,
			account,
		};

		struct viewer_call
		{
			application::github_viewer_request            Request;
			viewer_authentication                         Authentication = viewer_authentication::cli;
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
			if (ThrowOnProfileSubmit) {
				throw std::runtime_error{ "profile submit" };
			}
			Profiles.push_back({
				.Request    = aRequest,
				.Completion = std::move(aCompletion),
			});
		}

		void cancel_profile(application::github_profile_request aRequest) override
		{
			CancelledProfiles.push_back(aRequest);
			if (CompleteProfileFromWorkerOnCancel) {
				const auto found = std::ranges::find(Profiles, aRequest, &profile_call::Request);
				expect(found != Profiles.end(), "cancelled profile call exists");
				auto        completion = found->Completion;
				std::thread worker{
					[completion = std::move(completion), aRequest]() mutable {
						completion(application::github_profile_cancelled{
							.Request = aRequest,
						});
					}
				};
				worker.join();
			}
		}

		void observe_viewer(
			application::github_viewer_submission         aSubmission,
			application::github_viewer_completion_handler aCompletion) override
		{
			auto authentication = viewer_authentication::cli;
			if (std::holds_alternative<application::github_bootstrap_credential>(
					aSubmission.Authentication)) {
				authentication = viewer_authentication::bootstrap;
			} else if (std::holds_alternative<
						   application::github_credential_retrieve_success>(
						   aSubmission.Authentication)) {
				authentication = viewer_authentication::account;
			}
			Viewers.push_back({
				.Request        = aSubmission.Request,
				.Authentication = authentication,
				.Completion     = std::move(aCompletion),
			});
		}

		void cancel_viewer(application::github_viewer_request aRequest) override
		{
			CancelledViewers.push_back(aRequest);
		}

		void probe_capabilities(
			application::github_capability_request            aRequest,
			application::github_capability_completion_handler aCompletion) override
		{
			Capabilities.push_back({
				.Request    = aRequest,
				.Completion = std::move(aCompletion),
			});
		}

		void
		cancel_capabilities(application::github_capability_request aRequest) override
		{
			CancelledCapabilities.push_back(aRequest);
		}

		void fetch_pull_request(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion) override
		{
			PullRequests.push_back({
				.Request    = aRequest,
				.Completion = std::move(aCompletion),
			});
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
		bool ThrowOnProfileSubmit              = false;
		bool CompleteProfileFromWorkerOnCancel = false;
	};

	class fake_vault final : public application::github_credential_vault_port
	{
	public:
		struct store_call
		{
			application::github_credential_operation_context        Context;
			application::github_credential_store_completion_handler Completion;
		};

		struct retrieve_call
		{
			application::github_credential_operation_context           Context;
			application::github_credential_retrieve_completion_handler Completion;
		};

		struct delete_call
		{
			application::github_credential_operation_context         Context;
			application::github_credential_delete_completion_handler Completion;
		};

		void store(application::github_credential_enrollment aEnrollment,
				   application::github_credential_store_completion_handler
					   aCompletion) override
		{
			if (ThrowOnStore) {
				throw std::runtime_error{ "vault store" };
			}
			StoredCredentialBytes.push_back(aEnrollment.Credential.size());
			Stores.push_back({
				.Context    = aEnrollment.Context,
				.Completion = std::move(aCompletion),
			});
		}

		void retrieve(application::github_credential_operation_context aContext,
					  application::github_credential_retrieve_completion_handler
						  aCompletion) override
		{
			Retrieves.push_back({
				.Context    = aContext,
				.Completion = std::move(aCompletion),
			});
		}

		void
		delete_credential(application::github_credential_operation_context aContext,
						  application::github_credential_delete_completion_handler
							  aCompletion) override
		{
			Deletes.push_back({
				.Context    = aContext,
				.Completion = std::move(aCompletion),
			});
		}

		void
		cancel(application::github_credential_operation_context aContext) override
		{
			Cancellations.push_back(aContext);
		}

		std::vector<store_call>                                       Stores;
		std::vector<retrieve_call>                                    Retrieves;
		std::vector<delete_call>                                      Deletes;
		std::vector<std::size_t>                                      StoredCredentialBytes;
		std::vector<application::github_credential_operation_context> Cancellations;
		bool                                                          ThrowOnStore = false;
	};

	class throwing_once_lane final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			if (ThrowNext) {
				ThrowNext = false;
				throw std::runtime_error{ "decision post" };
			}
			Inner.post(std::move(aTask));
		}

		[[nodiscard]] auto pending() const noexcept -> std::size_t
		{
			return Inner.pending();
		}

		void run_next()
		{
			Inner.run_next();
		}

	private:
		application::manual_executor Inner;
		bool                         ThrowNext = true;
	};

	class recording_projections final : public application::github_projection_port
	{
	public:
		void publish(
			const application::github_presentation_update& aUpdate) override
		{
			if (aUpdate.Transition.has_value() && ThrowNextOutput) {
				ThrowNextOutput = false;
				throw std::runtime_error{ "transition projection" };
			}
			if (!aUpdate.HostDiagnostics.empty() && ThrowNextDiagnostic) {
				ThrowNextDiagnostic = false;
				throw std::runtime_error{ "diagnostic projection" };
			}
			if (aUpdate.Transition.has_value() && OnNextOutput) {
				auto action = std::exchange(OnNextOutput, std::function<void()>{ });
				action();
			}
			if (aUpdate.Transition.has_value() && OnEveryOutput) {
				OnEveryOutput();
			}
			Updates.push_back(aUpdate);
		}

		[[nodiscard]] auto transition_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(
				Updates,
				[](const auto& aUpdate) { return aUpdate.Transition.has_value(); }));
		}

		[[nodiscard]] auto session_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(
				Updates,
				[](const auto& aUpdate) { return aUpdate.Session.has_value(); }));
		}

		[[nodiscard]] auto pull_request_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(
				Updates,
				[](const auto& aUpdate) { return aUpdate.PullRequest.has_value(); }));
		}

		[[nodiscard]] auto host_diagnostic_count() const noexcept -> std::size_t
		{
			std::size_t count = 0U;
			for (const auto& update : Updates) {
				count += update.HostDiagnostics.size();
			}
			return count;
		}

		[[nodiscard]] auto last_session_update() const
			-> const application::github_presentation_update&
		{
			const auto found = std::ranges::find_if(
				Updates.rbegin(),
				Updates.rend(),
				[](const auto& aUpdate) { return aUpdate.Session.has_value(); });
			expect(found != Updates.rend(), "a session projection update exists");
			return *found;
		}

		[[nodiscard]] auto last_transition_update() const
			-> const application::github_presentation_update&
		{
			const auto found = std::ranges::find_if(
				Updates.rbegin(),
				Updates.rend(),
				[](const auto& aUpdate) { return aUpdate.Transition.has_value(); });
			expect(
				found != Updates.rend(),
				"a transition projection update exists");
			return *found;
		}

		[[nodiscard]] auto last_pull_request_update() const
			-> const application::github_presentation_update&
		{
			const auto found = std::ranges::find_if(
				Updates.rbegin(),
				Updates.rend(),
				[](const auto& aUpdate) { return aUpdate.PullRequest.has_value(); });
			expect(
				found != Updates.rend(),
				"a pull-request projection update exists");
			return *found;
		}

		[[nodiscard]] auto last_host_diagnostic() const
			-> const application::github_host_diagnostic_projection&
		{
			for (auto update = Updates.rbegin(); update != Updates.rend(); ++update) {
				if (!update->HostDiagnostics.empty()) {
					return update->HostDiagnostics.back();
				}
			}
			expect(false, "a host diagnostic projection exists");
			std::abort();
		}

		std::vector<application::github_presentation_update> Updates;
		bool                                                 ThrowNextOutput     = false;
		bool                                                 ThrowNextDiagnostic = false;
		std::function<void()>                                OnNextOutput;
		std::function<void()>                                OnEveryOutput;
	};

	class recording_epoch_handoffs final
		: public application::github_epoch_handoff_port
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
			if (ThrowInvalidation) {
				throw std::runtime_error{ "injected epoch invalidation failure" };
			}
		}

		std::vector<application::accepted_pull_request_handoff>       Accepted;
		std::vector<application::pull_request_authority_invalidation> Invalidated;
		bool                                                          ThrowInvalidation = false;
	};

	[[nodiscard]] auto initial_state() -> core::github_state
	{
		return core::github_state{
			.ApplicationSession    = core::application_session_id{ 41U },
			.NextRequest           = core::request_id{ 1U },
			.ActiveProfile         = std::nullopt,
			.IdentityGeneration    = { },
			.PendingSession        = std::nullopt,
			.Host                  = std::nullopt,
			.Account               = std::nullopt,
			.Capabilities          = { },
			.CapabilityRevision    = { },
			.PullRequestGeneration = { },
			.PendingPullRequest    = std::nullopt,
			.ActiveLocator         = std::nullopt,
			.ActiveNumber          = std::nullopt,
			.Repository            = std::nullopt,
			.PullRequest           = std::nullopt,
			.AcceptedSnapshot      = std::nullopt,
		};
	}

	[[nodiscard]] auto native_profile(const core::host_profile_id aProfile)
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

	[[nodiscard]] auto cli_profile(const core::host_profile_id aProfile)
		-> application::github_profile_authority
	{
		auto result           = native_profile(aProfile);
		result.Authentication = application::github_authentication_transport::github_cli;
		return result;
	}

	[[nodiscard]] auto ghes_cli_profile(const core::host_profile_id aProfile)
		-> application::github_profile_authority
	{
		auto result           = cli_profile(aProfile);
		result.Hostname       = application::github_canonical_hostname{ "ghe.example.invalid" };
		result.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		return result;
	}

	[[nodiscard]] auto ghes_native_profile(
		const core::host_profile_id aProfile,
		const std::string_view      aHostname = "ghe.example.invalid")
		-> application::github_profile_authority
	{
		auto result           = native_profile(aProfile);
		result.Hostname       = application::github_canonical_hostname{ aHostname };
		result.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		return result;
	}

	[[nodiscard]] auto
	viewer(const core::github_account_id aAccount = core::github_account_id{ 51U })
		-> application::github_viewer_record
	{
		return {
			.Account     = aAccount,
			.Node        = application::github_actor_id{ "viewer-node" },
			.Login       = "reviewer",
			.DisplayName = "Review Engineer",
		};
	}

	[[nodiscard]] auto
	rate_limit(const application::github_rate_limit_resource aResource = application::github_rate_limit_resource::core)
		-> application::github_rate_limit_record
	{
		return {
			.Resource         = aResource,
			.Limit            = 5'000U,
			.Remaining        = 4'999U,
			.ResetUnixSeconds = 2'000'000'000U,
		};
	}

	[[nodiscard]] auto rate_limits() -> application::github_rate_limit_state
	{
		auto search       = rate_limit(application::github_rate_limit_resource::search);
		search.Limit      = 30U;
		search.Remaining  = 29U;
		auto graphql      = rate_limit(application::github_rate_limit_resource::graphql);
		graphql.Remaining = 4'998U;
		return {
			.Core    = rate_limit(application::github_rate_limit_resource::core),
			.Search  = search,
			.Graphql = graphql,
		};
	}

	[[nodiscard]] auto projected_rate_limit(
		const application::github_rate_limit_record& aRecord)
		-> application::github_rate_limit_projection
	{
		application::github_rate_limit_resource_projection resource = application::github_rate_limit_resource_projection::core;
		if (aRecord.Resource == application::github_rate_limit_resource::search) {
			resource = application::github_rate_limit_resource_projection::search;
		} else if (aRecord.Resource == application::github_rate_limit_resource::graphql) {
			resource = application::github_rate_limit_resource_projection::graphql;
		}

		return {
			.Resource         = resource,
			.Limit            = aRecord.Limit,
			.Remaining        = aRecord.Remaining,
			.ResetUnixSeconds = aRecord.ResetUnixSeconds,
			.SecondaryLimited = aRecord.SecondaryLimited,
		};
	}

	[[nodiscard]] auto projected_rate_limits(
		const application::github_rate_limit_state& aState)
		-> application::github_rate_limits_projection
	{
		const auto project = [](const auto& aRecord)
			-> std::optional<application::github_rate_limit_projection> {
			return aRecord.has_value()
				? std::optional{ projected_rate_limit(*aRecord) }
				: std::nullopt;
		};
		return {
			.Core    = project(aState.Core),
			.Search  = project(aState.Search),
			.Graphql = project(aState.Graphql),
		};
	}

	[[nodiscard]] auto
	rate_limits_for(const application::github_rate_limit_record& aRecord)
		-> application::github_rate_limit_state
	{
		application::github_rate_limit_state result;
		expect(application::observe(result, aRecord),
			   "single-resource rate fixture is valid");
		return result;
	}

	[[nodiscard]] auto
	rate_limits_for(const application::github_rate_limit_resource aResource)
		-> application::github_rate_limit_state
	{
		return rate_limits_for(rate_limit(aResource));
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
			fact{
				.Capability = capability::review_threads,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::viewed_file_mutations,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::resolve_thread,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::unresolve_thread,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
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
			fact{ capability::review_threads_read,
				  state::supported,
				  source::observed_contract },
			fact{ capability::viewed_file_state_read,
				  state::supported,
				  source::observed_contract },
			fact{ capability::mark_file_as_viewed,
				  state::supported,
				  source::observed_contract },
			fact{ capability::unmark_file_as_viewed,
				  state::supported,
				  source::observed_contract },
			fact{ capability::resolve_permission,
				  state::supported,
				  source::observed_contract },
			fact{ capability::resolve_review_thread,
				  state::supported,
				  source::observed_contract },
			fact{ capability::unresolve_permission,
				  state::supported,
				  source::observed_contract },
			fact{ capability::unresolve_review_thread,
				  state::supported,
				  source::observed_contract },
		};
	}

	[[nodiscard]] auto unsupported_capability_facts()
		-> std::array<application::github_capability_fact,
					  application::github_capability_fact_count>
	{
		auto result = capability_facts();
		for (auto& fact : result) {
			fact.State = core::github_capability_state::unsupported;
		}
		return result;
	}

	[[nodiscard]] auto unsupported_schema_facts()
		-> std::array<application::github_schema_capability_fact,
					  application::github_schema_capability_fact_count>
	{
		auto result = schema_facts();
		for (auto& fact : result) {
			fact.State = core::github_capability_state::unsupported;
		}
		return result;
	}

	[[nodiscard]] auto credential() -> application::github_credential_material
	{
		const std::array bytes{
			std::byte{ 0x01 },
			std::byte{ 0x02 },
			std::byte{ 0x03 },
		};
		auto result = application::github_credential_material::create(bytes);
		expect(result.has_value(), "credential fixture allocates");
		return std::move(*result);
	}

	[[nodiscard]] auto
	snapshot(const application::github_pull_request_provider_request& aRequest)
		-> application::github_pull_request_snapshot
	{
		return {
			.Profile = aRequest.Context.Profile,
			.Host    = aRequest.Context.Host,
			.Account = aRequest.Context.Account,
			.Viewer  = {
				.Actor = aRequest.Viewer.Node,
				.Login = aRequest.Viewer.Login,
			},
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
				.Title       = "Host orchestration",
				.Body        = "Bounded.",
				.State       = application::github_pull_request_state::open,
				.Draft       = false,
				.Author      = std::nullopt,
				.BaseOid     = application::commit_oid{ "1111111111111111111111111111111111111111" },
				.HeadOid     = application::commit_oid{ "2222222222222222222222222222222222222222" },
				.BaseRef     = "main",
				.HeadRef     = "host",
			},
			.Files                     = { },
			.Commits                   = { },
			.Reviews                   = { },
			.ReviewThreadsAvailability = application::github_collection_availability::available,
			.ReviewThreads             = { },
			.IssueComments             = { },
			.Checks                    = { },
			.SearchResults             = { },
			.CapabilityEvidence        = {
				.Capabilities    = aRequest.CapabilityEvidence.Capabilities,
				.Revision        = aRequest.CapabilityEvidence.Revision,
				.ReadShape       = aRequest.CapabilityEvidence.ReadShape,
				.ContractVersion = 1U,
			},
			.RateLimits = rate_limits(),
		};
	}

	[[nodiscard]] auto rich_snapshot(
		const application::github_pull_request_provider_request& aRequest)
		-> application::github_pull_request_snapshot
	{
		auto                                   result = snapshot(aRequest);
		const application::github_actor_record author{
			.Actor = application::github_actor_id{ "author-node" },
			.Login = "octo-author",
		};
		const application::github_actor_record reviewer{
			.Actor = aRequest.Viewer.Node,
			.Login = aRequest.Viewer.Login,
		};
		constexpr std::string_view path = "src/application/host.cpp";

		result.PullRequest.Author = author;
		result.Files.push_back(application::github_file_record{
			.Path         = std::string{ path },
			.PreviousPath = std::nullopt,
			.Change       = application::github_file_change::modified,
			.Additions    = 4U,
			.Deletions    = 2U,
			.ContentKind  = application::github_file_content_kind::text,
			.ViewedState  = application::github_file_viewed_state::viewed,
		});
		result.Reviews.push_back(application::github_review_record{
			.DatabaseId = 4'001U,
			.Review     = application::github_review_id{ "review-1" },
			.Author     = reviewer,
			.State      = application::github_review_state::changes_requested,
			.Commit     = std::nullopt,
			.Body       = "Please preserve identity.",
		});
		result.ReviewThreads.push_back(application::github_review_thread_record{
			.Thread                    = application::github_thread_id{ "thread-1" },
			.Subject                   = application::github_review_thread_subject::line,
			.Path                      = std::string{ path },
			.Line                      = 42U,
			.Resolved                  = true,
			.Outdated                  = false,
			.ViewerResolvePermission   = application::github_permission_state::denied,
			.ViewerUnresolvePermission = application::github_permission_state::allowed,
			.Comments                  = {
				application::github_review_comment_record{
					.Comment  = application::github_comment_id{ "review-comment-1" },
					.ReplyTo  = std::nullopt,
					.Author   = reviewer,
					.Body     = "Thread root",
					.Path     = std::string{ path },
					.Line     = 42U,
					.Outdated = false,
					.Deleted  = false,
				},
				application::github_review_comment_record{
					.Comment  = application::github_comment_id{ "review-comment-2" },
					.ReplyTo  = application::github_comment_id{ "review-comment-1" },
					.Author   = author,
					.Body     = "Follow-up",
					.Path     = std::string{ path },
					.Line     = 42U,
					.Outdated = true,
					.Deleted  = true,
				},
			},
		});
		result.IssueComments.push_back(application::github_issue_comment_record{
			.Comment = application::github_comment_id{ "issue-comment-1" },
			.Author  = author,
			.Body    = "Issue follow-up",
			.Deleted = false,
		});
		return result;
	}

	struct host_fixture
	{
		fake_provider                                   Provider;
		fake_vault                                      Vault;
		application::manual_executor                    Lane;
		recording_projections                           Projections;
		application::github_credential_binding_catalog  OwnedBindings;
		application::github_credential_binding_catalog& Bindings;
		application::github_application_host            Host;

		host_fixture()
			: Bindings{ OwnedBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections }
		{
		}

		explicit host_fixture(
			const application::github_credential_shutdown_policy aShutdownPolicy)
			: Bindings{ OwnedBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections,
					aShutdownPolicy }
		{
		}

		explicit host_fixture(
			application::github_credential_binding_catalog&      aBindings,
			const application::github_credential_shutdown_policy aShutdownPolicy = application::github_credential_shutdown_policy::retain)
			: Bindings{ aBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections,
					aShutdownPolicy }
		{
		}

		explicit host_fixture(application::github_epoch_handoff_port& aEpochs)
			: Bindings{ OwnedBindings }
			, Host{ initial_state(),
					Provider,
					Provider,
					Provider,
					Provider,
					Vault,
					Bindings,
					Lane,
					Projections,
					application::github_credential_shutdown_policy::retain,
					&aEpochs }
		{
		}
	};

	auto advance_native_to_store(host_fixture& aFixture) -> void
	{
		aFixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
		const auto profile = aFixture.Provider.Profiles.front().Request;
		aFixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profile,
				.Authority = native_profile(profile.Context.Profile),
			});
		aFixture.Lane.run_next();
		const auto bootstrap = aFixture.Provider.Viewers.front().Request;
		aFixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = bootstrap,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = { },
			});
		aFixture.Lane.run_next();
	}

	auto advance_native_to_retrieve(host_fixture& aFixture) -> void
	{
		advance_native_to_store(aFixture);
		const auto store = aFixture.Vault.Stores.front().Context;
		aFixture.Vault.Stores.front().Completion(
			application::github_credential_store_success{
				.Context     = store,
				.Disposition = application::github_credential_store_disposition::stored,
			});
		aFixture.Lane.run_next();
	}

	auto establish_native_session(
		host_fixture&                 aFixture,
		const core::host_profile_id   aProfile = core::host_profile_id{ 11U },
		const core::github_account_id aAccount = core::github_account_id{ 51U },
		const std::optional<application::github_profile_authority>&
			aResolvedProfile = std::nullopt)
		-> std::shared_ptr<fake_lease>
	{
		aFixture.Host.enroll_profile(aProfile, credential());
		expect(aFixture.Provider.Profiles.size() == 1U,
			   "activation synchronously submits only profile resolution");
		const auto profileRequest = aFixture.Provider.Profiles.front().Request;
		aFixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profileRequest,
				.Authority = aResolvedProfile.value_or(
					native_profile(profileRequest.Context.Profile)),
			});
		expect(aFixture.Provider.Viewers.empty(),
			   "worker completion cannot advance the host before decision lane");
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Viewers.size() == 1U
				   && aFixture.Provider.Viewers.front().Authentication
					   == fake_provider::viewer_authentication::bootstrap,
			   "native enrollment performs a typed bootstrap viewer observation");

		const auto bootstrapRequest = aFixture.Provider.Viewers.front().Request;
		aFixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = bootstrapRequest,
				.Viewer           = viewer(aAccount),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = { },
			});
		expect(aFixture.Vault.Stores.empty(),
			   "bootstrap result is serialized before vault mutation");
		aFixture.Lane.run_next();
		expect(aFixture.Vault.Stores.size() == 1U
				   && aFixture.Vault.Stores.front().Context.Authority.Account
					   == aAccount,
			   "vault key uses only the account observed from bootstrap /user");

		const auto storeContext = aFixture.Vault.Stores.front().Context;
		aFixture.Vault.Stores.front().Completion(
			application::github_credential_store_success{
				.Context     = storeContext,
				.Disposition = application::github_credential_store_disposition::stored,
			});
		aFixture.Lane.run_next();
		expect(aFixture.Vault.Retrieves.size() == 1U
				   && aFixture.Vault.Retrieves.front().Context.Authority
					   == storeContext.Authority,
			   "stored bootstrap credential is retrieved under the same authority");

		const auto retrieveContext = aFixture.Vault.Retrieves.front().Context;
		aFixture.Vault.Retrieves.front().Completion(
			application::github_credential_retrieve_success{
				.Context    = retrieveContext,
				.Credential = credential(),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Viewers.size() == 2U
				   && aFixture.Provider.Viewers.back().Authentication
					   == fake_provider::viewer_authentication::account,
			   "exact retrieval drives a second account-authenticated /user");

		const auto accountRequest = aFixture.Provider.Viewers.back().Request;
		const auto lease          = std::make_shared<fake_lease>();
		aFixture.Provider.Viewers.back().Completion(
			application::github_viewer_success{
				.Request          = accountRequest,
				.Viewer           = viewer(aAccount),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = lease,
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Capabilities.size() == 1U
				   && aFixture.Provider.Capabilities.front().Request.AccountTransport
					   == lease,
			   "only the exact account-authenticated lease reaches probing");

		const auto capabilityRequest = aFixture.Provider.Capabilities.front().Request;
		aFixture.Provider.Capabilities.front().Completion(
			application::github_capability_success{
				.Request     = capabilityRequest,
				.Facts       = capability_facts(),
				.SchemaFacts = schema_facts(),
				.RateLimits  = rate_limits_for(
					application::github_rate_limit_resource::graphql),
			});
		expect(!aFixture.Host.accepted_session().has_value()
				   && aFixture.Host.capability_cache_size() == 0U,
			   "capability success cannot mutate core or cache off lane");
		aFixture.Lane.run_next();
		expect(aFixture.Host.accepted_session().has_value()
				   && aFixture.Host.capability_cache_size() == 1U
				   && aFixture.Projections.session_count() == 1U,
			   "core acceptance precedes one exact capability-cache commit");
		return lease;
	}

	[[nodiscard]] auto smart_context(
		const application::accepted_pull_request_handoff& aHandoff)
		-> application::git_smart_request_context
	{
		return {
			.ApplicationSession = aHandoff.ApplicationSession,
			.Authority          = {
				.Profile         = aHandoff.Profile,
				.ProfileRevision = aHandoff.ProfileRevision,
				.Host            = aHandoff.Host,
				.HostAuthority   = aHandoff.HostAuthority,
				.Account         = aHandoff.Account,
				.Viewer          = aHandoff.Viewer,
				.Repository      = aHandoff.Repository,
				.Cache           = opaque<core::repository_cache_scope_id>(1U),
				.PullRequest     = aHandoff.PullRequest,
				.Number          = aHandoff.Number,
				.Scope           = opaque<core::pull_request_authority_scope_id>(2U),
			},
			.Revision = {
				.Source = {
					.Observation           = opaque<core::epoch_source_observation_id>(3U),
					.Snapshot              = aHandoff.Snapshot,
					.IdentityGeneration    = aHandoff.IdentityGeneration,
					.PullRequestGeneration = aHandoff.PullRequestGeneration,
				},
				.Origin              = aHandoff.Origin,
				.BaseBranch          = aHandoff.BaseBranch,
				.Base                = aHandoff.Base,
				.Head                = aHandoff.Head,
				.CurrentViewerReview = aHandoff.CurrentViewerReview,
			},
			.Repository = aHandoff.Locator,
			.BaseRef    = aHandoff.BaseRef,
		};
	}

	auto advance_resume_to_capability(
		host_fixture&                                       aFixture,
		const application::github_credential_authority_key& aExpectedAuthority)
		-> application::github_capability_request
	{
		aFixture.Host.resume_profile(core::host_profile_id{ 11U });
		expect(aFixture.Provider.Profiles.size() == 1U,
			   "resume resolves only the selected profile");
		const auto profile = aFixture.Provider.Profiles.front().Request;
		aFixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profile,
				.Authority = native_profile(profile.Context.Profile),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Vault.Retrieves.size() == 1U
				   && aFixture.Vault.Retrieves.front().Context.Authority
					   == aExpectedAuthority
				   && aFixture.Vault.Stores.empty()
				   && aFixture.Provider.Viewers.empty(),
			   "trusted resume retrieves the exact authority without bootstrap or "
			   "store");

		const auto retrieve = aFixture.Vault.Retrieves.front().Context;
		aFixture.Vault.Retrieves.front().Completion(
			application::github_credential_retrieve_success{
				.Context    = retrieve,
				.Credential = credential(),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Viewers.size() == 1U
				   && aFixture.Provider.Viewers.front().Authentication
					   == fake_provider::viewer_authentication::account,
			   "resumed material can perform only the account-authenticated /user");

		const auto account = aFixture.Provider.Viewers.front().Request;
		aFixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = account,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		aFixture.Lane.run_next();
		expect(aFixture.Provider.Capabilities.size() == 1U,
			   "the resumed credential reaches probing only after exact /user");
		return aFixture.Provider.Capabilities.front().Request;
	}

	auto seed_persistent_binding(
		application::github_credential_binding_catalog& aBindings,
		const core::host_profile_id                     aProfile = core::host_profile_id{ 11U },
		const core::github_account_id                   aAccount = core::github_account_id{ 51U },
		const std::optional<application::github_profile_authority>&
			aResolvedProfile = std::nullopt)
		-> application::github_credential_authority_key
	{
		const auto   priorBindings = aBindings.size();
		host_fixture source{ aBindings };
		establish_native_session(source, aProfile, aAccount, aResolvedProfile);
		const auto authority = source.Vault.Stores.front().Context.Authority;
		source.Host.shutdown();
		expect(aBindings.size() == priorBindings + 1U
				   && source.Vault.Deletes.empty(),
			   "real enrollment seeds one retained host-observed binding");
		return authority;
	}

	auto test_enrollment_acceptance_snapshot_and_exact_delete() -> void
	{
		host_fixture fixture;
		establish_native_session(fixture);

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		expect(fixture.Provider.PullRequests.size() == 1U,
			   "accepted session submits one typed pull-request read");
		const auto request = fixture.Provider.PullRequests.front().Request;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = request,
				.Snapshot = snapshot(request),
			});
		expect(!fixture.Host.accepted_snapshot().has_value(),
			   "snapshot remains staged until the decision lane accepts it");
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_snapshot().has_value()
				   && fixture.Projections.pull_request_count() == 1U
				   && fixture.Host.rate_limits() == rate_limits(),
			   "exact core snapshot delta commits with distinct resource limits");

		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 12U },
		});
		expect(fixture.Vault.Deletes.size() == 1U
				   && fixture.Vault.Deletes.front().Context.Authority.Account
					   == viewer().Account
				   && fixture.Provider.Profiles.size() == 1U,
			   "identity invalidation exact-deletes before resolving the next "
			   "profile");

		const auto deleteContext = fixture.Vault.Deletes.front().Context;
		fixture.Vault.Deletes.front().Completion(
			application::github_credential_delete_error{
				.Context = deleteContext,
				.Failure = application::github_credential_vault_failure::operation_failed,
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Profiles.size() == 1U
				   && fixture.Projections.host_diagnostic_count() != 0U
				   && fixture.Projections.last_host_diagnostic().Reason
					   == core::github_reason_code::vault_unavailable,
			   "delete failure is visible and blocks new session establishment");
	}

	auto test_accepted_snapshot_handoff_and_exact_invalidation() -> void
	{
		recording_epoch_handoffs epochs;
		host_fixture             fixture{ epochs };
		const auto               accountLease = establish_native_session(fixture);

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto firstRequest  = fixture.Provider.PullRequests.front().Request;
		auto       firstSnapshot = snapshot(firstRequest);
		firstSnapshot.Reviews.push_back(application::github_review_record{
			.DatabaseId = 4'001U,
			.Review     = application::github_review_id{ "review-1" },
			.Author     = application::github_actor_record{
				.Actor = firstSnapshot.Viewer.Actor,
				.Login = firstSnapshot.Viewer.Login,
			},
			.State  = application::github_review_state::approved,
			.Commit = firstSnapshot.PullRequest.HeadOid,
			.Body   = "Reviewed",
		});
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = firstRequest,
				.Snapshot = firstSnapshot,
			});
		expect(epochs.Accepted.empty(),
			   "provider completion cannot reach epoch ingestion off lane");
		fixture.Lane.run_next();
		expect(epochs.Accepted.size() == 1U && epochs.Invalidated.empty(),
			   "only the core-accepted snapshot reaches epoch ingestion");

		const auto& first = epochs.Accepted.front();
		expect(application::valid(first)
				   && first.ApplicationSession
					   == core::application_session_id{ 41U }
				   && first.Profile == core::host_profile_id{ 11U }
				   && first.ProfileRevision == core::github_profile_revision{ 31U }
				   && first.Host == core::git_host_id{ 21U }
				   && first.Account == core::github_account_id{ 51U }
				   && first.Viewer == core::github_viewer_actor_id{ 51U }
				   && first.Repository == core::repository_id{ 71U }
				   && first.PullRequest == core::pull_request_id{ 81U }
				   && first.Number == core::pull_request_number{ 27U }
				   && first.Locator
					   == core::repository_locator{
						   .Owner = "openai",
						   .Name  = "better-pull-review",
					   }
				   && first.BaseRef.view() == "refs/heads/main" && first.ObjectFormat == core::git_hash_algorithm::sha1 && core::canonical_hex(first.Base).view() == "1111111111111111111111111111111111111111" && core::canonical_hex(first.Head).view() == "2222222222222222222222222222222222222222" && first.HostAuthority.valid() && first.BaseBranch.valid() && first.CurrentViewerReview.has_value() && first.CurrentViewerReview->Review == core::github_review_id{ 4'001U } && first.CurrentViewerReview->Actor == first.Viewer && first.CurrentViewerReview->Commit == first.Head,
			   "handoff contains one exact provider-free authority, revision, "
			   "base ref, and current-viewer review identity");
		const auto firstHostAuthority         = first.HostAuthority;
		const auto firstBaseBranch            = first.BaseBranch;
		const auto firstPullRequestGeneration = first.PullRequestGeneration;

		fixture.Host.dispatch(core::refresh_pull_request_intent{ });
		const auto secondRequest           = fixture.Provider.PullRequests.back().Request;
		auto       secondSnapshot          = snapshot(secondRequest);
		secondSnapshot.Snapshot            = core::provider_snapshot_id{ 92U };
		secondSnapshot.PullRequest.HeadOid = application::commit_oid{
			"3333333333333333333333333333333333333333"
		};
		fixture.Provider.PullRequests.back().Completion(
			application::github_pull_request_success{
				.Request  = secondRequest,
				.Snapshot = secondSnapshot,
			});
		fixture.Lane.run_next();
		expect(epochs.Accepted.size() == 2U && epochs.Invalidated.empty(),
			   "same-authority refresh hands off the new revision without an "
			   "authority invalidation");
		const auto& second = epochs.Accepted.back();
		expect(second.HostAuthority == firstHostAuthority
				   && second.BaseBranch == firstBaseBranch
				   && second.PullRequestGeneration.value()
					   > firstPullRequestGeneration.value()
				   && second.Snapshot == core::provider_snapshot_id{ 92U }
				   && core::canonical_hex(second.Head).view()
					   == "3333333333333333333333333333333333333333",
			   "refresh preserves stable authority identities while advancing "
			   "the exact source generation and head");
		expect(second.SmartGit != nullptr,
			   "accepted native-HTTPS handoff carries one smart authority lease");
		const auto context  = smart_context(second);
		auto       acquired = second.SmartGit->acquire(context);
		expect(acquired.has_value()
				   && *acquired == accountLease->LastTransport
				   && accountLease->AcquiredContexts
					   == std::vector{ context }
				   && !(*acquired)->revoked(),
			   "the exact handoff authority acquires one bound transport");

		const auto expectedInvalidation = application::pull_request_authority_invalidation{
			.ApplicationSession = second.ApplicationSession,
			.Profile            = second.Profile,
			.ProfileRevision    = second.ProfileRevision,
			.Host               = second.Host,
			.HostAuthority      = second.HostAuthority,
			.Account            = second.Account,
			.Viewer             = second.Viewer,
			.Repository         = second.Repository,
			.PullRequest        = second.PullRequest,
			.IdentityGeneration = second.IdentityGeneration,
		};
		epochs.ThrowInvalidation = true;
		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "another-repository" },
			.Number     = core::pull_request_number{ 28U },
		});
		expect(epochs.Invalidated
					   == std::vector{ expectedInvalidation }
				   && fixture.Host.epoch_handoff_delivery_failed()
				   && second.SmartGit->revoked()
				   && (*acquired)->revoked(),
			   "opening an unrelated pull request invalidates only the exact "
			   "previous epoch authority, revokes its issued transport before "
			   "new provider work, and contains a failing observer");
	}

	auto test_success_projection_is_safe_atomic_and_conversation_complete() -> void
	{
		static_assert(
			!exposes_requests<application::github_transition_projection>);
		static_assert(
			!exposes_accepted_session<application::github_transition_projection>);
		static_assert(
			!exposes_accepted_snapshot<application::github_transition_projection>);
		static_assert(!exposes_viewer<application::github_session_projection>);
		static_assert(
			!exposes_capability_evidence<
				application::github_session_projection>);
		static_assert(
			!exposes_account_transport<application::github_session_projection>);
		static_assert(!exposes_credential<application::github_session_projection>);
		static_assert(
			std::is_same_v<
				decltype(application::github_session_projection::RateLimits),
				application::github_rate_limits_projection>);

		host_fixture fixture;
		establish_native_session(fixture);

		const auto& sessionUpdate   = fixture.Projections.last_session_update();
		const auto& acceptedSession = *fixture.Host.accepted_session();
		expect(
			sessionUpdate.Transition.has_value()
				&& sessionUpdate.Session.has_value()
				&& !sessionUpdate.PullRequest.has_value()
				&& sessionUpdate.HostDiagnostics.empty(),
			"accepted session publishes one atomic transition and safe session");
		expect(
			sessionUpdate.Transition->Status.has_value()
				&& sessionUpdate.Transition->Status->Status
					== core::github_status::session_ready,
			"accepted session transition reports ready status");
		expect(
			sessionUpdate.Session->Profile == core::host_profile_id{ 11U }
				&& sessionUpdate.Session->Host == core::git_host_id{ 21U }
				&& sessionUpdate.Session->Account
					== core::github_account_id{ 51U }
				&& sessionUpdate.Session->Login == "reviewer"
				&& sessionUpdate.Session->DisplayName == "Review Engineer"
				&& sessionUpdate.Session->Capabilities
					== acceptedSession.CapabilityEvidence.Capabilities
				&& sessionUpdate.Session->CapabilityRevision
					== acceptedSession.CapabilityEvidence.Revision
				&& sessionUpdate.Session->RateLimits
					== projected_rate_limits(acceptedSession.RateLimits),
			"session projection contains only accepted identity, login, "
			"capability, and rate facts");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto request          = fixture.Provider.PullRequests.front().Request;
		const auto acceptedSnapshot = rich_snapshot(request);
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = request,
				.Snapshot = acceptedSnapshot,
			});
		fixture.Lane.run_next();

		const auto& pullRequestUpdate = fixture.Projections.last_pull_request_update();
		expect(
			pullRequestUpdate.Transition.has_value()
				&& !pullRequestUpdate.Session.has_value()
				&& pullRequestUpdate.PullRequest.has_value()
				&& pullRequestUpdate.HostDiagnostics.empty(),
			"accepted pull request publishes one atomic transition and projection");
		expect(
			pullRequestUpdate.Transition->Status.has_value()
				&& pullRequestUpdate.Transition->Status->Status
					== core::github_status::pull_request_ready,
			"accepted pull-request transition reports ready status");

		const auto& pullRequest = *pullRequestUpdate.PullRequest;
		expect(
			pullRequest.Profile == acceptedSnapshot.Profile
				&& pullRequest.Host == acceptedSnapshot.Host
				&& pullRequest.Account == acceptedSnapshot.Account
				&& pullRequest.Snapshot == acceptedSnapshot.Snapshot
				&& pullRequest.Repository
					== acceptedSnapshot.Repository.Repository
				&& pullRequest.PullRequest
					== acceptedSnapshot.PullRequest.PullRequest
				&& pullRequest.AuthorLogin == "octo-author"
				&& pullRequest.FileCount == 1U
				&& pullRequest.ReviewCount == 1U
				&& pullRequest.ReviewThreadCount == 1U
				&& pullRequest.IssueCommentCount == 1U
				&& pullRequest.ReviewThreadsAvailable
				&& pullRequest.Capabilities
					== acceptedSnapshot.CapabilityEvidence.Capabilities
				&& pullRequest.CapabilityRevision
					== acceptedSnapshot.CapabilityEvidence.Revision
				&& pullRequest.RateLimits
					== projected_rate_limits(acceptedSnapshot.RateLimits),
			"pull-request projection retains presentation-safe snapshot facts");
		expect(
			pullRequest.Conversations.size() == 5U
				&& pullRequest.Conversations[0U].Id == "review:review-1"
				&& pullRequest.Conversations[1U].Id
					== "review-thread:thread-1"
				&& pullRequest.Conversations[2U].Id
					== "review-comment:review-comment-1"
				&& pullRequest.Conversations[3U].Id
					== "review-comment:review-comment-2"
				&& pullRequest.Conversations[4U].Id
					== "issue-comment:issue-comment-1",
			"review conversation identities are stable, prefixed, and ordered");

		const auto& review       = pullRequest.Conversations[0U];
		const auto& thread       = pullRequest.Conversations[1U];
		const auto& threadRoot   = pullRequest.Conversations[2U];
		const auto& threadReply  = pullRequest.Conversations[3U];
		const auto& issueComment = pullRequest.Conversations[4U];
		expect(
			review.Kind == application::github_conversation_kind::review
				&& !review.ParentId.has_value()
				&& review.AuthorLogin == "reviewer"
				&& review.Body == "Please preserve identity."
				&& review.ReviewState
					== application::github_review_state_projection::
						changes_requested,
			"review projects its author, body, and review state");
		expect(
			thread.Kind == application::github_conversation_kind::review_thread
				&& !thread.ParentId.has_value()
				&& thread.Path == "src/application/host.cpp"
				&& thread.Line == 42U && thread.Resolved && !thread.Outdated,
			"review thread projects its location and state");
		expect(
			threadRoot.Kind
					== application::github_conversation_kind::review_comment
				&& threadRoot.ParentId == thread.Id
				&& threadRoot.AuthorLogin == "reviewer"
				&& threadRoot.Body == "Thread root"
				&& threadRoot.Path == thread.Path && threadRoot.Line == 42U
				&& threadRoot.Resolved && !threadRoot.Outdated
				&& !threadRoot.Deleted,
			"root review comment projects under its stable thread identity");
		expect(
			threadReply.Kind
					== application::github_conversation_kind::review_comment
				&& threadReply.ParentId == thread.Id
				&& threadReply.AuthorLogin == "octo-author"
				&& threadReply.Body == "Follow-up"
				&& threadReply.Resolved && threadReply.Outdated
				&& threadReply.Deleted,
			"reply review comment retains thread state and comment flags");
		expect(
			issueComment.Kind
					== application::github_conversation_kind::issue_comment
				&& !issueComment.ParentId.has_value()
				&& issueComment.AuthorLogin == "octo-author"
				&& issueComment.Body == "Issue follow-up"
				&& !issueComment.Deleted,
			"issue comment projects as a distinct top-level conversation");
	}

	auto test_every_provider_failure_maps_to_safe_pull_request_failure() -> void
	{
		using provider_failure = application::github_provider_failure;
		using reason           = core::github_reason_code;

		struct failure_mapping
		{
			provider_failure Provider;
			reason           CoreReason;
			bool             Retryable;
		};

		constexpr auto mappings = std::array{
			failure_mapping{
				provider_failure::authentication,
				reason::authentication_failed,
				false,
			},
			failure_mapping{
				provider_failure::permission,
				reason::permission_denied,
				false,
			},
			failure_mapping{
				provider_failure::network,
				reason::network_unavailable,
				true,
			},
			failure_mapping{
				provider_failure::api_version,
				reason::api_version_unsupported,
				false,
			},
			failure_mapping{
				provider_failure::unsupported_capability,
				reason::capability_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::rate_limit,
				reason::rate_limited,
				false,
			},
			failure_mapping{
				provider_failure::certificate,
				reason::certificate_rejected,
				false,
			},
			failure_mapping{
				provider_failure::redirect,
				reason::redirect_rejected,
				false,
			},
			failure_mapping{
				provider_failure::timeout,
				reason::timed_out,
				true,
			},
			failure_mapping{
				provider_failure::process_startup,
				reason::network_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::cli_unavailable,
				reason::cli_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::vault,
				reason::vault_unavailable,
				false,
			},
			failure_mapping{
				provider_failure::malformed_response,
				reason::malformed_response,
				false,
			},
			failure_mapping{
				provider_failure::response_limit,
				reason::response_too_large,
				false,
			},
			failure_mapping{
				provider_failure::unavailable,
				reason::network_unavailable,
				true,
			},
		};
		static_assert(
			mappings.size()
			== static_cast<std::size_t>(provider_failure::unavailable) + 1U);
		static_assert(!exposes_detail<application::github_provider_error>);
		static_assert(!exposes_detail<application::github_pull_request_error>);
		static_assert(
			!exposes_detail<application::github_host_diagnostic_projection>);
		static_assert(!exposes_detail<core::github_diagnostic>);

		for (const auto& mapping : mappings) {
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});

			const auto attempts = mapping.Retryable
				? application::maximum_github_provider_attempts
				: std::uint8_t{ 1U };
			for (std::uint8_t attempt = 0U; attempt < attempts; ++attempt) {
				const auto                         request = fixture.Provider.PullRequests.at(attempt).Request;
				application::github_provider_error error{
					.Failure           = mapping.Provider,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				};
				if (mapping.Provider == provider_failure::rate_limit) {
					auto exhausted       = rate_limit();
					exhausted.Remaining  = 0U;
					error.RateLimits     = rate_limits_for(exhausted);
					error.RateLimitCause = exhausted;
				}
				expect(application::valid(error),
					   "provider failure fixture is structurally valid");
				fixture.Provider.PullRequests.at(attempt).Completion(
					application::github_pull_request_error{
						.Request = request,
						.Error   = error,
					});
				fixture.Lane.run_next();
			}

			const auto& update              = fixture.Projections.last_transition_update();
			const auto  expectedDiagnostics = std::vector{ core::github_diagnostic{ .Code = mapping.CoreReason } };
			expect(
				fixture.Provider.PullRequests.size() == attempts
					&& !fixture.Host.accepted_snapshot().has_value()
					&& !fixture.Host.state().AcceptedSnapshot.has_value()
					&& update.Transition->Status
						== core::github_status_delta{
							.Status = core::github_status::pull_request_failed }
					&& update.Transition->Diagnostics == expectedDiagnostics && !update.Session.has_value() && !update.PullRequest.has_value() && update.HostDiagnostics.empty(),
				"every provider failure maps to one safe typed pull-request "
				"failure without an accepted snapshot");
		}
	}

	auto test_retries_are_bounded_and_stale_work_is_cancelled() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto oldRequest = fixture.Provider.Profiles.front().Request;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 12U },
		});
		expect(fixture.Provider.CancelledProfiles == std::vector{ oldRequest }
				   && fixture.Provider.Profiles.size() == 2U,
			   "profile switch cancels exact in-flight work");

		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = oldRequest,
				.Authority = cli_profile(oldRequest.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.empty(),
			   "reordered stale profile success cannot start viewer work");

		const auto currentRequest = fixture.Provider.Profiles.back().Request;
		fixture.Provider.Profiles.back().Completion(
			application::github_profile_success{
				.Request   = currentRequest,
				.Authority = cli_profile(currentRequest.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.size() == 1U,
			   "current profile success starts the viewer phase");

		const auto viewerRequest = fixture.Provider.Viewers.front().Request;
		fixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = viewerRequest,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Capabilities.size() == 1U,
			   "CLI viewer starts capability probing");

		for (std::uint8_t attempt = 1U; attempt <= 3U; ++attempt) {
			auto& call = fixture.Provider
							 .Capabilities[static_cast<std::size_t>(attempt - 1U)];
			expect(call.Request.Attempt
					   == application::github_provider_attempt{ attempt },
				   "retry attempt identity is monotone and exact");
			call.Completion(application::github_capability_error{
				.Request = call.Request,
				.Error   = {
					.Failure           = attempt == 1U
						? application::github_provider_failure::unavailable
						: attempt == 2U
						? application::github_provider_failure::timeout
						: application::github_provider_failure::network,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			});
			expect(fixture.Provider.Capabilities.size()
					   == static_cast<std::size_t>(attempt),
				   "retry is never submitted from the worker callback");
			fixture.Lane.run_next();
		}
		expect(
			fixture.Provider.Capabilities.size() == 3U
				&& !fixture.Host.accepted_session().has_value()
				&& fixture.Host.state().PendingSession == std::nullopt,
			"three total immediate serialized attempts exhaust to a core failure");
	}

	auto test_retry_after_is_terminal_and_preserved() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = 30U,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Profiles.size() == 1U
					   && fixture.Projections.host_diagnostic_count() == 1U
					   && fixture.Projections.last_host_diagnostic().Reason
						   == core::github_reason_code::network_unavailable
					   && fixture.Projections.last_host_diagnostic().RetryAfterSeconds
						   == 30U,
				   "positive Retry-After prevents an immediate retry and is "
				   "preserved");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto                                  request = fixture.Provider.PullRequests.front().Request;
			const application::github_rate_limit_record exhausted{
				.Resource         = application::github_rate_limit_resource::core,
				.Limit            = 5'000U,
				.Remaining        = 0U,
				.ResetUnixSeconds = 2'000'000'100U,
				.SecondaryLimited = false,
			};
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::rate_limit,
						.RetryAfterSeconds = application::maximum_github_retry_after_seconds,
						.RateLimits        = rate_limits_for(exhausted),
						.RateLimitCause    = exhausted,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.PullRequests.size() == 1U
					&& fixture.Projections.host_diagnostic_count() == 1U
					&& fixture.Projections.last_host_diagnostic().Reason
						== core::github_reason_code::rate_limited
					&& fixture.Projections.last_host_diagnostic().RetryAfterSeconds
						== application::maximum_github_retry_after_seconds
					&& fixture.Projections.last_host_diagnostic().RateLimits.Core
						== projected_rate_limits(fixture.Host.rate_limits()).Core
					&& fixture.Host.rate_limits().Core == exhausted,
				"bounded rate-limit Retry-After reaches the projection advisory");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request   = fixture.Provider.PullRequests.front().Request;
			auto       exhausted = rate_limit();
			exhausted.Remaining  = 0U;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(exhausted),
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.PullRequests.size() == 1U
					   && fixture.Host.rate_limits().Core == exhausted,
				   "primary exhaustion suppresses immediate unavailable retries");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto                                  request = fixture.Provider.PullRequests.front().Request;
			const application::github_rate_limit_record secondary{
				.Resource         = application::github_rate_limit_resource::core,
				.Limit            = 5'000U,
				.Remaining        = 4'999U,
				.ResetUnixSeconds = 2'000'000'200U,
				.SecondaryLimited = true,
			};
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::rate_limit,
						.RetryAfterSeconds = 60U,
						.RateLimits        = rate_limits_for(secondary),
						.RateLimitCause    = secondary,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Host.rate_limits().Core == secondary
					   && fixture.Projections.last_host_diagnostic().RateLimits.Core
						   == projected_rate_limit(secondary)
					   && fixture.Projections.last_host_diagnostic().RetryAfterSeconds
						   == 60U,
				   "secondary-limit state and fallback delay remain distinct");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			auto search      = rate_limit(application::github_rate_limit_resource::search);
			search.Remaining = 17U;
			const auto first = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = first,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(search),
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();

			auto core         = rate_limit(application::github_rate_limit_resource::core);
			core.Remaining    = 4'123U;
			const auto second = fixture.Provider.PullRequests.back().Request;
			fixture.Provider.PullRequests.back().Completion(
				application::github_pull_request_error{
					.Request = second,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = 30U,
						.RateLimits        = rate_limits_for(core),
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.PullRequests.size() == 2U
					&& fixture.Projections.host_diagnostic_count() == 1U
					&& fixture.Projections.last_host_diagnostic().RateLimits
						== projected_rate_limits(fixture.Host.rate_limits())
					&& fixture.Host.rate_limits().Core == core
					&& fixture.Host.rate_limits().Search == search
					&& fixture.Host.rate_limits().Graphql.has_value(),
				"terminal advisory projects all resource observations accumulated "
				"across retries");
		}
	}

	auto test_wrong_correlations_and_submit_exceptions_fail_closed() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto current    = fixture.Provider.Profiles.front().Request;
			auto       wrong      = current;
			wrong.Context.Request = core::request_id{ 999U };
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = wrong,
					.Authority = cli_profile(current.Context.Profile),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.state().PendingSession.has_value()
					   && fixture.Provider.Viewers.empty(),
				   "wrong-correlated current provider terminal fails closed");
		}

		{
			host_fixture fixture;
			fixture.Provider.ThrowOnProfileSubmit = true;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Lane.pending() == 1U,
				   "synchronous provider throw is converted to a lane completion");
			fixture.Lane.run_next();
			expect(!fixture.Host.state().PendingSession.has_value(),
				   "provider submit throw terminates the current session");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = 1U,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Profiles.size() == 1U
					&& !fixture.Host.state().PendingSession.has_value()
					&& fixture.Projections.host_diagnostic_count() == 0U,
				"invalid retry metadata is terminal without an advisory or retry");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_record{
								.Resource = application::
									github_rate_limit_resource::core,
								.Limit            = 5'000U,
								.Remaining        = 4'999U,
								.ResetUnixSeconds = 2'000'000'000U,
								.SecondaryLimited = false,
							}),
						.RateLimitCause = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.rate_limits().Core.has_value()
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.rate_limits().Graphql.has_value()
					   && fixture.Projections.host_diagnostic_count() == 0U
					   && !fixture.Host.state().PendingSession.has_value(),
				   "local profile resolution cannot fabricate remote rate state");
		}

		{
			fake_provider                                  provider;
			fake_vault                                     vault;
			throwing_once_lane                             lane;
			recording_projections                          projections;
			application::github_credential_binding_catalog bindings;
			application::github_application_host           host{
				initial_state(),
				provider,
				provider,
				provider,
				provider,
				vault,
				bindings,
				lane,
				projections,
			};
			host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = provider.Profiles.front().Request;
			provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = cli_profile(request.Context.Profile),
				});
			expect(lane.pending() == 1U,
				   "a throwing lane post falls back to one typed completion");
			lane.run_next();
			expect(!host.state().PendingSession.has_value()
					   && provider.Viewers.empty(),
				   "lane post failure cannot strand provider work");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto request = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_resource::search),
						.RateLimitCause = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Viewers.size() == 1U
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "viewer errors cannot inject a search rate observation");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = viewerRequest,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_error{
					.Request = capability,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_resource::search),
						.RateLimitCause = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Capabilities.size() == 1U
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "capability errors cannot inject a search rate observation");
		}
	}

	auto test_rate_observation_presence_follows_server_contract() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request          = viewerRequest,
					.Viewer           = viewer(),
					.RateLimits       = { },
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Capabilities.empty()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "GitHub.com viewer success requires a core rate observation");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = viewerRequest,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = { },
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_session().has_value(),
				   "GitHub.com capability success requires a GraphQL rate "
				   "observation");
		}

		{
			host_fixture fixture;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = ghes_cli_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request          = viewerRequest,
					.Viewer           = viewer(),
					.RateLimits       = { },
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = { },
				});
			fixture.Lane.run_next();
			expect(
				fixture.Host.accepted_session().has_value()
					&& !application::has_observations(fixture.Host.rate_limits()),
				"GHES with disabled rate limits preserves explicit no-observation "
				"state");

			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			auto       result  = snapshot(request);
			result.RateLimits  = { };
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(result),
				});
			fixture.Lane.run_next();
			expect(fixture.Host.accepted_snapshot().has_value()
					   && !application::has_observations(
						   fixture.Host.accepted_snapshot()->RateLimits),
				   "GHES disabled-limit snapshot remains explicitly unobserved");

			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 28U },
			});
			const auto partialRequest = fixture.Provider.PullRequests.back().Request;
			auto       partial        = snapshot(partialRequest);
			partial.RateLimits        = rate_limits_for(application::github_rate_limit_resource::search);
			fixture.Provider.PullRequests.back().Completion(
				application::github_pull_request_success{
					.Request  = partialRequest,
					.Snapshot = std::move(partial),
				});
			fixture.Lane.run_next();
			expect(fixture.Host.accepted_snapshot().has_value()
					   && fixture.Host.accepted_snapshot()
							  ->RateLimits.Search.has_value()
					   && !fixture.Host.accepted_snapshot()
							   ->RateLimits.Core.has_value()
					   && !fixture.Host.accepted_snapshot()
							   ->RateLimits.Graphql.has_value(),
				   "GHES accepts the valid subset observed when rate limiting is "
				   "mixed");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			auto       result  = snapshot(request);
			result.RateLimits.Search.reset();
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(result),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value(),
				   "GitHub.com snapshots require every exercised resource bucket");
		}
	}

	auto test_no_graphql_read_rejects_graphql_error_observations() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto profile = fixture.Provider.Profiles.front().Request;
		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = profile,
				.Authority = cli_profile(profile.Context.Profile),
			});
		fixture.Lane.run_next();
		const auto viewerRequest = fixture.Provider.Viewers.front().Request;
		fixture.Provider.Viewers.front().Completion(
			application::github_viewer_success{
				.Request          = viewerRequest,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		fixture.Lane.run_next();
		const auto capability = fixture.Provider.Capabilities.front().Request;
		fixture.Provider.Capabilities.front().Completion(
			application::github_capability_success{
				.Request     = capability,
				.Facts       = unsupported_capability_facts(),
				.SchemaFacts = unsupported_schema_facts(),
				.RateLimits  = rate_limits_for(
					application::github_rate_limit_resource::graphql),
			});
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_session().has_value()
				   && !fixture.Host.accepted_session()
						   ->CapabilityEvidence.ReadShape.ReviewThreads
				   && !fixture.Host.accepted_session()
						   ->CapabilityEvidence.ReadShape.ViewedFileState,
			   "unsupported schema establishes a REST-only read shape");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto request      = fixture.Provider.PullRequests.front().Request;
		const auto priorGraphql = fixture.Host.rate_limits().Graphql;
		auto       injected     = rate_limit(application::github_rate_limit_resource::graphql);
		injected.Remaining      = 123U;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_error{
				.Request = request,
				.Error   = {
					.Failure           = application::github_provider_failure::permission,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = rate_limits_for(injected),
					.RateLimitCause    = std::nullopt,
				},
			});
		fixture.Lane.run_next();
		expect(fixture.Host.rate_limits().Graphql == priorGraphql
				   && fixture.Projections.host_diagnostic_count() == 0U
				   && !fixture.Host.state().PendingPullRequest.has_value(),
			   "REST-only reads cannot inject a GraphQL error observation");
	}

	auto test_projection_failures_cannot_interrupt_effects_or_cleanup() -> void
	{
		{
			host_fixture fixture;
			fixture.Projections.ThrowNextOutput = true;
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Provider.Profiles.size() == 1U
					   && fixture.Host.projection_delivery_failed(),
				   "a failed transition projection cannot veto decided effects");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();

			const auto account = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capability = fixture.Provider.Capabilities.front().Request;

			fixture.Projections.ThrowNextDiagnostic = true;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_error{
					.Request = capability,
					.Error   = {
						.Failure           = application::github_provider_failure::unavailable,
						.RetryAfterSeconds = 30U,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			fixture.Lane.run_next();
			expect(
				fixture.Host.projection_delivery_failed()
					&& fixture.Vault.Deletes.size() == 1U,
				"a failed error advisory cannot strand a provisional credential");

			const auto cleanup                      = fixture.Vault.Deletes.front().Context;
			fixture.Projections.ThrowNextDiagnostic = true;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_error{
					.Context = cleanup,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.state().PendingSession.has_value()
					   && fixture.Projections.host_diagnostic_count() == 0U,
				   "a failed cleanup diagnostic cannot interrupt the deferred "
				   "terminal");
		}
	}

	auto test_projection_reentry_is_serialized_after_the_current_turn() -> void
	{
		{
			host_fixture fixture;
			fixture.Projections.OnNextOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(
				fixture.Provider.Profiles.size() == 1U
					&& fixture.Provider.CancelledProfiles
						== std::vector{ fixture.Provider.Profiles.front().Request }
					&& !fixture.Host.state().ActiveProfile.has_value()
					&& !fixture.Host.state().PendingSession.has_value(),
				"projection reentry is ordered after and cancels the current "
				"effect");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto account = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();

			fixture.Projections.OnNextOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			const auto capability            = fixture.Provider.Capabilities.front().Request;
			fixture.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = rate_limits_for(
						application::github_rate_limit_resource::graphql),
				});
			fixture.Lane.run_next();
			expect(fixture.Projections.session_count() == 1U
					   && !fixture.Host.accepted_session().has_value()
					   && fixture.Vault.Deletes.size() == 1U,
				   "session publication finishes before deferred reentrant "
				   "invalidation");
		}

		{
			host_fixture fixture;
			fixture.Projections.OnEveryOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Lane.pending() == 1U
					   && !fixture.Host.state().ActiveProfile.has_value(),
				   "repeated projection feedback yields after one deferred "
				   "generation");

			fixture.Projections.OnEveryOutput = { };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			expect(fixture.Provider.Profiles.size() == 1U
					   && !fixture.Host.state().ActiveProfile.has_value(),
				   "new ingress cannot overtake an older deferred generation");
			fixture.Lane.run_next();
			expect(fixture.Lane.pending() == 0U
					   && fixture.Provider.Profiles.size() == 2U
					   && fixture.Host.state().ActiveProfile
						   == core::host_profile_id{ 12U },
				   "deferred feedback stays FIFO and yields fairly on the decision "
				   "lane");
		}

		{
			host_fixture fixture;
			fixture.Projections.OnEveryOutput = [&fixture] { fixture.Host.dispatch(core::deactivate_github_identity_intent{ }); };
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			expect(fixture.Lane.pending() == 1U,
				   "feedback continuation is pending before terminal shutdown");
			fixture.Host.shutdown();
			expect(fixture.Host.is_closing()
					   && fixture.Provider.CancelledProfiles.size() == 1U,
				   "shutdown closes immediately despite an older deferred "
				   "continuation");
		}
	}

	auto test_invalid_enrollment_cannot_leak_into_a_later_activation() -> void
	{
		host_fixture fixture;
		fixture.Host.enroll_profile({ }, credential());
		expect(fixture.Provider.Profiles.empty(),
			   "invalid profile enrollment submits no provider work");

		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto request = fixture.Provider.Profiles.front().Request;
		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = request,
				.Authority = native_profile(request.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.empty()
				   && !fixture.Host.state().PendingSession.has_value(),
			   "a later activation cannot consume a rejected enrollment secret");
	}

	auto test_snapshot_authority_and_pr_cancellation_fail_closed() -> void
	{
		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request                         = fixture.Provider.PullRequests.front().Request;
			auto       fabricated                      = snapshot(request);
			fabricated.CapabilityEvidence.Capabilities = { };
			fabricated.CapabilityEvidence.ReadShape    = { };
			fabricated.ReviewThreadsAvailability       = application::github_collection_availability::unavailable;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(fabricated),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value()
					   && fixture.Projections.pull_request_count() == 0U,
				   "same-revision fabricated capability evidence cannot be staged");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request     = fixture.Provider.PullRequests.front().Request;
			auto       wrongViewer = snapshot(request);
			wrongViewer.Viewer     = {
				.Actor = application::github_actor_id{ "other-viewer" },
				.Login = "other",
			};
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = std::move(wrongViewer),
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value()
					   && fixture.Projections.pull_request_count() == 0U,
				   "provider snapshot cannot replace the authenticated viewer");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_success{
					.Request  = request,
					.Snapshot = snapshot(request),
				});
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			expect(fixture.Provider.CancelledPullRequests == std::vector{ request },
				   "identity switch cancels the exact in-flight PR request");
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_snapshot().has_value()
					   && fixture.Projections.pull_request_count() == 0U,
				   "queued old-identity PR success is inert after cancellation");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_error{
					.Request = request,
					.Error   = {
						.Failure           = application::github_provider_failure::rate_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = rate_limits_for(
							application::github_rate_limit_record{
								.Resource = application::
									github_rate_limit_resource::core,
								.Limit            = 5'000U,
								.Remaining        = 0U,
								.ResetUnixSeconds = 2'000'000'300U,
								.SecondaryLimited = false,
							}),
						.RateLimitCause = application::github_rate_limit_record{
							.Resource = application::
								github_rate_limit_resource::core,
							.Limit            = 5'000U,
							.Remaining        = 0U,
							.ResetUnixSeconds = 2'000'000'300U,
							.SecondaryLimited = false,
						},
					},
				});
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			fixture.Lane.run_next();
			expect(!fixture.Host.rate_limits().Core.has_value()
					   && !fixture.Host.rate_limits().Search.has_value()
					   && !fixture.Host.rate_limits().Graphql.has_value()
					   && fixture.Projections.host_diagnostic_count() == 0U,
				   "stale old-identity rate observations cannot repopulate host "
				   "state");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::open_pull_request_intent{
				.Repository = { .Owner = "openai", .Name = "better-pull-review" },
				.Number     = core::pull_request_number{ 27U },
			});
			const auto request = fixture.Provider.PullRequests.front().Request;
			fixture.Provider.PullRequests.front().Completion(
				application::github_pull_request_cancelled{
					.Request = request,
				});
			fixture.Lane.run_next();
			expect(
				!fixture.Host.state().PendingPullRequest.has_value()
					&& !fixture.Host.accepted_snapshot().has_value(),
				"current PR cancellation clears pending state without a snapshot");
		}
	}

	auto test_new_open_clears_host_snapshot_but_refresh_retains_it() -> void
	{
		host_fixture fixture;
		establish_native_session(fixture);
		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.Number     = core::pull_request_number{ 27U },
		});
		const auto first = fixture.Provider.PullRequests.front().Request;
		fixture.Provider.PullRequests.front().Completion(
			application::github_pull_request_success{
				.Request  = first,
				.Snapshot = snapshot(first),
			});
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_snapshot().has_value(),
			   "first PR snapshot is accepted");

		fixture.Host.dispatch(core::refresh_pull_request_intent{ });
		expect(fixture.Host.accepted_snapshot().has_value(),
			   "refresh retains the last accepted snapshot while loading");
		const auto refresh = fixture.Provider.PullRequests.back().Request;
		fixture.Provider.PullRequests.back().Completion(
			application::github_pull_request_cancelled{
				.Request = refresh,
			});
		fixture.Lane.run_next();
		expect(fixture.Host.accepted_snapshot().has_value(),
			   "cancelled refresh preserves the last accepted snapshot");

		fixture.Host.dispatch(core::open_pull_request_intent{
			.Repository = { .Owner = "openai", .Name = "another-repository" },
			.Number     = core::pull_request_number{ 28U },
		});
		expect(!fixture.Host.accepted_snapshot().has_value(),
			   "accepted new open clears the old host-owned snapshot immediately");
		const auto second = fixture.Provider.PullRequests.back().Request;
		fixture.Provider.PullRequests.back().Completion(
			application::github_pull_request_error{
				.Request = second,
				.Error   = {
					.Failure           = application::github_provider_failure::permission,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			});
		fixture.Lane.run_next();
		expect(!fixture.Host.accepted_snapshot().has_value(),
			   "failed new open cannot revive the previous PR snapshot");
	}

	auto test_vault_wrong_context_and_submit_throw_fail_closed() -> void
	{
		{
			host_fixture fixture;
			advance_native_to_store(fixture);
			const auto expected = fixture.Vault.Stores.front().Context;
			auto       wrong    = expected;
			wrong.Request       = core::request_id{ 999U };
			fixture.Vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = wrong,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority
						== expected.Authority
					&& fixture.Vault.Deletes.front().Context != wrong,
				"wrong-context store success cleans only host-captured authority");
			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			const auto diagnostics = fixture.Projections.host_diagnostic_count();
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Projections.host_diagnostic_count() == diagnostics
					&& !fixture.Host.state().PendingSession.has_value(),
				"duplicate delete terminal is stale and cannot rewrite lifecycle");
		}

		{
			host_fixture fixture;
			fixture.Vault.ThrowOnStore = true;
			advance_native_to_store(fixture);
			expect(fixture.Lane.pending() == 1U,
				   "synchronous vault throw is converted to a lane completion");
			fixture.Lane.run_next();
			expect(
				!fixture.Host.accepted_session().has_value()
					&& !fixture.Vault.Deletes.empty(),
				"vault submit throw terminates and cleans provisional authority");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.back().Request;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Viewers.size() == 2U
					&& fixture.Provider.CancelledViewers.empty()
					&& fixture.Vault.Deletes.empty()
					&& fixture.Provider.Viewers.back().Request == viewerRequest,
				"duplicate retrieve terminal is stale and cannot tear down viewer");
		}
	}

	auto test_reenrollment_waits_for_running_vault_cleanup() -> void
	{
		{
			host_fixture fixture;
			advance_native_to_store(fixture);
			const auto running = fixture.Vault.Stores.front().Context;

			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && fixture.Provider.Profiles.size() == 1U,
				   "same-authority re-enrollment waits for running store terminal");

			fixture.Vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = running,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.Deletes.front().Context.Authority
						   == running.Authority
					   && fixture.Provider.Profiles.size() == 1U,
				   "store terminal starts exact cleanup before replacement work");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Provider.Profiles.size() == 2U,
				   "replacement profile starts only after store cleanup succeeds");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto running = fixture.Vault.Retrieves.front().Context;

			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			expect(
				fixture.Vault.Cancellations == std::vector{ running }
					&& fixture.Vault.Deletes.empty()
					&& fixture.Provider.Profiles.size() == 1U,
				"same-authority re-enrollment waits for running retrieve terminal");

			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = running,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority
						== running.Authority
					&& fixture.Provider.Profiles.size() == 1U,
				"retrieve terminal starts exact cleanup before replacement work");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Profiles.size() == 2U,
				"replacement profile starts only after retrieve cleanup succeeds");
		}
	}

	auto test_bootstrap_lease_and_changed_account_fail_closed() -> void
	{
		{
			host_fixture fixture;
			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto bootstrap = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = bootstrap,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.Stores.empty()
					   && !fixture.Host.accepted_session().has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "a bootstrap provider cannot smuggle a reusable account lease");
		}

		{
			host_fixture fixture;
			fixture.Host.enroll_profile(core::host_profile_id{ 11U }, credential());
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto bootstrap = fixture.Provider.Viewers.front().Request;
			fixture.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = bootstrap,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = { },
				});
			fixture.Lane.run_next();
			const auto store = fixture.Vault.Stores.front().Context;
			fixture.Vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = store,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			fixture.Lane.run_next();
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto account = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(core::github_account_id{ 52U }),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			expect(
				fixture.Provider.Capabilities.empty()
					&& fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority.Account
						== core::github_account_id{ 51U },
				"changed second /user identity triggers exact provisional cleanup");
			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(!fixture.Host.accepted_session().has_value()
					   && !fixture.Host.state().PendingSession.has_value(),
				   "changed account never becomes an accepted core session");
		}
	}

	auto test_shutdown_cancels_and_suppresses_late_completion() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		const auto request         = fixture.Provider.Profiles.front().Request;
		const auto projectionCount = fixture.Projections.transition_count();
		fixture.Host.shutdown();
		expect(fixture.Host.is_closing() && fixture.Host.is_quiescent()
				   && fixture.Provider.CancelledProfiles == std::vector{ request }
				   && !fixture.Host.state().ActiveProfile.has_value(),
			   "shutdown closes ingress, invalidates core, and cancels exact work");
		fixture.Provider.Profiles.front().Completion(
			application::github_profile_success{
				.Request   = request,
				.Authority = cli_profile(request.Context.Profile),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Viewers.empty()
				   && fixture.Projections.transition_count() == projectionCount,
			   "late completion after shutdown cannot publish or start new work");
	}

	auto test_shutdown_preserves_persistent_credentials() -> void
	{
		static_assert(!std::is_trivially_copyable_v<
					  application::github_credential_binding_catalog>);
		static_assert(!std::is_copy_constructible_v<
					  application::github_credential_binding_catalog>);
		static_assert(!std::is_move_constructible_v<
					  application::github_credential_binding_catalog>);

		application::github_credential_binding_catalog bindings;
		host_fixture                                   fixture{ bindings };
		establish_native_session(fixture);
		expect(bindings.size() == 1U,
			   "exact /user acceptance records one host-owned binding");
		fixture.Host.shutdown();
		expect(fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent()
				   && bindings.size() == 1U,
			   "ordinary shutdown preserves accepted persistent credentials");
	}

	auto test_shutdown_deletes_persistent_credentials_by_policy() -> void
	{
		constexpr auto policy = application::github_credential_shutdown_policy::
			delete_retained_on_shutdown;

		{
			host_fixture fixture{ policy };
			establish_native_session(fixture);
			const auto authority       = fixture.Vault.Stores.front().Context.Authority;
			const auto transitionCount = fixture.Projections.transition_count();

			fixture.Host.shutdown();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority == authority
					&& fixture.Bindings.size() == 1U
					&& !fixture.Host.is_quiescent(),
				"delete-on-shutdown waits for exact accepted-credential cleanup");

			fixture.Host.shutdown();
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.Cancellations.empty(),
				   "duplicate shutdown preserves the one required delete");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::absent,
				});
			expect(!fixture.Host.is_quiescent() && fixture.Bindings.size() == 1U,
				   "delete completion cannot clear authority off the decision "
				   "lane");
			fixture.Lane.run_next();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 0U
					   && fixture.Projections.transition_count()
						   == transitionCount,
				   "authoritative deletion or absence completes shutdown cleanup");

			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_error{
					.Context = cleanup,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 0U,
				   "late duplicate delete cannot rewrite successful cleanup");
		}

		{
			host_fixture fixture{ policy };
			establish_native_session(fixture);
			fixture.Host.shutdown();
			const auto cleanup       = fixture.Vault.Deletes.front().Context;
			auto       wrong         = cleanup;
			wrong.IdentityGeneration = core::github_identity_generation{
				cleanup.IdentityGeneration.value() + 1U
			};
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = wrong,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Host.is_quiescent()
					&& fixture.Host.shutdown_cleanup_failed()
					&& fixture.Bindings.size() == 1U
					&& fixture.Projections.last_host_diagnostic().Reason
						== core::github_reason_code::vault_unavailable,
				"wrong cleanup terminal is visible and retains the exact binding "
				"tombstone");

			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 1U,
				   "late exact delete cannot conceal an already failed cleanup");
		}
	}

	auto test_caller_cache_cannot_bypass_capability_probe() -> void
	{
		static_assert(!std::is_constructible_v<
					  application::github_application_host,
					  core::github_state,
					  application::github_profile_port&,
					  application::github_viewer_port&,
					  application::github_capability_port&,
					  application::github_pull_request_port&,
					  application::github_credential_vault_port&,
					  application::github_credential_binding_catalog&,
					  application::github_capability_cache,
					  application::decision_executor&,
					  application::github_projection_port&>);

		auto untrusted = application::github_capability_cache::create({ });
		expect(untrusted.has_value(), "hostile cache fixture creates");
		const auto profile = native_profile(core::host_profile_id{ 11U });
		const auto forged  = untrusted->store(application::github_capability_probe_observation{
			.Authority = {
				.Profile               = profile.Profile,
				.ProfileRevision       = profile.ProfileRevision,
				.Host                  = profile.Host,
				.Account               = core::github_account_id{ 51U },
				.ServerContract        = profile.ServerContract,
				.ProbeContractRevision = profile.ProbeContractRevision,
			},
			.ObservedAt  = application::github_freshness_epoch{ 1U },
			.Facts       = capability_facts(),
			.SchemaFacts = schema_facts(),
		});
		expect(forged.stored() && untrusted->size() == 1U,
			   "a standalone caller cache can contain syntactically valid facts");

		host_fixture fixture;
		advance_native_to_retrieve(fixture);
		const auto retrieve = fixture.Vault.Retrieves.front().Context;
		fixture.Vault.Retrieves.front().Completion(
			application::github_credential_retrieve_success{
				.Context    = retrieve,
				.Credential = credential(),
			});
		fixture.Lane.run_next();
		const auto account = fixture.Provider.Viewers.back().Request;
		fixture.Provider.Viewers.back().Completion(
			application::github_viewer_success{
				.Request          = account,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = std::make_shared<fake_lease>(),
			});
		fixture.Lane.run_next();
		expect(fixture.Provider.Capabilities.size() == 1U
				   && fixture.Host.capability_cache_size() == 0U
				   && !fixture.Host.accepted_session().has_value(),
			   "production host owns an empty cache and still performs the probe");
	}

	auto test_catalog_resume_and_replacement_revalidate_persistent_credentials()
		-> void
	{
		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   resumed{ bindings };
			const auto                                     capability = advance_resume_to_capability(resumed, authority);
			resumed.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = rate_limits_for(
						application::github_rate_limit_resource::graphql),
				});
			resumed.Lane.run_next();
			expect(resumed.Host.accepted_session().has_value()
					   && resumed.Vault.Stores.empty() && bindings.size() == 1U,
				   "fresh host resumes only after exact account and capability "
				   "validation");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture mismatchedProfile{ bindings };
			mismatchedProfile.Host.resume_profile(core::host_profile_id{ 11U });
			const auto request      = mismatchedProfile.Provider.Profiles.front().Request;
			auto       changed      = native_profile(request.Context.Profile);
			changed.ProfileRevision = application::github_profile_revision{ 32U };
			mismatchedProfile.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = changed,
				});
			mismatchedProfile.Lane.run_next();
			expect(mismatchedProfile.Vault.Retrieves.empty()
					   && mismatchedProfile.Vault.Deletes.size() == 1U
					   && bindings.size() == 1U,
				   "profile revision replacement exact-deletes the retained "
				   "authority");
			const auto cleanup = mismatchedProfile.Vault.Deletes.front().Context;
			mismatchedProfile.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			mismatchedProfile.Lane.run_next();
			expect(
				bindings.size() == 0U
					&& !mismatchedProfile.Host.state().PendingSession.has_value(),
				"successful replacement cleanup forgets the trusted binding");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     profile = core::host_profile_id{ 11U };
			static_cast<void>(seed_persistent_binding(
				bindings,
				profile,
				core::github_account_id{ 51U },
				ghes_native_profile(profile)));
			host_fixture mismatchedHostname{ bindings };
			mismatchedHostname.Host.resume_profile(profile);
			const auto request = mismatchedHostname.Provider.Profiles.front().Request;
			const auto changed = ghes_native_profile(
				request.Context.Profile, "ghe.other.invalid");
			mismatchedHostname.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = changed,
				});
			mismatchedHostname.Lane.run_next();
			expect(
				mismatchedHostname.Vault.Retrieves.empty()
					&& mismatchedHostname.Vault.Deletes.size() == 1U
					&& bindings.size() == 1U,
				"hostname replacement exact-deletes the retained authority");
			const auto cleanup = mismatchedHostname.Vault.Deletes.front().Context;
			mismatchedHostname.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			mismatchedHostname.Lane.run_next();
			expect(
				bindings.size() == 0U
					&& !mismatchedHostname.Host.state()
							.PendingSession.has_value(),
				"hostname replacement cleanup forgets the trusted binding");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture changedAccount{ bindings };
			changedAccount.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = changedAccount.Provider.Profiles.front().Request;
			changedAccount.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			changedAccount.Lane.run_next();
			const auto retrieve = changedAccount.Vault.Retrieves.front().Context;
			changedAccount.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			changedAccount.Lane.run_next();
			const auto account = changedAccount.Provider.Viewers.front().Request;
			changedAccount.Provider.Viewers.front().Completion(
				application::github_viewer_success{
					.Request    = account,
					.Viewer     = viewer(core::github_account_id{ 52U }),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			changedAccount.Lane.run_next();
			expect(changedAccount.Provider.Capabilities.empty()
					   && changedAccount.Vault.Deletes.size() == 1U
					   && changedAccount.Vault.Deletes.front()
							   .Context.Authority.Account
						   == core::github_account_id{ 51U },
				   "resumed /user account mismatch exact-deletes the selected "
				   "authority");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   failedProbe{ bindings };
			const auto                                     capability = advance_resume_to_capability(failedProbe, authority);
			failedProbe.Provider.Capabilities.front().Completion(
				application::github_capability_error{
					.Request = capability,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			failedProbe.Lane.run_next();
			expect(
				failedProbe.Vault.Deletes.empty()
					&& !failedProbe.Host.accepted_session().has_value()
					&& !failedProbe.Host.state().PendingSession.has_value()
					&& bindings.size() == 1U,
				"ordinary resume failure preserves the trusted persistent binding");

			failedProbe.Host.resume_profile(core::host_profile_id{ 11U });
			expect(failedProbe.Vault.Deletes.empty()
					   && failedProbe.Provider.Profiles.size() == 2U,
				   "same-profile resume retry preserves rather than deletes its "
				   "binding");
			const auto retryProfile = failedProbe.Provider.Profiles.back().Request;
			failedProbe.Provider.Profiles.back().Completion(
				application::github_profile_success{
					.Request   = retryProfile,
					.Authority = native_profile(retryProfile.Context.Profile),
				});
			failedProbe.Lane.run_next();
			expect(failedProbe.Vault.Retrieves.size() == 2U
					   && failedProbe.Vault.Deletes.empty(),
				   "same-profile retry reaches the exact retained vault authority");
			const auto retryRetrieve = failedProbe.Vault.Retrieves.back().Context;
			failedProbe.Vault.Retrieves.back().Completion(
				application::github_credential_retrieve_error{
					.Context = retryRetrieve,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			failedProbe.Lane.run_next();

			failedProbe.Host.dispatch(core::deactivate_github_identity_intent{ });
			expect(failedProbe.Vault.Deletes.size() == 1U,
				   "explicit invalidation after failure exact-deletes retained "
				   "material");
			const auto cleanup = failedProbe.Vault.Deletes.front().Context;
			failedProbe.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			failedProbe.Lane.run_next();
			expect(bindings.size() == 0U,
				   "explicit invalidation forgets the retained catalog authority");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authorityA = seed_persistent_binding(
				bindings, core::host_profile_id{ 11U }, core::github_account_id{ 51U });
			const auto authorityB = seed_persistent_binding(
				bindings, core::host_profile_id{ 12U }, core::github_account_id{ 52U });
			host_fixture switchProfile{ bindings };
			const auto   capability = advance_resume_to_capability(switchProfile, authorityA);
			switchProfile.Provider.Capabilities.front().Completion(
				application::github_capability_success{
					.Request     = capability,
					.Facts       = capability_facts(),
					.SchemaFacts = schema_facts(),
					.RateLimits  = rate_limits_for(
						application::github_rate_limit_resource::graphql),
				});
			switchProfile.Lane.run_next();

			switchProfile.Host.resume_profile(core::host_profile_id{ 12U });
			expect(
				switchProfile.Vault.Deletes.size() == 1U
					&& switchProfile.Vault.Deletes.front().Context.Authority
						== authorityA
					&& switchProfile.Provider.Profiles.size() == 1U,
				"switching retained profiles deletes the active authority first");
			const auto cleanup = switchProfile.Vault.Deletes.front().Context;
			switchProfile.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			switchProfile.Lane.run_next();
			expect(bindings.size() == 1U
					   && switchProfile.Provider.Profiles.size() == 2U,
				   "successful predecessor cleanup starts the staged retained "
				   "profile");

			const auto failedB = switchProfile.Provider.Profiles.back().Request;
			switchProfile.Provider.Profiles.back().Completion(
				application::github_profile_error{
					.Request = failedB,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			switchProfile.Lane.run_next();
			expect(!switchProfile.Host.state().PendingSession.has_value()
					   && switchProfile.Vault.Deletes.size() == 1U,
				   "ordinary staged-profile failure preserves its trusted binding");

			switchProfile.Host.dispatch(core::deactivate_github_identity_intent{ });
			expect(
				switchProfile.Vault.Deletes.size() == 2U
					&& switchProfile.Vault.Deletes.back().Context.Authority
						== authorityB,
				"later deactivation exact-deletes the staged retained authority");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authorityA = seed_persistent_binding(
				bindings, core::host_profile_id{ 11U }, core::github_account_id{ 51U });
			const auto authorityB = seed_persistent_binding(
				bindings, core::host_profile_id{ 12U }, core::github_account_id{ 52U });
			host_fixture inflightSwitch{ bindings };
			inflightSwitch.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profileA = inflightSwitch.Provider.Profiles.front().Request;
			inflightSwitch.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profileA,
					.Authority = native_profile(profileA.Context.Profile),
				});
			inflightSwitch.Lane.run_next();
			expect(inflightSwitch.Vault.Retrieves.size() == 1U
					   && inflightSwitch.Vault.Retrieves.front().Context.Authority
						   == authorityA,
				   "profile A resume starts its exact retained retrieval");

			inflightSwitch.Host.resume_profile(core::host_profile_id{ 12U });
			expect(inflightSwitch.Vault.Deletes.empty()
					   && inflightSwitch.Provider.Profiles.size() == 1U,
				   "profile B waits for the running profile A vault terminal");
			const auto retrieveA = inflightSwitch.Vault.Retrieves.front().Context;
			inflightSwitch.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_cancelled{
					.Context = retrieveA,
				});
			inflightSwitch.Lane.run_next();
			expect(inflightSwitch.Vault.Deletes.size() == 1U
					   && inflightSwitch.Vault.Deletes.front().Context.Authority
						   == authorityA
					   && bindings.size() == 2U,
				   "deferred invalidation exact-deletes A rather than staged B");

			const auto cleanupA = inflightSwitch.Vault.Deletes.front().Context;
			inflightSwitch.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanupA,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			inflightSwitch.Lane.run_next();
			expect(bindings.size() == 1U
					   && inflightSwitch.Provider.Profiles.size() == 2U,
				   "profile B starts only after A cleanup forgets A");

			const auto profileB = inflightSwitch.Provider.Profiles.back().Request;
			inflightSwitch.Provider.Profiles.back().Completion(
				application::github_profile_error{
					.Request = profileB,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			inflightSwitch.Lane.run_next();
			inflightSwitch.Host.dispatch(
				core::deactivate_github_identity_intent{ });
			expect(inflightSwitch.Vault.Deletes.size() == 2U
					   && inflightSwitch.Vault.Deletes.back().Context.Authority
						   == authorityB,
				   "failed B remains selected for later exact invalidation");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture replacement{ bindings };
			replacement.Host.enroll_profile(core::host_profile_id{ 11U },
											credential());
			const auto profile = replacement.Provider.Profiles.front().Request;
			replacement.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			replacement.Lane.run_next();
			expect(replacement.Vault.Deletes.size() == 1U
					   && replacement.Provider.Viewers.empty(),
				   "fresh enrollment deletes an existing trusted binding first");
			const auto cleanup = replacement.Vault.Deletes.front().Context;
			replacement.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			replacement.Lane.run_next();
			expect(bindings.size() == 0U
					   && replacement.Provider.Profiles.size() == 2U,
				   "replacement cleanup resumes enrollment with no stale selector");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   failedProfile{ bindings };
			failedProfile.Host.resume_profile(core::host_profile_id{ 11U });
			const auto resume = failedProfile.Provider.Profiles.front().Request;
			failedProfile.Provider.Profiles.front().Completion(
				application::github_profile_error{
					.Request = resume,
					.Error   = {
						.Failure           = application::github_provider_failure::permission,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			failedProfile.Lane.run_next();
			expect(bindings.size() == 1U && failedProfile.Vault.Deletes.empty(),
				   "profile failure preserves the retained binding for recovery");

			failedProfile.Host.enroll_profile(core::host_profile_id{ 12U },
											  credential());
			expect(
				failedProfile.Vault.Deletes.size() == 1U
					&& failedProfile.Vault.Deletes.front().Context.Authority
						== authority,
				"later profile enrollment exact-deletes a failed resume binding");
			const auto cleanup = failedProfile.Vault.Deletes.front().Context;
			failedProfile.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			failedProfile.Lane.run_next();
			expect(bindings.size() == 0U
					   && failedProfile.Provider.Profiles.size() == 2U
					   && failedProfile.Provider.Profiles.back()
							   .Request.Context.Profile
						   == core::host_profile_id{ 12U },
				   "profile switch continues only after exact retained cleanup");
		}

		{
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   changedTransport{ bindings };
			changedTransport.Host.resume_profile(core::host_profile_id{ 11U });
			const auto request = changedTransport.Provider.Profiles.front().Request;
			changedTransport.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = cli_profile(request.Context.Profile),
				});
			changedTransport.Lane.run_next();
			expect(
				changedTransport.Vault.Deletes.size() == 1U
					&& changedTransport.Vault.Deletes.front().Context.Authority
						== authority
					&& changedTransport.Provider.Viewers.empty(),
				"native-to-CLI replacement exact-deletes the old native binding");
		}

		{
			application::github_credential_binding_catalog bindings;
			static_cast<void>(seed_persistent_binding(bindings));
			host_fixture absent{ bindings };
			absent.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = absent.Provider.Profiles.front().Request;
			absent.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			absent.Lane.run_next();
			const auto retrieve = absent.Vault.Retrieves.front().Context;
			absent.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_absent{
					.Context = retrieve,
				});
			absent.Lane.run_next();
			expect(bindings.size() == 0U && absent.Vault.Deletes.empty()
					   && !absent.Host.state().PendingSession.has_value(),
				   "authoritative vault absence forgets a stale binding without "
				   "delete");
		}
	}

	auto test_shutdown_preserves_inflight_persistent_resume() -> void
	{
		enum class retrieve_terminal {
			success,
			cancelled,
			error,
		};

		application::github_credential_binding_catalog bindings;
		static_cast<void>(seed_persistent_binding(bindings));

		for (const auto terminal : { retrieve_terminal::success,
									 retrieve_terminal::cancelled,
									 retrieve_terminal::error }) {
			host_fixture fixture{ bindings };
			fixture.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto running = fixture.Vault.Retrieves.front().Context;

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "shutdown cancels persistent retrieval without deleting its "
				   "binding");

			switch (terminal) {
			case retrieve_terminal::success:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_success{
						.Context    = running,
						.Credential = credential(),
					});
				break;
			case retrieve_terminal::cancelled:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_cancelled{
						.Context = running,
					});
				break;
			case retrieve_terminal::error:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			fixture.Lane.run_next();
			expect(fixture.Vault.Deletes.empty() && fixture.Host.is_quiescent()
					   && bindings.size() == 1U,
				   "every persistent retrieval terminal preserves the trusted "
				   "binding");
		}
	}

	auto test_shutdown_deletes_inflight_persistent_resume_by_policy() -> void
	{
		enum class retrieve_terminal {
			success,
			cancelled,
			error,
		};

		constexpr auto policy = application::github_credential_shutdown_policy::
			delete_retained_on_shutdown;
		for (const auto terminal : { retrieve_terminal::success,
									 retrieve_terminal::cancelled,
									 retrieve_terminal::error }) {
			application::github_credential_binding_catalog bindings;
			const auto                                     authority = seed_persistent_binding(bindings);
			host_fixture                                   fixture{ bindings, policy };
			fixture.Host.resume_profile(core::host_profile_id{ 11U });
			const auto profile = fixture.Provider.Profiles.front().Request;
			fixture.Provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			fixture.Lane.run_next();
			const auto running = fixture.Vault.Retrieves.front().Context;

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "delete policy waits for the in-flight retained retrieval "
				   "terminal");

			switch (terminal) {
			case retrieve_terminal::success:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_success{
						.Context    = running,
						.Credential = credential(),
					});
				break;
			case retrieve_terminal::cancelled:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_cancelled{
						.Context = running,
					});
				break;
			case retrieve_terminal::error:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority == authority
					&& !fixture.Host.is_quiescent(),
				"every retained retrieval terminal chains into one exact delete");

			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_cancelled{
					.Context = running,
				});
			fixture.Lane.run_next();
			expect(fixture.Vault.Deletes.size() == 1U
					   && !fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed(),
				   "duplicate retrieval terminal cannot overtake required delete");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed()
					   && bindings.size() == 0U,
				   "retained retrieval cleanup reaches exact deletion before "
				   "quiescence");
		}
	}

	auto test_shutdown_cancels_running_vault_work() -> void
	{
		enum class store_terminal {
			success,
			cancelled,
			error,
		};

		for (const auto terminal : {
				 store_terminal::success,
				 store_terminal::cancelled,
				 store_terminal::error,
			 }) {
			host_fixture fixture;
			advance_native_to_store(fixture);
			const auto running         = fixture.Vault.Stores.front().Context;
			const auto projectionCount = fixture.Projections.transition_count();

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "shutdown cancels a running store without early delete");

			switch (terminal) {
			case store_terminal::success:
				fixture.Vault.Stores.front().Completion(
					application::github_credential_store_success{
						.Context     = running,
						.Disposition = application::
							github_credential_store_disposition::stored,
					});
				break;
			case store_terminal::cancelled:
				fixture.Vault.Stores.front().Completion(
					application::github_credential_store_cancelled{
						.Context = running,
					});
				break;
			case store_terminal::error:
				fixture.Vault.Stores.front().Completion(
					application::github_credential_store_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			expect(fixture.Vault.Deletes.empty(),
				   "store terminal cannot clean up off the decision lane");
			fixture.Lane.run_next();
			expect(fixture.Vault.Deletes.size() == 1U
					   && fixture.Vault.Deletes.front().Context.Authority
						   == running.Authority
					   && fixture.Provider.Profiles.size() == 1U
					   && fixture.Projections.transition_count() == projectionCount
					   && !fixture.Host.is_quiescent(),
				   "every store terminal exact-cleans before shutdown quiesces");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.is_quiescent()
					   && fixture.Provider.Profiles.size() == 1U
					   && fixture.Projections.transition_count() == projectionCount,
				   "shutdown quiesces after provisional store cleanup terminates");
		}

		for (const auto terminal : {
				 store_terminal::success,
				 store_terminal::cancelled,
				 store_terminal::error,
			 }) {
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto running         = fixture.Vault.Retrieves.front().Context;
			const auto projectionCount = fixture.Projections.transition_count();

			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations == std::vector{ running }
					   && fixture.Vault.Deletes.empty()
					   && !fixture.Host.is_quiescent(),
				   "shutdown cancels a running retrieve without early delete");

			switch (terminal) {
			case store_terminal::success:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_success{
						.Context    = running,
						.Credential = credential(),
					});
				break;
			case store_terminal::cancelled:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_cancelled{
						.Context = running,
					});
				break;
			case store_terminal::error:
				fixture.Vault.Retrieves.front().Completion(
					application::github_credential_retrieve_error{
						.Context = running,
						.Failure = application::github_credential_vault_failure::
							operation_failed,
					});
				break;
			}
			fixture.Lane.run_next();
			expect(
				fixture.Vault.Deletes.size() == 1U
					&& fixture.Vault.Deletes.front().Context.Authority
						== running.Authority
					&& fixture.Provider.Viewers.size() == 1U
					&& fixture.Projections.transition_count() == projectionCount
					&& !fixture.Host.is_quiescent(),
				"every retrieve terminal exact-cleans the provisional credential");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.is_quiescent()
					   && !fixture.Host.shutdown_cleanup_failed(),
				   "retrieve shutdown cleanup succeeds before quiescence");
		}
	}

	auto test_shutdown_exact_cleans_every_provisional_phase() -> void
	{
		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.back().Request;

			fixture.Host.shutdown();
			expect(
				fixture.Provider.CancelledViewers == std::vector{ viewerRequest }
					&& fixture.Vault.Deletes.size() == 1U
					&& !fixture.Host.is_quiescent(),
				"shutdown during account viewer starts exact provisional cleanup");
		}

		{
			host_fixture fixture;
			advance_native_to_retrieve(fixture);
			const auto retrieve = fixture.Vault.Retrieves.front().Context;
			fixture.Vault.Retrieves.front().Completion(
				application::github_credential_retrieve_success{
					.Context    = retrieve,
					.Credential = credential(),
				});
			fixture.Lane.run_next();
			const auto viewerRequest = fixture.Provider.Viewers.back().Request;
			fixture.Provider.Viewers.back().Completion(
				application::github_viewer_success{
					.Request    = viewerRequest,
					.Viewer     = viewer(),
					.RateLimits = rate_limits_for(
						application::github_rate_limit_resource::core),
					.AccountTransport = std::make_shared<fake_lease>(),
				});
			fixture.Lane.run_next();
			const auto capabilityRequest = fixture.Provider.Capabilities.front().Request;

			fixture.Host.shutdown();
			expect(fixture.Provider.CancelledCapabilities
						   == std::vector{ capabilityRequest }
					   && fixture.Vault.Deletes.size() == 1U
					   && !fixture.Host.is_quiescent(),
				   "shutdown during capability probe exact-cleans persisted "
				   "enrollment");

			const auto cleanup = fixture.Vault.Deletes.front().Context;
			auto       wrong   = cleanup;
			wrong.Request      = core::request_id{ 999U };
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = wrong,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.is_quiescent()
					   && fixture.Host.shutdown_cleanup_failed()
					   && fixture.Bindings.size() == 1U,
				   "wrong cleanup terminal leaves a recoverable catalog tombstone");

			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_success{
					.Context     = cleanup,
					.Disposition = application::github_credential_delete_disposition::deleted,
				});
			fixture.Lane.run_next();
			expect(fixture.Host.is_quiescent()
					   && fixture.Host.shutdown_cleanup_failed(),
				   "a second terminal cannot rewrite failed shutdown cleanup");
		}

		{
			host_fixture fixture;
			establish_native_session(fixture);
			fixture.Host.dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 12U },
			});
			const auto cleanup = fixture.Vault.Deletes.front().Context;
			fixture.Host.shutdown();
			expect(fixture.Vault.Cancellations.empty()
					   && fixture.Vault.Deletes.size() == 1U
					   && !fixture.Host.is_quiescent(),
				   "shutdown preserves an exact delete already in flight");
			fixture.Vault.Deletes.front().Completion(
				application::github_credential_delete_error{
					.Context = cleanup,
					.Failure = application::github_credential_vault_failure::
						operation_failed,
				});
			fixture.Lane.run_next();
			expect(
				fixture.Host.is_quiescent()
					&& fixture.Host.shutdown_cleanup_failed()
					&& fixture.Bindings.size() == 1U
					&& fixture.Projections.host_diagnostic_count() != 0U
					&& fixture.Projections.last_host_diagnostic().Reason
						== core::github_reason_code::vault_unavailable,
				"terminal cleanup failure is distinct from successful quiescence");
		}
	}

	auto test_shutdown_cancel_has_no_completion_gate_lock_inversion() -> void
	{
		host_fixture fixture;
		fixture.Host.dispatch(core::activate_host_profile_intent{
			.Profile = core::host_profile_id{ 11U },
		});
		fixture.Provider.CompleteProfileFromWorkerOnCancel = true;
		fixture.Host.shutdown();
		expect(fixture.Host.is_quiescent() && fixture.Lane.pending() == 1U,
			   "worker callback can hand off while cancel waits during shutdown");
		fixture.Lane.drain();
	}

	auto test_destroyed_host_suppresses_late_callbacks_before_posting() -> void
	{
		{
			fake_provider                                  provider;
			fake_vault                                     vault;
			application::manual_executor                   lane;
			recording_projections                          projections;
			application::github_credential_binding_catalog bindings;
			auto                                           host = std::make_unique<application::github_application_host>(
				initial_state(),
				provider,
				provider,
				provider,
				provider,
				vault,
				bindings,
				lane,
				projections);
			host->dispatch(core::activate_host_profile_intent{
				.Profile = core::host_profile_id{ 11U },
			});
			const auto request = provider.Profiles.front().Request;
			host.reset();

			provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = request,
					.Authority = cli_profile(request.Context.Profile),
				});
			expect(lane.pending() == 0U && provider.Viewers.empty(),
				   "late provider terminal cannot post after host destruction");
		}

		{
			fake_provider                                  provider;
			fake_vault                                     vault;
			application::manual_executor                   lane;
			recording_projections                          projections;
			application::github_credential_binding_catalog bindings;
			auto                                           host = std::make_unique<application::github_application_host>(
				initial_state(),
				provider,
				provider,
				provider,
				provider,
				vault,
				bindings,
				lane,
				projections);
			host->enroll_profile(core::host_profile_id{ 11U }, credential());
			const auto profile = provider.Profiles.front().Request;
			provider.Profiles.front().Completion(
				application::github_profile_success{
					.Request   = profile,
					.Authority = native_profile(profile.Context.Profile),
				});
			lane.run_next();
			const auto bootstrap = provider.Viewers.front().Request;
			provider.Viewers.front().Completion(application::github_viewer_success{
				.Request          = bootstrap,
				.Viewer           = viewer(),
				.RateLimits       = rate_limits_for(application::github_rate_limit_resource::core),
				.AccountTransport = { },
			});
			lane.run_next();
			const auto store = vault.Stores.front().Context;
			host.reset();

			vault.Stores.front().Completion(
				application::github_credential_store_success{
					.Context     = store,
					.Disposition = application::github_credential_store_disposition::stored,
				});
			expect(lane.pending() == 0U && vault.Deletes.empty()
					   && bindings.size() == 1U,
				   "late stored secret retains an exact catalog tombstone");
		}
	}
}

auto main() -> int
{
	test_enrollment_acceptance_snapshot_and_exact_delete();
	test_accepted_snapshot_handoff_and_exact_invalidation();
	test_success_projection_is_safe_atomic_and_conversation_complete();
	test_every_provider_failure_maps_to_safe_pull_request_failure();
	test_retries_are_bounded_and_stale_work_is_cancelled();
	test_retry_after_is_terminal_and_preserved();
	test_wrong_correlations_and_submit_exceptions_fail_closed();
	test_rate_observation_presence_follows_server_contract();
	test_no_graphql_read_rejects_graphql_error_observations();
	test_projection_failures_cannot_interrupt_effects_or_cleanup();
	test_projection_reentry_is_serialized_after_the_current_turn();
	test_invalid_enrollment_cannot_leak_into_a_later_activation();
	test_snapshot_authority_and_pr_cancellation_fail_closed();
	test_new_open_clears_host_snapshot_but_refresh_retains_it();
	test_vault_wrong_context_and_submit_throw_fail_closed();
	test_reenrollment_waits_for_running_vault_cleanup();
	test_bootstrap_lease_and_changed_account_fail_closed();
	test_shutdown_cancels_and_suppresses_late_completion();
	test_shutdown_preserves_persistent_credentials();
	test_shutdown_deletes_persistent_credentials_by_policy();
	test_caller_cache_cannot_bypass_capability_probe();
	test_catalog_resume_and_replacement_revalidate_persistent_credentials();
	test_shutdown_preserves_inflight_persistent_resume();
	test_shutdown_deletes_inflight_persistent_resume_by_policy();
	test_shutdown_cancels_running_vault_work();
	test_shutdown_exact_cleans_every_provisional_phase();
	test_shutdown_cancel_has_no_completion_gate_lock_inversion();
	test_destroyed_host_suppresses_late_callbacks_before_posting();
	std::cout << "application github host tests passed\n";
	return EXIT_SUCCESS;
}
