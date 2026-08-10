/// \file
/// \brief Proves nothing launches an agent except a reviewer asking.

#include "bpr/qt/agent_launch_panel.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <ranges>
#include <string_view>
#include <tuple>
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

	/// \brief Terminal projection is visible and failed output is never retained.
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
				&& panel.output_text().isEmpty(),
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

		const auto active  = identity(4U);
		const auto foreign = identity(5U);
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
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_panel_only_emits_intents();
	test_the_scope_is_shown_before_launch_is_possible();
	test_cancellation_is_a_distinct_action();
	test_existing_checkout_is_off_by_default();
	test_projection_uses_the_shipped_visible_controls();
	test_prestart_terminals_keep_the_exact_admitted_identity();
	std::cout << "agent-launch-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
