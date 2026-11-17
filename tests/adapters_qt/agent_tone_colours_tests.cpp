/// \file
/// \brief [[BPR-2236]] (owner, 2026-11-16: "it looks like we have colors in
/// the agent tab and the report which correspond to the same elements but
/// use different colors").
///
/// \details One review, shown in the Agent tab and as its report page. A
/// Major finding's card and severity, the "issue (blocking)" label of its
/// suggested comment, and the verdict wear the theme's Needs work tone in
/// the tab; the same finding's label, rule and the verdict wear it on the
/// page. A Minor finding and a suggestion wear the Half done tone on both.
/// Changing a tone in the profile changes it on both surfaces, and the
/// built-in Needs work is the quieter red the owner asked for.

#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_report.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/report_page.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	void open_review(qt::workbench_window& aWindow)
	{
		aWindow.resize(1'440, 900);
		aWindow.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = 520U };
		aWindow.publish(opened);
		settle();
		aWindow.choose_modality(qt::review_modality::review);
		settle();
		aWindow.tray_tabs()->setCurrentIndex(2);
		auto profile        = core::agent_profile{ };
		profile.Id          = core::agent_profile_id{ 1U };
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		aWindow.agent_tab()->refresh();
		settle();
	}

	const auto review = std::string{ "# Review of #21\n\n**Verdict:** Needs changes\n\nOne thing must be fixed before this merges.\n\n"
									 "## Must fix\n\n### issue (blocking): A zero height still asserts\n\n`src/engine/renderer.cpp:12-13`\n\nThe height is not clamped.\n\n"
									 "## Worth a look\n\n### suggestion (non-blocking): Say where the engine moved\n\n`README.md`\n\nIt still points at `render/`.\n" };

	[[nodiscard]] auto finding(const core::defect_severity aSeverity, const std::uint32_t aLine, const std::string& aComment) -> core::agent_finding
	{
		auto made             = core::agent_finding{ };
		made.Provider         = core::agent_profile_id{ 1U };
		made.Severity         = aSeverity;
		made.Confidence       = core::agent_finding_confidence::high;
		made.Category         = core::defect_category::correctness;
		made.Path             = "src/engine/renderer.cpp";
		made.Lines            = { .Begin = aLine, .End = aLine };
		made.Claim            = "Finding at line " + std::to_string(aLine + 1U) + ".";
		made.SuggestedComment = aComment;
		return made;
	}

	[[nodiscard]] auto ink_of(const QWidget* aCard, const char* aName) -> QColor
	{
		const auto* const label = aCard == nullptr ? nullptr : aCard->findChild<QLabel*>(QString::fromLatin1(aName));
		return label == nullptr ? QColor{ } : label->palette().color(label->foregroundRole());
	}

	/// Both surfaces, in one profile's tones: the tab's cards and verdict,
	/// and the page's label, rule and verdict, each the profile's role.
	void expect_both_surfaces(qt::agent_tab_view& aTab, qt::agent_report_view& aPage, const qt::appearance_profile& aProfile, const std::string& aWhen)
	{
		aTab.adopt_appearance(aProfile);
		aPage.set_tone_colours(qt::tone_colours_of(aProfile));
		settle();
		const auto open    = aProfile.color(core::appearance_role::report_verdict_open);
		const auto partial = aProfile.color(core::appearance_role::report_verdict_partial);
		expect(aTab.tones() == aPage.colours().Tones, aWhen + ": the tab and the page hold the same three tones");
		expect(qt::report_tone_colour(core::report_tone::open, aPage.colours()) == open && qt::tone_colour(core::report_tone::open, aTab.tones()) == open,
			   aWhen + ": Needs work is the profile's role on both");

		const auto cards = aTab.turn_cards(0U);
		expect(cards.size() == 2U, aWhen + ": the review's two findings are two cards");
		expect(ink_of(cards[0], "agentFindingSeverity") == open, aWhen + ": Major on its card wears Needs work");
		expect(ink_of(cards[0], "agentFindingSuggestionLabel") == open, aWhen + ": and so does its suggested comment's issue (blocking)");
		expect(ink_of(cards[1], "agentFindingSeverity") == partial, aWhen + ": Minor on its card wears Half done");
		expect(ink_of(cards[1], "agentFindingSuggestionLabel") == partial, aWhen + ": and so does its suggestion (non-blocking)");
		const auto* const verdict = aTab.turn_verdict(0U);
		expect(verdict != nullptr && verdict->isVisibleTo(&aTab) && verdict->text().contains(QStringLiteral("Needs changes")) && verdict->text().contains(open.name()),
			   aWhen + ": the tab's verdict is filled with Needs work");

		const auto* const page = aPage.findChild<QTextEdit*>();
		expect(page != nullptr, aWhen + ": the page");
		const auto html = page->toHtml().toLower();
		expect(html.contains(QStringLiteral("background-color:%1").arg(open.name())), aWhen + ": the page's verdict mark is filled with Needs work");
		expect(html.contains(QStringLiteral("color:%1").arg(open.name())) && html.contains(QStringLiteral("issue (blocking)")), aWhen + ": its issue (blocking) wears it");
		expect(html.contains(QStringLiteral("color:%1").arg(partial.name())), aWhen + ": its suggestion (non-blocking) wears Half done");
	}

	void test_one_role_one_colour_on_both_surfaces()
	{
		qt::workbench_window window;
		open_review(window);
		auto* const agent = window.agent_tab();

		const auto block              = std::string{ "```bpr-findings\n{\"findings\":[{\"path\":\"src/engine/renderer.cpp\",\"startLine\":12,\"claim\":\"x\"}]}\n```" };
		const auto answer             = review + "\n" + block;
		auto       identity           = application::agent_invocation_projection_identity{ };
		identity.ApplicationSession   = core::application_session_id{ 1U };
		identity.Session.Id           = core::agent_session_id{ 9U };
		identity.InvocationGeneration = 1U;
		window.agent_panel()->admit(identity);
		window.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
		window.agent_panel()->stream(identity, application::agent_stream_event{ .Session = identity.Session.Id, .Sequence = 1U, .Text = answer });
		auto delivery    = application::agent_finding_delivery{ };
		delivery.Session = identity.Session.Id;
		delivery.Findings.push_back(application::agent_delivered_finding{ .Finding = finding(core::defect_severity::major, 11U, "issue (blocking): clamp the height too."), .Verdict = { } });
		delivery.Findings.push_back(application::agent_delivered_finding{ .Finding = finding(core::defect_severity::minor, 30U, "suggestion (non-blocking): say where it moved."), .Verdict = { } });
		window.finding_queue()->present(delivery);
		window.agent_panel()->finish(identity, application::agent_run_result{ .Session = identity.Session.Id, .Outcome = application::agent_run_outcome::success });
		settle();
		expect(agent->turn_count() == 1U && agent->turn_cards(0U).size() == 2U, "the review's findings are cards under its answer");

		auto page = qt::agent_report_view{ };
		page.resize(820, 900);
		page.show();
		page.present({ .Turn = 1U, .Title = QStringLiteral("Review of #21"), .Markdown = QString::fromStdString(review), .Question = QStringLiteral("Review this pull request"), .Byline = { } });
		settle();

		auto light = qt::appearance_profile{ };
		expect(light.variant() == core::appearance_variant::light || light.variant() == core::appearance_variant::dark, "a built-in profile");
		expect_both_surfaces(*agent, page, light, "the built-in theme");

		// A tone changed in Appearance shows on both.
		auto changed = qt::appearance_profile{ };
		expect(changed.customise(core::appearance_role::report_verdict_open, core::appearance_color{ .Red = 0x33U, .Green = 0x66U, .Blue = 0xAAU, .Alpha = 255U })
				   && changed.customise(core::appearance_role::report_verdict_partial, core::appearance_color{ .Red = 0x55U, .Green = 0x44U, .Blue = 0x22U, .Alpha = 255U }),
			   "two tones customised");
		expect_both_surfaces(*agent, page, changed, "a customised theme");
	}

	/// The owner, 2026-11-16: "tone major down a tiny bit". The built-in
	/// Needs work is a step quieter than the Agent tab's old red and the
	/// report's old slate red, and still plainly apart from Half done.
	void test_major_is_toned_down()
	{
		for (const auto variant : { core::appearance_variant::light, core::appearance_variant::dark }) {
			const auto& palette = qt::builtin_appearance_palette(variant);
			const auto  of      = [&palette](const core::appearance_role aRole) {
				const auto colour = palette[core::index_of(aRole)];
				return QColor{ colour.Red, colour.Green, colour.Blue };
			};
			const auto dark    = variant == core::appearance_variant::dark;
			const auto open    = of(core::appearance_role::report_verdict_open);
			const auto partial = of(core::appearance_role::report_verdict_partial);
			const auto tab     = dark ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
			const auto slate   = dark ? QColor{ 0xE5, 0xA0, 0x93 } : QColor{ 0xB2, 0x39, 0x2A };
			const auto which   = std::string{ dark ? "dark" : "light" };
			expect(open.hslSaturation() < slate.hslSaturation() && open.hslSaturation() < tab.hslSaturation(), which + ": Needs work is less saturated than either old red");
			expect(std::abs(open.lightness() - slate.lightness()) <= 8, which + ": at the slate's lightness, a tiny step");
			expect(std::abs(open.hslHue() - partial.hslHue()) >= 25, which + ": still plainly apart from Half done");
			const auto ground = of(core::appearance_role::code_background);
			const auto ratio  = qt::contrast_ratio(core::appearance_color{ .Red = static_cast<std::uint8_t>(open.red()), .Green = static_cast<std::uint8_t>(open.green()), .Blue = static_cast<std::uint8_t>(open.blue()), .Alpha = 255U },
												   core::appearance_color{ .Red = static_cast<std::uint8_t>(ground.red()), .Green = static_cast<std::uint8_t>(ground.green()), .Blue = static_cast<std::uint8_t>(ground.blue()), .Alpha = 255U });
			expect(ratio >= 4.5, which + ": readable on its ground");
			expect(qt::builtin_tone_colours(dark).Open == open, which + ": a view no theme reached reads the same built-in");
		}
	}
} // namespace

int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	test_one_role_one_colour_on_both_surfaces();
	test_major_is_toned_down();
	std::cout << "agent-tone-colours:pass\n";
	return EXIT_SUCCESS;
}
