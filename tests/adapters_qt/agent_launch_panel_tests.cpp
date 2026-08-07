/// \file
/// \brief Proves nothing launches an agent except a reviewer asking.

#include "bpr/qt/agent_launch_panel.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>

#include <cstdlib>
#include <iostream>
#include <string_view>
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
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_panel_only_emits_intents();
	test_the_scope_is_shown_before_launch_is_possible();
	test_cancellation_is_a_distinct_action();
	test_existing_checkout_is_off_by_default();
	std::cout << "agent-launch-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
