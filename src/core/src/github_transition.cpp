#include "bpr/core/github_transition.hpp"

#include "bpr/core/anonymous_read_policy.hpp"

#include <type_traits>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto next_request(github_state& aState) -> request_id
		{
			const auto request = aState.NextRequest;
			aState.NextRequest = request_id{ request.value() + 1U };
			return request;
		}

		[[nodiscard]] auto identity_scope(const github_state& aState)
			-> github_identity_scope
		{
			return github_identity_scope{
				.Profile     = aState.ActiveProfile,
				.Host        = aState.Host,
				.Account     = aState.Account,
				.Repository  = aState.Repository,
				.PullRequest = aState.PullRequest,
				.Snapshot    = aState.AcceptedSnapshot,
			};
		}

		[[nodiscard]] auto identity_delta(const github_state& aState)
			-> github_identity_delta
		{
			return github_identity_delta{
				.Profile    = aState.ActiveProfile,
				.Host       = aState.Host,
				.Account    = aState.Account,
				.Generation = aState.IdentityGeneration,
			};
		}

		[[nodiscard]] auto capability_delta(const github_state& aState)
			-> github_capability_delta
		{
			return github_capability_delta{
				.Capabilities = aState.Capabilities,
				.Revision     = aState.CapabilityRevision,
			};
		}

		[[nodiscard]] auto summary(const github_state& aState)
			-> std::optional<github_pull_request_summary>
		{
			if (
				!aState.ActiveLocator.has_value() || !aState.ActiveNumber.has_value()
				|| !aState.Repository.has_value() || !aState.PullRequest.has_value()
				|| !aState.AcceptedSnapshot.has_value()) {
				return std::nullopt;
			}

			return github_pull_request_summary{
				.Locator     = *aState.ActiveLocator,
				.Number      = *aState.ActiveNumber,
				.Repository  = *aState.Repository,
				.PullRequest = *aState.PullRequest,
				.Snapshot    = *aState.AcceptedSnapshot,
			};
		}

		[[nodiscard]] auto command(
			const github_command_id  aId,
			const bool               aEnabled,
			const github_reason_code aReason = github_reason_code::none)
			-> github_command_projection
		{
			return github_command_projection{
				.Id             = aId,
				.Enabled        = aEnabled,
				.DisabledReason = aEnabled ? github_reason_code::none : aReason,
			};
		}

		[[nodiscard]] auto reject(const github_reason_code aReason)
			-> github_transition_output
		{
			github_transition_output output;
			output.Changed = github_changed_domain::diagnostics;
			output.Diagnostics.push_back(github_diagnostic{ .Code = aReason });
			return output;
		}

		auto publish_commands(
			const github_state&       aState,
			github_transition_output& aOutput) -> void
		{
			aOutput.Commands = github_commands(aState);
			aOutput.Changed  = aOutput.Changed | github_changed_domain::commands;
		}

		[[nodiscard]] auto current_session(
			const github_state&                   aState,
			const github_session_request_context& aContext) noexcept -> bool
		{
			return aState.PendingSession.has_value()
				&& *aState.PendingSession == aContext
				&& aState.ActiveProfile == aContext.Profile
				&& aState.ApplicationSession == aContext.ApplicationSession
				&& aState.IdentityGeneration == aContext.IdentityGeneration;
		}

		[[nodiscard]] auto current_pull_request(
			const github_state&                 aState,
			const pull_request_request_context& aContext) noexcept -> bool
		{
			return aState.PendingPullRequest.has_value()
				&& *aState.PendingPullRequest == aContext
				&& aState.ActiveProfile == aContext.Profile && aState.Host == aContext.Host
				&& aState.Account == aContext.Account
				&& aState.ApplicationSession == aContext.ApplicationSession
				&& aState.IdentityGeneration == aContext.IdentityGeneration
				&& aState.PullRequestGeneration == aContext.Generation
				&& aState.CapabilityRevision == aContext.Capabilities
				&& aState.ActiveLocator == aContext.Repository
				&& aState.ActiveNumber == aContext.Number;
		}

		[[nodiscard]] constexpr auto valid_capability_state(
			const github_capability_state aState) noexcept -> bool
		{
			return aState == github_capability_state::unknown
				|| aState == github_capability_state::supported
				|| aState == github_capability_state::unsupported;
		}

		[[nodiscard]] constexpr auto valid_capabilities(
			const github_capabilities& aCapabilities) noexcept -> bool
		{
			return valid_capability_state(aCapabilities.ReviewThreads)
				&& valid_capability_state(aCapabilities.ViewedFileMutations)
				&& valid_capability_state(aCapabilities.ResolveThread)
				&& valid_capability_state(aCapabilities.UnresolveThread);
		}

		void advance_and_clear_identity_scope(github_state& aState) noexcept
		{
			aState.IdentityGeneration = github_identity_generation{
				aState.IdentityGeneration.value() + 1U
			};
			aState.ActiveProfile.reset();
			aState.PendingSession.reset();
			aState.Host.reset();
			aState.Account.reset();
			aState.Capabilities       = { };
			aState.CapabilityRevision = { };
			aState.PendingPullRequest.reset();
			aState.ActiveLocator.reset();
			aState.ActiveNumber.reset();
			aState.Repository.reset();
			aState.PullRequest.reset();
			aState.AcceptedSnapshot.reset();
			// A cleared scope owns no mode. The next activation names the one
			// it wants, so an abandoned public-preview scope can never leave
			// the state answering as signed out.
			aState.AccessMode = github_access_mode::authenticated;
		}

		[[nodiscard]] auto activate_profile(
			github_state&                       aState,
			const activate_host_profile_intent& aIntent) -> github_transition_output
		{
			if (aIntent.Profile.value() == 0U) {
				return reject(github_reason_code::invalid_profile);
			}

			const auto previous = identity_scope(aState);
			advance_and_clear_identity_scope(aState);
			aState.ActiveProfile = aIntent.Profile;
			aState.AccessMode    = aIntent.Mode;

			const github_session_request_context context{
				.Request            = next_request(aState),
				.ApplicationSession = aState.ApplicationSession,
				.Profile            = aIntent.Profile,
				.IdentityGeneration = aState.IdentityGeneration,
			};
			aState.PendingSession = context;

			github_transition_output output;
			output.Changed = github_changed_domain::identity
				| github_changed_domain::capabilities
				| github_changed_domain::pull_request
				| github_changed_domain::status;
			output.Requests.emplace_back(invalidate_github_identity_request{
				.Request            = context.Request,
				.ApplicationSession = aState.ApplicationSession,
				.NewGeneration      = aState.IdentityGeneration,
				.Previous           = previous,
			});
			output.Requests.emplace_back(establish_github_session_request{ .Context = context });
			output.Identity     = identity_delta(aState);
			output.Capabilities = capability_delta(aState);
			output.PullRequest  = pull_request_summary_delta{ .Summary = std::nullopt };
			output.Status       = github_status_delta{ .Status = github_status::establishing_session };
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto deactivate_identity(github_state& aState)
			-> github_transition_output
		{
			const auto previous = identity_scope(aState);
			advance_and_clear_identity_scope(aState);

			github_transition_output output;
			output.Changed = github_changed_domain::identity
				| github_changed_domain::capabilities
				| github_changed_domain::pull_request
				| github_changed_domain::status;
			output.Requests.emplace_back(invalidate_github_identity_request{
				.Request            = next_request(aState),
				.ApplicationSession = aState.ApplicationSession,
				.NewGeneration      = aState.IdentityGeneration,
				.Previous           = previous,
			});
			output.Identity     = identity_delta(aState);
			output.Capabilities = capability_delta(aState);
			output.PullRequest  = pull_request_summary_delta{ .Summary = std::nullopt };
			output.Status       = github_status_delta{ .Status = github_status::no_profile };
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto session_ready(
			github_state&                     aState,
			const github_session_ready_event& aEvent) -> github_transition_output
		{
			if (!current_session(aState, aEvent.Context)) {
				return reject(github_reason_code::stale_completion);
			}

			aState.PendingSession.reset();
			// The mode the profile was activated in decides what an accepted
			// session must carry. Public preview has no reader to name, so a
			// nonzero account is as wrong there as a zero one is under the
			// authenticated mode: neither session can be mistaken for the
			// other, and neither is a relaxation of the other's rule.
			const auto accountAsExpected = aState.AccessMode
					== github_access_mode::public_preview
				? aEvent.Account.value() == 0U
				: aEvent.Account.value() != 0U;
			if (
				aEvent.Host.value() == 0U || !accountAsExpected
				|| aEvent.CapabilityRevision.value() == 0U) {
				auto output    = reject(github_reason_code::completion_identity_mismatch);
				output.Status  = github_status_delta{ .Status = github_status::session_failed };
				output.Changed = output.Changed | github_changed_domain::status;
				publish_commands(aState, output);
				return output;
			}
			if (!valid_capabilities(aEvent.Capabilities)) {
				auto output    = reject(github_reason_code::malformed_response);
				output.Status  = github_status_delta{ .Status = github_status::session_failed };
				output.Changed = output.Changed | github_changed_domain::status;
				publish_commands(aState, output);
				return output;
			}

			aState.Host               = aEvent.Host;
			aState.Account            = aEvent.Account;
			aState.Capabilities       = aEvent.Capabilities;
			aState.CapabilityRevision = aEvent.CapabilityRevision;

			github_transition_output output;
			output.Changed = github_changed_domain::identity
				| github_changed_domain::capabilities
				| github_changed_domain::status;
			output.Identity        = identity_delta(aState);
			output.Capabilities    = capability_delta(aState);
			output.AcceptedSession = accepted_session_delta{
				.Context            = aEvent.Context,
				.Host               = aEvent.Host,
				.Account            = aEvent.Account,
				.Capabilities       = aEvent.Capabilities,
				.CapabilityRevision = aEvent.CapabilityRevision,
			};
			output.Status = github_status_delta{ .Status = github_status::session_ready };
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto session_failed(
			github_state&                         aState,
			const github_session_request_context& aContext,
			const github_failure                  aFailure) -> github_transition_output
		{
			if (!current_session(aState, aContext)) {
				return reject(github_reason_code::stale_completion);
			}

			aState.PendingSession.reset();
			auto output    = reject(reason_for(aFailure));
			output.Status  = github_status_delta{ .Status = github_status::session_failed };
			output.Changed = output.Changed | github_changed_domain::status;
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto session_cancelled(
			github_state&                         aState,
			const github_session_request_context& aContext) -> github_transition_output
		{
			if (!current_session(aState, aContext)) {
				return reject(github_reason_code::stale_completion);
			}

			aState.PendingSession.reset();
			auto output    = reject(github_reason_code::cancelled);
			output.Status  = github_status_delta{ .Status = github_status::session_cancelled };
			output.Changed = output.Changed | github_changed_domain::status;
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto start_pull_request(
			github_state&             aState,
			repository_locator        aLocator,
			const pull_request_number aNumber,
			const bool                aRefresh) -> github_transition_output
		{
			if (aState.PendingSession.has_value()) {
				return reject(github_reason_code::session_in_progress);
			}
			if (
				!aState.ActiveProfile.has_value() || !aState.Host.has_value()
				|| !aState.Account.has_value()) {
				return reject(github_reason_code::session_required);
			}
			if (aState.PendingPullRequest.has_value()) {
				return reject(github_reason_code::pull_request_load_in_progress);
			}
			if (!valid(aLocator)) {
				return reject(github_reason_code::invalid_repository);
			}
			if (!aNumber.valid()) {
				return reject(github_reason_code::invalid_pull_request_number);
			}
			if (
				aRefresh
				&& (!aState.Repository.has_value() || !aState.PullRequest.has_value()
					|| !aState.AcceptedSnapshot.has_value())) {
				return reject(github_reason_code::no_active_pull_request);
			}

			const auto expectedRepository  = aRefresh ? aState.Repository : std::optional<repository_id>{ };
			const auto expectedPullRequest = aRefresh ? aState.PullRequest : std::optional<pull_request_id>{ };

			if (!aRefresh) {
				aState.ActiveLocator = aLocator;
				aState.ActiveNumber  = aNumber;
				aState.Repository.reset();
				aState.PullRequest.reset();
				aState.AcceptedSnapshot.reset();
			}

			aState.PullRequestGeneration = request_generation{
				aState.PullRequestGeneration.value() + 1U
			};
			pull_request_request_context context{
				.Request             = next_request(aState),
				.ApplicationSession  = aState.ApplicationSession,
				.Profile             = *aState.ActiveProfile,
				.Host                = *aState.Host,
				.Account             = *aState.Account,
				.Repository          = std::move(aLocator),
				.Number              = aNumber,
				.ExpectedRepository  = expectedRepository,
				.ExpectedPullRequest = expectedPullRequest,
				.Capabilities        = aState.CapabilityRevision,
				.IdentityGeneration  = aState.IdentityGeneration,
				.Generation          = aState.PullRequestGeneration,
				.Mode                = aState.AccessMode,
			};
			aState.PendingPullRequest = context;

			github_transition_output output;
			output.Changed = github_changed_domain::status | github_changed_domain::pull_request;
			output.Requests.emplace_back(fetch_pull_request_request{ .Context = context });
			output.Status = github_status_delta{ .Status = github_status::loading_pull_request };
			if (!aRefresh) {
				output.PullRequest = pull_request_summary_delta{ .Summary = std::nullopt };
			}
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto open_pull_request(
			github_state&                   aState,
			const open_pull_request_intent& aIntent) -> github_transition_output
		{
			return start_pull_request(
				aState,
				aIntent.Repository,
				aIntent.Number,
				false);
		}

		[[nodiscard]] auto refresh_pull_request(github_state& aState)
			-> github_transition_output
		{
			if (!aState.ActiveLocator.has_value() || !aState.ActiveNumber.has_value()) {
				return reject(github_reason_code::no_active_pull_request);
			}

			return start_pull_request(
				aState,
				*aState.ActiveLocator,
				*aState.ActiveNumber,
				true);
		}

		[[nodiscard]] auto pull_request_loaded(
			github_state&                    aState,
			const pull_request_loaded_event& aEvent) -> github_transition_output
		{
			if (!current_pull_request(aState, aEvent.Context)) {
				return reject(github_reason_code::stale_completion);
			}

			aState.PendingPullRequest.reset();
			const auto validIdentity = aEvent.Repository.value() != 0U && aEvent.PullRequest.value() != 0U
				&& aEvent.Snapshot.value() != 0U
				&& (!aEvent.Context.ExpectedRepository.has_value()
					|| *aEvent.Context.ExpectedRepository == aEvent.Repository)
				&& (!aEvent.Context.ExpectedPullRequest.has_value()
					|| *aEvent.Context.ExpectedPullRequest == aEvent.PullRequest);
			if (!validIdentity) {
				auto output    = reject(github_reason_code::completion_identity_mismatch);
				output.Status  = github_status_delta{ .Status = github_status::pull_request_failed };
				output.Changed = output.Changed | github_changed_domain::status;
				publish_commands(aState, output);
				return output;
			}

			aState.Repository       = aEvent.Repository;
			aState.PullRequest      = aEvent.PullRequest;
			aState.AcceptedSnapshot = aEvent.Snapshot;

			github_transition_output output;
			output.Changed = github_changed_domain::pull_request
				| github_changed_domain::status;
			output.AcceptedSnapshot = accepted_snapshot_delta{
				.Context     = aEvent.Context,
				.Repository  = aEvent.Repository,
				.PullRequest = aEvent.PullRequest,
				.Snapshot    = aEvent.Snapshot,
			};
			output.PullRequest = pull_request_summary_delta{ .Summary = summary(aState) };
			output.Status      = github_status_delta{ .Status = github_status::pull_request_ready };
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto pull_request_failed(
			github_state&                       aState,
			const pull_request_request_context& aContext,
			const github_failure                aFailure) -> github_transition_output
		{
			if (!current_pull_request(aState, aContext)) {
				return reject(github_reason_code::stale_completion);
			}

			aState.PendingPullRequest.reset();
			auto output    = reject(reason_for(aFailure));
			output.Status  = github_status_delta{ .Status = github_status::pull_request_failed };
			output.Changed = output.Changed | github_changed_domain::status;
			// [[BPR-1200]]. A read with no accepted review behind it refused the
			// pull request it named, on this connection. A refresh expects the
			// review it already has and keeps it, so it refuses nothing here.
			if (!aContext.ExpectedPullRequest.has_value()) {
				output.Refusal = pull_request_refusal{
					.Reason     = reason_for(aFailure),
					.Connection = aContext.Mode == github_access_mode::public_preview
						? refused_connection::signed_out
						: refused_connection::account,
					.Repository = aContext.Repository,
					.Number     = aContext.Number,
				};
			}
			publish_commands(aState, output);
			return output;
		}

		[[nodiscard]] auto pull_request_cancelled(
			github_state&                       aState,
			const pull_request_request_context& aContext) -> github_transition_output
		{
			if (!current_pull_request(aState, aContext)) {
				return reject(github_reason_code::stale_completion);
			}

			aState.PendingPullRequest.reset();
			auto output    = reject(github_reason_code::cancelled);
			output.Status  = github_status_delta{ .Status = github_status::pull_request_cancelled };
			output.Changed = output.Changed | github_changed_domain::status;
			publish_commands(aState, output);
			return output;
		}
	}

	auto github_commands(const github_state& aState)
		-> std::vector<github_command_projection>
	{
		std::vector<github_command_projection> commands;
		commands.reserve(4U);
		commands.push_back(
			command(github_command_id::activate_host_profile, true));

		if (aState.PendingSession.has_value()) {
			commands.push_back(command(
				github_command_id::open_pull_request,
				false,
				github_reason_code::session_in_progress));
		} else if (!aState.Host.has_value() || !aState.Account.has_value()) {
			commands.push_back(command(
				github_command_id::open_pull_request,
				false,
				github_reason_code::session_required));
		} else if (aState.PendingPullRequest.has_value()) {
			commands.push_back(command(
				github_command_id::open_pull_request,
				false,
				github_reason_code::pull_request_load_in_progress));
		} else {
			commands.push_back(
				command(github_command_id::open_pull_request, true));
		}

		if (aState.PendingPullRequest.has_value()) {
			commands.push_back(command(
				github_command_id::refresh_pull_request,
				false,
				github_reason_code::pull_request_load_in_progress));
		} else if (!aState.AcceptedSnapshot.has_value()) {
			commands.push_back(command(
				github_command_id::refresh_pull_request,
				false,
				github_reason_code::no_active_pull_request));
		} else {
			commands.push_back(
				command(github_command_id::refresh_pull_request, true));
		}

		if (!aState.AcceptedSnapshot.has_value()) {
			commands.push_back(command(
				github_command_id::show_review_threads,
				false,
				github_reason_code::no_active_pull_request));
		} else if (
			aState.Capabilities.ReviewThreads
			== github_capability_state::unsupported) {
			commands.push_back(command(
				github_command_id::show_review_threads,
				false,
				github_reason_code::capability_unsupported));
		} else if (
			aState.Capabilities.ReviewThreads
			== github_capability_state::supported) {
			commands.push_back(
				command(github_command_id::show_review_threads, true));
		} else {
			commands.push_back(command(
				github_command_id::show_review_threads,
				false,
				github_reason_code::capability_unknown));
		}

		return commands;
	}

	auto transition(
		github_state&       aState,
		const github_event& aEvent) -> github_transition_output
	{
		return std::visit(
			[&aState](const auto& aTypedEvent) -> github_transition_output {
				using event_type = std::decay_t<decltype(aTypedEvent)>;
				if constexpr (
					std::is_same_v<
						event_type,
						activate_host_profile_intent>) {
					return activate_profile(aState, aTypedEvent);
				} else if constexpr (
					std::is_same_v<
						event_type,
						deactivate_github_identity_intent>) {
					return deactivate_identity(aState);
				} else if constexpr (
					std::is_same_v<event_type, open_pull_request_intent>) {
					return open_pull_request(aState, aTypedEvent);
				} else if constexpr (
					std::is_same_v<event_type, refresh_pull_request_intent>) {
					return refresh_pull_request(aState);
				} else if constexpr (
					std::is_same_v<event_type, github_session_ready_event>) {
					return session_ready(aState, aTypedEvent);
				} else if constexpr (
					std::is_same_v<event_type, github_session_failed_event>) {
					return session_failed(
						aState,
						aTypedEvent.Context,
						aTypedEvent.Failure);
				} else if constexpr (
					std::is_same_v<event_type, github_session_cancelled_event>) {
					return session_cancelled(aState, aTypedEvent.Context);
				} else if constexpr (
					std::is_same_v<event_type, pull_request_loaded_event>) {
					return pull_request_loaded(aState, aTypedEvent);
				} else if constexpr (
					std::is_same_v<
						event_type,
						pull_request_load_failed_event>) {
					return pull_request_failed(
						aState,
						aTypedEvent.Context,
						aTypedEvent.Failure);
				} else {
					return pull_request_cancelled(aState, aTypedEvent.Context);
				}
			},
			aEvent);
	}
}
