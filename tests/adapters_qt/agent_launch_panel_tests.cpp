/// \file
/// \brief Proves nothing launches an agent except a reviewer asking.

#include "bpr/qt/agent_launch_panel.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QToolButton>

#include <array>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class recorder final
	{
	public:
		[[nodiscard]] auto sink() -> qt::agent_intent_dispatch
		{
			return [this](qt::agent_intent aIntent) {
				Intents.push_back(aIntent);
			};
		}

		[[nodiscard]] auto starts() const -> std::size_t
		{
			auto count = std::size_t{ 0U };
			for (const auto& intent : Intents) {
				count += std::holds_alternative<qt::start_agent_intent>(intent)
					? 1U
					: 0U;
			}
			return count;
		}

		[[nodiscard]] auto cancels() const -> std::size_t
		{
			auto count = std::size_t{ 0U };
			for (const auto& intent : Intents) {
				count += std::holds_alternative<qt::cancel_agent_intent>(intent)
					? 1U
					: 0U;
			}
			return count;
		}

		std::vector<qt::agent_intent> Intents;
	};

	[[nodiscard]] auto profiles() -> std::vector<core::agent_profile>
	{
		return {
			{
				.Id          = core::agent_profile_id{ 1U },
				.DisplayName = "Codex",
				.Executable  = "/opt/agents/codex",
				.Protocol    = core::agent_protocol::codex_jsonl_exec,
				.Workspace
				= core::agent_workspace_policy::disposable_review_workspace,
				.AllowedEnvironment = { "HOME" },
			},
		};
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	/// \brief The surface emits an intent and can start nothing itself.
	void test_the_panel_only_emits_intents()
	{
		auto recorded = recorder{ };
		auto panel    = qt::agent_launch_panel{ recorded.sink() };
		panel.offer_profiles(profiles());
		panel.disclose_scope(QStringLiteral(
			"Codex will read the review head and your selected notes."));

		expect(
			panel.start_action()->isEnabled(),
			"a disclosed provider can be asked");
		panel.start_action()->trigger();
		expect(
			recorded.starts() == 1U,
			"asking emits exactly one start intent");
		expect(
			std::get<qt::start_agent_intent>(recorded.Intents.front()).Profile
				== core::agent_profile_id{ 1U },
			"and the intent names the provider the reviewer chose");

		// The surface itself did nothing else. It holds no process and no
		// launcher, so there is no path by which any of the above could have
		// started anything.
		expect(
			recorded.Intents.size() == 1U,
			"and nothing else left the surface");
	}

	/// \brief [[BPR-1749]], RFC-BPR-0092, as [[BPR-1823]] amends it (owner
	/// ruling 2026-10-29: "The picker should keep your choice. It should never
	/// automatically switch agents for you, outside of course of the wizard
	/// where you set it up."). The saved profile is the first pick; after
	/// that the reviewer's choice is kept through re-offers and setup updates.
	void test_the_saved_profile_is_the_first_pick_only()
	{
		auto       recorded = recorder{ };
		auto       panel    = qt::agent_launch_panel{ recorded.sink() };
		const auto shipped  = core::built_in_agent_profiles();
		expect(shipped.size() >= 2U, "two shipped profiles to choose between");
		const auto claude = QVariant::fromValue<qulonglong>(core::claude_stream_json_profile_id.value());
		const auto first  = QVariant::fromValue<qulonglong>(shipped.front().Id.value());
		panel.offer_profiles(shipped);
		expect(panel.profile_selector()->currentData() == first, "before setup is known the first offered profile is selected, as before");
		panel.prefer_profile(core::claude_stream_json_profile_id);
		expect(panel.profile_selector()->currentData() == claude, "the saved profile, once known, is the first pick");
		const auto other     = panel.profile_selector()->findData(claude) == 0 ? 1 : 0;
		const auto otherData = panel.profile_selector()->itemData(other);
		panel.profile_selector()->setCurrentIndex(other);
		expect(panel.profile_selector()->currentData() == otherData, "the reviewer chooses another");
		panel.offer_profiles(shipped);
		expect(panel.profile_selector()->currentData() == otherData, "offered again, the reviewer's choice is kept, not the saved profile");
		panel.prefer_profile(core::claude_stream_json_profile_id);
		panel.offer_profiles(shipped);
		expect(panel.profile_selector()->currentData() == otherData, "a setup update with the same saved profile keeps it too");
		panel.prefer_profile(std::nullopt);
		panel.offer_profiles(shipped);
		expect(panel.profile_selector()->currentData() == otherData, "cleared, the choice is still kept");
		expect(panel.unavailable_agent_name().isEmpty(), "an offered choice is available");
		expect(recorded.Intents.empty(), "preferring or offering a profile emits no intent");
	}

	/// \brief [[BPR-1823]]. The setup wizard's choice is the one automatic
	/// change; Reset agents keeps no choice.
	void test_the_setup_wizard_sets_the_choice()
	{
		auto       recorded = recorder{ };
		auto       panel    = qt::agent_launch_panel{ recorded.sink() };
		const auto shipped  = core::built_in_agent_profiles();
		const auto claude   = QVariant::fromValue<qulonglong>(core::claude_stream_json_profile_id.value());
		panel.prefer_profile(std::nullopt);
		panel.offer_profiles(shipped);
		const auto other = panel.profile_selector()->findData(claude) == 0 ? 1 : 0;
		panel.profile_selector()->setCurrentIndex(other);
		expect(panel.profile_selector()->currentData() != claude, "the reviewer chose another agent");
		panel.choose_profile_from_setup(core::claude_stream_json_profile_id);
		expect(panel.profile_selector()->currentData() == claude, "the wizard's choice moves the picker");
		panel.offer_profiles(shipped);
		expect(panel.profile_selector()->currentData() == claude, "and is kept when offered again");
		panel.profile_selector()->setCurrentIndex(other);
		panel.forget_choice();
		panel.prefer_profile(core::claude_stream_json_profile_id);
		expect(panel.profile_selector()->currentData() == claude, "after Reset agents the saved agent is the first pick again");
		expect(recorded.Intents.empty(), "the wizard's choice emits no intent");
	}

	/// \brief [[BPR-1823]]. A chosen agent that is removed is said as
	/// unavailable; nothing is put in its place and nothing can start.
	void test_a_removed_choice_is_unavailable_not_replaced()
	{
		auto       recorded = recorder{ };
		auto       panel    = qt::agent_launch_panel{ recorded.sink() };
		const auto shipped  = core::built_in_agent_profiles();
		panel.prefer_profile(std::nullopt);
		panel.offer_profiles(shipped);
		panel.disclose_scope(QStringLiteral("the open pull request"));
		const auto chosenAt = 1;
		const auto chosen   = panel.profile_selector()->itemData(chosenAt);
		const auto name     = panel.profile_selector()->itemText(chosenAt);
		panel.profile_selector()->setCurrentIndex(chosenAt);
		expect(panel.start_action()->isEnabled(), "an offered choice can start");
		auto remaining = shipped;
		std::erase_if(remaining, [&chosen](const core::agent_profile& aProfile) { return aProfile.Id.value() == chosen.toULongLong(); });
		panel.offer_profiles(remaining);
		expect(!panel.selected_profile().has_value(), "a removed choice is not replaced by another agent");
		expect(panel.unavailable_agent_name() == name, "the removed agent is named as unavailable");
		expect(panel.profile_selector()->currentText() == QStringLiteral("%1 (unavailable)").arg(name), "the menu shows it as unavailable");
		expect(panel.offered_count() == remaining.size(), "the unavailable entry is not an offered agent");
		expect(!panel.start_action()->isEnabled(), "and nothing can start");
		panel.offer_profiles(remaining);
		expect(panel.unavailable_agent_name() == name && panel.profile_selector()->count() == static_cast<int>(remaining.size()) + 1, "offered again, still one unavailable entry");
		panel.profile_selector()->setCurrentIndex(0);
		expect(panel.selected_profile().has_value() && panel.unavailable_agent_name().isEmpty(), "choosing another agent ends the unavailable state");
		expect(panel.profile_selector()->count() == static_cast<int>(remaining.size()), "and the entry goes");
		expect(recorded.Intents.empty(), "nothing was started");
	}

	/// \brief Disclosure comes before the action, not after.
	void test_the_scope_is_shown_before_launch_is_possible()
	{
		auto recorded = recorder{ };
		auto panel    = qt::agent_launch_panel{ recorded.sink() };
		panel.offer_profiles(profiles());

		expect(
			panel.disclosure_text().isEmpty(),
			"nothing has been disclosed yet");
		expect(
			!panel.start_action()->isEnabled(),
			"and the agent cannot be asked, because a reviewer who cannot see "
			"which tool is about to read their repository has not been asked");

		panel.start_action()->trigger();
		expect(
			recorded.starts() == 0U,
			"triggering a disabled action emits nothing");

		panel.disclose_scope(QStringLiteral("Codex will read the review head."));
		expect(
			panel.start_action()->isEnabled()
				&& !panel.disclosure_text().isEmpty(),
			"once the scope is shown, the agent can be asked");
	}

	/// \brief Cancellation is a distinct action, offered only while running.
	void test_cancellation_is_a_distinct_action()
	{
		auto recorded = recorder{ };
		auto panel    = qt::agent_launch_panel{ recorded.sink() };
		panel.offer_profiles(profiles());
		panel.disclose_scope(QStringLiteral("scope"));

		expect(
			!panel.cancel_action()->isEnabled(),
			"nothing is running, so there is nothing to stop");
		expect(
			panel.cancel_action() != panel.start_action(),
			"stopping is never the same gesture as starting");

		panel.set_running(true);
		expect(
			panel.cancel_action()->isEnabled()
				&& !panel.start_action()->isEnabled(),
			"while a session runs, cancellation is offered and a second launch "
			"is not");
		panel.cancel_action()->trigger();
		expect(
			recorded.cancels() == 1U && recorded.starts() == 0U,
			"cancelling emits a cancel intent and never a start");
	}

	/// \brief Using the reviewer's checkout is off unless chosen now.
	void test_existing_checkout_is_off_by_default()
	{
		auto recorded = recorder{ };
		auto panel    = qt::agent_launch_panel{ recorded.sink() };
		panel.offer_profiles(profiles());
		panel.disclose_scope(QStringLiteral("scope"));

		expect(
			!panel.existing_checkout_action()->isChecked(),
			"the reviewer's working copy is not offered to the agent by "
			"default");
		panel.start_action()->trigger();
		expect(
			!std::get<qt::start_agent_intent>(recorded.Intents.front())
				 .ExistingCheckoutChosen,
			"so the intent carries the choice as not made");

		panel.existing_checkout_action()->setChecked(true);
		panel.start_action()->trigger();
		expect(
			std::get<qt::start_agent_intent>(recorded.Intents.back())
				.ExistingCheckoutChosen,
			"and carries it as made once the reviewer makes it, for this run");
	}

	void test_private_context_choices_are_explicit_identity_only_and_reset()
	{
		auto recorded = recorder{ };
		auto panel    = qt::agent_launch_panel{ recorded.sink() };
		panel.offer_profiles(profiles());
		panel.disclose_scope(QStringLiteral("scope"));
		const auto notes = std::array{
			application::local_note_id{ 7U }, application::local_note_id{ 9U }
		};
		const auto draft = application::agent_draft_choice{
			.Id = core::review_draft_id{ 11U }, .Revision = 3U
		};
		panel.offer_private_context(notes, draft);

		expect(
			panel.note_selector()->count() == 2
				&& panel.note_selector()->item(0)->checkState() == Qt::Unchecked
				&& panel.note_selector()->item(1)->checkState() == Qt::Unchecked
				&& !panel.draft_selector()->isChecked()
				&& !panel.note_selector()->item(0)->text().contains("prose"),
			"opaque note identities and the current draft revision are offered unchecked without prose");
		panel.start_action()->trigger();
		const auto& unchecked
			= std::get<qt::start_agent_intent>(recorded.Intents.back());
		expect(
			unchecked.IncludedNotes.empty()
				&& !unchecked.IncludedDraft.has_value(),
			"an ordinary start includes no private context without explicit checks");

		panel.note_selector()->item(1)->setCheckState(Qt::Checked);
		panel.draft_selector()->setChecked(true);
		panel.start_action()->trigger();
		const auto& selected
			= std::get<qt::start_agent_intent>(recorded.Intents.back());
		expect(
			selected.IncludedNotes
					== std::vector{ application::local_note_id{ 9U } }
				&& selected.IncludedDraft == std::optional{ draft },
			"the launch intent carries only the checked note ID and exact draft identity/revision");

		const auto changedDraft = application::agent_draft_choice{
			.Id = draft.Id, .Revision = draft.Revision + 1U
		};
		panel.offer_private_context(
			std::span<const application::local_note_id>{ notes }.first(1U),
			changedDraft);
		expect(
			panel.note_selector()->count() == 1
				&& panel.note_selector()->item(0)->checkState() == Qt::Unchecked
				&& !panel.draft_selector()->isChecked(),
			"a removed note or changed draft revision clears every earlier choice");
	}

	void test_latest_session_metadata_and_staleness_are_visible_without_retargeting()
	{
		auto       panel   = qt::agent_launch_panel{ };
		const auto session = core::agent_session{
			.Id      = core::agent_session_id{ 7U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = {
				.Host        = core::git_host_id{ 1U },
				.Repository  = core::repository_id{ 2U },
				.PullRequest = core::pull_request_id{ 3U },
				.BaseOid     = commit(4U),
				.HeadOid     = commit(5U),
				.LeftEpoch   = core::epoch_ordinal{ 6U },
				.RightEpoch  = core::epoch_ordinal{ 7U },
			},
		};
		const auto pinnedHead = QString::fromUtf8(
			core::canonical_hex(session.Binding.HeadOid).view());
		for (const auto staleness : {
				 core::agent_session_staleness::current,
				 core::agent_session_staleness::head_moved,
				 core::agent_session_staleness::epoch_pair_changed,
				 core::agent_session_staleness::different_review,
			 }) {
			panel.present_session_catalog(
				application::agent_session_catalog_projection{
					.Latest       = session,
					.LatestStatus = core::agent_session_status{
						.Staleness = staleness,
						.Current   = staleness
							== core::agent_session_staleness::current,
					},
					.Next = core::agent_session_id{ 8U },
				});
			expect(
				panel.session_catalog_text().contains(
					QString::fromUtf8(core::stable_name(staleness)))
					&& panel.session_catalog_text().contains(pinnedHead)
					&& panel.session_catalog_text().contains("next #8"),
				"each core classification is visible against the immutable retained head");
		}
	}

	/// \brief Terminal projection is visible and failed answer output is cleared.
	void test_projection_uses_the_shipped_visible_controls()
	{
		auto panel = qt::agent_launch_panel{ };
		panel.offer_profiles(profiles());
		panel.disclose_scope(QStringLiteral("scope"));
		const auto buttons = panel.findChildren<QToolButton*>();
		expect(
			buttons.size() == 3U
				&& std::ranges::all_of(buttons, [](const QToolButton* const aButton) {
					   return !aButton->accessibleName().isEmpty() && !aButton->isHidden();
				   }),
			"the working-copy, start, and cancel gestures are explicit accessible controls");

		const auto projectionIdentity
			= application::agent_invocation_projection_identity{
				  .ApplicationSession   = core::application_session_id{ 1U },
				  .Session              = { .Id = core::agent_session_id{ 2U } },
				  .InvocationGeneration = 1U,
			  };
		panel.admit(projectionIdentity);
		panel.start(
			projectionIdentity,
			application::prepared_workspace{ .Path = "/tmp/exact-agent-head" });
		panel.stream(
			projectionIdentity,
			application::agent_stream_event{
				.Session = projectionIdentity.Session.Id,
				.Text    = "partial result",
			});
		auto foreignIdentity = projectionIdentity;
		++foreignIdentity.InvocationGeneration;
		panel.stream(
			foreignIdentity,
			application::agent_stream_event{
				.Session = projectionIdentity.Session.Id,
				.Text    = "foreign result",
			});
		expect(
			panel.status_text().contains("/tmp/exact-agent-head")
				&& panel.output_text().contains("partial result")
				&& !panel.output_text().contains("foreign result"),
			"the visible status discloses the accepted workspace and streams output");
		panel.finish(
			foreignIdentity,
			application::agent_run_result{
				.Session = projectionIdentity.Session.Id,
				.Outcome = application::agent_run_outcome::success,
			});
		expect(
			panel.status_text().startsWith("Running in"),
			"foreign terminal projection cannot finish the active invocation");
		panel.finish(
			projectionIdentity,
			application::agent_run_result{
				.Session = projectionIdentity.Session.Id,
				.Outcome = application::agent_run_outcome::protocol_failure,
			});
		expect(
			panel.status_text() == "Finished: protocol_failure"
				&& !panel.output_text().contains("partial result")
				// [[BPR-1827]]. In its place, the log of why it stopped.
				&& panel.output_text().startsWith("What went wrong: "),
			"a typed failed terminal clears partial output instead of presenting it as an answer");
	}

	void test_prestart_terminals_keep_the_exact_admitted_identity()
	{
		auto panel = qt::agent_launch_panel{ };
		panel.offer_profiles(profiles());
		panel.disclose_scope(QStringLiteral("scope"));
		const auto identity = [](const std::uint64_t aGeneration) {
			return application::agent_invocation_projection_identity{
				.ApplicationSession   = core::application_session_id{ 1U },
				.Session              = { .Id = core::agent_session_id{ 2U } },
				.InvocationGeneration = aGeneration,
			};
		};
		const auto finish = [&panel](
								const application::agent_invocation_projection_identity& aIdentity,
								const application::agent_run_outcome                     aOutcome) {
			panel.finish(
				aIdentity,
				application::agent_run_result{
					.Session = aIdentity.Session.Id,
					.Outcome = aOutcome,
				});
		};

		for (const auto& [generation, outcome, expected] : {
				 std::tuple{ 1U,
							 application::agent_run_outcome::cancelled,
							 QStringLiteral("Finished: cancelled") },
				 std::tuple{ 2U,
							 application::agent_run_outcome::stale_result,
							 QStringLiteral("Finished: stale_result") },
				 std::tuple{ 3U,
							 application::agent_run_outcome::failed_to_start,
							 QStringLiteral("Finished: failed_to_start") },
			 }) {
			const auto admitted = identity(generation);
			panel.admit(admitted);
			expect(
				panel.status_text() == "Preparing agent invocation"
					&& panel.cancel_action()->isEnabled(),
				"pre-effect admission is visible and cancellable before start");
			finish(admitted, outcome);
			expect(
				panel.status_text() == expected
					&& !panel.cancel_action()->isEnabled(),
				"cancel, stale, and workspace-failure terminals remain visible before start");
		}

		const auto workspaceFailure = identity(4U);
		const auto diagnostic       = QStringLiteral(
			"fatal: could not materialize /tmp/bpr-agent-workspace from "
			"<redacted-url>\n");
		panel.admit(workspaceFailure);
		panel.finish(
			workspaceFailure,
			application::agent_run_result{
				.Session       = workspaceFailure.Session.Id,
				.Outcome       = application::agent_run_outcome::workspace_unavailable,
				.StandardError = diagnostic.toStdString(),
			});
		expect(
			panel.status_text() == "Finished: workspace_unavailable"
				&& panel.output_text() == diagnostic,
			"a failed workspace terminal visibly renders its bounded diagnostic");
		expect(
			panel.stop_reason_text().startsWith(QStringLiteral("The agent's copy of this pull request could not be made."))
				&& panel.stop_reason_text().contains(QStringLiteral("ask again")),
			"a workspace stop with no reason says the copy failed and what to do [[BPR-1738]]");
		for (const auto& [reason, phrase] : std::array{
				 std::pair{ application::agent_workspace_unavailable_reason::download_failed, QStringLiteral("Downloading the rest failed.") },
				 std::pair{ application::agent_workspace_unavailable_reason::connection_failed, QStringLiteral("Check the connection, then ask again.") },
				 std::pair{ application::agent_workspace_unavailable_reason::sign_in_refused, QStringLiteral("Check that gh is signed in") },
				 std::pair{ application::agent_workspace_unavailable_reason::too_large, QStringLiteral("larger than the agent's private copy allows") },
				 std::pair{ application::agent_workspace_unavailable_reason::too_large_not_in_clone, QStringLiteral("Fetch the pull request's branch in your clone") },
			 }) {
			const auto reasoned = identity(40U + static_cast<std::uint64_t>(reason));
			panel.admit(reasoned);
			expect(panel.stop_reason_text().isEmpty(), "an admitted run has no stop reason yet");
			panel.finish(
				reasoned,
				application::agent_run_result{
					.Session         = reasoned.Session.Id,
					.Outcome         = application::agent_run_outcome::workspace_unavailable,
					.StandardError   = diagnostic.toStdString(),
					.WorkspaceReason = reason,
				});
			expect(
				panel.status_text() == "Finished: workspace_unavailable" && panel.output_text() == diagnostic
					&& panel.stop_reason_text().contains(phrase),
				"a workspace stop says why in plain words and keeps Git's text in the log [[BPR-1738]]");
		}

		const auto active  = identity(5U);
		const auto foreign = identity(6U);
		panel.admit(active);
		panel.admit(foreign);
		panel.start(
			foreign,
			application::prepared_workspace{ .Path = "/tmp/foreign" });
		finish(foreign, application::agent_run_outcome::success);
		expect(
			panel.status_text() == "Preparing agent invocation"
				&& panel.output_text().isEmpty(),
			"foreign generations cannot replace, start, or finish the admitted invocation");
		panel.start(
			active,
			application::prepared_workspace{ .Path = "/tmp/exact" });
		finish(active, application::agent_run_outcome::cancelled);
		expect(
			panel.status_text() == "Finished: cancelled",
			"post-materialization cancellation finishes the same admitted identity");
	}

	/// [[BPR-1827]]. A run that ended without an answer says why in words,
	/// how much the agent wrote, the line refused and its last errors.
	void test_a_failed_run_log_says_what_happened()
	{
		auto result                = application::agent_run_result{ };
		result.Outcome             = application::agent_run_outcome::protocol_failure;
		result.ProtocolFailure     = application::agent_protocol_failure::frame_too_large;
		result.ExitCode            = 0;
		result.StandardOutputBytes = 223211U;
		result.RejectedOutputLine  = std::string{ R"({"type":"user","message":)" } + std::string(600U, 'x');
		for (auto line = 0; line < 30; ++line) {
			result.StandardError += "warning " + std::to_string(line) + "\n";
		}
		const auto log = qt::agent_launch_panel::failure_log(result);
		expect(log.contains(QStringLiteral("What went wrong: One line of its output is longer than this app reads. (frame_too_large)")), "the cause is said in words and named");
		expect(log.contains(QStringLiteral("It wrote 223211 bytes of output and ")) && log.contains(QStringLiteral("Exit code: 0")), "what it wrote is counted");
		expect(log.contains(QStringLiteral("First line this app couldn't read:\n{\"type\":\"user\"")) && !log.contains(QString(450, QLatin1Char{ 'x' })), "the refused line is shown, bounded");
		expect(log.contains(QStringLiteral("warning 29")) && log.contains(QStringLiteral("warning 10")) && !log.contains(QStringLiteral("warning 9\n")), "the last twenty lines of errors are shown");
		expect(!log.contains(QStringLiteral("wrote nothing")), "a run that wrote something never says it wrote nothing");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_panel_only_emits_intents();
	test_the_saved_profile_is_the_first_pick_only();
	test_the_setup_wizard_sets_the_choice();
	test_a_removed_choice_is_unavailable_not_replaced();
	test_the_scope_is_shown_before_launch_is_possible();
	test_cancellation_is_a_distinct_action();
	test_existing_checkout_is_off_by_default();
	test_private_context_choices_are_explicit_identity_only_and_reset();
	test_latest_session_metadata_and_staleness_are_visible_without_retargeting();
	test_projection_uses_the_shipped_visible_controls();
	test_prestart_terminals_keep_the_exact_admitted_identity();
	test_a_failed_run_log_says_what_happened();
	std::cout << "agent-launch-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
