#include "github_application/window_composition.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/pull_request_link_location.hpp"
#include "bpr/application/remembered_connection_host.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/github_transition.hpp"
#include "bpr/core/github_window_read.hpp"
#include "bpr/core/refresh_feedback.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "github_application/credential_prompt.hpp"
#include "github_cli_runtime.hpp"

#include <QByteArrayView>
#include <QDesktopServices>

#include <algorithm>
#include <variant>

#if defined(BPR_BUILD_TESTING)
	#include "github_public_path_fixture.hpp"
#endif

namespace bpr::app {
	auto github_application::implementation::same_name(const std::string_view aLeft, const std::string_view aRight) -> bool
	{
		// GitHub owner and repository names compare without case; a link
		// may carry either.
		return std::ranges::equal(aLeft, aRight, [](const char aOne, const char aTwo) {
			return std::tolower(static_cast<unsigned char>(aOne)) == std::tolower(static_cast<unsigned char>(aTwo));
		});
	}

	auto github_application::implementation::shows_destination(const core::setup_destination& aDestination) const -> bool
	{
		const auto& snapshot = WindowRead->accepted_snapshot();
		return snapshot.has_value() && !WindowRead->state().PendingPullRequest.has_value()
			&& snapshot->PullRequest.Number == aDestination.Number
			&& same_name(snapshot->Repository.Owner, aDestination.Repository.Owner)
			&& same_name(snapshot->Repository.Name, aDestination.Repository.Name);
	}

	void github_application::implementation::reread_after_publication()
	{
		if (Closing) {
			return;
		}
		RereadOwed  = true;
		RereadQuiet = false;
		schedule_publication_reread();
	}

	/// \brief [[BPR-1867]]. What the profile's activity host hands its
	/// windows after a poll, for a course whose profile never polls.
	void github_application::implementation::hand_on_activity(const std::vector<core::pull_request_ref>& aActive)
	{
		if (Profile) {
			Profile->activity_arrived(aActive);
		}
	}

	/// \brief [[BPR-1867]]. Something changed on GitHub that this window
	/// shows, or it came back to an aged read: one reread is owed, as
	/// after a publication, but it may wait while the reviewer writes.
	void github_application::implementation::reread_for_news()
	{
		if (Closing) {
			return;
		}
		if (!RereadOwed) {
			RereadQuiet = true;
		}
		RereadOwed = true;
		schedule_publication_reread();
	}

	void github_application::implementation::schedule_publication_reread()
	{
		if (Closing || !RereadOwed || RereadQueued || Window == nullptr) {
			return;
		}
		RereadQueued = true;
		QTimer::singleShot(0, Window.get(), [this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
			if (weak.expired()) {
				return;
			}
			RereadQueued = false;
			pay_publication_reread();
		});
	}

	void github_application::implementation::pay_publication_reread()
	{
		if (Closing || !RereadOwed || Window == nullptr || !WindowRead) {
			return;
		}
		const auto reading = WindowRead->state().PendingPullRequest.has_value();
		const auto step    = core::decide_reread({ .Owed = RereadOwed, .Quiet = RereadQuiet, .Reading = reading, .Composing = Window->composing() });
		if (step == core::reread_step::wait) {
			// A read under way pays it when it settles: its update schedules
			// this again. Writing is asked about again shortly.
			if (!reading && !RereadRetrying) {
				RereadRetrying = true;
				QTimer::singleShot(reread_writing_retry, Window.get(), [this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
					if (weak.expired()) {
						return;
					}
					RereadRetrying = false;
					schedule_publication_reread();
				});
			}
			return;
		}
		if (step == core::reread_step::none) {
			return;
		}
		RereadOwed  = false;
		RereadQuiet = false;
		// The core's refresh of the shown pull request, as Refresh reads it,
		// but not as a gesture: nothing the reviewer is doing (a pasted
		// link waiting on sign-in, a restoration, the status line) is
		// retired by a read the window makes for itself.
		if (WindowRead->accepted_snapshot()) {
			// [[BPR-1575]]. Presented as the application's own update, not the
			// reviewer's Refresh.
			Window->run_automatic_refresh([this] { WindowRead->dispatch(core::github_window_read_event{ core::refresh_pull_request_intent{ } }); });
		}
	}

	void github_application::implementation::schedule_link_location()
	{
		if (Closing || !PendingLinkLocation || LinkLocationQueued) {
			return;
		}
		LinkLocationQueued = true;
		DecisionLane.post([this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
			if (weak.expired()) {
				return;
			}
			LinkLocationQueued = false;
			advance_link_location();
		});
	}

