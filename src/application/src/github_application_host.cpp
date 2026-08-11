#include "bpr/application/github_application_host.hpp"

#include "github_projection_mapping.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
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

			~bound_git_smart_authority() override { revoke(); }

			[[nodiscard]] auto binding() const noexcept
				-> const git_smart_authority_binding& override
			{
				return Binding;
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
				auto* const factory = dynamic_cast<git_smart_transport_factory_port*>(Account.get());
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

		[[nodiscard]] auto capability_key(const github_profile_authority& aProfile,
										  const core::github_account_id   aAccount)
			-> github_capability_authority_key
		{
			return {
				.Profile               = aProfile.Profile,
				.ProfileRevision       = aProfile.ProfileRevision,
				.Host                  = aProfile.Host,
				.Account               = aAccount,
				.ServerContract        = aProfile.ServerContract,
				.ProbeContractRevision = aProfile.ProbeContractRevision,
			};
		}

		[[nodiscard]] auto credential_key(const github_profile_authority& aProfile,
										  const core::github_account_id   aAccount)
			-> github_credential_authority_key
		{
			return {
				.Profile         = aProfile.Profile,
				.ProfileRevision = aProfile.ProfileRevision,
				.Host            = aProfile.Host,
				.Hostname        = aProfile.Hostname,
				.Account         = aAccount,
				.ServerContract  = aProfile.ServerContract,
			};
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

		[[nodiscard]] auto
		valid_viewer(const github_profile_authority&             aAuthority,
					 const core::github_session_request_context& aContext,
					 const github_viewer_record&                 aViewer,
					 const github_rate_limit_state&              aRateLimits) noexcept -> bool
		{
			if (!valid_rate_limit_resources(aRateLimits, true, false, false)
				|| is_secondary_limited(aRateLimits)
				|| (requires_rate_limit_observations(aAuthority.ServerContract)
					&& !aRateLimits.Core.has_value())) {
				return false;
			}
			const github_session_read_model model{
				.Profile            = aAuthority.Profile,
				.Host               = aAuthority.Host,
				.Viewer             = aViewer,
				.CapabilityEvidence = {
					.Capabilities    = { },
					.Revision        = core::capability_revision{ 1U },
					.ReadShape       = { },
					.ContractVersion = 1U,
				},
				.RateLimits = aRateLimits,
			};
			return validate(model, aContext).valid();
		}

		[[nodiscard]] constexpr auto valid_capability_rate_limits(
			const github_profile_authority& aAuthority,
			const github_rate_limit_state&  aRateLimits) noexcept -> bool
		{
			return valid_rate_limit_resources(aRateLimits, false, false, true)
				&& !is_secondary_limited(aRateLimits)
				&& (!requires_rate_limit_observations(aAuthority.ServerContract)
					|| aRateLimits.Graphql.has_value());
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

		template <typename Completion>
		[[nodiscard]] auto completion_context(const Completion& aCompletion)
		{
			return std::visit([](const auto& aTerminal) { return aTerminal.Context; },
							  aCompletion);
		}

		template <typename Completion>
		[[nodiscard]] decltype(auto)
		provider_completion_request(const Completion& aCompletion)
		{
			return std::visit([](const auto& aTerminal) -> const auto& { return aTerminal.Request; },
							  aCompletion);
		}

		[[nodiscard]] auto create_host_capability_cache() -> github_capability_cache
		{
			auto cache = github_capability_cache::create({ });
			if (!cache.has_value()) {
				throw std::bad_alloc{ };
			}
			return std::move(*cache);
		}

	}

	github_credential_binding_catalog::~github_credential_binding_catalog() = default;

	auto github_credential_binding_catalog::find(
		const core::host_profile_id aProfile) const noexcept
		-> const github_credential_authority_key*
	{
		if (aProfile.value() == 0U) {
			return nullptr;
		}
		for (const auto& entry : Entries) {
			if (entry.has_value() && entry->Profile == aProfile) {
				return &*entry;
			}
		}
		return nullptr;
	}

	auto github_credential_binding_catalog::remember(
		const github_credential_authority_key aAuthority) noexcept -> bool
	{
		if (!valid(aAuthority)) {
			return false;
		}
		for (const auto& entry : Entries) {
			if (entry.has_value() && entry->Profile == aAuthority.Profile) {
				return *entry == aAuthority;
			}
		}
		for (auto& entry : Entries) {
			if (!entry.has_value()) {
				entry = aAuthority;
				return true;
			}
		}
		return false;
	}

	void github_credential_binding_catalog::forget(
		const github_credential_authority_key& aAuthority) noexcept
	{
		if (!valid(aAuthority)) {
			return;
		}
		for (auto& entry : Entries) {
			if (entry == aAuthority) {
				entry.reset();
				return;
			}
		}
	}

	auto github_credential_binding_catalog::size() const noexcept -> std::size_t
	{
		std::size_t result = 0U;
		for (const auto& entry : Entries) {
			if (entry.has_value()) {
				++result;
			}
		}
		return result;
	}

	struct github_application_host::implementation final
	{
		enum class viewer_mode : std::uint8_t {
			none,
			cli,
			bootstrap,
			account,
		};

		enum class vault_operation : std::uint8_t {
			none,
			store,
			retrieve,
			delete_credential,
		};

		enum class credential_lifecycle : std::uint8_t {
			none,
			provisional,
			persistent,
		};

		struct deferred_session_terminal
		{
			core::github_session_request_context Context;
			std::optional<core::github_failure>  Failure;
		};

		struct session_candidate
		{
			core::github_session_request_context         Context;
			github_profile_authority                     ProfileAuthority;
			github_viewer_record                         Viewer;
			github_capability_cache_entry                CapabilityEvidence;
			github_account_transport                     AccountTransport;
			github_rate_limit_state                      RateLimits;
			std::optional<github_capability_cache_stage> CacheStage;
		};

		struct deferred_dispatch
		{
			core::github_event Event;
		};

		struct deferred_enrollment
		{
			core::host_profile_id      Profile;
			github_credential_material Credential;
		};

		struct deferred_resume
		{
			core::host_profile_id Profile;
		};

		struct deferred_shutdown
		{
		};

		using deferred_ingress = std::variant<deferred_dispatch,
											  deferred_enrollment,
											  deferred_resume,
											  deferred_shutdown>;

		using pending_provider_request = std::variant<std::monostate,
													  github_profile_request,
													  github_viewer_request,
													  github_capability_request,
													  github_pull_request_provider_request>;

		struct completion_gate
		{
			std::recursive_mutex         Barrier;
			std::atomic<implementation*> Host = nullptr;
		};

		implementation(core::github_state                 aInitialState,
					   github_profile_port&               aProfiles,
					   github_viewer_port&                aViewers,
					   github_capability_port&            aCapabilities,
					   github_pull_request_port&          aPullRequests,
					   github_credential_vault_port&      aCredentials,
					   github_credential_binding_catalog& aCredentialBindings,
					   decision_executor&                 aDecisionLane,
					   github_projection_port&            aProjections,
					   const github_credential_shutdown_policy
														aCredentialShutdownPolicy,
					   github_epoch_handoff_port* const aEpochs)
			: State{ std::move(aInitialState) }
			, Profiles{ aProfiles }
			, Viewers{ aViewers }
			, Capabilities{ aCapabilities }
			, PullRequests{ aPullRequests }
			, Credentials{ aCredentials }
			, CredentialBindings{ aCredentialBindings }
			, CapabilityCache{ create_host_capability_cache() }
			, DecisionLane{ aDecisionLane }
			, Projections{ aProjections }
			, CredentialShutdownPolicy{ aCredentialShutdownPolicy }
			, Epochs{ aEpochs }
			, CompletionGate{ std::make_shared<completion_gate>() }
		{
			CompletionGate->Host.store(this, std::memory_order_release);
		}

		~implementation()
		{
			const std::lock_guard barrier{ CompletionGate->Barrier };
			CompletionGate->Host.store(nullptr, std::memory_order_release);
		}

		template <typename Action>
		void run_turn(Action&& aAction)
		{
			TurnActive = true;
			try {
				std::forward<Action>(aAction)();
				const auto deferredCount = DeferredIngress.size();
				for (std::size_t index = 0U; index < deferredCount; ++index) {
					auto deferred = std::move(DeferredIngress.front());
					DeferredIngress.pop_front();
					std::visit(
						[this](auto&& aIngress) {
							using ingress_type = std::decay_t<decltype(aIngress)>;
							if constexpr (std::is_same_v<ingress_type,
														 deferred_dispatch>) {
								dispatch_now(aIngress.Event);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_enrollment>) {
								enroll_profile_now(aIngress.Profile,
												   std::move(aIngress.Credential));
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_resume>) {
								resume_profile_now(aIngress.Profile);
							} else {
								shutdown_now();
							}
						},
						std::move(deferred));
				}
				TurnActive = false;
				schedule_deferred_ingress();
			} catch (...) {
				DeferredIngress.clear();
				TurnActive = false;
				throw;
			}
		}

		void schedule_deferred_ingress() noexcept
		{
			if (DeferredIngress.empty() || DeferredIngressScheduled) {
				return;
			}
			DeferredIngressScheduled = true;
			const auto gate          = std::weak_ptr<completion_gate>{ CompletionGate };
			try {
				DecisionLane.post(
					[gate] {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard completionBarrier{ locked->Barrier };
						auto* const           host = locked->Host.load(std::memory_order_acquire);
						if (host == nullptr) {
							return;
						}
						host->DeferredIngressScheduled = false;
						host->run_turn([] { });
					});
			} catch (...) {
				DeferredIngressScheduled = false;
				DeferredIngress.clear();
				ProjectionDeliveryFailed = true;
			}
		}

		void dispatch(const core::github_event& aEvent)
		{
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_dispatch{ aEvent });
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_dispatch{ aEvent });
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, &aEvent] { dispatch_now(aEvent); });
		}

		void dispatch_now(const core::github_event& aEvent)
		{
			if (Closing) {
				return;
			}
			const auto intent = std::holds_alternative<core::activate_host_profile_intent>(aEvent)
				|| std::holds_alternative<core::deactivate_github_identity_intent>(
									aEvent)
				|| std::holds_alternative<core::open_pull_request_intent>(aEvent)
				|| std::holds_alternative<core::refresh_pull_request_intent>(aEvent);
			if (!intent) {
				return;
			}
			if (std::holds_alternative<core::activate_host_profile_intent>(aEvent)) {
				NextEnrollment.reset();
				NextResumeProfile.reset();
			}
			dispatch_core(aEvent);
		}

		void enroll_profile(const core::host_profile_id aProfile,
							github_credential_material  aCredential)
		{
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_enrollment{
					.Profile    = aProfile,
					.Credential = std::move(aCredential),
				});
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_enrollment{
					.Profile    = aProfile,
					.Credential = std::move(aCredential),
				});
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, aProfile, credential = std::move(aCredential)]() mutable { enroll_profile_now(aProfile, std::move(credential)); });
		}

		void enroll_profile_now(const core::host_profile_id aProfile,
								github_credential_material  aCredential)
		{
			if (Closing) {
				return;
			}
			NextEnrollment.reset();
			NextResumeProfile.reset();
			if (valid(aCredential)) {
				NextEnrollment.emplace(std::move(aCredential));
			}
			try {
				dispatch_core(
					core::activate_host_profile_intent{ .Profile = aProfile });
			} catch (...) {
				NextEnrollment.reset();
				throw;
			}
			// A valid activation synchronously moved this material into the
			// session scratch state. An invalid activation emitted no request, so
			// retaining it here would let a later activation consume the wrong
			// enrollment secret.
			NextEnrollment.reset();
		}

		void resume_profile(const core::host_profile_id aProfile)
		{
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_resume{
					.Profile = aProfile,
				});
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_resume{
					.Profile = aProfile,
				});
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, aProfile] { resume_profile_now(aProfile); });
		}

		void resume_profile_now(const core::host_profile_id aProfile)
		{
			if (Closing || aProfile.value() == 0U) {
				return;
			}
			NextEnrollment.reset();
			NextResumeProfile = aProfile;
			try {
				dispatch_core(core::activate_host_profile_intent{
					.Profile = aProfile,
				});
			} catch (...) {
				NextResumeProfile.reset();
				throw;
			}
			NextResumeProfile.reset();
		}

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

			const auto viewer        = core::github_viewer_actor_id{ aSnapshot.Account.value() };
			auto       currentReview = std::optional<core::current_viewer_review_candidate>{ };
			for (std::size_t index = 0U; index < aSnapshot.Reviews.size(); ++index) {
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
				.CurrentViewerReview   = std::move(currentReview),
			};

			if (dynamic_cast<git_smart_transport_factory_port*>(
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

		void dispatch_core(const core::github_event& aEvent)
		{
			auto output = core::transition(State, aEvent);
			for (const auto& request : output.Requests) {
				const auto* fetch = std::get_if<core::fetch_pull_request_request>(&request);
				if (fetch != nullptr
					&& !fetch->Context.ExpectedRepository.has_value()
					&& !fetch->Context.ExpectedPullRequest.has_value()) {
					invalidate_epoch_authority();
					StagedSnapshot.reset();
					revoke_review_mutation_transport();
					AcceptedSnapshot.reset();
				}
			}
			publish_and_execute(output);
		}

		void publish_and_execute(const core::github_transition_output& aOutput)
		{
			if (Closing) {
				return;
			}
			publish_transition(aOutput);
			for (const auto& request : aOutput.Requests) {
				execute(request);
			}
		}

		void publish_transition(
			const core::github_transition_output&     aOutput,
			const github_session_read_model* const    aSession     = nullptr,
			const github_pull_request_snapshot* const aPullRequest = nullptr) noexcept
		{
			try {
				std::optional<github_session_projection> sessionProjection;
				if (aSession != nullptr) {
					sessionProjection = project_github_session(*aSession);
				}
				std::optional<github_pull_request_projection>
					pullRequestProjection;
				if (aPullRequest != nullptr) {
					pullRequestProjection = project_github_pull_request(*aPullRequest);
				}
				const github_presentation_update update{
					.Transition      = project_github_transition(aOutput),
					.Session         = std::move(sessionProjection),
					.PullRequest     = std::move(pullRequestProjection),
					.HostDiagnostics = { },
				};
				Projections.publish(update);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void publish_diagnostic(const github_host_diagnostic& aDiagnostic) noexcept
		{
			try {
				const github_presentation_update update{
					.Transition      = std::nullopt,
					.Session         = std::nullopt,
					.PullRequest     = std::nullopt,
					.HostDiagnostics = { project_github_host_diagnostic(aDiagnostic) },
				};
				Projections.publish(update);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void observe_provider_error(const github_provider_error& aError)
		{
			if (valid(aError)) {
				static_cast<void>(merge(RateLimits, aError.RateLimits));
			}
		}

		void publish_error_advisory(const github_provider_error& aError)
		{
			if (valid(aError)
				&& (aError.RetryAfterSeconds.has_value()
					|| has_observations(aError.RateLimits))) {
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::reason_for(mapped_failure(aError.Failure)),
					.RetryAfterSeconds = aError.RetryAfterSeconds,
					.RateLimits        = RateLimits,
					.RateLimitCause    = aError.RateLimitCause,
				});
			}
		}

		[[nodiscard]] auto requires_shutdown_credential_cleanup() const noexcept
			-> bool
		{
			if (!CredentialAuthority.has_value()
				|| !CredentialSessionContext.has_value()) {
				return false;
			}
			return CredentialLifecycle == credential_lifecycle::provisional
				|| CredentialShutdownPolicy
				== github_credential_shutdown_policy::
					delete_retained_on_shutdown;
		}

		void execute(const core::github_host_request& aRequest)
		{
			std::visit(
				[this](const auto& aTypedRequest) {
					using request_type = std::decay_t<decltype(aTypedRequest)>;
					if constexpr (std::is_same_v<
									  request_type,
									  core::invalidate_github_identity_request>) {
						invalidate(aTypedRequest);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::establish_github_session_request>) {
						DeferredSession   = aTypedRequest.Context;
						SessionEnrollment = std::move(NextEnrollment);
						NextEnrollment.reset();
						SessionResumeProfile = NextResumeProfile;
						NextResumeProfile.reset();
						if (ActiveVaultOperation == vault_operation::none) {
							start_deferred_session();
						}
					} else {
						start_pull_request(aTypedRequest.Context);
					}
				},
				aRequest);
		}

		void invalidate(const core::invalidate_github_identity_request& aRequest)
		{
			invalidate_epoch_authority();
			cancel_provider();
			const auto* resumeBinding          = NextResumeProfile.has_value()
				? CredentialBindings.find(*NextResumeProfile)
				: nullptr;
			const auto  preserveForExactResume = CredentialLifecycle == credential_lifecycle::persistent
				&& CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value() && resumeBinding != nullptr
				&& *resumeBinding == *CredentialAuthority;
			const auto vaultOperationRunning = ActiveVaultOperation == vault_operation::store
				|| ActiveVaultOperation == vault_operation::retrieve;
			if (vaultOperationRunning) {
				if (ActiveVaultContext.has_value()) {
					try {
						Credentials.cancel(*ActiveVaultContext);
					} catch (...) {
					}
				}
				DeferredInvalidationGeneration          = aRequest.NewGeneration;
				DeferredInvalidationPreservesCredential = preserveForExactResume;
			} else {
				cancel_vault();
				DeferredInvalidationGeneration.reset();
				DeferredInvalidationPreservesCredential = false;
			}
			StagedSession.reset();
			StagedSnapshot.reset();
			revoke_review_mutation_transport();
			AcceptedSnapshot.reset();
			RateLimits = { };
			AccountTransport.reset();
			CurrentProfile.reset();
			BootstrapViewer.reset();
			SessionEnrollment.reset();
			SessionResumeProfile.reset();
			ViewerMode = viewer_mode::none;
			DeferredTerminal.reset();
			DeferredSession.reset();

			if (aRequest.Previous.Profile.has_value()
				&& aRequest.Previous.Host.has_value()
				&& aRequest.Previous.Account.has_value()) {
				static_cast<void>(
					CapabilityCache.invalidate_identity(*aRequest.Previous.Profile,
														*aRequest.Previous.Host,
														*aRequest.Previous.Account));
			} else if (aRequest.Previous.Profile.has_value()) {
				static_cast<void>(
					CapabilityCache.invalidate_profile(*aRequest.Previous.Profile));
			}

			AcceptedSession.reset();
			AcceptedProfileAuthority.reset();
			AcceptedCapabilityEvidence.reset();
			if (CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()) {
				if (!vaultOperationRunning && !preserveForExactResume) {
					start_delete(aRequest.NewGeneration, std::nullopt);
				}
			}
		}

		void select_persistent_credential(
			const github_credential_authority_key&      aAuthority,
			const core::github_session_request_context& aContext)
		{
			CredentialAuthority         = aAuthority;
			CredentialSessionContext    = aContext;
			CredentialLifecycle         = credential_lifecycle::persistent;
			CredentialBindingRemembered = true;
		}

		void start_deferred_session()
		{
			if (Closing || !DeferredSession.has_value()
				|| ActiveVaultOperation != vault_operation::none) {
				return;
			}

			const auto context = *DeferredSession;
			DeferredSession.reset();
			if (SessionResumeProfile.has_value()) {
				if (const auto* retained = CredentialBindings.find(*SessionResumeProfile);
					retained != nullptr) {
					select_persistent_credential(*retained, context);
				}
			}
			const github_profile_request request{
				.Context = context,
				.Attempt = github_provider_attempt{ 1U },
			};
			submit_profile(request);
		}

		void submit_profile(const github_profile_request& aRequest)
		{
			PendingProvider      = aRequest;
			const auto  token    = ++ProviderOperationToken;
			const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane     = &DecisionLane;
			const auto  callback = [gate, lane, token, aRequest](
									   github_profile_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_profile_completion>(
						std::move(aCompletion));
					lane->post(
						[gate, payload, token]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_profile(token, std::move(*payload));
							}
						});
				} catch (...) {
					try {
						lane->post(
							[gate, token, aRequest]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_profile(
										token,
										github_profile_error{
											.Request = aRequest,
											.Error   = {
												.Failure = github_provider_failure::
													response_limit,
												.RetryAfterSeconds = std::nullopt,
												.RateLimits        = { },
												.RateLimitCause    = std::nullopt,
											},
										});
								}
							});
					} catch (...) {
					}
				}
			};
			try {
				Profiles.resolve_profile(aRequest, callback);
			} catch (...) {
				callback(github_profile_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_profile(const std::uint64_t       aToken,
							github_profile_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_profile_now(aToken, std::move(completion)); });
		}

		void handle_profile_now(const std::uint64_t       aToken,
								github_profile_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_profile_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				fail_session(current.Context,
							 core::github_failure::malformed_response);
				return;
			}

			if (auto* success = std::get_if<github_profile_success>(&aCompletion)) {
				const auto context = success->Request.Context;
				PendingProvider    = std::monostate{ };
				if (!valid(success->Authority)
					|| success->Authority.Profile != context.Profile) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}
				CurrentProfile = success->Authority;

				if (success->Authority.Authentication
					== github_authentication_transport::github_cli) {
					const auto hasNativeMaterial = SessionEnrollment.has_value()
						|| SessionResumeProfile.has_value();
					if (const auto* retained = CredentialBindings.find(context.Profile);
						retained != nullptr) {
						select_persistent_credential(*retained, context);
						if (hasNativeMaterial) {
							SessionEnrollment.reset();
							SessionResumeProfile.reset();
							fail_session(context,
										 core::github_failure::malformed_response,
										 true);
						} else {
							DeferredSession = context;
							start_delete(context.IdentityGeneration, std::nullopt);
						}
						return;
					}
					if (hasNativeMaterial) {
						SessionEnrollment.reset();
						SessionResumeProfile.reset();
						fail_session(context,
									 core::github_failure::malformed_response);
						return;
					}
					ViewerMode = viewer_mode::cli;
					submit_viewer(
						github_viewer_request{
							.Context   = context,
							.Authority = success->Authority,
							.Attempt   = github_provider_attempt{ 1U },
						},
						std::monostate{ });
					return;
				}

				if (SessionEnrollment.has_value()
					&& SessionResumeProfile.has_value()) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}
				if (SessionEnrollment.has_value()) {
					if (const auto* retained = CredentialBindings.find(context.Profile);
						retained != nullptr) {
						select_persistent_credential(*retained, context);
						DeferredSession = context;
						start_delete(context.IdentityGeneration, std::nullopt);
						return;
					}
					ViewerMode = viewer_mode::bootstrap;
					submit_bootstrap_viewer(github_viewer_request{
						.Context   = context,
						.Authority = success->Authority,
						.Attempt   = github_provider_attempt{ 1U },
					});
					return;
				}
				if (!SessionResumeProfile.has_value()
					|| *SessionResumeProfile != context.Profile) {
					fail_session(context, core::github_failure::vault);
					return;
				}

				const auto* retained = CredentialBindings.find(context.Profile);
				if (retained == nullptr) {
					fail_session(context, core::github_failure::vault);
					return;
				}
				const auto resumeAuthority = *retained;
				select_persistent_credential(resumeAuthority, context);
				SessionResumeProfile.reset();
				if (resumeAuthority.Profile != success->Authority.Profile
					|| resumeAuthority.ProfileRevision
						!= success->Authority.ProfileRevision
					|| resumeAuthority.Host != success->Authority.Host
					|| resumeAuthority.Hostname != success->Authority.Hostname
					|| resumeAuthority.ServerContract
						!= success->Authority.ServerContract) {
					fail_session(
						context, core::github_failure::malformed_response, true);
					return;
				}
				ViewerMode = viewer_mode::account;
				start_retrieve(github_provider_attempt{ 1U });
				return;
			}

			if (auto* error = std::get_if<github_profile_error>(&aCompletion)) {
				const auto errorValid = valid(error->Error)
					&& !has_observations(error->Error.RateLimits);
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					submit_profile(retry);
					return;
				}
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				fail_session(error->Request.Context,
							 errorValid
								 ? mapped_failure(error->Error.Failure)
								 : core::github_failure::malformed_response);
				return;
			}

			const auto& cancelled = std::get<github_profile_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			cancel_session(cancelled.Request.Context);
		}

		void submit_bootstrap_viewer(const github_viewer_request& aRequest)
		{
			if (!SessionEnrollment.has_value()) {
				fail_session(aRequest.Context, core::github_failure::vault);
				return;
			}
			auto copy = github_credential_material::create(SessionEnrollment->bytes());
			if (!copy.has_value()) {
				fail_session(aRequest.Context, core::github_failure::vault);
				return;
			}
			submit_viewer(aRequest,
						  github_bootstrap_credential{
							  .Credential = std::move(*copy),
						  });
		}

		void submit_viewer(const github_viewer_request& aRequest,
						   github_viewer_authentication aAuthentication)
		{
			PendingProvider      = aRequest;
			const auto  token    = ++ProviderOperationToken;
			const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane     = &DecisionLane;
			const auto  callback = [gate, lane, token, aRequest](
									   github_viewer_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_viewer_completion>(
						std::move(aCompletion));
					lane->post(
						[gate, payload, token]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_viewer(token, std::move(*payload));
							}
						});
				} catch (...) {
					try {
						lane->post(
							[gate, token, aRequest]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_viewer(
										token,
										github_viewer_error{
											.Request = aRequest,
											.Error   = {
												.Failure = github_provider_failure::
													response_limit,
												.RetryAfterSeconds = std::nullopt,
												.RateLimits        = { },
												.RateLimitCause    = std::nullopt,
											},
										});
								}
							});
					} catch (...) {
					}
				}
			};
			try {
				Viewers.observe_viewer(
					github_viewer_submission{
						.Request        = aRequest,
						.Authentication = std::move(aAuthentication),
					},
					callback);
			} catch (...) {
				callback(github_viewer_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_viewer(const std::uint64_t      aToken,
						   github_viewer_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_viewer_now(aToken, std::move(completion)); });
		}

		void handle_viewer_now(const std::uint64_t      aToken,
							   github_viewer_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_viewer_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				fail_session(current.Context,
							 core::github_failure::malformed_response);
				return;
			}

			if (auto* success = std::get_if<github_viewer_success>(&aCompletion)) {
				const auto context   = success->Request.Context;
				const auto authority = success->Request.Authority;
				PendingProvider      = std::monostate{ };
				if (!valid_viewer(
						authority, context, success->Viewer, success->RateLimits)) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}

				if (ViewerMode == viewer_mode::bootstrap) {
					if (success->AccountTransport != nullptr) {
						fail_session(context,
									 core::github_failure::malformed_response);
						return;
					}
					static_cast<void>(merge(RateLimits, success->RateLimits));
					BootstrapViewer          = success->Viewer;
					CredentialAuthority      = credential_key(authority, success->Viewer.Account);
					CredentialSessionContext = context;
					CredentialLifecycle      = credential_lifecycle::provisional;
					if (!CredentialBindings.remember(*CredentialAuthority)) {
						fail_session(context, core::github_failure::vault);
						return;
					}
					CredentialBindingRemembered = true;
					start_store(context);
					return;
				}

				if (success->AccountTransport == nullptr) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}
				if (ViewerMode == viewer_mode::account) {
					if (!CredentialAuthority.has_value()
						|| success->Viewer.Account != CredentialAuthority->Account) {
						fail_session(context,
									 core::github_failure::malformed_response,
									 true);
						return;
					}
					if (BootstrapViewer.has_value()
						&& success->Viewer != *BootstrapViewer) {
						fail_session(context,
									 core::github_failure::malformed_response);
						return;
					}
				}

				static_cast<void>(merge(RateLimits, success->RateLimits));
				AccountTransport = success->AccountTransport;
				begin_capabilities(
					context, authority, success->Viewer, success->AccountTransport);
				return;
			}

			if (auto* error = std::get_if<github_viewer_error>(&aCompletion)) {
				const auto errorValid = valid_provider_error_resources(
					error->Error, true, false, false);
				if (errorValid) {
					observe_provider_error(error->Error);
				}
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					if (ViewerMode == viewer_mode::bootstrap) {
						submit_bootstrap_viewer(retry);
					} else if (ViewerMode == viewer_mode::account) {
						PendingProvider = std::monostate{ };
						start_retrieve(retry.Attempt);
					} else {
						submit_viewer(retry, std::monostate{ });
					}
					return;
				}
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				fail_session(error->Request.Context,
							 errorValid
								 ? mapped_failure(error->Error.Failure)
								 : core::github_failure::malformed_response);
				return;
			}

			const auto& cancelled = std::get<github_viewer_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			cancel_session(cancelled.Request.Context);
		}

		[[nodiscard]] auto next_epoch() -> std::optional<github_freshness_epoch>
		{
			if (LogicalEpoch == std::numeric_limits<std::uint64_t>::max()) {
				return std::nullopt;
			}
			++LogicalEpoch;
			return github_freshness_epoch{ LogicalEpoch };
		}

		void begin_capabilities(const core::github_session_request_context& aContext,
								const github_profile_authority&             aProfile,
								const github_viewer_record&                 aViewer,
								const github_account_transport&             aTransport)
		{
			const auto epoch = next_epoch();
			if (!epoch.has_value()) {
				fail_session(aContext, core::github_failure::malformed_response);
				return;
			}
			const auto key    = capability_key(aProfile, aViewer.Account);
			const auto lookup = CapabilityCache.lookup(key, *epoch);
			if (lookup.hit()) {
				accept_session(session_candidate{
					.Context            = aContext,
					.ProfileAuthority   = aProfile,
					.Viewer             = aViewer,
					.CapabilityEvidence = *lookup.Entry,
					.AccountTransport   = aTransport,
					.RateLimits         = RateLimits,
					.CacheStage         = std::nullopt,
				});
				return;
			}
			if (lookup.Status != github_capability_cache_lookup_status::miss
				&& lookup.Status != github_capability_cache_lookup_status::expired) {
				fail_session(aContext, core::github_failure::malformed_response);
				return;
			}

			submit_capability(github_capability_request{
				.Context          = aContext,
				.ProfileAuthority = aProfile,
				.Viewer           = aViewer,
				.AccountTransport = aTransport,
				.Attempt          = github_provider_attempt{ 1U },
			});
		}

		void submit_capability(const github_capability_request& aRequest)
		{
			PendingProvider   = aRequest;
			const auto  token = ++ProviderOperationToken;
			const auto  gate  = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane  = &DecisionLane;
			const auto  callback =
				[gate, lane, token, aRequest](
					github_capability_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_capability_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_capability(token, std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, aRequest]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_capability(
											token,
											github_capability_error{
												.Request = aRequest,
												.Error   = {
													.Failure = github_provider_failure::
														response_limit,
													.RetryAfterSeconds = std::nullopt,
													.RateLimits        = { },
													.RateLimitCause    = std::nullopt,
												},
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Capabilities.probe_capabilities(aRequest, callback);
			} catch (...) {
				callback(github_capability_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_capability(const std::uint64_t          aToken,
							   github_capability_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_capability_now(aToken, std::move(completion)); });
		}

		void handle_capability_now(const std::uint64_t          aToken,
								   github_capability_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_capability_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				fail_session(current.Context,
							 core::github_failure::malformed_response);
				return;
			}

			if (auto* success = std::get_if<github_capability_success>(&aCompletion)) {
				PendingProvider  = std::monostate{ };
				const auto epoch = next_epoch();
				if (!epoch.has_value()
					|| !valid_capability_rate_limits(
						success->Request.ProfileAuthority, success->RateLimits)) {
					fail_session(success->Request.Context,
								 core::github_failure::malformed_response);
					return;
				}
				const github_capability_probe_observation observation{
					.Authority   = capability_key(success->Request.ProfileAuthority,
												  success->Request.Viewer.Account),
					.ObservedAt  = *epoch,
					.Facts       = success->Facts,
					.SchemaFacts = success->SchemaFacts,
				};
				const auto staged = CapabilityCache.stage(
					github_capability_probe_result{ observation });
				if (!staged.staged()) {
					fail_session(
						success->Request.Context,
						staged.Error
								== github_capability_cache_error::capacity_exhausted
							? core::github_failure::response_too_large
							: core::github_failure::malformed_response);
					return;
				}
				static_cast<void>(merge(RateLimits, success->RateLimits));
				accept_session(session_candidate{
					.Context            = success->Request.Context,
					.ProfileAuthority   = success->Request.ProfileAuthority,
					.Viewer             = success->Request.Viewer,
					.CapabilityEvidence = staged.Stage->entry(),
					.AccountTransport   = success->Request.AccountTransport,
					.RateLimits         = RateLimits,
					.CacheStage         = staged.Stage,
				});
				return;
			}

			if (auto* error = std::get_if<github_capability_error>(&aCompletion)) {
				const auto errorValid = valid_provider_error_resources(
					error->Error, false, false, true);
				if (errorValid) {
					observe_provider_error(error->Error);
				}
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					submit_capability(retry);
					return;
				}
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				fail_session(error->Request.Context,
							 errorValid
								 ? mapped_failure(error->Error.Failure)
								 : core::github_failure::malformed_response);
				return;
			}

			const auto& cancelled = std::get<github_capability_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			cancel_session(cancelled.Request.Context);
		}

		void accept_session(session_candidate aCandidate)
		{
			const github_session_read_model model{
				.Profile            = aCandidate.ProfileAuthority.Profile,
				.Host               = aCandidate.ProfileAuthority.Host,
				.Viewer             = aCandidate.Viewer,
				.CapabilityEvidence = {
					.Capabilities    = aCandidate.CapabilityEvidence.Capabilities,
					.Revision        = aCandidate.CapabilityEvidence.Revision,
					.ReadShape       = aCandidate.CapabilityEvidence.ReadShape,
					.ContractVersion = 1U,
				},
				.RateLimits = aCandidate.RateLimits,
			};
			if (!validate(model, aCandidate.Context).valid()) {
				fail_session(aCandidate.Context,
							 core::github_failure::malformed_response);
				return;
			}
			if (CredentialAuthority.has_value() && !CredentialBindingRemembered) {
				if (!CredentialBindings.remember(*CredentialAuthority)) {
					SessionEnrollment.reset();
					fail_session(aCandidate.Context, core::github_failure::vault);
					return;
				}
				CredentialBindingRemembered = true;
			}

			StagedSession = aCandidate;
			auto output   = core::transition(
				State,
				core::github_session_ready_event{
					.Context            = aCandidate.Context,
					.Host               = aCandidate.ProfileAuthority.Host,
					.Account            = aCandidate.Viewer.Account,
					.Capabilities       = aCandidate.CapabilityEvidence.Capabilities,
					.CapabilityRevision = aCandidate.CapabilityEvidence.Revision,
				});
			const core::accepted_session_delta expected{
				.Context            = aCandidate.Context,
				.Host               = aCandidate.ProfileAuthority.Host,
				.Account            = aCandidate.Viewer.Account,
				.Capabilities       = aCandidate.CapabilityEvidence.Capabilities,
				.CapabilityRevision = aCandidate.CapabilityEvidence.Revision,
			};
			if (!output.AcceptedSession.has_value()
				|| *output.AcceptedSession != expected) {
				StagedSession.reset();
				publish_and_execute(output);
				if (CredentialLifecycle == credential_lifecycle::provisional
					&& CredentialAuthority.has_value()
					&& CredentialSessionContext.has_value()
					&& ActiveVaultOperation == vault_operation::none) {
					start_delete(aCandidate.Context.IdentityGeneration,
								 std::nullopt);
				}
				return;
			}

			if (aCandidate.CacheStage.has_value()) {
				const auto committed = CapabilityCache.commit(*aCandidate.CacheStage);
				if (!committed.stored()
					|| committed.Entry != aCandidate.CapabilityEvidence) {
					StagedSession.reset();
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::malformed_response,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
					auto rollback = core::transition(
						State, core::deactivate_github_identity_intent{ });
					publish_and_execute(rollback);
					return;
				}
			}

			AcceptedSession            = model;
			AcceptedProfileAuthority   = aCandidate.ProfileAuthority;
			AcceptedCapabilityEvidence = aCandidate.CapabilityEvidence;
			AccountTransport           = aCandidate.AccountTransport;
			RateLimits                 = aCandidate.RateLimits;
			StagedSession.reset();
			SessionEnrollment.reset();
			SessionResumeProfile.reset();
			BootstrapViewer.reset();
			ViewerMode = viewer_mode::none;
			if (CredentialAuthority.has_value()) {
				CredentialLifecycle = credential_lifecycle::persistent;
			}
			publish_transition(output, &*AcceptedSession);
		}

		void start_store(const core::github_session_request_context& aContext)
		{
			if (!CredentialAuthority.has_value() || !SessionEnrollment.has_value()) {
				fail_session(aContext, core::github_failure::vault);
				return;
			}
			const github_credential_operation_context operation{
				.Request            = aContext.Request,
				.ApplicationSession = aContext.ApplicationSession,
				.Authority          = *CredentialAuthority,
				.IdentityGeneration = aContext.IdentityGeneration,
			};
			ActiveVaultOperation = vault_operation::store;
			ActiveVaultContext   = operation;
			const auto  token    = ++VaultOperationToken;
			const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane     = &DecisionLane;
			const auto  callback =
				[gate, lane, token, operation](
					github_credential_store_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_credential_store_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token, operation]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_store(
										token, operation, std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, operation]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_store(
											token,
											operation,
											github_credential_store_error{
												.Context = operation,
												.Failure = github_credential_vault_failure::
													allocation_failure,
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Credentials.store(
					github_credential_enrollment{
						.Context    = operation,
						.Credential = std::move(*SessionEnrollment),
					},
					callback);
			} catch (...) {
				callback(github_credential_store_error{
					.Context = operation,
					.Failure = github_credential_vault_failure::operation_failed,
				});
			}
			SessionEnrollment.reset();
		}

		void
		handle_store(const std::uint64_t                        aToken,
					 const github_credential_operation_context& aExpectedOperation,
					 github_credential_store_completion         aCompletion)
		{
			run_turn(
				[this,
				 aToken,
				 aExpectedOperation,
				 completion = std::move(aCompletion)]() mutable {
					handle_store_now(
						aToken, aExpectedOperation, std::move(completion));
				});
		}

		void handle_store_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_store_completion         aCompletion)
		{
			if (aToken != VaultOperationToken) {
				return;
			}
			++VaultOperationToken;
			if (Closing) {
				if (ActiveVaultOperation == vault_operation::store
					&& ActiveVaultContext == aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredInvalidationGeneration.reset();
					if (requires_shutdown_credential_cleanup()) {
						start_delete(aExpectedOperation.IdentityGeneration,
									 std::nullopt);
					}
				} else {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			if (DeferredInvalidationGeneration.has_value()) {
				const auto generation = *DeferredInvalidationGeneration;
				DeferredInvalidationGeneration.reset();
				const auto preserve  = std::exchange(DeferredInvalidationPreservesCredential, false);
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				if (preserve) {
					start_deferred_session();
				} else {
					start_delete(generation, std::nullopt);
				}
				return;
			}
			if (ActiveVaultOperation != vault_operation::store
				|| !ActiveVaultContext.has_value()
				|| *ActiveVaultContext != aExpectedOperation
				|| !CredentialSessionContext.has_value()) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
				if (CredentialSessionContext.has_value()) {
					fail_session(*CredentialSessionContext,
								 core::github_failure::vault);
				}
				return;
			}
			if (!valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				if (CredentialSessionContext.has_value()) {
					fail_session(*CredentialSessionContext,
								 core::github_failure::vault);
				} else {
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			const auto session   = *CredentialSessionContext;
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();

			if (std::holds_alternative<github_credential_store_success>(aCompletion)) {
				ViewerMode = viewer_mode::account;
				start_retrieve(github_provider_attempt{ 1U });
				return;
			}
			if (std::holds_alternative<github_credential_store_cancelled>(
					aCompletion)) {
				cancel_session(session);
				return;
			}
			fail_session(session, core::github_failure::vault);
		}

		void start_retrieve(const github_provider_attempt aAttempt)
		{
			if (!CredentialAuthority.has_value()
				|| !CredentialSessionContext.has_value()) {
				return;
			}
			const auto&                               session = *CredentialSessionContext;
			const github_credential_operation_context operation{
				.Request            = session.Request,
				.ApplicationSession = session.ApplicationSession,
				.Authority          = *CredentialAuthority,
				.IdentityGeneration = session.IdentityGeneration,
			};
			PendingRetrieveAttempt = aAttempt;
			ActiveVaultOperation   = vault_operation::retrieve;
			ActiveVaultContext     = operation;
			const auto  token      = ++VaultOperationToken;
			const auto  gate       = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane       = &DecisionLane;
			const auto  callback =
				[gate, lane, token, operation](
					github_credential_retrieve_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_credential_retrieve_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token, operation]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_retrieve(
										token, operation, std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, operation]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_retrieve(
											token,
											operation,
											github_credential_retrieve_error{
												.Context = operation,
												.Failure = github_credential_vault_failure::
													allocation_failure,
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Credentials.retrieve(operation, callback);
			} catch (...) {
				callback(github_credential_retrieve_error{
					.Context = operation,
					.Failure = github_credential_vault_failure::operation_failed,
				});
			}
		}

		void handle_retrieve(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_retrieve_completion      aCompletion)
		{
			run_turn(
				[this,
				 aToken,
				 aExpectedOperation,
				 completion = std::move(aCompletion)]() mutable {
					handle_retrieve_now(
						aToken, aExpectedOperation, std::move(completion));
				});
		}

		void handle_retrieve_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_retrieve_completion      aCompletion)
		{
			if (aToken != VaultOperationToken) {
				return;
			}
			++VaultOperationToken;
			if (Closing) {
				if (ActiveVaultOperation == vault_operation::retrieve
					&& ActiveVaultContext == aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredInvalidationGeneration.reset();
					if (requires_shutdown_credential_cleanup()) {
						start_delete(aExpectedOperation.IdentityGeneration,
									 std::nullopt);
					}
				} else {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			if (DeferredInvalidationGeneration.has_value()) {
				const auto generation = *DeferredInvalidationGeneration;
				DeferredInvalidationGeneration.reset();
				const auto preserve  = std::exchange(DeferredInvalidationPreservesCredential, false);
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				if (preserve) {
					start_deferred_session();
				} else {
					start_delete(generation, std::nullopt);
				}
				return;
			}
			if (ActiveVaultOperation != vault_operation::retrieve
				|| !ActiveVaultContext.has_value()
				|| *ActiveVaultContext != aExpectedOperation
				|| !CurrentProfile.has_value()
				|| !CredentialSessionContext.has_value()) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
				if (CredentialSessionContext.has_value()) {
					fail_session(*CredentialSessionContext,
								 core::github_failure::vault);
				}
				return;
			}
			if (!valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				const auto context   = *CredentialSessionContext;
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				fail_session(context, core::github_failure::vault);
				return;
			}
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			const auto context = *CredentialSessionContext;

			if (auto* success = std::get_if<github_credential_retrieve_success>(&aCompletion)) {
				submit_viewer(
					github_viewer_request{
						.Context   = context,
						.Authority = *CurrentProfile,
						.Attempt   = PendingRetrieveAttempt,
					},
					std::move(*success));
				return;
			}
			if (std::holds_alternative<github_credential_retrieve_absent>(
					aCompletion)) {
				if (CredentialBindingRemembered) {
					CredentialBindings.forget(aExpectedOperation.Authority);
				}
				CredentialAuthority.reset();
				CredentialSessionContext.reset();
				CredentialLifecycle         = credential_lifecycle::none;
				CredentialBindingRemembered = false;
				fail_session_direct(context, core::github_failure::vault);
				return;
			}
			if (std::holds_alternative<github_credential_retrieve_cancelled>(
					aCompletion)) {
				cancel_session(context);
				return;
			}
			fail_session(context, core::github_failure::vault);
		}

		void start_delete(const core::github_identity_generation   aGeneration,
						  std::optional<deferred_session_terminal> aTerminal)
		{
			if (!CredentialAuthority.has_value()
				|| !CredentialSessionContext.has_value()) {
				if (aTerminal.has_value()) {
					finish_deferred_terminal(*aTerminal);
				}
				return;
			}
			DeferredTerminal                                  = std::move(aTerminal);
			const auto&                               session = *CredentialSessionContext;
			const github_credential_operation_context operation{
				.Request            = session.Request,
				.ApplicationSession = session.ApplicationSession,
				.Authority          = *CredentialAuthority,
				.IdentityGeneration = aGeneration,
			};
			ActiveVaultOperation = vault_operation::delete_credential;
			ActiveVaultContext   = operation;
			const auto  token    = ++VaultOperationToken;
			const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane     = &DecisionLane;
			const auto  callback =
				[gate, lane, token, operation](
					github_credential_delete_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_credential_delete_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token, operation]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_delete(
										token, operation, std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, operation]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_delete(
											token,
											operation,
											github_credential_delete_error{
												.Context = operation,
												.Failure = github_credential_vault_failure::
													allocation_failure,
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Credentials.delete_credential(operation, callback);
			} catch (...) {
				callback(github_credential_delete_error{
					.Context = operation,
					.Failure = github_credential_vault_failure::operation_failed,
				});
			}
		}

		void
		handle_delete(const std::uint64_t                        aToken,
					  const github_credential_operation_context& aExpectedOperation,
					  github_credential_delete_completion        aCompletion)
		{
			run_turn(
				[this,
				 aToken,
				 aExpectedOperation,
				 completion = std::move(aCompletion)]() mutable {
					handle_delete_now(
						aToken, aExpectedOperation, std::move(completion));
				});
		}

		void handle_delete_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_delete_completion        aCompletion)
		{
			if (aToken != VaultOperationToken) {
				return;
			}
			++VaultOperationToken;
			if (Closing) {
				if (ActiveVaultOperation != vault_operation::delete_credential
					|| !ActiveVaultContext.has_value()
					|| *ActiveVaultContext != aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredTerminal.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
					return;
				}

				if (!valid(aCompletion)
					|| completion_context(aCompletion) != aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredTerminal.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
					return;
				}

				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				DeferredTerminal.reset();
				if (std::holds_alternative<github_credential_delete_success>(
						aCompletion)) {
					if (CredentialBindingRemembered) {
						CredentialBindings.forget(aExpectedOperation.Authority);
					}
					CredentialAuthority.reset();
					CredentialSessionContext.reset();
					CredentialLifecycle         = credential_lifecycle::none;
					CredentialBindingRemembered = false;
				} else {
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			if (ActiveVaultOperation != vault_operation::delete_credential
				|| !ActiveVaultContext.has_value()
				|| *ActiveVaultContext != aExpectedOperation || !valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				fail_delete_operation();
				return;
			}
			if (!std::holds_alternative<github_credential_delete_success>(
					aCompletion)) {
				fail_delete_operation();
				return;
			}

			const auto terminal = DeferredTerminal;
			DeferredTerminal.reset();
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			if (CredentialBindingRemembered) {
				CredentialBindings.forget(aExpectedOperation.Authority);
			}
			CredentialAuthority.reset();
			CredentialSessionContext.reset();
			CredentialLifecycle         = credential_lifecycle::none;
			CredentialBindingRemembered = false;
			if (terminal.has_value()) {
				finish_deferred_terminal(*terminal);
			} else {
				start_deferred_session();
			}
		}

		void fail_delete_operation()
		{
			const auto terminal = DeferredTerminal;
			DeferredTerminal.reset();
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			publish_diagnostic(github_host_diagnostic{
				.Reason            = core::github_reason_code::vault_unavailable,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			});
			if (terminal.has_value()) {
				finish_deferred_terminal(deferred_session_terminal{
					.Context = terminal->Context,
					.Failure = core::github_failure::vault,
				});
			} else if (DeferredSession.has_value()) {
				const auto context = *DeferredSession;
				DeferredSession.reset();
				fail_session_direct(context, core::github_failure::vault);
			}
		}

		void fail_session(const core::github_session_request_context& aContext,
						  const core::github_failure                  aFailure,
						  const bool                                  aDeletePersistentCredential = false)
		{
			cancel_provider();
			if (CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()
				&& !AcceptedSession.has_value()
				&& ActiveVaultOperation == vault_operation::none
				&& (CredentialLifecycle == credential_lifecycle::provisional
					|| aDeletePersistentCredential)) {
				start_delete(aContext.IdentityGeneration,
							 deferred_session_terminal{
								 .Context = aContext,
								 .Failure = aFailure,
							 });
				return;
			}
			fail_session_direct(aContext, aFailure);
		}

		void
		fail_session_direct(const core::github_session_request_context& aContext,
							const core::github_failure                  aFailure)
		{
			reset_session_scratch();
			dispatch_core(core::github_session_failed_event{
				.Context = aContext,
				.Failure = aFailure,
			});
		}

		void cancel_session(const core::github_session_request_context& aContext)
		{
			cancel_provider();
			if (CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()
				&& !AcceptedSession.has_value()
				&& ActiveVaultOperation == vault_operation::none
				&& CredentialLifecycle == credential_lifecycle::provisional) {
				start_delete(aContext.IdentityGeneration,
							 deferred_session_terminal{
								 .Context = aContext,
								 .Failure = std::nullopt,
							 });
				return;
			}
			reset_session_scratch();
			dispatch_core(core::github_session_cancelled_event{
				.Context = aContext,
			});
		}

		void finish_deferred_terminal(const deferred_session_terminal& aTerminal)
		{
			if (aTerminal.Failure.has_value()) {
				fail_session_direct(aTerminal.Context, *aTerminal.Failure);
			} else {
				reset_session_scratch();
				dispatch_core(core::github_session_cancelled_event{
					.Context = aTerminal.Context,
				});
			}
		}

		void reset_session_scratch()
		{
			PendingProvider = std::monostate{ };
			StagedSession.reset();
			CurrentProfile.reset();
			BootstrapViewer.reset();
			AccountTransport.reset();
			SessionEnrollment.reset();
			SessionResumeProfile.reset();
			ViewerMode = viewer_mode::none;
		}

		void start_pull_request(const core::pull_request_request_context& aContext)
		{
			if (!AcceptedSession.has_value() || !AcceptedProfileAuthority.has_value()
				|| !AcceptedCapabilityEvidence.has_value()
				|| AccountTransport == nullptr
				|| aContext.Profile != AcceptedProfileAuthority->Profile
				|| aContext.Host != AcceptedProfileAuthority->Host
				|| aContext.Account != AcceptedSession->Viewer.Account
				|| aContext.Capabilities != AcceptedCapabilityEvidence->Revision
				|| AcceptedCapabilityEvidence->Authority.Account != aContext.Account) {
				dispatch_core(core::pull_request_load_failed_event{
					.Context = aContext,
					.Failure = core::github_failure::malformed_response,
				});
				return;
			}
			submit_pull_request(github_pull_request_provider_request{
				.Context            = aContext,
				.ProfileAuthority   = *AcceptedProfileAuthority,
				.Viewer             = AcceptedSession->Viewer,
				.CapabilityEvidence = *AcceptedCapabilityEvidence,
				.AccountTransport   = AccountTransport,
				.Attempt            = github_provider_attempt{ 1U },
			});
		}

		void
		submit_pull_request(const github_pull_request_provider_request& aRequest)
		{
			PendingProvider   = aRequest;
			const auto  token = ++ProviderOperationToken;
			const auto  gate  = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane  = &DecisionLane;
			const auto  callback =
				[gate, lane, token, aRequest](
					github_pull_request_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_pull_request_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_pull_request(token,
															  std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, aRequest]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_pull_request(
											token,
											github_pull_request_error{
												.Request = aRequest,
												.Error   = {
													.Failure = github_provider_failure::
														response_limit,
													.RetryAfterSeconds = std::nullopt,
													.RateLimits        = { },
													.RateLimitCause    = std::nullopt,
												},
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				PullRequests.fetch_pull_request(aRequest, callback);
			} catch (...) {
				callback(github_pull_request_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_pull_request(const std::uint64_t            aToken,
								 github_pull_request_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_pull_request_now(aToken, std::move(completion)); });
		}

		void handle_pull_request_now(const std::uint64_t            aToken,
									 github_pull_request_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_pull_request_provider_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				dispatch_core(core::pull_request_load_failed_event{
					.Context = current.Context,
					.Failure = core::github_failure::malformed_response,
				});
				return;
			}

			if (auto* success = std::get_if<github_pull_request_success>(&aCompletion)) {
				PendingProvider = std::monostate{ };
				const github_actor_record expectedViewer{
					.Actor = success->Request.Viewer.Node,
					.Login = success->Request.Viewer.Login,
				};
				if (success->Snapshot.Viewer != expectedViewer
					|| success->Snapshot.CapabilityEvidence
						!= projected_capability_evidence(
							success->Request.CapabilityEvidence)
					|| !AcceptedProfileAuthority.has_value()
					|| !valid_snapshot_rate_limits(*AcceptedProfileAuthority,
												   success->Snapshot)
					|| !validate(success->Snapshot, success->Request.Context)
							.valid()) {
					dispatch_core(core::pull_request_load_failed_event{
						.Context = success->Request.Context,
						.Failure = core::github_failure::malformed_response,
					});
					return;
				}
				StagedSnapshot = success->Snapshot;
				auto output    = core::transition(
					State,
					core::pull_request_loaded_event{
						.Context     = success->Request.Context,
						.Repository  = success->Snapshot.Repository.Repository,
						.PullRequest = success->Snapshot.PullRequest.PullRequest,
						.Snapshot    = success->Snapshot.Snapshot,
					});
				const core::accepted_snapshot_delta expected{
					.Context     = success->Request.Context,
					.Repository  = success->Snapshot.Repository.Repository,
					.PullRequest = success->Snapshot.PullRequest.PullRequest,
					.Snapshot    = success->Snapshot.Snapshot,
				};
				if (output.AcceptedSnapshot.has_value()
					&& *output.AcceptedSnapshot == expected) {
					AcceptedSnapshot = std::move(StagedSnapshot);
					StagedSnapshot.reset();
					renew_review_mutation_transport();
					static_cast<void>(
						merge(RateLimits, AcceptedSnapshot->RateLimits));
					publish_transition(
						output,
						nullptr,
						&*AcceptedSnapshot);
					deliver_epoch_handoff(*AcceptedSnapshot);
				} else {
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
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				dispatch_core(core::pull_request_load_failed_event{
					.Context = error->Request.Context,
					.Failure = errorValid
						? mapped_failure(error->Error.Failure)
						: core::github_failure::malformed_response,
				});
				return;
			}

			const auto& cancelled = std::get<github_pull_request_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			dispatch_core(core::pull_request_load_cancelled_event{
				.Context = cancelled.Request.Context,
			});
		}

		void cancel_provider()
		{
			std::visit(
				[this](const auto& aRequest) {
					using request_type = std::decay_t<decltype(aRequest)>;
					if constexpr (std::is_same_v<request_type,
												 github_profile_request>) {
						try {
							Profiles.cancel_profile(aRequest);
						} catch (...) {
						}
					} else if constexpr (std::is_same_v<request_type,
														github_viewer_request>) {
						try {
							Viewers.cancel_viewer(aRequest);
						} catch (...) {
						}
					} else if constexpr (std::is_same_v<request_type,
														github_capability_request>) {
						try {
							Capabilities.cancel_capabilities(aRequest);
						} catch (...) {
						}
					} else if constexpr (std::is_same_v<
											 request_type,
											 github_pull_request_provider_request>) {
						try {
							PullRequests.cancel_pull_request(aRequest);
						} catch (...) {
						}
					}
				},
				PendingProvider);
			PendingProvider = std::monostate{ };
			++ProviderOperationToken;
		}

		void cancel_vault()
		{
			if (ActiveVaultContext.has_value()) {
				try {
					Credentials.cancel(*ActiveVaultContext);
				} catch (...) {
				}
			}
			++VaultOperationToken;
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
		}

		void shutdown()
		{
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_shutdown{ });
				return;
			}
			run_turn([this] { shutdown_now(); });
		}

		void shutdown_now()
		{
			{
				const std::lock_guard barrier{ CompletionGate->Barrier };
				if (Closing) {
					return;
				}

				if (State.ActiveProfile.has_value()
					|| State.PendingSession.has_value()
					|| State.PendingPullRequest.has_value()) {
					static_cast<void>(core::transition(
						State, core::deactivate_github_identity_intent{ }));
				}

				Closing = true;
			}

			// Wipe raw host-owned material before an external cancellation can
			// block or re-enter. PendingProvider remains intact until its exact
			// cancellation has been submitted.
			NextEnrollment.reset();
			SessionEnrollment.reset();
			NextResumeProfile.reset();
			SessionResumeProfile.reset();

			// Never call an external port while holding the completion gate.
			// A cancellation implementation may wait for a worker callback that
			// itself needs the gate in order to enqueue its terminal.
			invalidate_epoch_authority();
			cancel_provider();
			if ((ActiveVaultOperation == vault_operation::store
				 || ActiveVaultOperation == vault_operation::retrieve)
				&& ActiveVaultContext.has_value()) {
				try {
					Credentials.cancel(*ActiveVaultContext);
				} catch (...) {
				}
			} else if (ActiveVaultOperation == vault_operation::delete_credential
					   && ActiveVaultContext.has_value()) {
				// An exact cleanup is already in flight. Its terminal remains the
				// only authority for shutdown quiescence.
			} else if (requires_shutdown_credential_cleanup()) {
				start_delete(CredentialSessionContext->IdentityGeneration,
							 std::nullopt);
			} else {
				cancel_vault();
			}
			static_cast<void>(CapabilityCache.clear());
			DeferredSession.reset();
			revoke_review_mutation_transport();
			reset_session_scratch();
			StagedSnapshot.reset();
			AcceptedSession.reset();
			AcceptedSnapshot.reset();
			AcceptedProfileAuthority.reset();
			AcceptedCapabilityEvidence.reset();
		}

		core::github_state                 State;
		github_profile_port&               Profiles;
		github_viewer_port&                Viewers;
		github_capability_port&            Capabilities;
		github_pull_request_port&          PullRequests;
		github_credential_vault_port&      Credentials;
		github_credential_binding_catalog& CredentialBindings;
		github_capability_cache            CapabilityCache;
		decision_executor&                 DecisionLane;
		github_projection_port&            Projections;
		github_credential_shutdown_policy  CredentialShutdownPolicy = github_credential_shutdown_policy::retain;
		github_epoch_handoff_port*         Epochs                   = nullptr;
		std::shared_ptr<completion_gate>   CompletionGate;
		std::deque<deferred_ingress>       DeferredIngress;
		bool                               TurnActive               = false;
		bool                               DeferredIngressScheduled = false;

		pending_provider_request                            PendingProvider;
		std::uint64_t                                       ProviderOperationToken = 0U;
		std::optional<github_credential_material>           NextEnrollment;
		std::optional<github_credential_material>           SessionEnrollment;
		std::optional<core::host_profile_id>                NextResumeProfile;
		std::optional<core::host_profile_id>                SessionResumeProfile;
		std::optional<core::github_session_request_context> DeferredSession;
		std::optional<github_profile_authority>             CurrentProfile;
		std::optional<github_viewer_record>                 BootstrapViewer;
		viewer_mode                                         ViewerMode = viewer_mode::none;
		github_account_transport                            AccountTransport;

		vault_operation                                    ActiveVaultOperation = vault_operation::none;
		std::optional<github_credential_operation_context> ActiveVaultContext;
		std::uint64_t                                      VaultOperationToken = 0U;
		github_provider_attempt                            PendingRetrieveAttempt{ 1U };
		std::optional<github_credential_authority_key>     CredentialAuthority;
		std::optional<core::github_session_request_context>
												 CredentialSessionContext;
		credential_lifecycle                     CredentialLifecycle         = credential_lifecycle::none;
		bool                                     CredentialBindingRemembered = false;
		std::optional<deferred_session_terminal> DeferredTerminal;
		std::optional<core::github_identity_generation>
			 DeferredInvalidationGeneration;
		bool DeferredInvalidationPreservesCredential = false;

		std::optional<session_candidate>                   StagedSession;
		std::optional<github_pull_request_snapshot>        StagedSnapshot;
		std::optional<github_session_read_model>           AcceptedSession;
		std::optional<github_profile_authority>            AcceptedProfileAuthority;
		std::optional<github_capability_cache_entry>       AcceptedCapabilityEvidence;
		std::optional<github_pull_request_snapshot>        AcceptedSnapshot;
		review_mutation_transport                          ReviewMutationTransport;
		github_rate_limit_state                            RateLimits;
		std::uint64_t                                      LogicalEpoch               = 0U;
		bool                                               Closing                    = false;
		bool                                               ShutdownCleanupFailed      = false;
		bool                                               ProjectionDeliveryFailed   = false;
		bool                                               EpochHandoffDeliveryFailed = false;
		std::optional<pull_request_authority_invalidation> ActiveEpochAuthority;
		std::shared_ptr<smart_authority_state>             ActiveEpochSmartAuthority;
	};

	github_application_host::github_application_host(
		core::github_state                      aInitialState,
		github_profile_port&                    aProfiles,
		github_viewer_port&                     aViewers,
		github_capability_port&                 aCapabilities,
		github_pull_request_port&               aPullRequests,
		github_credential_vault_port&           aCredentials,
		github_credential_binding_catalog&      aCredentialBindings,
		decision_executor&                      aDecisionLane,
		github_projection_port&                 aProjections,
		const github_credential_shutdown_policy aCredentialShutdownPolicy,
		github_epoch_handoff_port* const        aEpochs)
		: Implementation{ std::make_unique<implementation>(std::move(aInitialState),
														   aProfiles,
														   aViewers,
														   aCapabilities,
														   aPullRequests,
														   aCredentials,
														   aCredentialBindings,
														   aDecisionLane,
														   aProjections,
														   aCredentialShutdownPolicy,
														   aEpochs) }
	{
	}

	github_application_host::~github_application_host()
	{
		shutdown();
	}

	void github_application_host::dispatch(const core::github_event& aEvent)
	{
		Implementation->dispatch(aEvent);
	}

	void github_application_host::enroll_profile(
		const core::host_profile_id aProfile, github_credential_material aCredential)
	{
		Implementation->enroll_profile(aProfile, std::move(aCredential));
	}

	void github_application_host::resume_profile(
		const core::host_profile_id aProfile)
	{
		Implementation->resume_profile(aProfile);
	}

	void github_application_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto github_application_host::state() const noexcept
		-> const core::github_state&
	{
		return Implementation->State;
	}

	auto github_application_host::accepted_session() const noexcept
		-> const std::optional<github_session_read_model>&
	{
		return Implementation->AcceptedSession;
	}

	auto github_application_host::accepted_snapshot() const noexcept
		-> const std::optional<github_pull_request_snapshot>&
	{
		return Implementation->AcceptedSnapshot;
	}

	auto github_application_host::accepted_review_mutation_transport() const noexcept
		-> review_mutation_transport
	{
		const auto& host = *Implementation;
		if (host.Closing || host.ReviewMutationTransport == nullptr
			|| !host.ReviewMutationTransport->active()
			|| !host.AcceptedSession.has_value()
			|| !host.AcceptedProfileAuthority.has_value()
			|| !host.AcceptedSnapshot.has_value()) {
			return { };
		}
		const auto& session   = *host.AcceptedSession;
		const auto& authority = *host.AcceptedProfileAuthority;
		const auto& snapshot  = *host.AcceptedSnapshot;
		if (session.Profile != authority.Profile
			|| session.Host != authority.Host
			|| snapshot.Profile != session.Profile
			|| snapshot.Host != session.Host
			|| snapshot.Account != session.Viewer.Account
			|| snapshot.Viewer.Actor != session.Viewer.Node
			|| snapshot.Viewer.Login != session.Viewer.Login) {
			return { };
		}
		return host.ReviewMutationTransport;
	}

	auto github_application_host::rate_limits() const noexcept
		-> const github_rate_limit_state&
	{
		return Implementation->RateLimits;
	}

	auto github_application_host::capability_cache_size() const noexcept
		-> std::size_t
	{
		return Implementation->CapabilityCache.size();
	}

	auto github_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto github_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing
			&& std::holds_alternative<std::monostate>(
				   Implementation->PendingProvider)
			&& Implementation->ActiveVaultOperation
			== implementation::vault_operation::none;
	}

	auto github_application_host::shutdown_cleanup_failed() const noexcept -> bool
	{
		return Implementation->ShutdownCleanupFailed;
	}

	auto github_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}

	auto github_application_host::epoch_handoff_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->EpochHandoffDeliveryFailed;
	}
}
