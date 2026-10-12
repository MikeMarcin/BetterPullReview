#include "bpr/application/github_window_read_host.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/github_read_admission.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/sha256.hpp"
#include "github_projection_mapping.hpp"
#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bpr::application {
	auto object_loading_for(const std::optional<accepted_pull_request_shape>& aShape) noexcept
		-> pull_request_object_loading
	{
		const auto limits = github_read_limits{ };
		if (!aShape || !aShape->ReportedCommitCount
			|| aShape->FileCount > limits.MaximumFiles
			|| aShape->ReturnedCommitCount > limits.MaximumCommits
			|| std::min(*aShape->ReportedCommitCount, limits.MaximumCommits) != aShape->ReturnedCommitCount) {
			return pull_request_object_loading::complete;
		}
		// PR counts do not bound the repository's trees, blobs or ancestors.
		// Even a one-file change can exceed the complete-clone budget. The
		// epoch host still requires a sparse-capable lease and exact selected
		// tree/blob evidence before it can acquire missing material.
		return pull_request_object_loading::selected_objects;
	}

	namespace {
		void append_u64(std::vector<std::uint8_t>& aBytes,
						const std::uint64_t        aValue)
		{
			for (std::size_t shift = 8U; shift != 0U; --shift) {
				aBytes.push_back(
					static_cast<std::uint8_t>(aValue >> ((shift - 1U) * 8U)));
			}
		}

		void append_text(std::vector<std::uint8_t>& aBytes,
						 const std::string_view     aValue)
		{
			append_u64(aBytes, aValue.size());
			aBytes.insert(aBytes.end(), aValue.begin(), aValue.end());
		}

		[[nodiscard]] auto host_authority_fingerprint(
			const github_profile_authority& aProfile)
			-> core::host_authority_fingerprint
		{
			auto bytes = std::vector<std::uint8_t>{ };
			bytes.reserve(128U + aProfile.Hostname.value().size());
			append_text(bytes, "BetterPullReview/github-host-authority/v1");
			append_u64(bytes, aProfile.Profile.value());
			append_u64(bytes, aProfile.ProfileRevision.value());
			append_u64(bytes, aProfile.Host.value());
			append_text(bytes, aProfile.Hostname.value());
			append_u64(bytes, static_cast<std::uint8_t>(aProfile.ServerContract));
			append_u64(bytes, static_cast<std::uint8_t>(aProfile.Authentication));
			append_u64(bytes, aProfile.ProbeContractRevision.value());
			return core::host_authority_fingerprint{ core::sha256(bytes) };
		}

		[[nodiscard]] auto base_branch_identity(
			const core::repository_id  aRepository,
			const git_base_branch_ref& aRef) -> core::base_branch_id
		{
			auto bytes = std::vector<std::uint8_t>{ };
			bytes.reserve(96U + aRef.view().size());
			append_text(bytes, "BetterPullReview/base-branch/v1");
			append_u64(bytes, aRepository.value());
			append_text(bytes, aRef.view());
			return core::base_branch_id{ core::sha256(bytes) };
		}

		[[nodiscard]] constexpr auto epoch_review_state(
			const github_review_state aState) noexcept
			-> std::optional<core::github_review_state>
		{
			switch (aState) {
			case github_review_state::pending:
				return core::github_review_state::pending;
			case github_review_state::approved:
				return core::github_review_state::approved;
			case github_review_state::changes_requested:
				return core::github_review_state::changes_requested;
			case github_review_state::commented:
				return core::github_review_state::commented;
			case github_review_state::dismissed:
				return core::github_review_state::dismissed;
			case github_review_state::uninitialized:
				return std::nullopt;
			}
			return std::nullopt;
		}

		struct smart_authority_state final
		{
			void retain(const git_smart_transport& aTransport)
			{
				const std::scoped_lock lock{ Mutex };
				std::erase_if(Transports,
							  [](const auto& aCandidate) { return aCandidate.expired(); });
				Transports.emplace_back(aTransport);
			}

			void revoke() noexcept
			{
				if (Revoked.exchange(true, std::memory_order_acq_rel)) {
					return;
				}
				auto transports = std::vector<
					std::weak_ptr<git_smart_transport_port>>{ };
				{
					const std::scoped_lock lock{ Mutex };
					transports.swap(Transports);
				}
				for (const auto& candidate : transports) {
					if (const auto transport = candidate.lock()) {
						transport->revoke();
					}
				}
			}

			std::atomic_bool                                     Revoked = false;
			std::mutex                                           Mutex;
			std::vector<std::weak_ptr<git_smart_transport_port>> Transports;
		};

		class bound_git_smart_authority final : public git_smart_authority_lease
		{
		public:
			bound_git_smart_authority(git_smart_authority_binding            aBinding,
									  github_account_transport               aAccount,
									  std::shared_ptr<smart_authority_state> aState)
				: Binding{ std::move(aBinding) }
				, Account{ std::move(aAccount) }
				, State{ std::move(aState) }
			{
			}

			// The credential-free binding. Composition owns the anonymous
			// factory for the host's lifetime, so the lease borrows it rather
			// than sharing an account transport it must never hold.
			bound_git_smart_authority(git_smart_authority_binding            aBinding,
									  git_smart_transport_factory_port*      aAnonymous,
									  std::shared_ptr<smart_authority_state> aState)
				: Binding{ std::move(aBinding) }
				, Anonymous{ aAnonymous }
				, State{ std::move(aState) }
			{
			}

			~bound_git_smart_authority() override { revoke(); }

			[[nodiscard]] auto binding() const noexcept
				-> const git_smart_authority_binding& override
			{
				return Binding;
			}

			[[nodiscard]] auto supports_sparse_commit_trees() const noexcept -> bool override
			{
				if (State->Revoked.load(std::memory_order_acquire)) {
					return false;
				}
				auto* const factory = Account == nullptr
					? Anonymous
					: dynamic_cast<git_smart_transport_factory_port*>(Account.get());
				return factory != nullptr && factory->supports_sparse_commit_trees();
			}

			[[nodiscard]] auto acquire(const git_smart_request_context& aContext)
				-> git_smart_transport_result<git_smart_transport> override
			{
				if (State->Revoked.load(std::memory_order_acquire)
					|| !application::valid(aContext)
					|| aContext.ApplicationSession != Binding.ApplicationSession
					|| aContext.Authority.Profile != Binding.Profile
					|| aContext.Authority.ProfileRevision != Binding.ProfileRevision
					|| aContext.Authority.Host != Binding.Host
					|| aContext.Authority.HostAuthority != Binding.HostAuthority
					|| aContext.Authority.Account != Binding.Account
					|| aContext.Authority.Repository != Binding.Repository
					|| aContext.Repository != Binding.Locator
					|| aContext.Revision.Base.algorithm() != Binding.ObjectFormat
					|| aContext.Revision.Head.algorithm() != Binding.ObjectFormat) {
					return std::unexpected{ git_smart_transport_error{
						.Failure = git_smart_transport_failure::invalid_request } };
				}
				auto* const factory = Account == nullptr
					? Anonymous
					: dynamic_cast<git_smart_transport_factory_port*>(Account.get());
				if (factory == nullptr) {
					return std::unexpected{ git_smart_transport_error{
						.Failure = git_smart_transport_failure::unavailable } };
				}
				auto acquired = factory->acquire(aContext);
				if (!acquired) {
					return std::unexpected{ acquired.error() };
				}
				State->retain(*acquired);
				if (State->Revoked.load(std::memory_order_acquire)) {
					(*acquired)->revoke();
					return std::unexpected{ git_smart_transport_error{
						.Failure = git_smart_transport_failure::cancelled } };
				}
				return std::move(*acquired);
			}

			void revoke() noexcept override { State->revoke(); }

			[[nodiscard]] auto revoked() const noexcept -> bool override
			{
				return State->Revoked.load(std::memory_order_acquire);
			}

			[[nodiscard]] auto state() const noexcept
				-> std::shared_ptr<smart_authority_state>
			{
				return State;
			}

		private:
			git_smart_authority_binding            Binding;
			github_account_transport               Account;
			git_smart_transport_factory_port*      Anonymous = nullptr;
			std::shared_ptr<smart_authority_state> State;
		};

		[[nodiscard]] constexpr auto
		retryable(const github_provider_error& aError) noexcept -> bool
		{
			if (!valid(aError)
				|| (aError.RetryAfterSeconds.has_value()
					&& *aError.RetryAfterSeconds != 0U)
				|| is_limited(aError.RateLimits)) {
				return false;
			}
			return aError.Failure == github_provider_failure::network
				|| aError.Failure == github_provider_failure::timeout
				|| aError.Failure == github_provider_failure::unavailable;
		}

		[[nodiscard]] constexpr auto
		mapped_failure(const github_provider_failure aFailure) noexcept
			-> core::github_failure
		{
			switch (aFailure) {
			case github_provider_failure::authentication:
				return core::github_failure::authentication;
			case github_provider_failure::permission:
				return core::github_failure::permission;
			case github_provider_failure::network:
			case github_provider_failure::process_startup:
			case github_provider_failure::unavailable:
				return core::github_failure::network;
			case github_provider_failure::api_version:
				return core::github_failure::api_version;
			case github_provider_failure::unsupported_capability:
				return core::github_failure::capability;
			case github_provider_failure::rate_limit:
				return core::github_failure::rate_limit;
			case github_provider_failure::certificate:
				return core::github_failure::certificate;
			case github_provider_failure::redirect:
				return core::github_failure::redirect;
			case github_provider_failure::timeout:
				return core::github_failure::timeout;
			case github_provider_failure::cli_unavailable:
				return core::github_failure::cli_unavailable;
			case github_provider_failure::vault:
				return core::github_failure::vault;
			case github_provider_failure::malformed_response:
				return core::github_failure::malformed_response;
			case github_provider_failure::response_limit:
				return core::github_failure::response_too_large;
			}
			return core::github_failure::malformed_response;
		}

		[[nodiscard]] constexpr auto
		valid_rate_limit_resources(const github_rate_limit_state& aState,
								   const bool                     aAllowCore,
								   const bool                     aAllowSearch,
								   const bool                     aAllowGraphql) noexcept -> bool
		{
			return valid(aState) && (aAllowCore || !aState.Core.has_value())
				&& (aAllowSearch || !aState.Search.has_value())
				&& (aAllowGraphql || !aState.Graphql.has_value());
		}

		[[nodiscard]] constexpr auto
		requires_rate_limit_observations(const github_server_contract aContract) noexcept
			-> bool
		{
			return aContract == github_server_contract::github_com_2022_11_28;
		}

		[[nodiscard]] constexpr auto valid_snapshot_rate_limits(
			const github_profile_authority&     aAuthority,
			const github_pull_request_snapshot& aSnapshot) noexcept -> bool
		{
			if (!valid(aSnapshot.RateLimits) || is_secondary_limited(aSnapshot.RateLimits)) {
				return false;
			}
			const auto usesGraphql = aSnapshot.CapabilityEvidence.ReadShape.ReviewThreads
				|| aSnapshot.CapabilityEvidence.ReadShape.ViewedFileState;
			if (aSnapshot.RateLimits.Graphql.has_value() && !usesGraphql) {
				return false;
			}
			if (!requires_rate_limit_observations(aAuthority.ServerContract)) {
				return true;
			}
			return aSnapshot.RateLimits.Core.has_value()
				&& aSnapshot.RateLimits.Search.has_value()
				&& aSnapshot.RateLimits.Graphql.has_value() == usesGraphql;
		}

		[[nodiscard]] constexpr auto
		valid_provider_error_resources(const github_provider_error& aError,
									   const bool                   aAllowCore,
									   const bool                   aAllowSearch,
									   const bool                   aAllowGraphql) noexcept -> bool
		{
			return valid(aError)
				&& valid_rate_limit_resources(
					   aError.RateLimits, aAllowCore, aAllowSearch, aAllowGraphql);
		}

		[[nodiscard]] auto projected_capability_evidence(
			const github_capability_cache_entry& aEntry) noexcept
			-> github_capability_evidence
		{
			return {
				.Capabilities    = aEntry.Capabilities,
				.Revision        = aEntry.Revision,
				.ReadShape       = aEntry.ReadShape,
				.ContractVersion = 1U,
			};
		}

		template <class T>
		auto provider_completion_request(const T& aCompletion)
		{
			return std::visit([](const auto& aValue) { return aValue.Request; }, aCompletion);
		}

	}
	struct github_window_read_host::implementation final : std::enable_shared_from_this<implementation>
	{
		implementation(core::bpr_window_context aWindow, core::application_session_id aSession,
					   github_pull_request_port& aReads, decision_executor& aLane, publication aPublish,
					   github_epoch_handoff_port* aEpochs, github_pull_request_port* aNonymous,
					   git_smart_transport_factory_port* aGit, diagnostic_log_port* aLog,
					   cached_review_overview_port* aOverviews, effect_executor* aCacheEffects, cached_review_source_port* aSources, std::shared_ptr<github_read_admission> aAdmission)
			: Policy{ aWindow, aSession }
			, State{ Policy.state() }
			, DiagnosticWindowSession{ aSession }
			, PullRequests{ aReads }
			, DecisionLane{ aLane }
			, Publish{ std::move(aPublish) }
			, Epochs{ aEpochs }
			, AnonymousPullRequests{ aNonymous }
			, AnonymousSmartGit{ aGit }
			, DiagnosticLog{ aLog }
			, Overviews{ aOverviews }
			, CacheEffects{ aCacheEffects }
			, Sources{ aSources }
			, Admission{ aAdmission ? std::move(aAdmission) : std::make_shared<github_read_admission>() }
		{ }
		void record_pull_request(const diagnostic_outcome aOutcome, const core::request_id aRequest,
								 const std::optional<core::github_failure>         aFailure            = std::nullopt,
								 const std::optional<github_read_validation_error> aValidation         = std::nullopt,
								 const std::optional<diagnostic_paged_operation>   aLastOperation      = std::nullopt,
								 const std::optional<github_read_validation_error> aRelationshipDetail = std::nullopt,
								 const github_provider_error*                      aProviderError      = nullptr) const noexcept
		{
			if (DiagnosticLog == nullptr) {
				return;
			}
			auto event = diagnostic_event{
				.Operation      = diagnostic_operation::pull_request,
				.Outcome        = aOutcome,
				.Request        = aRequest.value(),
				.WindowSession  = DiagnosticWindowSession.value(),
				.PagedOperation = aLastOperation.value_or(diagnostic_paged_operation::none),
			};
			if (aValidation) {
				event.GithubFailure = *aValidation;
				if (*aValidation == github_read_validation_error::invalid_relationship) {
					event.RelationshipDetail = aRelationshipDetail;
				}
			} else if (aFailure) {
				event.GithubFailure = *aFailure;
			}
			if (aProviderError != nullptr) {
				event.HttpStatus             = aProviderError->HttpStatus;
				event.TransportFailureSource = aProviderError->TransportFailureSource;
			}
			static_cast<void>(DiagnosticLog->record(event));
		}
		bool                           is_public_preview() const { return State.AccessMode == core::github_access_mode::public_preview; }
		core::github_transition_output decide(const core::github_window_read_event& aEvent)
		{
			auto result = Policy.apply(Policy.window(), aEvent);
			if (result.Fetch) {
				result.Projection.Requests.emplace_back(*result.Fetch);
			}
			return std::move(result.Projection);
		}
		void publish_transition(const core::github_transition_output& aOutput,
								const github_session_read_model* aSession = nullptr, const github_pull_request_snapshot* aSnapshot = nullptr,
								const bool                                       aPreserveWindowNavigation = false,
								std::optional<github_cached_overview_projection> aCached                   = std::nullopt)
		{
			if (!Publish || Closing || SuppressPublication) {
				return;
			}
			try {
				auto error                  = std::exchange(LastError, std::nullopt);
				auto presentation           = github_presentation_update{ .Transition = project_github_transition(aOutput), .PreserveWindowNavigation = aPreserveWindowNavigation };
				presentation.CachedOverview = std::move(aCached);
				if (aSession != nullptr) {
					presentation.Session = project_github_session(*aSession);
				}
				if (aSnapshot != nullptr) {
					presentation.PullRequest                  = project_github_pull_request(*aSnapshot);
					presentation.PullRequest->MarkdownContext = markdown_identity();
					MarkdownBodies.clear();
					for (const auto& body : presentation.PullRequest->Conversations) {
						if (!body.Deleted) {
							MarkdownBodies.insert(body.Id);
						}
					}
				}
				if (error && (error->RetryAfterSeconds || has_observations(error->RateLimits))) {
					presentation.HostDiagnostics.push_back(project_github_host_diagnostic({
						.Reason            = core::reason_for(mapped_failure(error->Failure)),
						.RetryAfterSeconds = error->RetryAfterSeconds,
						.RateLimits        = RateLimits,
						.RateLimitCause    = error->RateLimitCause,
					}));
				}
				Publish({ Policy.window(), aOutput, std::move(error), PublicPreviewRefusal, std::move(presentation) });
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}
		void publish_and_execute(const core::github_transition_output& aOutput)
		{
			publish_transition(aOutput);
			if (Closing || !Session) {
				return;
			}
			for (const auto& request : aOutput.Requests) {
				if (const auto* fetch = std::get_if<core::fetch_pull_request_request>(&request)) {
					start_pull_request(fetch->Context);
				}
			}
		}
		void dispatch_core(const core::github_window_read_event& aEvent)
		{
			forget_cached_source();
			auto output = decide(aEvent);
			for (const auto& request : output.Requests) {
				const auto* fetch = std::get_if<core::fetch_pull_request_request>(&request);
				if (fetch && !fetch->Context.ExpectedRepository && !fetch->Context.ExpectedPullRequest) {
					invalidate_epoch_authority();
					revoke_review_mutation_transport();
					AcceptedSnapshot.reset();
				}
			}
			publish_and_execute(output);
		}
		void               observe_provider_error(const github_provider_error& aError) { static_cast<void>(merge(RateLimits, aError.RateLimits)); }
		void               publish_error_advisory(const github_provider_error& aError) { LastError = aError; }
		[[nodiscard]] auto make_epoch_handoff(
			const github_pull_request_snapshot& aSnapshot)
			-> std::optional<accepted_pull_request_handoff>
		{
			if (!AcceptedSession || !AcceptedProfileAuthority
				|| aSnapshot.Profile != AcceptedProfileAuthority->Profile
				|| aSnapshot.Host != AcceptedProfileAuthority->Host
				|| aSnapshot.Account != AcceptedSession->Viewer.Account
				|| aSnapshot.Viewer.Actor != AcceptedSession->Viewer.Node) {
				return std::nullopt;
			}

			const auto base = core::parse_git_commit_oid(
				aSnapshot.PullRequest.BaseOid.value());
			const auto head = core::parse_git_commit_oid(
				aSnapshot.PullRequest.HeadOid.value());
			auto baseRefText = std::string{ "refs/heads/" };
			baseRefText += aSnapshot.PullRequest.BaseRef;
			const auto baseRef = parse_git_base_branch_ref(baseRefText);
			if (!base || !head || !baseRef
				|| base->algorithm() != head->algorithm()) {
				return std::nullopt;
			}

			// A signed-out read has no reader, so the handoff names no account
			// and no viewer. The zero here is the authority's discriminant,
			// not a missing value: `core::pull_request_authority` refuses a
			// public-preview authority that carries either.
			const auto anonymous = aSnapshot.Mode
				== core::github_access_mode::public_preview;
			const auto viewer        = anonymous
				? core::github_viewer_actor_id{ }
				: core::github_viewer_actor_id{ aSnapshot.Account.value() };
			auto       currentReview = std::optional<core::current_viewer_review_candidate>{ };
			for (std::size_t index = 0U; !anonymous && index < aSnapshot.Reviews.size();
				 ++index) {
				const auto& review = aSnapshot.Reviews[index];
				const auto  state  = epoch_review_state(review.State);
				const auto  commit = review.Commit
					? core::parse_git_commit_oid(review.Commit->value())
					: std::nullopt;
				if (review.DatabaseId == 0U || !review.Author || !state || !commit
					|| review.Author->Actor != aSnapshot.Viewer.Actor
					|| *state == core::github_review_state::pending
					|| commit->algorithm() != base->algorithm()
					|| index >= core::maximum_github_review_order) {
					continue;
				}
				currentReview = core::current_viewer_review_candidate{
					.Review = core::github_review_id{ review.DatabaseId },
					.Actor  = viewer,
					.Order  = core::github_review_order{
						static_cast<std::uint16_t>(index + 1U) },
					.State  = *state,
					.Commit = *commit,
				};
			}

			auto handoff = accepted_pull_request_handoff{
				.ApplicationSession = State.ApplicationSession,
				.Profile            = aSnapshot.Profile,
				.ProfileRevision    = core::github_profile_revision{
					AcceptedProfileAuthority->ProfileRevision.value() },
				.Host           = aSnapshot.Host,
				.HostAuthority  = host_authority_fingerprint(*AcceptedProfileAuthority),
				.Hostname       = AcceptedProfileAuthority->Hostname,
				.ServerContract = AcceptedProfileAuthority->ServerContract,
				.Account        = aSnapshot.Account,
				.Viewer         = viewer,
				.Repository     = aSnapshot.Repository.Repository,
				.Locator        = {
					.Owner = aSnapshot.Repository.Owner,
					.Name  = aSnapshot.Repository.Name,
				},
				.PullRequest           = aSnapshot.PullRequest.PullRequest,
				.Number                = aSnapshot.PullRequest.Number,
				.Snapshot              = aSnapshot.Snapshot,
				.IdentityGeneration    = State.IdentityGeneration,
				.PullRequestGeneration = State.PullRequestGeneration,
				.Origin                = core::pull_request_revision_origin::live,
				.ObjectFormat          = base->algorithm(),
				.BaseBranch            = base_branch_identity(aSnapshot.Repository.Repository, *baseRef),
				.BaseRef               = *baseRef,
				.Base                  = *base,
				.Head                  = *head,
				.Mode                  = aSnapshot.Mode,
				.CurrentViewerReview   = std::move(currentReview),
			};
			handoff.Shape = accepted_pull_request_shape{
				.FileCount           = aSnapshot.Files.size(),
				.ReturnedCommitCount = aSnapshot.Commits.size(),
				.ReportedCommitCount = aSnapshot.PullRequest.ReportedCommitCount,
			};
			switch (aSnapshot.TimelineAvailability) {
			case github_timeline_availability::unavailable:
				handoff.TimelineAvailability = epoch_timeline_availability::unavailable;
				break;
			case github_timeline_availability::available:
				handoff.TimelineAvailability = epoch_timeline_availability::available;
				break;
			case github_timeline_availability::refused:
				handoff.TimelineAvailability = epoch_timeline_availability::refused;
				break;
			case github_timeline_availability::capped:
				handoff.TimelineAvailability = epoch_timeline_availability::capped;
				break;
			case github_timeline_availability::failed:
				handoff.TimelineAvailability = epoch_timeline_availability::failed;
				break;
			}
			if (aSnapshot.ForcePushEvents.size() > core::maximum_epoch_occurrence_count) {
				handoff.TimelineAvailability = epoch_timeline_availability::capped;
			}
			if (handoff.TimelineAvailability == epoch_timeline_availability::available) {
				for (const auto& event : aSnapshot.ForcePushEvents) {
					const auto eventHead = core::parse_git_commit_oid(event.AfterHeadOid);
					if (!eventHead || eventHead->algorithm() != handoff.ObjectFormat
						|| event.EventId == 0U || event.CreatedAtUnixMilliseconds <= 0) {
						handoff.ForcePushes.clear();
						handoff.TimelineAvailability = epoch_timeline_availability::failed;
						break;
					}
					handoff.ForcePushes.push_back(epoch_force_push_observation{
						.EventId              = event.EventId,
						.Head                 = *eventHead,
						.EventUtcMilliseconds = event.CreatedAtUnixMilliseconds,
					});
				}
			}

			if (anonymous) {
				// The anonymous authority reaches Git only through the
				// credential-free transport composition supplied. Without it
				// there is no repository to materialize, so the caller keeps
				// the handoff unavailable rather than sending an authority
				// that could not fetch.
				if (AnonymousSmartGit != nullptr) {
					auto state       = std::make_shared<smart_authority_state>();
					handoff.SmartGit = std::make_unique<bound_git_smart_authority>(
						authority_binding(handoff), AnonymousSmartGit, state);
				}
			} else if (dynamic_cast<git_smart_transport_factory_port*>(
						   AccountTransport.get())
					   != nullptr) {
				auto state       = std::make_shared<smart_authority_state>();
				handoff.SmartGit = std::make_unique<bound_git_smart_authority>(
					authority_binding(handoff), AccountTransport, state);
			}
			return valid(handoff) ? std::optional{ std::move(handoff) }
								  : std::nullopt;
		}

		void revoke_review_mutation_transport() noexcept
		{
			if (ReviewMutationTransport != nullptr) {
				ReviewMutationTransport->revoke();
				ReviewMutationTransport.reset();
			}
		}

		void renew_review_mutation_transport() noexcept
		{
			revoke_review_mutation_transport();
			if (AccountTransport == nullptr) {
				return;
			}
			auto* const factory = dynamic_cast<review_mutation_transport_factory_port*>(
				AccountTransport.get());
			if (factory == nullptr) {
				return;
			}
			ReviewMutationTransport = factory->acquire_review_mutation_transport();
			if (ReviewMutationTransport != nullptr
				&& !ReviewMutationTransport->active()) {
				ReviewMutationTransport.reset();
			}
		}

		void install_snapshot(github_pull_request_snapshot aSnapshot)
		{
			if (AcceptedSnapshot && AcceptedSnapshot->Profile == aSnapshot.Profile && AcceptedSnapshot->Host == aSnapshot.Host && AcceptedSnapshot->Account == aSnapshot.Account && AcceptedSnapshot->Mode == aSnapshot.Mode && AcceptedSnapshot->Repository.Repository == aSnapshot.Repository.Repository && AcceptedSnapshot->PullRequest.PullRequest == aSnapshot.PullRequest.PullRequest && AcceptedSnapshot->PullRequest.HeadOid == aSnapshot.PullRequest.HeadOid && AcceptedSnapshot->PullRequest.BaseOid == aSnapshot.PullRequest.BaseOid && AcceptedSnapshot->PullRequest.BaseRef == aSnapshot.PullRequest.BaseRef) {
				const auto& previous = *AcceptedSnapshot;
				if (aSnapshot.ReviewRequestsAvailability != github_observation_availability::available && !aSnapshot.ReviewRequestActorsObserved && (previous.ReviewRequestsAvailability == github_observation_availability::available || previous.ReviewRequestActorsObserved || previous.ReviewRequestsStale)) {
					aSnapshot.ReviewRequests              = previous.ReviewRequests;
					aSnapshot.ReviewRequestsStale         = true;
					aSnapshot.ReviewRequestActorsObserved = previous.ReviewRequestActorsObserved;
				}
				if (aSnapshot.Merge.Availability != github_observation_availability::available && (previous.Merge.Availability == github_observation_availability::available || previous.Merge.Stale)) {
					const auto availability      = aSnapshot.Merge.Availability;
					aSnapshot.Merge              = previous.Merge;
					aSnapshot.Merge.Availability = availability;
					aSnapshot.Merge.Stale        = true;
				}
				if (aSnapshot.BranchPolicy.RulesAvailability != github_observation_availability::available && (previous.BranchPolicy.RulesAvailability == github_observation_availability::available || previous.BranchPolicy.RulesStale)) {
					aSnapshot.BranchPolicy.Rules      = previous.BranchPolicy.Rules;
					aSnapshot.BranchPolicy.RulesStale = true;
				}
				if (aSnapshot.BranchPolicy.ProtectionAvailability != github_observation_availability::available && (previous.BranchPolicy.ProtectionAvailability == github_observation_availability::available || previous.BranchPolicy.ProtectionStale)) {
					aSnapshot.BranchPolicy.Protection      = previous.BranchPolicy.Protection;
					aSnapshot.BranchPolicy.ProtectionStale = true;
				}
			}
			AcceptedSnapshot = std::move(aSnapshot);
			if (MarkdownRevision != std::numeric_limits<std::uint64_t>::max()) {
				++MarkdownRevision;
			} else {
				MarkdownExhausted = true;
			}
			if (!is_public_preview()) {
				// Signed out there is no mutation lease to acquire.
				renew_review_mutation_transport();
			}
			static_cast<void>(merge(RateLimits, AcceptedSnapshot->RateLimits));
		}

		[[nodiscard]] static auto invalidation_for(
			const accepted_pull_request_handoff& aHandoff)
			-> pull_request_authority_invalidation
		{
			return {
				.ApplicationSession = aHandoff.ApplicationSession,
				.Profile            = aHandoff.Profile,
				.ProfileRevision    = aHandoff.ProfileRevision,
				.Host               = aHandoff.Host,
				.HostAuthority      = aHandoff.HostAuthority,
				.Account            = aHandoff.Account,
				.Viewer             = aHandoff.Viewer,
				.Repository         = aHandoff.Repository,
				.PullRequest        = aHandoff.PullRequest,
				.IdentityGeneration = aHandoff.IdentityGeneration,
				.Mode               = aHandoff.Mode,
			};
		}

		void invalidate_epoch_authority() noexcept
		{
			if (ActiveEpochSmartAuthority) {
				ActiveEpochSmartAuthority->revoke();
			}
			if (Epochs && ActiveEpochAuthority) {
				try {
					Epochs->invalidate(*ActiveEpochAuthority);
				} catch (...) {
					EpochHandoffDeliveryFailed = true;
				}
			}
			ActiveEpochSmartAuthority.reset();
			ActiveEpochAuthority.reset();
		}

		void deliver_epoch_handoff(
			const github_pull_request_snapshot& aSnapshot) noexcept
		{
			if (Epochs == nullptr) {
				return;
			}
			EpochHandoffUnavailable = false;
			if (is_public_preview() && AnonymousSmartGit == nullptr) {
				// Composition supplied no credential-free Git authority, so
				// the signed-out mode opens no catalog and no cache session:
				// it reports the unavailable state and sends nothing. Sending
				// nothing here is the designed answer, not a delivery that
				// failed.
				invalidate_epoch_authority();
				EpochHandoffUnavailable = true;
				return;
			}
			try {
				auto handoff = make_epoch_handoff(aSnapshot);
				if (!handoff) {
					EpochHandoffDeliveryFailed = true;
					return;
				}
				const auto authority = invalidation_for(*handoff);
				if (ActiveEpochAuthority && *ActiveEpochAuthority != authority) {
					invalidate_epoch_authority();
				}
				auto smartState = std::shared_ptr<smart_authority_state>{ };
				if (auto* lease = dynamic_cast<bound_git_smart_authority*>(
						handoff->SmartGit.get())) {
					smartState = lease->state();
				}
				Epochs->accept(std::move(*handoff));
				ActiveEpochAuthority      = authority;
				ActiveEpochSmartAuthority = std::move(smartState);
			} catch (...) {
				EpochHandoffDeliveryFailed = true;
			}
		}

		void start_pull_request(const core::pull_request_request_context& aContext)
		{
			// Publication can synchronously retire or replace this window's
			// scope. Only the still-pending exact request may spend transport.
			if (Closing || !Session || State.PendingPullRequest != aContext) {
				return;
			}
			record_pull_request(diagnostic_outcome::started, aContext.Request);
			// Signed out there is no account transport to hold, and holding a
			// null one is the point rather than an omission. The credential-
			// free authority is the port itself.
			const auto anonymousRead  = is_public_preview();
			const auto transportReady = anonymousRead
				? (AnonymousPullRequests != nullptr && AccountTransport == nullptr)
				: AccountTransport != nullptr;
			if (!AcceptedSession.has_value() || !AcceptedProfileAuthority.has_value()
				|| !AcceptedCapabilityEvidence.has_value() || !transportReady
				|| aContext.Profile != AcceptedProfileAuthority->Profile
				|| aContext.Host != AcceptedProfileAuthority->Host
				|| aContext.Account != AcceptedSession->Viewer.Account
				|| aContext.Capabilities != AcceptedCapabilityEvidence->Revision
				|| AcceptedCapabilityEvidence->Authority.Account != aContext.Account) {
				record_pull_request(diagnostic_outcome::failed, aContext.Request,
									core::github_failure::malformed_response);
				dispatch_core(core::pull_request_load_failed_event{
					.Context = aContext,
					.Failure = core::github_failure::malformed_response,
				});
				return;
			}
			submit_pull_request(github_pull_request_provider_request{
				.Context               = aContext,
				.ProfileAuthority      = *AcceptedProfileAuthority,
				.Viewer                = AcceptedSession->Viewer,
				.CapabilityEvidence    = *AcceptedCapabilityEvidence,
				.AccountTransport      = AccountTransport,
				.Attempt               = github_provider_attempt{ 1U },
				.Mode                  = AcceptedSession->Mode,
				.AuthenticationSession = AcceptedSession->Mode == core::github_access_mode::authenticated
					? std::optional{ Session->ApplicationSession }
					: std::nullopt,
			});
		}

		// One request names one authority. The mode on the request selects the
		// port, so a signed-out read can never reach the authenticated
		// provider and an authenticated read can never reach the anonymous
		// one, whichever of them a caller composed.
		[[nodiscard]] auto pull_request_port_for(
			const github_pull_request_provider_request& aRequest) noexcept
			-> github_pull_request_port&
		{
			if (aRequest.Mode == core::github_access_mode::public_preview
				&& AnonymousPullRequests != nullptr) {
				return *AnonymousPullRequests;
			}
			return PullRequests;
		}

		void submit_pull_request(const github_pull_request_provider_request& aRequest)
		{
			const auto usesGraphql = aRequest.CapabilityEvidence.ReadShape.ReviewThreads || aRequest.CapabilityEvidence.ReadShape.ViewedFileState;
			if (const auto refusal = Admission->refusal(aRequest.ProfileAuthority, aRequest.Viewer, usesGraphql)) {
				PendingProvider.reset();
				++ProviderOperationToken;
				observe_provider_error(*refusal);
				publish_error_advisory(*refusal);
				record_pull_request(diagnostic_outcome::failed, aRequest.Context.Request, mapped_failure(refusal->Failure));
				dispatch_core(core::pull_request_load_failed_event{ aRequest.Context, mapped_failure(refusal->Failure) });
				return;
			}
			PendingProvider  = aRequest;
			ProgressSequence = 0U;
			const auto token = ++ProviderOperationToken;
			auto       weak  = weak_from_this();
			auto*      lane  = &DecisionLane;
			++Outstanding;
			auto once     = std::make_shared<std::atomic_bool>(false);
			auto callback = [weak, lane, token, once](github_pull_request_completion aCompletion) mutable {
				if (once->exchange(true)) {
					return;
				}
				auto locked = weak.lock();
				if (!locked) {
					return;
				}
				try {
					lane->post([weak, token, terminal = std::move(aCompletion)]() mutable {
						if (auto host = weak.lock()) {
							--host->Outstanding;
							host->handle_pull_request_now(token, std::move(terminal));
						}
					});
				} catch (...) {
					--locked->Outstanding;
					locked->ProjectionDeliveryFailed = true;
				}
			};
			try {
				pull_request_port_for(aRequest).fetch_pull_request_with_observations(aRequest, callback,
																					 progress_callback(token, aRequest.Context, once), overview_callback(token, aRequest.Context, once));
			} catch (...) {
				callback(github_pull_request_error{ .Request = aRequest,
													.Error   = { .Failure = github_provider_failure::response_limit } });
			}
		}

		[[nodiscard]] auto overview_callback(const std::uint64_t                       aToken,
											 const core::pull_request_request_context& aContext, const std::shared_ptr<std::atomic_bool>& aTerminal)
			-> github_pull_request_overview_handler
		{
			auto posted = std::make_shared<std::atomic_bool>(false);
			return [weak = weak_from_this(), lane = &DecisionLane, aToken, aContext, aTerminal, posted](github_pull_request_overview_observation aObservation) {
				if (aObservation.Context != aContext || aTerminal->load() || posted->exchange(true)) {
					return;
				}
				auto host = weak.lock();
				if (!host) {
					return;
				}
				++host->Outstanding;
				try {
					lane->post([weak, aToken, observation = std::move(aObservation)] {
						if (auto current = weak.lock()) {
							--current->Outstanding;
							current->publish_overview(aToken, observation);
						}
					});
				} catch (...) {
					--host->Outstanding;
					host->ProjectionDeliveryFailed = true;
				}
			};
		}

		void publish_overview(const std::uint64_t aToken, const github_pull_request_overview_observation& aObservation)
		{
			if (Closing || !Publish || aToken != ProviderOperationToken || !PendingProvider
				|| PendingProvider->Context != aObservation.Context) {
				return;
			}
			try {
				auto projection = project_github_loading_overview(aObservation);
				if (!projection || !Policy.accept_overview_observation(Policy.window(), aObservation.Context)) {
					return;
				}
				Publish({ Policy.window(), { }, { }, PublicPreviewRefusal, github_presentation_update{ .LoadingOverview = std::move(projection) } });
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		struct progress_mailbox
		{
			std::mutex                          Mutex;
			std::optional<github_read_progress> Latest;
			bool                                Posted = false;
		};

		[[nodiscard]] auto progress_callback(const std::uint64_t                       aToken,
											 const core::pull_request_request_context& aContext,
											 const std::shared_ptr<std::atomic_bool>&  aTerminal)
			-> github_pull_request_progress_handler
		{
			auto mailbox = std::make_shared<progress_mailbox>();
			return [weak = weak_from_this(), lane = &DecisionLane, mailbox, aToken, aContext, aTerminal](
					   core::pull_request_request_context aObserved, github_read_progress aProgress) {
				if (aObserved != aContext || aTerminal->load() || aProgress.Sequence == 0U
					|| aProgress.Sequence > maximum_github_read_progress_sequence
					|| aProgress.PagesReceived > maximum_github_read_progress_pages
					|| aProgress.Phase > github_read_phase::update_history) {
					return;
				}
				auto host = weak.lock();
				if (!host) {
					return;
				}
				const std::lock_guard lock{ mailbox->Mutex };
				if (!mailbox->Latest || mailbox->Latest->Sequence < aProgress.Sequence) {
					mailbox->Latest = aProgress;
				}
				if (mailbox->Posted) {
					return;
				}
				mailbox->Posted = true;
				++host->Outstanding;
				try {
					lane->post([weak, mailbox, aToken, aContext] {
						std::optional<github_read_progress> progress;
						{
							const std::lock_guard mailboxLock{ mailbox->Mutex };
							progress        = std::exchange(mailbox->Latest, std::nullopt);
							mailbox->Posted = false;
						}
						if (auto current = weak.lock()) {
							--current->Outstanding;
							if (progress) {
								current->publish_progress(aToken, aContext, *progress);
							}
						}
					});
				} catch (...) {
					--host->Outstanding;
					mailbox->Posted                = false;
					host->ProjectionDeliveryFailed = true;
				}
			};
		}

		void publish_progress(const std::uint64_t                       aToken,
							  const core::pull_request_request_context& aContext, const github_read_progress& aProgress)
		{
			if (Closing || !Publish || aToken != ProviderOperationToken || !PendingProvider
				|| State.PendingPullRequest != aContext || PendingProvider->Context != aContext
				|| aProgress.Sequence <= ProgressSequence) {
				return;
			}
			ProgressSequence = aProgress.Sequence;
			try {
				Publish({ Policy.window(), { }, { }, PublicPreviewRefusal, github_presentation_update{ .ReadProgress = aProgress } });
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}
		auto overview_key(const core::pull_request_request_context& aRequest) const -> core::cached_review_key
		{
			return { host_authority_fingerprint(*AcceptedProfileAuthority).bytes(),
					 aRequest.Profile, aRequest.Host, aRequest.Account, aRequest.Repository, aRequest.Number };
		}
		auto retain_file_catalogue(const github_pull_request_snapshot& aSnapshot) const
			-> std::optional<std::vector<core::cached_review_file>>
		{
			if (aSnapshot.Files.size() > core::maximum_cached_files) {
				return std::nullopt;
			}
			std::vector<core::cached_review_file> files;
			files.reserve(aSnapshot.Files.size());
			for (const auto& file : aSnapshot.Files) {
				core::cached_file_change change;
				switch (file.Change) {
				case github_file_change::added:         change = core::cached_file_change::added; break;
				case github_file_change::modified:      change = core::cached_file_change::modified; break;
				case github_file_change::removed:       change = core::cached_file_change::removed; break;
				case github_file_change::renamed:       change = core::cached_file_change::renamed; break;
				case github_file_change::copied:        change = core::cached_file_change::copied; break;
				case github_file_change::type_changed:  change = core::cached_file_change::type_changed; break;
				case github_file_change::unchanged:     change = core::cached_file_change::unchanged; break;
				case github_file_change::uninitialized:
				default:                                return std::nullopt;
				}
				files.push_back({ file.Path, file.PreviousPath, change, file.Additions, file.Deletions });
			}
			return core::valid_cached_catalogue(files) ? std::optional{ std::move(files) } : std::nullopt;
		}
		static auto retained_thread_side(github_diff_side aSide) -> core::cached_thread_side
		{
			switch (aSide) {
			case github_diff_side::left:          return core::cached_thread_side::left;
			case github_diff_side::right:         return core::cached_thread_side::right;
			case github_diff_side::uninitialized: return core::cached_thread_side::unknown;
			}
			return core::cached_thread_side::unknown;
		}
		static auto retain_threads(const github_pull_request_snapshot& aSnapshot)
			-> std::optional<std::vector<core::cached_review_thread>>
		{
			if (aSnapshot.ReviewThreadsAvailability != github_collection_availability::available
				|| aSnapshot.ReviewThreads.size() > core::maximum_cached_threads) {
				return std::nullopt;
			}
			std::vector<core::cached_review_thread> threads;
			std::size_t                             comments = 0, bytes = 24;
			for (const auto& thread : aSnapshot.ReviewThreads) {
				if (thread.Comments.size() > core::maximum_cached_thread_comments - comments) {
					return std::nullopt;
				}
				comments += thread.Comments.size();
				bytes += 88 + thread.Thread.value().size() + thread.Path.size();
				core::cached_review_thread retained{
					.Id = std::string{ thread.Thread.value() }, .Path = thread.Path, .Line = thread.Line, .OriginalLine = thread.OriginalLine, .Side = retained_thread_side(thread.DiffSide), .FileLevel = thread.Subject == github_review_thread_subject::file, .Resolved = thread.Resolved, .Outdated = thread.Outdated
				};
				for (const auto& comment : thread.Comments) {
					bytes += 96 + comment.Comment.value().size() + comment.Body.size()
						+ (comment.Author ? comment.Author->Login.size() : 0) + (comment.ReplyTo ? comment.ReplyTo->value().size() : 0);
					if (bytes > core::maximum_cached_threads_bytes || comment.Body.size() > 64 * 1024) {
						return std::nullopt;
					}
					retained.Comments.push_back({ .Id = std::string{ comment.Comment.value() }, .ReplyTo = comment.ReplyTo ? std::optional{ std::string{ comment.ReplyTo->value() } } : std::nullopt, .AuthorLogin = comment.Author ? comment.Author->Login : std::string{ }, .Body = comment.Body, .Line = comment.Line, .OriginalLine = comment.OriginalLine, .Outdated = comment.Outdated, .Deleted = comment.Deleted });
				}
				threads.push_back(std::move(retained));
			}
			return core::valid_cached_threads(threads) ? std::optional{ std::move(threads) } : std::nullopt;
		}
		static auto retain_discussion(const github_pull_request_snapshot& aSnapshot, bool aReviews)
			-> std::optional<std::vector<core::cached_discussion_message>>
		{
			if ((aReviews ? aSnapshot.Reviews.size() : aSnapshot.IssueComments.size()) > core::maximum_cached_discussion_messages) {
				return std::nullopt;
			}
			std::vector<core::cached_discussion_message> messages;
			if (aReviews) {
				for (const auto& review : aSnapshot.Reviews) {
					core::cached_discussion_kind kind;
					switch (review.State) {
					case github_review_state::pending:           kind = core::cached_discussion_kind::pending_review; break;
					case github_review_state::commented:         kind = core::cached_discussion_kind::commented_review; break;
					case github_review_state::approved:          kind = core::cached_discussion_kind::approved_review; break;
					case github_review_state::changes_requested: kind = core::cached_discussion_kind::changes_requested_review; break;
					case github_review_state::dismissed:         kind = core::cached_discussion_kind::dismissed_review; break;
					case github_review_state::uninitialized:
					default:                                     return std::nullopt;
					}
					messages.push_back({ .Id = std::string{ review.Review.value() }, .AuthorLogin = review.Author ? review.Author->Login : std::string{ }, .Body = review.Body, .Kind = kind, .CommitOid = review.Commit ? std::optional{ std::string{ review.Commit->value() } } : std::nullopt });
				}
			} else {
				for (const auto& comment : aSnapshot.IssueComments) {
					messages.push_back({ .Id = std::string{ comment.Comment.value() }, .AuthorLogin = comment.Author ? comment.Author->Login : std::string{ }, .Body = comment.Body, .Deleted = comment.Deleted });
				}
			}
			return core::valid_cached_discussion(messages, aReviews) ? std::optional{ std::move(messages) } : std::nullopt;
		}
		void retain_overview(const core::pull_request_request_context& aRequest)
		{
			if (!Overviews || !CacheEffects || CacheWritePending || !AcceptedSnapshot
				|| !AcceptedProfileAuthority || aRequest.Mode != core::github_access_mode::authenticated) {
				return;
			}
			const auto& snapshot = *AcceptedSnapshot;
			if (snapshot.PullRequest.ReportedCommitCount.value_or(snapshot.Commits.size()) > 1000000) {
				return;
			}
			auto retained = core::cached_review_overview{
				.Key = overview_key(aRequest), .Repository = snapshot.Repository.Repository, .PullRequest = snapshot.PullRequest.PullRequest, .BaseOid = std::string{ snapshot.PullRequest.BaseOid.value() }, .HeadOid = std::string{ snapshot.PullRequest.HeadOid.value() }, .Title = snapshot.PullRequest.Title, .Body = snapshot.PullRequest.Body, .AuthorLogin = snapshot.PullRequest.Author ? snapshot.PullRequest.Author->Login : std::string{ }, .BaseRef = snapshot.PullRequest.BaseRef, .HeadRef = snapshot.PullRequest.HeadRef, .Files = static_cast<std::uint32_t>(snapshot.Files.size()), .Commits = static_cast<std::uint32_t>(snapshot.PullRequest.ReportedCommitCount.value_or(snapshot.Commits.size())), .State = static_cast<std::uint8_t>(snapshot.PullRequest.State), .Draft = snapshot.PullRequest.Draft, .FileCatalogue = retain_file_catalogue(snapshot), .ReviewThreads = retain_threads(snapshot), .GeneralComments = retain_discussion(snapshot, false), .ReviewSummaries = retain_discussion(snapshot, true)
			};
			if (!core::valid(retained)) {
				return;
			}
			CacheWritePending = true;
			++Outstanding;
			auto self = shared_from_this();
			try {
				CacheEffects->post([self, overview = std::move(retained)] {
					try {
						static_cast<void>(self->Overviews->save(overview, { }));
					} catch (...) { }
					self->DecisionLane.post([self] { --self->Outstanding; self->CacheWritePending = false; });
				});
			} catch (...) {
				--Outstanding;
				CacheWritePending = false;
			}
		}
		auto load_cached_overview(const github_pull_request_error& aError) -> bool
		{
			if (!Overviews || !CacheEffects || !AcceptedProfileAuthority || AcceptedSnapshot
				|| aError.Request.Mode != core::github_access_mode::authenticated
				|| aError.Error.Failure != github_provider_failure::network || aError.Error.HttpStatus != 0) {
				return false;
			}
			const auto key   = overview_key(aError.Request.Context);
			CacheReadStop    = std::stop_source{ };
			const auto stop  = CacheReadStop.get_token();
			const auto token = ProviderOperationToken;
			++Outstanding;
			auto self = shared_from_this();
			try {
				CacheEffects->post([self, key, stop, token, error = aError] {
					std::optional<core::cached_review_overview> loaded;
					try {
						loaded = self->Overviews->load(key, stop);
					} catch (...) { }
					self->DecisionLane.post([self, key, stop, token, error, cached = std::move(loaded)] {
						--self->Outstanding;
						if (self->Closing || stop.stop_requested() || token != self->ProviderOperationToken
							|| !self->PendingProvider || *self->PendingProvider != error.Request) {
							return;
						}
						const auto admitted = cached && core::admit_cached_overview(self->State, error.Request.Context, core::github_failure::network, key, *cached);
						self->PendingProvider.reset();
						self->publish_error_advisory(error.Error);
						auto                                             output = self->decide(core::pull_request_load_failed_event{ error.Request.Context, core::github_failure::network });
						std::optional<github_cached_overview_projection> projection;
						if (admitted) {
							self->CachedOverview = *cached;
							self->CachedRequest  = error.Request.Context;
							projection           = github_cached_overview_projection{
								.Overview        = { .Title = cached->Title, .Body = cached->Body, .AuthorLogin = cached->AuthorLogin, .BaseRef = cached->BaseRef, .HeadRef = cached->HeadRef, .ReportedFiles = cached->Files, .ReportedCommits = cached->Commits, .State = static_cast<github_pull_request_state_projection>(cached->State), .Draft = cached->Draft },
								.BaseOid         = cached->BaseOid,
								.HeadOid         = cached->HeadOid,
								.FileCatalogue   = std::move(cached->FileCatalogue),
								.ReviewThreads   = std::move(cached->ReviewThreads),
								.GeneralComments = std::move(cached->GeneralComments),
								.ReviewSummaries = std::move(cached->ReviewSummaries)
							};
						}
						self->publish_transition(output, nullptr, nullptr, false, std::move(projection));
					});
				});
				record_pull_request(diagnostic_outcome::failed, aError.Request.Context.Request,
									core::github_failure::network, std::nullopt, aError.LastReadOperation);
				return true;
			} catch (...) {
				--Outstanding;
				return false;
			}
		}
		void handle_pull_request_now(const std::uint64_t            aToken,
									 github_pull_request_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = (PendingProvider ? &*PendingProvider : nullptr);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider.reset();
				record_pull_request(diagnostic_outcome::failed, current.Context.Request,
									core::github_failure::malformed_response);
				dispatch_core(core::pull_request_load_failed_event{
					.Context = current.Context,
					.Failure = core::github_failure::malformed_response,
				});
				return;
			}

			if (auto* success = std::get_if<github_pull_request_success>(&aCompletion)) {
				PendingProvider.reset();
				const github_actor_record expectedViewer{
					.Actor = success->Request.Viewer.Node,
					.Login = success->Request.Viewer.Login,
				};
				// The mode is the host's own fact about the request it issued,
				// so it is stamped here rather than read back from the
				// provider. A provider cannot promote a read into a mode it
				// was not asked for, and a snapshot cannot arrive claiming
				// one.
				auto observed         = success->Snapshot;
				observed.Mode         = success->Request.Mode;
				const auto validation = validate(observed, success->Request.Context);
				if (observed.Viewer != expectedViewer
					|| observed.CapabilityEvidence
						!= projected_capability_evidence(
							success->Request.CapabilityEvidence)
					|| !AcceptedProfileAuthority.has_value()
					|| !valid_snapshot_rate_limits(*AcceptedProfileAuthority,
												   observed)
					|| !validation.valid()) {
					record_pull_request(diagnostic_outcome::failed,
										success->Request.Context.Request,
										validation.valid() ? std::optional{ core::github_failure::malformed_response } : std::nullopt,
										validation.valid() ? std::nullopt : std::optional{ validation.Error },
										success->LastReadOperation, validation.Detail);
					dispatch_core(core::pull_request_load_failed_event{
						.Context = success->Request.Context,
						.Failure = core::github_failure::malformed_response,
					});
					return;
				}
				const core::accepted_snapshot_delta expected{
					.Context     = success->Request.Context,
					.Repository  = observed.Repository.Repository,
					.PullRequest = observed.PullRequest.PullRequest,
					.Snapshot    = observed.Snapshot,
				};
				StagedSnapshot = std::move(observed);
				auto output    = decide(
					core::pull_request_loaded_event{
						.Context     = expected.Context,
						.Repository  = expected.Repository,
						.PullRequest = expected.PullRequest,
						.Snapshot    = expected.Snapshot,
					});
				if (output.AcceptedSnapshot.has_value()
					&& *output.AcceptedSnapshot == expected) {
					record_pull_request(diagnostic_outcome::succeeded,
										success->Request.Context.Request, std::nullopt, std::nullopt,
										success->LastReadOperation);
					Admission->succeeded(success->Request.ProfileAuthority, success->Request.Viewer,
										 success->Request.CapabilityEvidence.ReadShape.ReviewThreads || success->Request.CapabilityEvidence.ReadShape.ViewedFileState);
					install_snapshot(std::move(*StagedSnapshot));
					retain_overview(success->Request.Context);
					StagedSnapshot.reset();
					publish_transition(
						output,
						nullptr,
						&*AcceptedSnapshot);
					if (AcceptedSnapshot && !Closing && State.AcceptedSnapshot == expected.Snapshot
						&& State.PullRequestGeneration == expected.Context.Generation) {
						deliver_epoch_handoff(*AcceptedSnapshot);
					}
				} else {
					record_pull_request(diagnostic_outcome::failed,
										success->Request.Context.Request,
										core::github_failure::malformed_response, std::nullopt,
										success->LastReadOperation);
					StagedSnapshot.reset();
					publish_and_execute(output);
				}
				return;
			}

			if (auto* error = std::get_if<github_pull_request_error>(&aCompletion)) {
				const auto usesGraphql = error->Request.CapabilityEvidence.ReadShape.ReviewThreads
					|| error->Request.CapabilityEvidence.ReadShape.ViewedFileState;
				const auto errorValid = valid_provider_error_resources(
					error->Error, true, true, usesGraphql);
				if (errorValid) {
					observe_provider_error(error->Error);
					Admission->observe(error->Request.ProfileAuthority, error->Request.Viewer, error->Error);
					if (error->Error.Failure == github_provider_failure::rate_limit) {
						if (auto remaining = Admission->refusal(error->Request.ProfileAuthority, error->Request.Viewer, usesGraphql)) {
							error->Error.RetryAfterSeconds = remaining->RetryAfterSeconds;
						}
					}
				}
				if (error->Request.Mode
						== core::github_access_mode::public_preview
					&& error->Error.Failure == github_provider_failure::rate_limit) {
					// The signed-out budget is the mode's own limit, not a
					// network fault and not a missing credential. It is
					// reported once, with the limit and the reset the server
					// named, and it offers sign-in. Nothing retries past it.
					PendingProvider.reset();
					const auto budget    = error->Error.RateLimitCause;
					PublicPreviewRefusal = github_public_preview_refusal{
						.Cause  = github_public_preview_refusal_cause::policy_refused,
						.Policy = core::refuse_rate_limited(
							core::anonymous_rate_limit_fact{
								.Limit = budget.has_value()
									? static_cast<std::uint32_t>(budget->Limit)
									: 0U,
								.Reset = budget.has_value()
									? budget->ResetUnixSeconds
									: 0U,
							}),
					};
					if (errorValid) {
						publish_error_advisory(error->Error);
					}
					record_pull_request(diagnostic_outcome::failed,
										error->Request.Context.Request, core::github_failure::rate_limit,
										std::nullopt, error->LastReadOperation);
					dispatch_core(core::pull_request_load_failed_event{
						.Context = error->Request.Context,
						.Failure = core::github_failure::rate_limit,
					});
					return;
				}
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					submit_pull_request(retry);
					return;
				}
				if (errorValid && load_cached_overview(*error)) {
					return;
				}
				PendingProvider.reset();
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				record_pull_request(diagnostic_outcome::failed,
									error->Request.Context.Request,
									errorValid ? mapped_failure(error->Error.Failure)
											   : core::github_failure::malformed_response,
									errorValid ? error->ValidationError : std::nullopt,
									error->LastReadOperation, error->RelationshipDetail,
									errorValid ? &error->Error : nullptr);
				dispatch_core(core::pull_request_load_failed_event{
					.Context = error->Request.Context,
					.Failure = errorValid
						? mapped_failure(error->Error.Failure)
						: core::github_failure::malformed_response,
				});
				return;
			}

			const auto& cancelled = std::get<github_pull_request_cancelled>(aCompletion);
			PendingProvider.reset();
			record_pull_request(diagnostic_outcome::cancelled,
								cancelled.Request.Context.Request, std::nullopt, std::nullopt,
								cancelled.LastReadOperation);
			dispatch_core(core::pull_request_load_cancelled_event{
				.Context = cancelled.Request.Context,
			});
		}

		void forget_cached_source()
		{
			CachedSourceStop.request_stop();
			CachedOverview.reset();
			CachedRequest.reset();
		}
		void publish_cached_source(cached_review_source_projection aSource)
		{
			if (!Publish || Closing || SuppressPublication) {
				return;
			}
			try {
				Publish({ Policy.window(), { }, { }, { }, github_presentation_update{ .CachedSource = std::move(aSource) } });
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}
		auto source_admitted(std::string_view aPath) const -> bool
		{
			return !Closing && Session && AcceptedProfileAuthority && CachedRequest && CachedOverview
				&& core::admit_cached_source(State, *CachedRequest, overview_key(*CachedRequest), *CachedOverview, aPath);
		}
		void read_cached_source(std::string aPath)
		{
			if (!source_admitted(aPath)) {
				return;
			}
			CachedSourceStop.request_stop();
			CachedSourceStop        = std::stop_source{ };
			const auto  stop        = CachedSourceStop.get_token();
			const auto& overview    = *CachedOverview;
			const auto  unavailable = cached_review_source_projection{ .Path = aPath, .BaseOid = overview.BaseOid, .HeadOid = overview.HeadOid };
			if (!Sources) {
				publish_cached_source(unavailable);
				return;
			}
			const auto base = core::parse_git_commit_oid(overview.BaseOid), head = core::parse_git_commit_oid(overview.HeadOid);
			if (!base || !head || base->algorithm() != head->algorithm()) {
				publish_cached_source(unavailable);
				return;
			}
			const auto  file    = std::ranges::find(*overview.FileCatalogue, aPath, &core::cached_review_file::Path);
			const auto& profile = *AcceptedProfileAuthority;
			auto        request = cached_review_source_request{
				.Catalog = { .ApplicationSession = State.ApplicationSession,
							 .Authority          = { .Profile = profile.Profile, .ProfileRevision = core::github_profile_revision{ profile.ProfileRevision.value() }, .Host = profile.Host, .HostAuthority = host_authority_fingerprint(profile), .Account = overview.Key.Account, .Viewer = core::github_viewer_actor_id{ overview.Key.Account.value() }, .Repository = overview.Repository, .PullRequest = overview.PullRequest, .Number = overview.Key.Number, .ObjectFormat = base->algorithm() },
							 .ExistingOnly       = true },
				.Base    = *base,
				.Head    = *head,
				.File    = *file
			};
			auto loading   = unavailable;
			loading.Status = cached_source_status::loading;
			publish_cached_source(std::move(loading));
			if (stop.stop_requested() || !source_admitted(aPath)) {
				return;
			}
			++Outstanding;
			auto self     = shared_from_this();
			auto admitted = false;
			try {
				admitted = Sources->read_cached_source(std::move(request), stop, [self, stop, unavailable](cached_review_source_projection aSource) {
					--self->Outstanding;
					if (stop.stop_requested() || !self->source_admitted(unavailable.Path)) {
						return;
					}
					const auto textValid = [](const auto& aText) { return !aText || (aText->size() <= maximum_cached_source_bytes && aText->find('\0') == std::string::npos); };
					if (aSource.Path != unavailable.Path || aSource.BaseOid != unavailable.BaseOid || aSource.HeadOid != unavailable.HeadOid
						|| !textValid(aSource.BaseText) || !textValid(aSource.HeadText)
						|| aSource.Status == cached_source_status::loading || aSource.Status > cached_source_status::too_large) {
						aSource = unavailable;
					}
					self->publish_cached_source(std::move(aSource));
				});
			} catch (...) { }
			if (!admitted) {
				--Outstanding;
				if (!stop.stop_requested() && source_admitted(aPath)) {
					publish_cached_source(unavailable);
				}
			}
		}
		void cancel()
		{
			forget_cached_source();
			CacheReadStop.request_stop();
			if (!PendingProvider) {
				return;
			}
			const auto request = *PendingProvider;
			PendingProvider.reset();
			++ProviderOperationToken;
			record_pull_request(diagnostic_outcome::cancelled,
								request.Context.Request);
			try {
				pull_request_port_for(request).cancel_pull_request(request);
			} catch (...) { }
			dispatch_core(core::pull_request_load_cancelled_event{ request.Context });
		}
		void release()
		{
			cancel();
			invalidate_epoch_authority();
			revoke_review_mutation_transport();
			AcceptedSnapshot.reset();
			StagedSnapshot.reset();
			AccountTransport.reset();
			AcceptedSession.reset();
			AcceptedProfileAuthority.reset();
			AcceptedCapabilityEvidence.reset();
			Session.reset();
		}
		auto markdown_identity() const -> std::optional<core::markdown_context>
		{
			if (Closing || !AcceptedSnapshot || !Session || MarkdownRevision == 0 || MarkdownExhausted) {
				return std::nullopt;
			}
			return core::markdown_context{ Policy.window(), DiagnosticWindowSession, State.IdentityGeneration, AcceptedSnapshot->Snapshot, MarkdownRevision };
		}
		auto open_markdown_link(const core::markdown_link_intent& aIntent, device_authorization_browser_port* aBrowser) -> core::markdown_link_result
		{
			const auto  current   = markdown_identity();
			const auto& snapshot  = AcceptedSnapshot;
			const auto  head      = snapshot ? snapshot->PullRequest.HeadOid.value() : std::string_view{ };
			const auto  checkLink = snapshot && aIntent.Body.starts_with("check:")
				&& std::ranges::any_of(snapshot->Checks, [&](const github_check_record& aCheck) {
									   return aIntent.Body == "check:" + std::string{ aCheck.Check.value() } && aIntent.Destination == aCheck.DetailsUrl;
								   });
			const auto decision = core::admit_markdown_link(aIntent, {
																		 .Context          = current,
																		 .BodyAvailable    = current && (aIntent.Body.starts_with("check:") ? checkLink : (aIntent.Body.empty() || MarkdownBodies.contains(aIntent.Body))),
																		 .BrowserAvailable = aBrowser != nullptr,
																		 .Hostname         = AcceptedProfileAuthority ? std::string_view{ AcceptedProfileAuthority->Hostname.value() } : std::string_view{ },
																		 .Owner            = snapshot ? std::string_view{ snapshot->Repository.Owner } : std::string_view{ },
																		 .Repository       = snapshot ? std::string_view{ snapshot->Repository.Name } : std::string_view{ },
																		 .Revision         = head,
																		 .Number           = snapshot ? snapshot->PullRequest.Number : core::pull_request_number{ },
																	 });
			if (!decision) {
				return decision.error();
			}
			if (aBrowser == nullptr) {
				return core::markdown_link_result::unavailable;
			}
			try {
				auto address = decision->Target;
				if (decision->Kind != core::markdown_destination_kind::absolute_web) {
					const auto root = "https://" + std::string{ AcceptedProfileAuthority->Hostname.value() } + "/" + snapshot->Repository.Owner + "/" + snapshot->Repository.Name;
					address         = decision->Kind == core::markdown_destination_kind::repository_path
						? root + "/blob/" + std::string{ head } + "/" + decision->Target
						: root + "/pull/" + std::to_string(snapshot->PullRequest.Number.value()) + decision->Target;
				}
				return aBrowser->open(address) ? core::markdown_link_result::opened : core::markdown_link_result::failed;
			} catch (...) {
				return core::markdown_link_result::failed;
			}
		}
		/// [[BPR-1338]]. The open pull request's own page, for a gesture made
		/// against this window's current publication only.
		auto open_pull_request_page(const core::markdown_context& aContext, device_authorization_browser_port* aBrowser) -> core::markdown_link_result
		{
			const auto current = markdown_identity();
			if (!current || aContext != *current || aContext.Revision == 0 || !AcceptedProfileAuthority) {
				return core::markdown_link_result::stale;
			}
			const auto& pull = *AcceptedSnapshot;
			const auto  page = core::admit_pull_request_page({
				.BrowserAvailable = aBrowser != nullptr,
				.Hostname         = AcceptedProfileAuthority->Hostname.value(),
				.Owner            = pull.Repository.Owner,
				.Repository       = pull.Repository.Name,
				.Number           = pull.PullRequest.Number,
			});
			if (!page) {
				return page.error();
			}
			try {
				return aBrowser->open(pull_request_page_address(*page)) ? core::markdown_link_result::opened : core::markdown_link_result::failed;
			} catch (...) {
				return core::markdown_link_result::failed;
			}
		}
		std::uint64_t                                       MarkdownRevision  = 0;
		bool                                                MarkdownExhausted = false;
		std::unordered_set<std::string>                     MarkdownBodies;
		bool                                                SuppressPublication = false;
		core::github_window_read                            Policy;
		const core::github_state&                           State;
		core::application_session_id                        DiagnosticWindowSession;
		github_pull_request_port&                           PullRequests;
		decision_executor&                                  DecisionLane;
		publication                                         Publish;
		github_epoch_handoff_port*                          Epochs;
		github_pull_request_port*                           AnonymousPullRequests;
		git_smart_transport_factory_port*                   AnonymousSmartGit;
		diagnostic_log_port*                                DiagnosticLog;
		cached_review_overview_port*                        Overviews;
		effect_executor*                                    CacheEffects;
		cached_review_source_port*                          Sources;
		std::shared_ptr<github_read_admission>              Admission;
		std::optional<core::cached_review_overview>         CachedOverview;
		std::optional<core::pull_request_request_context>   CachedRequest;
		std::stop_source                                    CachedSourceStop;
		std::stop_source                                    CacheReadStop;
		bool                                                CacheWritePending = false;
		std::optional<github_profile_session>               Session;
		std::optional<github_session_read_model>            AcceptedSession;
		std::optional<github_profile_authority>             AcceptedProfileAuthority;
		std::optional<github_capability_cache_entry>        AcceptedCapabilityEvidence;
		github_account_transport                            AccountTransport;
		std::optional<github_pull_request_snapshot>         AcceptedSnapshot, StagedSnapshot;
		std::optional<github_pull_request_provider_request> PendingProvider;
		std::uint64_t                                       ProviderOperationToken = 0;
		std::atomic_size_t                                  Outstanding{ 0 };
		std::uint32_t                                       ProgressSequence = 0U;
		review_mutation_transport                           ReviewMutationTransport;
		github_rate_limit_state                             RateLimits;
		std::optional<github_provider_error>                LastError;
		std::optional<github_public_preview_refusal>        PublicPreviewRefusal;
		std::optional<pull_request_authority_invalidation>  ActiveEpochAuthority;
		std::shared_ptr<smart_authority_state>              ActiveEpochSmartAuthority;
		bool                                                Closing = false, EpochHandoffDeliveryFailed = false, EpochHandoffUnavailable = false;
		std::atomic_bool                                    ProjectionDeliveryFailed{ false };
	};

	namespace {
		auto core_session(const github_profile_session& aSession) -> core::github_window_session
		{
			return { .Profile  = aSession.BprProfile,
					 .Identity = { .Context            = { .Request = core::request_id{ 1 }, .ApplicationSession = aSession.ApplicationSession, .Profile = aSession.ProfileAuthority.Profile, .IdentityGeneration = aSession.Generation },
								   .Host               = aSession.Session.Host,
								   .Account            = aSession.Session.Viewer.Account,
								   .Capabilities       = aSession.CapabilityEvidence.Capabilities,
								   .CapabilityRevision = aSession.CapabilityEvidence.Revision },
					 .Mode     = aSession.Session.Mode };
		}
	}
	github_window_read_host::github_window_read_host(core::bpr_window_context     aWindow,
													 core::application_session_id aSession, github_pull_request_port& aReads,
													 decision_executor& aDecision, publication aPublication, github_epoch_handoff_port* aEpochs,
													 github_pull_request_port* aAnonymousReads, git_smart_transport_factory_port* aAnonymousGit, diagnostic_log_port* aLog, cached_review_overview_port* aOverviews, effect_executor* aCacheEffects, cached_review_source_port* aSources, std::shared_ptr<github_read_admission> aAdmission)
		: Implementation{ std::make_shared<implementation>(aWindow, aSession, aReads, aDecision, std::move(aPublication), aEpochs, aAnonymousReads, aAnonymousGit, aLog, aOverviews, aCacheEffects, aSources, std::move(aAdmission)) }
	{ }
	github_window_read_host::~github_window_read_host()
	{
		Implementation->Publish = { };
		shutdown();
	}
	auto github_window_read_host::adopt(const github_profile_session& aSession) -> bool
	{
		auto& host = *Implementation;
		if (host.Closing || (host.Session && *host.Session != aSession)) {
			return false;
		}
		const auto coreSession = core_session(aSession);
		if (aSession.CapabilityEvidence.Authority != github_capability_authority_key{ aSession.ProfileAuthority.Profile, aSession.ProfileAuthority.ProfileRevision, aSession.ProfileAuthority.Host, aSession.Session.Viewer.Account, aSession.ProfileAuthority.ServerContract, aSession.ProfileAuthority.ProbeContractRevision }
			|| aSession.ProfileAuthority.Profile != aSession.Session.Profile
			|| aSession.ProfileAuthority.Host != aSession.Session.Host
			|| aSession.CapabilityEvidence.Authority.Account != aSession.Session.Viewer.Account
			|| projected_capability_evidence(aSession.CapabilityEvidence) != aSession.Session.CapabilityEvidence
			|| !validate(aSession.Session, coreSession.Identity.Context).valid()
			|| (aSession.Session.Mode == core::github_access_mode::public_preview
					? aSession.AccountTransport != nullptr
					: aSession.AccountTransport == nullptr)
			|| !host.Policy.adopt(coreSession)) {
			return false;
		}
		host.Session                    = aSession;
		host.AcceptedSession            = aSession.Session;
		host.AcceptedProfileAuthority   = aSession.ProfileAuthority;
		host.AcceptedCapabilityEvidence = aSession.CapabilityEvidence;
		host.AccountTransport           = aSession.AccountTransport;
		host.RateLimits                 = aSession.Session.RateLimits;
		return true;
	}
	auto github_window_read_host::adopted_presentation() const -> std::optional<github_presentation_update>
	{
		const auto& host     = *Implementation;
		const auto& adoption = host.Policy.adoption();
		if (host.Closing || !adoption || !host.Session || host.Session->Session.Mode != core::github_access_mode::authenticated) {
			return std::nullopt;
		}
		return github_presentation_update{ .Transition = project_github_transition(*adoption), .Session = project_github_session(host.Session->Session) };
	}
	auto github_window_read_host::retire(const github_profile_session& aSession) -> bool
	{
		auto& host = *Implementation;
		if (!host.Session || *host.Session != aSession) {
			return false;
		}
		const auto retired    = host.Policy.retire(core_session(aSession));
		const auto suppressed = std::exchange(host.SuppressPublication, true);
		host.release();
		host.SuppressPublication = suppressed;
		return retired;
	}
	void github_window_read_host::advance_request_floor(const core::request_id aNext) noexcept { Implementation->Policy.advance_request_floor(aNext); }
	void github_window_read_host::dispatch(const core::github_window_read_event& aEvent)
	{
		if (!Implementation->Closing
			&& (std::holds_alternative<core::open_pull_request_intent>(aEvent)
				|| std::holds_alternative<core::refresh_pull_request_intent>(aEvent))) {
			Implementation->dispatch_core(aEvent);
		}
	}
	void github_window_read_host::read_cached_source(std::string aPath) { Implementation->read_cached_source(std::move(aPath)); }
	void github_window_read_host::cancel() { Implementation->cancel(); }
	void github_window_read_host::share_snapshot(
		const std::span<const std::shared_ptr<github_window_read_host>> aWindows,
		const std::uint64_t                                             aSequence)
	{
		auto& source = *Implementation;
		if (source.Closing || !source.Session || !source.AcceptedSnapshot
			|| aWindows.size() > core::maximum_bpr_windows_per_profile
			|| !source.Policy.share_snapshot(source.Policy, aSequence)) {
			return;
		}
		// Publication can close/navigate/reauthenticate any window, or admit a
		// newer observation. Keep the payload and authority immutable across
		// callbacks; each recipient rechecks lifetime, session and sequence.
		const auto policy   = source.Policy;
		const auto session  = *source.Session;
		const auto snapshot = *source.AcceptedSnapshot;
		for (const auto& window : aWindows) {
			if (!window || window.get() == this) {
				continue;
			}
			auto& target = *window->Implementation;
			if (target.Closing || !target.Session || *target.Session != session
				|| !target.Policy.share_snapshot(policy, aSequence)) {
				continue;
			}
			target.install_snapshot(snapshot);
			// No AcceptedSnapshot delta: this is not a provider completion and
			// must not initiate another fanout or a restoration/epoch handoff.
			target.publish_transition({ .Changed = core::github_changed_domain::pull_request }, nullptr, &*target.AcceptedSnapshot, true);
		}
	}
	void github_window_read_host::shutdown()
	{
		Implementation->Closing = true;
		Implementation->release();
	}
	auto github_window_read_host::state() const noexcept -> const core::github_state& { return Implementation->State; }
	auto github_window_read_host::window() const noexcept -> core::bpr_window_context { return Implementation->Policy.window(); }
	auto github_window_read_host::open_markdown_link(const core::markdown_link_intent& aIntent, device_authorization_browser_port* aBrowser) -> core::markdown_link_result
	{
		return Implementation->open_markdown_link(aIntent, aBrowser);
	}
	auto pull_request_page_address(const core::pull_request_page& aPage) -> std::string
	{
		return "https://" + aPage.Hostname + "/" + aPage.Owner + "/" + aPage.Repository + "/pull/" + std::to_string(aPage.Number.value());
	}
	auto github_window_read_host::open_pull_request_page(const core::markdown_context& aContext, device_authorization_browser_port* aBrowser) -> core::markdown_link_result
	{
		return Implementation->open_pull_request_page(aContext, aBrowser);
	}
	auto github_window_read_host::accepted_snapshot() const noexcept -> const std::optional<github_pull_request_snapshot>& { return Implementation->AcceptedSnapshot; }
	auto github_window_read_host::mutation_transport() const noexcept -> review_mutation_transport { return Implementation->ReviewMutationTransport; }
	auto github_window_read_host::rate_limits() const noexcept -> const github_rate_limit_state& { return Implementation->RateLimits; }
	auto github_window_read_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding == 0; }
	auto github_window_read_host::projection_delivery_failed() const noexcept -> bool { return Implementation->ProjectionDeliveryFailed; }
	auto github_window_read_host::epoch_handoff_delivery_failed() const noexcept -> bool { return Implementation->EpochHandoffDeliveryFailed; }
	auto github_window_read_host::epoch_handoff_unavailable() const noexcept -> bool { return Implementation->EpochHandoffUnavailable; }
}