	void github_application::implementation::finish_link_location(const core::pull_request_location_refusal aRefusal, const bool aWholeLocation)
	{
		const auto requested = PendingLinkLocation ? PendingLinkLocation->Destination.Location : std::string{ };
		PendingLinkLocation.reset();
		if (aRefusal == core::pull_request_location_refusal::none || Window == nullptr) {
			return;
		}
		const auto lead = aWholeLocation ? QStringLiteral("The requested location is unavailable; opened the pull request. ")
										 : QStringLiteral("Opened the file; the requested line is unavailable. ");
		Window->present_link_location_notice(lead + QString::fromUtf8(core::explanation(aRefusal)) + QStringLiteral(" Requested location: ")
											 + QString::fromStdString(requested));
	}

	auto github_application::implementation::comparison_ends_at_head() const -> bool
	{
		const auto& epoch = EpochHost->state();
		return epoch.AcceptedComparison.has_value() && !epoch.PendingComparison.has_value()
			&& conversation_endpoint_is_current(*epoch.AcceptedComparison, core::diff_side::right);
	}

	void github_application::implementation::advance_link_location()
	{
		if (Closing || !PendingLinkLocation || Window == nullptr || WorkbenchHost == nullptr || EpochHost == nullptr) {
			return;
		}
		if (WindowRead->state().PendingPullRequest.has_value()) {
			return;
		}
		if (!shows_destination(PendingLinkLocation->Destination)) {
			// The open failed or the window moved on; nothing is placed.
			PendingLinkLocation.reset();
			return;
		}
		auto& pending = *PendingLinkLocation;
		if (!pending.Target) {
			// A line is judged against the comparison the window settles
			// on, so resolution waits until one is accepted.
			if (pending.Destination.Target.Kind == core::pull_request_location_kind::file && pending.Destination.Target.Side
				&& (!EpochHost->state().AcceptedComparison || EpochHost->state().PendingComparison)) {
				return;
			}
			const auto facts = application::pull_request_link_location_facts_of(*WindowRead->accepted_snapshot());
			pending.Target   = core::resolve_pull_request_location(pending.Destination.Target, facts.facts(comparison_ends_at_head()));
		}
		const auto& target = *pending.Target;
		switch (target.Action) {
		case core::pull_request_location_action::none:        finish_link_location(core::pull_request_location_refusal::none, true); return;
		case core::pull_request_location_action::unavailable: finish_link_location(target.Refusal, true); return;
		case core::pull_request_location_action::reveal_conversation:
			// [[BPR-1293]]. A comment on a file is read in Review, in place
			// at that post; a review, which is not on a file, in Discuss.
			if (Window->reveal_conversation_in_review(target.Conversation) || Window->reveal_conversation(target.Conversation)) {
				finish_link_location(core::pull_request_location_refusal::none, true);
			} else {
				finish_link_location(pending.Destination.Target.Kind == core::pull_request_location_kind::review ? core::pull_request_location_refusal::review_not_read
																												 : core::pull_request_location_refusal::comment_not_identified,
									 true);
			}
			return;
		case core::pull_request_location_action::select_file: break;
		}
		if (!pending.File) {
			// The file list is this pull request's once the epoch host holds
			// its authority: the workbench observes that authority first.
			const auto& snapshot  = *WindowRead->accepted_snapshot();
			const auto& authority = EpochHost->state().Authority;
			if (!authority || authority->PullRequest != snapshot.PullRequest.PullRequest || authority->Repository != snapshot.Repository.Repository
				|| !WorkbenchHost->state().Navigation.has_value()) {
				return;
			}
			const auto file = core::file_identity_of(target.Path);
			if (!WorkbenchHost->contains_file(file)) {
				finish_link_location(core::pull_request_location_refusal::file_not_in_pull_request, true);
				return;
			}
			pending.File = file;
			// A link is an explicit new reading choice. Entering Review
			// must not restore the tab's previous file over that choice.
			abandon_tab_load();
			TabModality = core::workbench_modality::review;
			Window->choose_home(false);
			Window->choose_modality(qt::review_modality::review);
			if (WorkbenchHost->selected() != file) {
				WorkbenchHost->apply(core::select_workbench_file_intent{ file });
			}
			// [[BPR-1478]]. Kept as the reviewer's choice until the tab takes it.
			if (TabHost && WorkbenchHost->selected() == file) {
				TabHost->observe_file_choice(file);
			}
			if (!target.RightLine) {
				finish_link_location(target.Refusal, false);
				return;
			}
		}
		if (WorkbenchHost->selected() != pending.File) {
			// Another file was chosen first; that gesture wins.
			PendingLinkLocation.reset();
			return;
		}
		const auto* surface     = Window->comparison_surface();
		const auto& publication = SelectedDiffPublication;
		if (!publication || publication->Request.File.File != *pending.File || !surface->current_result() || !surface->current_context()
			|| surface->current_context()->Key != publication->Key || !surface->current_semantic_index()) {
			return;
		}
		if (!comparison_ends_at_head()) {
			finish_link_location(core::pull_request_location_refusal::line_needs_head, false);
			return;
		}
		const auto& index     = *surface->current_semantic_index();
		const auto  selection = core::select_line(index, core::source_line_ref{ .Comparison = index.diff().Alignment.Digest, .Side = core::diff_side::right, .Line = *target.RightLine });
		const auto  placed    = selection.has_value() && Window->comparison_surface()->restore_selection(*selection);
		finish_link_location(placed ? core::pull_request_location_refusal::none : core::pull_request_location_refusal::line_not_in_file, false);
	}

