#include "bpr/qt/defect_card_view.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
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

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto defect(const core::defect_state aState)
		-> core::defect_record
	{
		return core::defect_record{
			.Id     = core::defect_id{ 1U },
			.Anchor = core::source_line_ref{
				.Comparison = opaque<core::file_comparison_digest>(2U),
				.Side       = core::diff_side::right,
				.Line       = 9U },
			.State = aState,
		};
	}

	constexpr auto every_state = std::array{
		core::defect_state::open,
		core::defect_state::fix_claimed,
		core::defect_state::needs_verification,
		core::defect_state::verified,
		core::defect_state::reopened,
		core::defect_state::withdrawn,
		core::defect_state::accepted_as_is,
	};

	using log = std::vector<core::defect_command>;

	[[nodiscard]] auto card_for(log& aLog) -> qt::defect_card_view*
	{
		return new qt::defect_card_view{
			[&aLog](core::defect_command aCommand) { aLog.push_back(aCommand); },
		};
	}

	// The third and final layer that must prove this independently. A card that
	// advanced its own badge would show a defect as verified before anyone had
	// decided it was.
	void test_no_action_verifies_the_card_itself()
	{
		auto  intents = log{ };
		auto* card    = card_for(intents);

		for (const auto starting : every_state) {
			card->present(defect(starting));
			const auto before = card->shown_defect();

			card->verify_action()->trigger();
			card->reopen_action()->trigger();
			card->withdraw_action()->trigger();
			card->accept_as_is_action()->trigger();

			expect(
				card->shown_defect() == before,
				"no action moves the card's own state");
			expect(
				card->shown_defect()->State == starting,
				"and the badge still shows what was presented");
		}

		delete card;
	}

	void test_verifying_emits_the_command_and_waits()
	{
		auto  intents = log{ };
		auto* card    = card_for(intents);
		card->present(defect(core::defect_state::fix_claimed));

		card->verify_action()->trigger();

		expect(intents.size() == 1U, "one command was emitted");
		expect(
			intents.front().Kind == core::defect_command_kind::verify
				&& intents.front().Id == core::defect_id{ 1U },
			"the command names the verification and the defect");
		expect(
			card->shown_defect()->State == core::defect_state::fix_claimed,
			"the card still shows the unverified state");

		card->present(defect(core::defect_state::verified));
		expect(
			card->shown_defect()->State == core::defect_state::verified
				&& card->state_label()->text().contains(
					qt::badge_for(core::defect_state::verified)),
			"only the accepted projection moves the badge");

		delete card;
	}

	// An action offered where the core would refuse it teaches the reviewer
	// that the tool is unreliable.
	void test_enablement_mirrors_what_the_core_would_allow()
	{
		auto  intents = log{ };
		auto* card    = card_for(intents);

		expect(
			!card->verify_action()->isEnabled()
				&& !card->reopen_action()->isEnabled(),
			"with nothing presented, nothing is offered");

		for (const auto starting : every_state) {
			card->present(defect(starting));

			const auto verifiable
				= starting == core::defect_state::fix_claimed
				|| starting == core::defect_state::needs_verification;
			expect(
				card->verify_action()->isEnabled() == verifiable,
				"verify is offered exactly where the core would allow it");

			expect(
				card->reopen_action()->isEnabled()
					== (starting == core::defect_state::verified),
				"reopen is offered only for a verified defect");

			const auto settled = starting == core::defect_state::withdrawn
				|| starting == core::defect_state::accepted_as_is;
			expect(
				card->withdraw_action()->isEnabled() != settled
					&& card->accept_as_is_action()->isEnabled() != settled,
				"a settled defect offers no further decision");
		}

		delete card;
	}

	void test_every_state_reads_without_colour()
	{
		auto badges = std::vector<QString>{ };
		for (const auto state : every_state) {
			const auto badge = qt::badge_for(state);
			expect(!badge.isEmpty(), "every state has a badge");
			expect(
				!qt::explanation_for(state).isEmpty(),
				"every state has an explanation");
			badges.push_back(badge);
		}

		for (auto first = std::size_t{ 0U }; first < badges.size(); ++first) {
			for (auto second = first + 1U; second < badges.size(); ++second) {
				expect(
					badges[first] != badges[second],
					"two states never share a badge");
			}
		}

		auto  intents = log{ };
		auto* card    = card_for(intents);
		card->present(defect(core::defect_state::needs_verification));
		expect(
			card->state_label()->text().startsWith(
				qt::badge_for(core::defect_state::needs_verification)),
			"the badge leads the label");
		expect(
			!card->state_label()->accessibleName().isEmpty(),
			"the state is announced, not only drawn");

		delete card;
	}

	// Severity and category never reach the published comment, so the card says
	// they are local rather than letting a reviewer assume a collaborator saw
	// them.
	void test_the_classification_is_labelled_local()
	{
		auto  intents = log{ };
		auto* card    = card_for(intents);

		auto record     = defect(core::defect_state::open);
		record.Severity = core::defect_severity::critical;
		record.Category = core::defect_category::security;
		card->present(record);

		const auto text = card->classification_label()->text();
		expect(
			text.contains(QStringLiteral("critical"))
				&& text.contains(QStringLiteral("security")),
			"the severity and category are shown");
		expect(
			text.contains(QStringLiteral("Local")),
			"and are named as local rather than presented as shared");

		delete card;
	}

	void test_actions_are_distinctly_named()
	{
		auto  intents = log{ };
		auto* card    = card_for(intents);

		const auto actions = std::array{
			card->verify_action(),
			card->reopen_action(),
			card->withdraw_action(),
			card->accept_as_is_action(),
		};

		for (auto first = std::size_t{ 0U }; first < actions.size(); ++first) {
			expect(
				!actions[first]->text().isEmpty(),
				"every action carries the text a screen reader announces");
			for (auto second = first + 1U; second < actions.size(); ++second) {
				expect(
					actions[first]->text() != actions[second]->text()
						&& actions[first]->objectName()
							!= actions[second]->objectName(),
					"two actions never share text or object name");
			}
		}

		delete card;
	}

	void test_history_and_reclassification_remain_projection_only()
	{
		auto       intents = log{ };
		auto*      card    = card_for(intents);
		auto       record  = defect(core::defect_state::open);
		const auto history = core::defect_history{
			.Defect         = record,
			.OriginalAnchor = record.Anchor,
			.Availability   = core::defect_anchor_availability::outdated,
		};
		card->present(history);

		expect(
			card->context_label()->text().contains(QStringLiteral("unavailable"))
				&& card->context_label()->text().contains(
					QStringLiteral("original semantic anchor")),
			"lost shared context remains visible with the original anchor retained");
		card->severity_selector()->setCurrentIndex(
			card->severity_selector()->findData(
				static_cast<int>(core::defect_severity::critical)));
		card->category_selector()->setCurrentIndex(
			card->category_selector()->findData(
				static_cast<int>(core::defect_category::security)));
		const auto before = card->shown_defect();
		card->reclassify_action()->trigger();

		expect(
			intents.size() == 1U
				&& intents.front().Kind == core::defect_command_kind::reclassify
				&& intents.front().Severity == core::defect_severity::critical
				&& intents.front().Category == core::defect_category::security,
			"local selectors emit one exact semantic reclassification command");
		expect(
			card->shown_defect() == before,
			"reclassification waits for an accepted host projection");

		delete card;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);

	test_no_action_verifies_the_card_itself();
	test_verifying_emits_the_command_and_waits();
	test_enablement_mirrors_what_the_core_would_allow();
	test_every_state_reads_without_colour();
	test_the_classification_is_labelled_local();
	test_actions_are_distinctly_named();
	test_history_and_reclassification_remain_projection_only();

	std::cout << "defect-card-tests:pass\n";
	return EXIT_SUCCESS;
}
