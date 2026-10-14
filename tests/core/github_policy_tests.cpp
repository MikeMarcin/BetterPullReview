#include "bpr/core/github_transition.hpp"

#include "bpr/core/anonymous_read_policy.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto initial_state() -> github_state
	{
		return github_state{
			.ApplicationSession    = application_session_id{ 41 },
			.NextRequest           = request_id{ 1 },
			.ActiveProfile         = { },
			.IdentityGeneration    = { },
			.PendingSession        = { },
			.Host                  = { },
			.Account               = { },
			.Capabilities          = { },
			.CapabilityRevision    = { },
			.PullRequestGeneration = { },
			.PendingPullRequest    = { },
			.ActiveLocator         = { },
			.ActiveNumber          = { },
			.Repository            = { },
			.PullRequest           = { },
			.AcceptedSnapshot      = { },
		};
	}

	[[nodiscard]] auto locator(
		std::string aOwner = "octo-org",
		std::string aName  = "review-repository") -> repository_locator
	{
		return repository_locator{
			.Owner = std::move(aOwner),
			.Name  = std::move(aName),
		};
	}

	[[nodiscard]] auto all_supported() -> github_capabilities
	{
		return github_capabilities{
			.ReviewThreads       = github_capability_state::supported,
			.ViewedFileMutations = github_capability_state::supported,
			.ResolveThread       = github_capability_state::supported,
			.UnresolveThread     = github_capability_state::supported,
		};
	}

	[[nodiscard]] auto session_context(const github_transition_output& aOutput)
		-> github_session_request_context
	{
		expect(aOutput.Requests.size() == 2U, "activation emits two requests");
		expect(
			std::holds_alternative<invalidate_github_identity_request>(
				aOutput.Requests[0]),
			"identity invalidation precedes session establishment");
		const auto* request = std::get_if<establish_github_session_request>(&aOutput.Requests[1]);
		expect(request != nullptr, "activation emits a typed session request");
		expect(std::get<invalidate_github_identity_request>(aOutput.Requests[0]).Request == request->Context.Request,
			   "activation cleanup reuses its core-owned session request identity");
		return request->Context;
	}

	[[nodiscard]] auto fetch_context(const github_transition_output& aOutput)
		-> pull_request_request_context
	{
		expect(aOutput.Requests.size() == 1U, "open or refresh emits one request");
		const auto* request = std::get_if<fetch_pull_request_request>(&aOutput.Requests.front());
		expect(request != nullptr, "request is a typed read-only PR fetch");
		return request->Context;
	}

	[[nodiscard]] auto command_projection(
		const std::vector<github_command_projection>& aCommands,
		const github_command_id                       aId) -> github_command_projection
	{
		for (const auto& projection : aCommands) {
			if (projection.Id == aId) {
				return projection;
			}
		}

		expect(false, "expected command projection is present");
		return { };
	}

	auto activate_and_ready(
		github_state&             aState,
		const host_profile_id     aProfile      = host_profile_id{ 11 },
		const git_host_id         aHost         = git_host_id{ 21 },
		const github_account_id   aAccount      = github_account_id{ 31 },
		const github_capabilities aCapabilities = all_supported(),
		const capability_revision aRevision     = capability_revision{ 41 }) -> void
	{
		const auto activation = transition(aState, activate_host_profile_intent{ .Profile = aProfile });
		const auto context    = session_context(activation);
		const auto ready      = transition(
			aState,
			github_session_ready_event{
				.Context            = context,
				.Host               = aHost,
				.Account            = aAccount,
				.Capabilities       = aCapabilities,
				.CapabilityRevision = aRevision,
			});
		expect(ready.Diagnostics.empty(), "session fixture becomes ready");
	}

	[[nodiscard]] auto open_request(
		github_state&             aState,
		repository_locator        aRepository = locator(),
		const pull_request_number aNumber     = pull_request_number{ 17 })
		-> pull_request_request_context
	{
		return fetch_context(transition(
			aState,
			open_pull_request_intent{
				.Repository = std::move(aRepository),
				.Number     = aNumber,
			}));
	}

	auto load(
		github_state&                       aState,
		const pull_request_request_context& aContext,
		const repository_id                 aRepository  = repository_id{ 51 },
		const pull_request_id               aPullRequest = pull_request_id{ 61 },
		const provider_snapshot_id          aSnapshot    = provider_snapshot_id{ 71 })
		-> github_transition_output
	{
		return transition(
			aState,
			pull_request_loaded_event{
				.Context     = aContext,
				.Repository  = aRepository,
				.PullRequest = aPullRequest,
				.Snapshot    = aSnapshot,
			});
	}

	[[nodiscard]] auto loaded_state(
		const github_capabilities aCapabilities = all_supported()) -> github_state
	{
		auto state = initial_state();
		activate_and_ready(state, host_profile_id{ 11 }, git_host_id{ 21 },
						   github_account_id{ 31 }, aCapabilities);
		const auto context = open_request(state);
		const auto output  = load(state, context);
		expect(output.AcceptedSnapshot.has_value(), "fixture snapshot is accepted");
		return state;
	}

	auto expect_stale(const github_transition_output& aOutput) -> void
	{
		expect(
			aOutput.Diagnostics
				== std::vector{
					github_diagnostic{
						.Code = github_reason_code::stale_completion,
					} },
			"mismatched completion reports only stale identity");
		expect(
			!aOutput.AcceptedSnapshot.has_value(),
			"stale completion emits no snapshot acceptance");
		expect(
			!aOutput.AcceptedSession.has_value(),
			"stale completion emits no session acceptance");
	}

	auto test_strong_types_and_compact_state() -> void
	{
		static_assert(!std::is_same_v<host_profile_id, git_host_id>);
		static_assert(!std::is_same_v<git_host_id, github_account_id>);
		static_assert(!std::is_same_v<repository_id, pull_request_id>);
		static_assert(!std::is_same_v<pull_request_id, provider_snapshot_id>);
		static_assert(
			!std::is_same_v<capability_revision, request_generation>);
		static_assert(sizeof(github_state) <= 512U);
		static_assert(std::variant_size_v<github_host_request> == 3U);

		expect(valid(locator()), "bounded repository locator is valid");
		expect(!valid(locator("", "repository")), "empty owner is rejected");
		expect(!valid(locator("owner", "bad/name")), "slash is rejected");
		expect(!valid(locator(".", "repository")), "dot owner is rejected");
		expect(!valid(locator("owner", "..")), "dot-dot repository is rejected");
		expect(
			!valid(locator(
				std::string(maximum_repository_owner_length + 1U, 'a'),
				"repository")),
			"oversized owner is rejected");
		expect(!pull_request_number{ }.valid(), "zero PR number is invalid");
		expect(
			pull_request_number{ maximum_pull_request_number }.valid(),
			"maximum GraphQL-compatible PR number is valid");
		expect(
			!pull_request_number{ maximum_pull_request_number + 1U }.valid(),
			"PR number above GraphQL Int range is invalid");
	}

	auto test_profile_activation_and_session_acceptance() -> void
	{
		auto       state  = initial_state();
		const auto output = transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 11 } });
		const auto context = session_context(output);

		expect(
			state.IdentityGeneration == github_identity_generation{ 1 },
			"activation advances identity generation");
		expect(context.Request == request_id{ 1 }, "request ID is deterministic");
		expect(
			context.ApplicationSession == state.ApplicationSession
				&& context.Profile == host_profile_id{ 11 }
				&& context.IdentityGeneration == state.IdentityGeneration,
			"session request carries complete correlation");
		expect(
			state.Capabilities == github_capabilities{ },
			"activation resets every capability to unknown");
		expect(
			output.Identity.has_value() && output.Capabilities.has_value()
				&& output.PullRequest.has_value()
				&& !output.PullRequest->Summary.has_value(),
			"activation clears identity-sensitive projections");
		const auto open = command_projection(
			output.Commands,
			github_command_id::open_pull_request);
		expect(
			!open.Enabled
				&& open.DisabledReason == github_reason_code::session_in_progress,
			"open is disabled while session establishment is pending");

		const auto connect = command_projection(output.Commands, github_command_id::activate_host_profile);
		expect(!connect.Enabled && connect.DisabledReason == github_reason_code::session_in_progress,
			   "connection setup is unavailable while a session is pending");
		const auto beforeReentry = state;
		const auto reentry       = transition(state, activate_host_profile_intent{ .Profile = host_profile_id{ 11 } });
		expect(state == beforeReentry && reentry.Requests.empty()
				   && reentry.Diagnostics == std::vector{ github_diagnostic{ .Code = github_reason_code::session_in_progress } },
			   "repeated activation preserves the exact pending request and identity");

		const auto ready = transition(
			state,
			github_session_ready_event{
				.Context            = context,
				.Host               = git_host_id{ 21 },
				.Account            = github_account_id{ 31 },
				.Capabilities       = all_supported(),
				.CapabilityRevision = capability_revision{ 41 },
			});
		expect(
			state.Host == git_host_id{ 21 }
				&& state.Account == github_account_id{ 31 },
			"matching completion establishes exact host and account");
		expect(
			state.Capabilities == all_supported()
				&& state.CapabilityRevision == capability_revision{ 41 },
			"matching completion applies exact capability facts");
		expect(
			ready.AcceptedSession
				== accepted_session_delta{
					.Context            = context,
					.Host               = git_host_id{ 21 },
					.Account            = github_account_id{ 31 },
					.Capabilities       = all_supported(),
					.CapabilityRevision = capability_revision{ 41 },
				},
			"matching completion authorizes the exact staged session");
		expect(
			command_projection(
				ready.Commands,
				github_command_id::open_pull_request)
				.Enabled,
			"ready session enables read-only open");
	}

	auto test_deactivation_invalidates_without_reestablishing() -> void
	{
		auto       state              = loaded_state();
		const auto previousGeneration = state.IdentityGeneration;
		const auto nextRequest        = state.NextRequest;
		const auto output             = transition(state, deactivate_github_identity_intent{ });
		expect(std::get<invalidate_github_identity_request>(output.Requests.front()).Request == nextRequest
				   && state.NextRequest.value() == nextRequest.value() + 1U,
			   "deactivation allocates a cleanup request without establishing a session");

		expect(
			state.IdentityGeneration
				== github_identity_generation{
					previousGeneration.value() + 1U },
			"deactivation advances identity generation");
		expect(
			!state.ActiveProfile.has_value() && !state.PendingSession.has_value()
				&& !state.Host.has_value() && !state.Account.has_value()
				&& !state.PendingPullRequest.has_value()
				&& !state.AcceptedSnapshot.has_value(),
			"deactivation clears every identity-sensitive core field");
		expect(
			output.Requests.size() == 1U
				&& std::holds_alternative<invalidate_github_identity_request>(
					output.Requests.front()),
			"deactivation emits invalidation without session establishment");
		expect(
			output.Status
					== github_status_delta{ .Status = github_status::no_profile }
				&& output.PullRequest.has_value()
				&& !output.PullRequest->Summary.has_value()
				&& !output.AcceptedSession.has_value()
				&& !output.AcceptedSnapshot.has_value(),
			"deactivation projects a terminal empty identity");
	}

	auto assert_session_context_is_stale(
		const github_session_request_context& aStale) -> void
	{
		auto       state   = initial_state();
		const auto current = session_context(transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 11 } }));
		const auto before  = state;
		const auto output  = transition(
			state,
			github_session_ready_event{
				.Context            = aStale,
				.Host               = git_host_id{ 21 },
				.Account            = github_account_id{ 31 },
				.Capabilities       = all_supported(),
				.CapabilityRevision = capability_revision{ 41 },
			});

		expect(state == before, "stale session completion cannot mutate state");
		expect(state.PendingSession == current, "current session remains pending");
		expect_stale(output);
	}

	auto test_every_session_identity_dimension_and_duplicate_are_rejected()
		-> void
	{
		const github_session_request_context current{
			.Request            = request_id{ 1 },
			.ApplicationSession = application_session_id{ 41 },
			.Profile            = host_profile_id{ 11 },
			.IdentityGeneration = github_identity_generation{ 1 },
		};

		auto stale    = current;
		stale.Request = request_id{ 99 };
		assert_session_context_is_stale(stale);
		stale                    = current;
		stale.ApplicationSession = application_session_id{ 99 };
		assert_session_context_is_stale(stale);
		stale         = current;
		stale.Profile = host_profile_id{ 99 };
		assert_session_context_is_stale(stale);
		stale                    = current;
		stale.IdentityGeneration = github_identity_generation{ 99 };
		assert_session_context_is_stale(stale);

		auto                             state   = initial_state();
		const auto                       context = session_context(transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 11 } }));
		const github_session_ready_event completion{
			.Context            = context,
			.Host               = git_host_id{ 21 },
			.Account            = github_account_id{ 31 },
			.Capabilities       = all_supported(),
			.CapabilityRevision = capability_revision{ 41 },
		};
		static_cast<void>(transition(state, completion));
		const auto before = state;
		expect_stale(transition(state, completion));
		expect(state == before, "duplicate session completion is inert");
	}

	auto test_capability_tristate_property_and_no_mutation_surface() -> void
	{
		constexpr std::array states{
			github_capability_state::unknown,
			github_capability_state::supported,
			github_capability_state::unsupported,
		};

		for (const auto reviewThreads : states) {
			for (const auto viewed : states) {
				for (const auto resolve : states) {
					for (const auto unresolve : states) {
						const github_capabilities capabilities{
							.ReviewThreads       = reviewThreads,
							.ViewedFileMutations = viewed,
							.ResolveThread       = resolve,
							.UnresolveThread     = unresolve,
						};
						auto state = loaded_state(capabilities);

						expect(
							capability_state(
								state.Capabilities,
								github_capability::review_threads)
								== reviewThreads,
							"review-thread capability is independent");
						expect(
							capability_state(
								state.Capabilities,
								github_capability::viewed_file_mutations)
								== viewed,
							"viewed-state capability is independent");
						expect(
							capability_state(
								state.Capabilities,
								github_capability::resolve_thread)
								== resolve,
							"resolve capability is independent");
						expect(
							capability_state(
								state.Capabilities,
								github_capability::unresolve_thread)
								== unresolve,
							"unresolve capability is independent");

						const auto threads = command_projection(
							github_commands(state),
							github_command_id::show_review_threads);
						if (reviewThreads == github_capability_state::supported) {
							expect(
								threads.Enabled,
								"supported read capability enables its command");
						} else {
							expect(
								!threads.Enabled,
								"unknown and unsupported do not enable command");
							expect(
								threads.DisabledReason
									== (reviewThreads
												== github_capability_state::unknown
											? github_reason_code::
												  capability_unknown
											: github_reason_code::
												  capability_unsupported),
								"unknown remains distinct from unsupported");
						}
					}
				}
			}
		}

		expect(
			static_cast<std::uint8_t>(
				github_command_id::show_review_threads)
				== 3U,
			"core exposes only profile/open/refresh/read-thread commands");
	}

	auto test_malformed_capability_value_fails_closed() -> void
	{
		for (std::uint8_t dimension = 0; dimension < 4U; ++dimension) {
			auto       state        = initial_state();
			const auto context      = session_context(transition(
				state,
				activate_host_profile_intent{ .Profile = host_profile_id{ 11 } }));
			auto       capabilities = all_supported();
			const auto malformed    = static_cast<github_capability_state>(255U);
			switch (dimension) {
			case 0:
				capabilities.ReviewThreads = malformed;
				break;
			case 1:
				capabilities.ViewedFileMutations = malformed;
				break;
			case 2:
				capabilities.ResolveThread = malformed;
				break;
			default:
				capabilities.UnresolveThread = malformed;
				break;
			}

			const auto output = transition(
				state,
				github_session_ready_event{
					.Context            = context,
					.Host               = git_host_id{ 21 },
					.Account            = github_account_id{ 31 },
					.Capabilities       = capabilities,
					.CapabilityRevision = capability_revision{ 41 },
				});

			expect(
				output.Diagnostics
					== std::vector{
						github_diagnostic{
							.Code = github_reason_code::malformed_response,
						} },
				"unknown capability representation is rejected as malformed");
			expect(
				!state.Host.has_value() && !state.Account.has_value()
					&& state.Capabilities == github_capabilities{ },
				"malformed capability cannot partially establish identity");
			expect(
				!command_projection(
					 output.Commands,
					 github_command_id::show_review_threads)
					 .Enabled,
				"malformed capability value fails closed");
		}
	}

	auto test_open_legality_and_context() -> void
	{
		auto       state           = initial_state();
		const auto noActiveRefresh = transition(state, refresh_pull_request_intent{ });
		expect(
			noActiveRefresh.Requests.empty()
				&& noActiveRefresh.Diagnostics.front().Code
					== github_reason_code::no_active_pull_request,
			"refresh is illegal without an accepted pull request");

		const auto beforeSession = transition(
			state,
			open_pull_request_intent{
				.Repository = locator(),
				.Number     = pull_request_number{ 17 },
			});
		expect(
			beforeSession.Requests.empty()
				&& beforeSession.Diagnostics.front().Code
					== github_reason_code::session_required,
			"open requires an accepted session");

		static_cast<void>(transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 11 } }));
		const auto duringSession = transition(
			state,
			open_pull_request_intent{
				.Repository = locator(),
				.Number     = pull_request_number{ 17 },
			});
		expect(
			duringSession.Diagnostics.front().Code
				== github_reason_code::session_in_progress,
			"open reports in-progress session consistently with command policy");

		state = initial_state();
		activate_and_ready(state);
		const auto invalidRepository = transition(
			state,
			open_pull_request_intent{
				.Repository = locator("", "repository"),
				.Number     = pull_request_number{ 17 },
			});
		expect(
			invalidRepository.Requests.empty()
				&& invalidRepository.Diagnostics.front().Code
					== github_reason_code::invalid_repository,
			"invalid locator emits no request");
		const auto dotSegment = transition(
			state,
			open_pull_request_intent{
				.Repository = locator("owner", ".."),
				.Number     = pull_request_number{ 17 },
			});
		expect(
			dotSegment.Requests.empty()
				&& dotSegment.Diagnostics.front().Code
					== github_reason_code::invalid_repository,
			"dot-segment locator emits no request");
		const auto invalidNumber = transition(
			state,
			open_pull_request_intent{
				.Repository = locator(),
				.Number     = pull_request_number{ },
			});
		expect(
			invalidNumber.Requests.empty()
				&& invalidNumber.Diagnostics.front().Code
					== github_reason_code::invalid_pull_request_number,
			"invalid PR number emits no request");

		const auto context = open_request(state);
		expect(
			context.ApplicationSession == state.ApplicationSession
				&& context.Profile == *state.ActiveProfile
				&& context.Host == *state.Host
				&& context.Account == *state.Account
				&& context.Repository == locator()
				&& context.Number == pull_request_number{ 17 }
				&& context.Capabilities == state.CapabilityRevision
				&& context.IdentityGeneration == state.IdentityGeneration
				&& context.Generation == state.PullRequestGeneration,
			"fetch request carries every active identity dimension");
		expect(
			!context.ExpectedRepository.has_value()
				&& !context.ExpectedPullRequest.has_value(),
			"initial open has no unproven canonical identity");

		const auto duplicate = transition(
			state,
			open_pull_request_intent{
				.Repository = locator("other", "repository"),
				.Number     = pull_request_number{ 18 },
			});
		expect(
			duplicate.Requests.empty()
				&& duplicate.Diagnostics.front().Code
					== github_reason_code::pull_request_load_in_progress,
			"conflicting open has explicit deterministic policy");
	}

	auto test_opening_a_new_target_clears_the_previous_snapshot() -> void
	{
		auto       state  = loaded_state();
		const auto output = transition(
			state,
			open_pull_request_intent{
				.Repository = locator("other-owner", "other-repository"),
				.Number     = pull_request_number{ 18 },
			});

		const auto context = fetch_context(output);
		expect(
			context.Repository
					== locator("other-owner", "other-repository")
				&& context.Number == pull_request_number{ 18 },
			"new open request is bound to the new semantic target");
		expect(
			!state.Repository.has_value() && !state.PullRequest.has_value()
				&& !state.AcceptedSnapshot.has_value()
				&& output.PullRequest.has_value()
				&& !output.PullRequest->Summary.has_value(),
			"old snapshot is not presented as current for a new target");
	}

	auto state_with_pending_open(
		pull_request_request_context& aContext) -> github_state
	{
		auto state = initial_state();
		activate_and_ready(state);
		aContext = open_request(state);
		return state;
	}

	auto assert_fetch_context_is_stale(
		const pull_request_request_context& aStale) -> void
	{
		pull_request_request_context current;
		auto                         state  = state_with_pending_open(current);
		const auto                   before = state;
		const auto                   output = load(
			state,
			aStale,
			repository_id{ 51 },
			pull_request_id{ 61 },
			provider_snapshot_id{ 71 });
		expect(state == before, "stale fetch completion cannot mutate state");
		expect(
			state.PendingPullRequest == current,
			"stale fetch cannot clear current work");
		expect_stale(output);
	}

	auto test_every_fetch_context_dimension_is_checked() -> void
	{
		pull_request_request_context current;
		static_cast<void>(state_with_pending_open(current));
		auto stale    = current;
		stale.Request = request_id{ 99 };
		assert_fetch_context_is_stale(stale);
		stale                    = current;
		stale.ApplicationSession = application_session_id{ 99 };
		assert_fetch_context_is_stale(stale);
		stale         = current;
		stale.Profile = host_profile_id{ 99 };
		assert_fetch_context_is_stale(stale);
		stale      = current;
		stale.Host = git_host_id{ 99 };
		assert_fetch_context_is_stale(stale);
		stale         = current;
		stale.Account = github_account_id{ 99 };
		assert_fetch_context_is_stale(stale);
		stale            = current;
		stale.Repository = locator("different", "repository");
		assert_fetch_context_is_stale(stale);
		stale        = current;
		stale.Number = pull_request_number{ 99 };
		assert_fetch_context_is_stale(stale);
		stale                    = current;
		stale.ExpectedRepository = repository_id{ 99 };
		assert_fetch_context_is_stale(stale);
		stale                     = current;
		stale.ExpectedPullRequest = pull_request_id{ 99 };
		assert_fetch_context_is_stale(stale);
		stale              = current;
		stale.Capabilities = capability_revision{ 99 };
		assert_fetch_context_is_stale(stale);
		stale                    = current;
		stale.IdentityGeneration = github_identity_generation{ 99 };
		assert_fetch_context_is_stale(stale);
		stale            = std::move(current);
		stale.Generation = request_generation{ 99 };
		assert_fetch_context_is_stale(stale);
	}

	auto test_exact_snapshot_acceptance_and_duplicate_rejection() -> void
	{
		pull_request_request_context context;
		auto                         state  = state_with_pending_open(context);
		const auto                   output = load(state, context);
		expect(
			output.AcceptedSnapshot
				== accepted_snapshot_delta{
					.Context     = context,
					.Repository  = repository_id{ 51 },
					.PullRequest = pull_request_id{ 61 },
					.Snapshot    = provider_snapshot_id{ 71 },
				},
			"acceptance delta names the exact staged snapshot and context");
		expect(
			state.Repository == repository_id{ 51 }
				&& state.PullRequest == pull_request_id{ 61 }
				&& state.AcceptedSnapshot == provider_snapshot_id{ 71 }
				&& !state.PendingPullRequest.has_value(),
			"matching completion atomically updates accepted identities");
		expect(
			output.PullRequest.has_value()
				&& output.PullRequest->Summary.has_value()
				&& output.PullRequest->Summary->Snapshot
					== provider_snapshot_id{ 71 },
			"accepted completion emits a bounded summary");

		const auto before = state;
		expect_stale(load(state, context));
		expect(state == before, "duplicate loaded event is inert");
	}

	auto test_invalid_loaded_identity_never_authorizes_snapshot() -> void
	{
		pull_request_request_context context;
		auto                         state  = state_with_pending_open(context);
		const auto                   output = load(
			state,
			context,
			repository_id{ 51 },
			pull_request_id{ 61 },
			provider_snapshot_id{ });

		expect(
			!output.AcceptedSnapshot.has_value()
				&& output.Diagnostics.front().Code
					== github_reason_code::completion_identity_mismatch,
			"default snapshot identity is rejected");
		expect(
			!state.Repository.has_value() && !state.PullRequest.has_value()
				&& !state.AcceptedSnapshot.has_value()
				&& !state.PendingPullRequest.has_value(),
			"invalid loaded identity cannot partially update authority");
	}

	auto test_refresh_identity_and_canonical_mismatch() -> void
	{
		auto       state       = loaded_state();
		const auto oldSnapshot = state.AcceptedSnapshot;
		const auto refresh     = fetch_context(
			transition(state, refresh_pull_request_intent{ }));
		expect(
			refresh.ExpectedRepository == repository_id{ 51 }
				&& refresh.ExpectedPullRequest == pull_request_id{ 61 },
			"refresh binds the immutable accepted canonical target");
		expect(
			state.AcceptedSnapshot == oldSnapshot,
			"accepted snapshot stays active during refresh");

		const auto mismatch = load(
			state,
			refresh,
			repository_id{ 999 },
			pull_request_id{ 61 },
			provider_snapshot_id{ 72 });
		expect(
			!mismatch.AcceptedSnapshot.has_value()
				&& mismatch.Diagnostics.front().Code
					== github_reason_code::completion_identity_mismatch,
			"canonical repository mismatch cannot authorize commit");
		expect(
			state.AcceptedSnapshot == oldSnapshot
				&& state.Repository == repository_id{ 51 }
				&& state.PullRequest == pull_request_id{ 61 },
			"canonical mismatch preserves the prior accepted snapshot");

		state                    = loaded_state();
		const auto secondRefresh = fetch_context(transition(state, refresh_pull_request_intent{ }));
		const auto prMismatch    = load(
			state,
			secondRefresh,
			repository_id{ 51 },
			pull_request_id{ 999 },
			provider_snapshot_id{ 72 });
		expect(
			!prMismatch.AcceptedSnapshot.has_value()
				&& state.PullRequest == pull_request_id{ 61 },
			"canonical PR mismatch cannot replace authority");
	}

	auto test_failure_categories_and_refresh_preservation() -> void
	{
		constexpr std::array failures{
			std::pair{
				github_failure::authentication,
				github_reason_code::authentication_failed },
			std::pair{
				github_failure::permission,
				github_reason_code::permission_denied },
			std::pair{
				github_failure::network,
				github_reason_code::network_unavailable },
			std::pair{
				github_failure::api_version,
				github_reason_code::api_version_unsupported },
			std::pair{
				github_failure::capability,
				github_reason_code::capability_unavailable },
			std::pair{
				github_failure::rate_limit,
				github_reason_code::rate_limited },
			std::pair{
				github_failure::malformed_response,
				github_reason_code::malformed_response },
			std::pair{
				github_failure::response_too_large,
				github_reason_code::response_too_large },
			std::pair{
				github_failure::vault,
				github_reason_code::vault_unavailable },
			std::pair{
				github_failure::certificate,
				github_reason_code::certificate_rejected },
			std::pair{
				github_failure::redirect,
				github_reason_code::redirect_rejected },
			std::pair{
				github_failure::timeout,
				github_reason_code::timed_out },
			std::pair{
				github_failure::cli_unavailable,
				github_reason_code::cli_unavailable },
		};

		for (const auto& [failure, reason] : failures) {
			auto       state    = loaded_state();
			const auto previous = state;
			const auto context  = fetch_context(transition(state, refresh_pull_request_intent{ }));
			const auto output   = transition(
				state,
				pull_request_load_failed_event{
					.Context = context,
					.Failure = failure,
				});
			expect(
				output.Diagnostics
					== std::vector{ github_diagnostic{ .Code = reason } },
				"failure maps to one typed safe reason");
			expect(
				!output.AcceptedSnapshot.has_value(),
				"failure never emits acceptance authority");
			expect(
				state.Repository == previous.Repository
					&& state.PullRequest == previous.PullRequest
					&& state.AcceptedSnapshot == previous.AcceptedSnapshot,
				"failed refresh preserves accepted authority");
			expect(
				!state.PendingPullRequest.has_value(),
				"matching failure clears only matching work");
			expect(
				!output.Refusal.has_value(),
				"a failed refresh keeps its review and refuses nothing");
		}

		auto       state     = loaded_state();
		const auto context   = fetch_context(transition(state, refresh_pull_request_intent{ }));
		const auto cancelled = transition(
			state,
			pull_request_load_cancelled_event{ .Context = context });
		expect(
			cancelled.Diagnostics.front().Code == github_reason_code::cancelled
				&& cancelled.Status
					== github_status_delta{
						.Status = github_status::pull_request_cancelled,
					}
				&& !cancelled.AcceptedSnapshot.has_value(),
			"PR cancellation is a visible normal state and is non-accepting");
	}

	// [[BPR-1200]]. A first read that fails refuses the pull request it named,
	// on the connection it was read through, whatever the reason.
	auto test_first_read_failure_names_the_refused_pull_request() -> void
	{
		auto account = initial_state();
		activate_and_ready(account);
		const auto denied = transition(
			account,
			pull_request_load_failed_event{
				.Context = open_request(account, locator("octo-org", "inaccessible-repository")),
				.Failure = github_failure::permission,
			});
		expect(
			denied.Refusal
				== pull_request_refusal{
					.Reason     = github_reason_code::permission_denied,
					.Connection = refused_connection::account,
					.Repository = locator("octo-org", "inaccessible-repository"),
					.Number     = pull_request_number{ 17 },
				},
			"an account's first read refuses the named pull request on that account");

		const auto unreachable = transition(
			account,
			pull_request_load_failed_event{
				.Context = open_request(account),
				.Failure = github_failure::network,
			});
		expect(
			unreachable.Refusal.has_value()
				&& unreachable.Refusal->Reason == github_reason_code::network_unavailable,
			"the refusal carries the typed reason, not only a permission denial");

		auto       signedOut  = initial_state();
		const auto activation = transition(
			signedOut,
			activate_host_profile_intent{
				.Profile = host_profile_id{ 11 },
				.Mode    = github_access_mode::public_preview,
			});
		const auto ready = transition(
			signedOut,
			github_session_ready_event{
				.Context            = session_context(activation),
				.Host               = git_host_id{ 21 },
				.Account            = github_account_id{ 0 },
				.Capabilities       = all_supported(),
				.CapabilityRevision = capability_revision{ 41 },
			});
		expect(ready.Diagnostics.empty(), "a signed-out session is ready");
		const auto hidden = transition(
			signedOut,
			pull_request_load_failed_event{
				.Context = open_request(signedOut, locator("octo-org", "private-repository"), pull_request_number{ 9 }),
				.Failure = github_failure::permission,
			});
		expect(
			hidden.Refusal
				== pull_request_refusal{
					.Reason     = github_reason_code::permission_denied,
					.Connection = refused_connection::signed_out,
					.Repository = locator("octo-org", "private-repository"),
					.Number     = pull_request_number{ 9 },
				},
			"a signed-out first read is refused as signed out");

		auto cancelling = initial_state();
		activate_and_ready(cancelling);
		const auto cancelled = transition(
			cancelling,
			pull_request_load_cancelled_event{ .Context = open_request(cancelling) });
		expect(!cancelled.Refusal.has_value(), "a cancelled first read is not a refusal");
	}

	auto test_stale_failure_and_profile_switch_invalidation() -> void
	{
		auto       state          = loaded_state();
		const auto oldProfile     = state.ActiveProfile;
		const auto oldHost        = state.Host;
		const auto oldAccount     = state.Account;
		const auto oldRepository  = state.Repository;
		const auto oldPullRequest = state.PullRequest;
		const auto oldSnapshot    = state.AcceptedSnapshot;
		const auto oldGeneration  = state.IdentityGeneration;
		const auto refresh        = fetch_context(transition(state, refresh_pull_request_intent{ }));

		auto staleFailureContext       = refresh;
		staleFailureContext.Generation = request_generation{ 999 };
		const auto beforeStale         = state;
		expect_stale(transition(
			state,
			pull_request_load_failed_event{
				.Context = staleFailureContext,
				.Failure = github_failure::network,
			}));
		expect(state == beforeStale, "stale failure cannot clear current work");

		const auto switchOutput = transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 12 } });
		const auto  nextSession  = session_context(switchOutput);
		const auto* invalidation = std::get_if<invalidate_github_identity_request>(
			&switchOutput.Requests.front());
		expect(invalidation != nullptr, "switch emits explicit invalidation first");
		expect(
			invalidation->Previous.Profile == oldProfile
				&& invalidation->Previous.Host == oldHost
				&& invalidation->Previous.Account == oldAccount
				&& invalidation->Previous.Repository == oldRepository
				&& invalidation->Previous.PullRequest == oldPullRequest
				&& invalidation->Previous.Snapshot == oldSnapshot,
			"invalidation identifies the complete previous authority");
		expect(
			state.IdentityGeneration
					== github_identity_generation{ oldGeneration.value() + 1U }
				&& nextSession.Request.value() > refresh.Request.value(),
			"switch advances identity before effects and preserves request monotonicity");
		expect(
			!state.Host.has_value() && !state.Account.has_value()
				&& !state.Repository.has_value() && !state.PullRequest.has_value()
				&& !state.AcceptedSnapshot.has_value()
				&& state.Capabilities == github_capabilities{ },
			"switch immediately clears identity-sensitive state");

		const auto afterSwitch = state;
		expect_stale(load(
			state,
			refresh,
			repository_id{ 51 },
			pull_request_id{ 61 },
			provider_snapshot_id{ 72 }));
		expect(state == afterSwitch, "old completion cannot cross profile switch");
		expect_stale(transition(
			state,
			pull_request_load_failed_event{
				.Context = refresh,
				.Failure = github_failure::network,
			}));
		expect(state == afterSwitch, "old failure cannot cross profile switch");

		const auto sameProfileGeneration = state.IdentityGeneration;
		static_cast<void>(transition(
			state,
			github_session_revalidation_event{ .Profile = host_profile_id{ 12 } }));
		expect(
			state.IdentityGeneration
				== github_identity_generation{
					sameProfileGeneration.value() + 1U },
			"host credential revalidation renews possibly changed account identity");
	}

	auto test_successful_refresh_and_deterministic_replay() -> void
	{
		auto                               left  = initial_state();
		auto                               right = initial_state();
		const activate_host_profile_intent activation{
			.Profile = host_profile_id{ 11 },
		};
		const auto leftActivation  = transition(left, activation);
		const auto rightActivation = transition(right, activation);
		expect(
			left == right && leftActivation == rightActivation,
			"profile activation replay is deterministic");

		const github_session_ready_event ready{
			.Context            = session_context(leftActivation),
			.Host               = git_host_id{ 21 },
			.Account            = github_account_id{ 31 },
			.Capabilities       = all_supported(),
			.CapabilityRevision = capability_revision{ 41 },
		};
		const auto leftReady  = transition(left, ready);
		const auto rightReady = transition(right, ready);
		expect(
			left == right && leftReady == rightReady,
			"session completion replay is deterministic");

		const open_pull_request_intent open{
			.Repository = locator(),
			.Number     = pull_request_number{ 17 },
		};
		const auto leftOpen  = transition(left, open);
		const auto rightOpen = transition(right, open);
		expect(
			left == right && leftOpen == rightOpen,
			"open request replay is deterministic");

		const pull_request_loaded_event loaded{
			.Context     = fetch_context(leftOpen),
			.Repository  = repository_id{ 51 },
			.PullRequest = pull_request_id{ 61 },
			.Snapshot    = provider_snapshot_id{ 71 },
		};
		const auto leftLoaded  = transition(left, loaded);
		const auto rightLoaded = transition(right, loaded);
		expect(
			left == right && leftLoaded == rightLoaded,
			"initial snapshot acceptance replay is deterministic");

		const auto leftRequest  = transition(left, refresh_pull_request_intent{ });
		const auto rightRequest = transition(right, refresh_pull_request_intent{ });
		expect(
			left == right && leftRequest == rightRequest,
			"refresh request replay is deterministic");

		const auto                      context = fetch_context(leftRequest);
		const pull_request_loaded_event event{
			.Context     = context,
			.Repository  = repository_id{ 51 },
			.PullRequest = pull_request_id{ 61 },
			.Snapshot    = provider_snapshot_id{ 72 },
		};
		const auto leftOutput  = transition(left, event);
		const auto rightOutput = transition(right, event);
		expect(
			left == right && leftOutput == rightOutput,
			"accepted completion replay is deterministic");
		expect(
			left.AcceptedSnapshot == provider_snapshot_id{ 72 }
				&& leftOutput.AcceptedSnapshot->Snapshot
					== provider_snapshot_id{ 72 },
			"successful refresh replaces authority through exact acceptance");
	}

	auto test_session_failure_and_invalid_completion_are_safe() -> void
	{
		auto       state   = initial_state();
		auto       context = session_context(transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 11 } }));
		const auto failed  = transition(
			state,
			github_session_failed_event{
				.Context = context,
				.Failure = github_failure::authentication,
			});
		expect(
			!state.PendingSession.has_value()
				&& failed.Diagnostics.front().Code
					== github_reason_code::authentication_failed
				&& !failed.AcceptedSnapshot.has_value(),
			"matching session failure is typed and non-accepting");

		state              = initial_state();
		context            = session_context(transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 11 } }));
		const auto invalid = transition(
			state,
			github_session_ready_event{
				.Context            = context,
				.Host               = git_host_id{ },
				.Account            = github_account_id{ 31 },
				.Capabilities       = all_supported(),
				.CapabilityRevision = capability_revision{ 41 },
			});
		expect(
			invalid.Diagnostics.front().Code
					== github_reason_code::completion_identity_mismatch
				&& !state.Host.has_value() && !state.Account.has_value(),
			"invalid completion identity cannot partially establish session");

		state                           = initial_state();
		context                         = session_context(transition(
			state,
			activate_host_profile_intent{ .Profile = host_profile_id{ 11 } }));
		auto staleContext               = context;
		staleContext.IdentityGeneration = github_identity_generation{ 99 };
		const auto beforeStale          = state;
		expect_stale(transition(
			state,
			github_session_failed_event{
				.Context = staleContext,
				.Failure = github_failure::network,
			}));
		expect(state == beforeStale, "stale session failure is inert");

		const auto cancelled = transition(
			state,
			github_session_cancelled_event{ .Context = context });
		expect(
			!state.PendingSession.has_value()
				&& cancelled.Diagnostics.front().Code
					== github_reason_code::cancelled
				&& cancelled.Status
					== github_status_delta{
						.Status = github_status::session_cancelled,
					},
			"matching session cancellation is a visible non-failure state");
	}
}

auto main() -> int
{
	test_strong_types_and_compact_state();
	test_profile_activation_and_session_acceptance();
	test_deactivation_invalidates_without_reestablishing();
	test_every_session_identity_dimension_and_duplicate_are_rejected();
	test_capability_tristate_property_and_no_mutation_surface();
	test_malformed_capability_value_fails_closed();
	test_open_legality_and_context();
	test_opening_a_new_target_clears_the_previous_snapshot();
	test_every_fetch_context_dimension_is_checked();
	test_exact_snapshot_acceptance_and_duplicate_rejection();
	test_invalid_loaded_identity_never_authorizes_snapshot();
	test_refresh_identity_and_canonical_mismatch();
	test_failure_categories_and_refresh_preservation();
	test_first_read_failure_names_the_refused_pull_request();
	test_stale_failure_and_profile_switch_invalidation();
	test_successful_refresh_and_deterministic_replay();
	test_session_failure_and_invalid_completion_are_safe();

	std::cout << "All deterministic GitHub core policy tests passed\n";
	return EXIT_SUCCESS;
}