	void github_application::implementation::dispatch_intent(application::github_intent aIntent)
	{
		if (std::holds_alternative<core::activate_host_profile_intent>(aIntent)
			&& (Closing || Host == nullptr || !core::github_connection_setup_available(Host->state()))) {
			return;
		}
		if (GithubCliDiscoveryPending) {
			retire_cli_discovery();
		}
		cancel_restoration(true);
		retire_setup_effects();
		auto setupActivation = false;
		if (const auto* activation = std::get_if<core::activate_host_profile_intent>(&aIntent);
			activation && activation->Mode == core::github_access_mode::authenticated && SetupDestination && SetupDestination->pending()) {
			setupActivation = SetupDestination->begin_authentication(SetupDestination->pending()->Request, activation->Profile);
		}
		if (!setupActivation) {
			SetupOwner = next_setup_owner();
			if (SetupDestination) {
				SetupDestination->cancel();
			}
		}
		if (Closing || Host == nullptr) {
			return;
		}
		std::visit(
			[this, setupActivation, expectedSetup = SetupDestination->pending(), discoveryGeneration = GithubCliDiscoveryGeneration](auto aValue) {
				using intent_type = decltype(aValue);
				if constexpr (std::is_same_v<
								  intent_type,
								  core::activate_host_profile_intent>) {
					const auto* profile = find_profile(aValue.Profile);
					if (profile == nullptr) {
						return;
					}
					if (profile->authentication_transport()
						== application::github_authentication_transport::
							github_cli) {
					// [[BPR-0797]]. Run-time, for the same reason and
					// with the same consequence if it had been left
					// behind: this build offers the row labelled "uses
					// your gh sign-in", so it owes the reviewer the
					// connection question that says what accepting it means.
					// A run inside the boundary composes no factory that
					// can serve a CLI read, so it never reaches here.
#if defined(BPR_GITHUB_CLI_PLATFORM)
						if (profile->cli_account_login().empty()) {
							discover_github_cli_accounts(true);
							return;
						}
						const auto remembered = Profile->selected_connection(profile->hostname());
						if (Profile->cli_read_offered_here() && !Profile->CliConsent.contains(aValue.Profile) && (!remembered || remembered->Profile != aValue.Profile)) {
							// [[BPR-0933]]. A short decision; the full
							// residual-risk list is behind Show Details.
							const auto consent = qt::ask_github_cli_read_consent(
								Window.get(), QStringLiteral("Use configured GitHub CLI"),
								QString::fromStdString(std::string{ profile->hostname() }), QString::fromStdString(std::string{ profile->cli_account_login() }));
							if (!consent) {
								return;
							}
						}
#endif
						if (Closing || GithubCliDiscoveryGeneration != discoveryGeneration || (setupActivation && SetupDestination->pending() != expectedSetup)) {
							return;
						}
						Profile->CliConsent.insert(aValue.Profile);
						DeviceHost->release_refresh_authority();
						DeviceRefreshAuthority.reset();
						if (setupActivation) {
							SetupAcceptedGeneration = core::github_identity_generation{ Host->state().IdentityGeneration.value() + 1U };
						}
						Host->dispatch(core::github_event{
							core::activate_host_profile_intent{ .Profile = aValue.Profile },
						});
						return;
					}
					if (aValue.Mode == core::github_access_mode::public_preview) {
						DeviceHost->release_refresh_authority();
						DeviceRefreshAuthority.reset();
						Host->activate_public_preview(aValue.Profile);
						return;
					}

					if (setupActivation && SetupDestination->pending()) {
						authenticate_manual(*SetupDestination->pending());
						return;
					}
					auto credential = std::optional<
						application::github_credential_material>{ };
#if defined(BPR_BUILD_TESTING)
					if (Fixture != nullptr) {
						credential.emplace(
							Fixture->take_enrollment_credential());
					} else
#endif
					{
						credential = prompt_for_credential(Window.get());
					}
					if (credential.has_value()) {
						DeviceHost->release_refresh_authority();
						DeviceRefreshAuthority.reset();
						Host->enroll_profile(
							native_sign_in_profile(aValue.Profile), std::move(*credential), application::github_credential_provenance::manual, std::nullopt, false,
							[runtime = Profile.get()](const application::github_profile_authority& aAuthority, const application::github_viewer_record& aViewer) { return runtime->rebind_native_sign_in(aAuthority, aViewer); });
					}
				} else {
					if constexpr (std::is_same_v<intent_type, core::deactivate_github_identity_intent>) {
						if (Host->state().ActiveProfile) {
							const auto* active = find_profile(*Host->state().ActiveProfile);
							if (active && Profile->Connections) {
								const auto hostname = std::string{ active->hostname() };
								Profile->for_each_window([&](const auto& aObserver) { if (aObserver.RetireServerSetup) { aObserver.RetireServerSetup(hostname); } });
								static_cast<void>(Profile->Connections->disable(hostname));
							}
							DeviceHost->dispatch(core::cancel_device_authorization_intent{ });
						}
						if (DiagnosticLog) {
							DiagnosticLog->discard_context();
						}
						DeviceHost->release_refresh_authority();
						DeviceRefreshAuthority.reset();
					}
					if constexpr (std::is_same_v<intent_type, core::open_pull_request_intent> || std::is_same_v<intent_type, core::refresh_pull_request_intent>) {
						WindowRead->dispatch(core::github_window_read_event{ std::move(aValue) });
					} else {
						Host->dispatch(core::github_event{ std::move(aValue) });
					}
				}
			},
			std::move(aIntent));
	}

	void github_application::implementation::dispatch_epoch_intent(application::epoch_intent aIntent)
	{
		if (Closing || EpochHost == nullptr) {
			return;
		}
#if defined(BPR_BUILD_TESTING)
		if (std::holds_alternative<core::prune_review_epochs_intent>(aIntent)) {
			++EpochPruneIntentDispatchCount;
		} else if (std::holds_alternative<core::delete_repository_cache_intent>(
					   aIntent)) {
			++EpochDeleteIntentDispatchCount;
		} else if (std::holds_alternative<core::cancel_epoch_maintenance_intent>(
					   aIntent)) {
			++EpochCancelIntentDispatchCount;
		}
#endif
		if (
			std::holds_alternative<core::select_named_comparison_intent>(aIntent)
			|| std::holds_alternative<core::select_comparison_anchors_intent>(aIntent)
			|| std::holds_alternative<core::select_comparison_anchor_intent>(aIntent)) {
			SearchComparisonGesture = WorkbenchHost && tab_modality(WorkbenchHost->state().Modality);
			cancel_review_navigation();
			cancel_restoration(false, true);
			abandon_tab_load();
			clear_diff_selection();
		}
		if (const auto* deletion = std::get_if<core::delete_repository_cache_intent>(&aIntent)) {
			invalidate_diff_if([deletion](const auto& aAuthority) {
				return aAuthority.Cache == deletion->Cache;
			});
		}
		EpochHost->dispatch(std::move(aIntent));
	}

	/// \brief [[BPR-1211]]. Admits one agent session through this window's
	/// ledger; the answer arrives later on the decision lane.
	void github_application::implementation::admit(
		const application::agent_session_admission&                                                aAdmission,
		std::function<void(std::expected<core::agent_session, application::ledger_store_failure>)> aDone)
	{
		if (Closing || ReviewLedgerHost == nullptr) {
			DecisionLane.post([done = std::move(aDone)] { done(std::unexpected(application::ledger_store_failure::unavailable)); });
			return;
		}
		ReviewLedgerHost->admit(aAdmission, std::move(aDone));
	}

	/// \brief [[BPR-1211]]. Reads this window's agent session catalog; the
	/// answer arrives later on the decision lane.
	void github_application::implementation::session_catalog(
		std::function<void(std::expected<application::agent_session_catalog_state, application::ledger_store_failure>)> aDone)
	{
		if (Closing || ReviewLedgerHost == nullptr) {
			DecisionLane.post([done = std::move(aDone)] { done(std::unexpected(application::ledger_store_failure::unavailable)); });
			return;
		}
		ReviewLedgerHost->session_catalog(std::move(aDone));
	}
}
