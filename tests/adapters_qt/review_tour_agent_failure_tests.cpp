// [[BPR-1892]], owner 2026-11-06 ("make the tour error a) stand out more and
// b) send you there"). A failed Tour tab agent request says so beside the
// control that made it, in the Agent tab's own words for the failure, with
// the way to the Agent tab's card for the run. It clears when the request is
// made again or succeeds, and Dismiss hides it. Pass a path prefix to save
// renders.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include <QApplication>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QToolButton>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
namespace {
	using namespace bpr;

	void expect(bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	template <typename Widget>
	auto child(QWidget& aParent, const char* aName) -> Widget*
	{
		auto* const value = aParent.findChild<Widget*>(QString::fromUtf8(aName));
		expect(value != nullptr, aName);
		return value;
	}
	void render(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		QApplication::processEvents();
		expect(aEditor.grab().save(QString::fromStdString(aPrefix + aName + ".png")), "render saved");
	}
	auto dark_palette(QPalette aPalette) -> QPalette
	{
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			aPalette.setColor(group, QPalette::Window, QColor{ 0x32, 0x32, 0x32 });
			aPalette.setColor(group, QPalette::Base, QColor{ 0x1E, 0x1E, 0x1E });
			aPalette.setColor(group, QPalette::Button, QColor{ 0x3C, 0x3C, 0x3C });
			aPalette.setColor(group, QPalette::WindowText, QColor{ 0xE6, 0xE6, 0xE6 });
			aPalette.setColor(group, QPalette::ButtonText, QColor{ 0xE6, 0xE6, 0xE6 });
			aPalette.setColor(group, QPalette::Text, QColor{ 0xE6, 0xE6, 0xE6 });
		}
		return aPalette;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };

	auto draft     = tests::review_tour_fixture();
	draft.Problem  = { };
	draft.Solution = { };
	qt::review_tour_editor editor;
	editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
	auto asked = 0;
	editor.set_writing_dispatch([&asked](core::writing_hook, qt::prose_capture) { ++asked; }, [] { });
	auto opened = 0;
	editor.set_agent_failure_dispatch([&opened] { ++opened; });
	editor.resize(380, 760);
	editor.show();
	editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor.show_problem();
	QApplication::processEvents();

	auto* const magic  = child<QPushButton>(editor, "tourProblemMagic");
	auto* const notice = editor.agent_failure_notice(core::writing_hook::problem);
	expect(notice != nullptr && !notice->isVisible() && editor.agent_failure_text(core::writing_hook::problem).isEmpty(), "nothing is said before a request fails");
	for (const auto hook : { core::writing_hook::solution, core::writing_hook::comment, core::writing_hook::outline }) {
		expect(editor.agent_failure_notice(hook) != nullptr, "every agent request's control has its notice");
	}

	// AC1. A failure: one line beside the helper, in the Agent tab's words.
	magic->click();
	expect(asked == 1, "the helper asks");
	const auto why = qt::agent_tab_view::run_failure_words(application::agent_run_outcome::nonzero_exit, QStringLiteral("Claude Code"));
	editor.show_agent_failure(core::writing_hook::problem, why);
	QApplication::processEvents();
	expect(notice->isVisible(), "a failed request shows its notice on the Tour tab");
	const auto said = editor.agent_failure_text(core::writing_hook::problem);
	expect(said == QStringLiteral("Draft from PR context failed. ") + why, "it names the request as its control did and says why in the Agent tab's own words: " + said.toStdString());
	expect(notice->parentWidget() == magic->parentWidget(), "the notice sits with the helper that made the request");
	const auto below = notice->mapTo(&editor, QPoint{ 0, 0 }).y() - (magic->mapTo(&editor, QPoint{ 0, 0 }).y() + magic->height());
	expect(below >= 0 && below <= 16, "it sits right under the helper's row");
	expect(!editor.agent_failure_notice(core::writing_hook::solution)->isVisible() && !editor.agent_failure_notice(core::writing_hook::outline)->isVisible(), "the other controls say nothing");
	auto* const show = child<QPushButton>(*notice, "tourAgentFailureOpen");
	expect(show->palette().color(QPalette::Active, QPalette::ButtonText) == qt::agent_accent(editor.palette()) && !show->icon().isNull(), "the way to the Agent tab wears the agent accent and spark");
	render(editor, prefix, "failure-light");

	// AC2, [[BPR-1900]] AC5. The run is the tour session's: Show the run
	// opens the session's transcript on this tab, and the Agent tab is not
	// asked for.
	auto       session = core::tour_agent_session{ };
	const auto run     = core::begin_tour_run(session, core::tour_agent_request::draft_field, core::tour_agent_tools::read_only, "Draft from PR context");
	static_cast<void>(core::finish_tour_run(session, *run, core::tour_agent_run_state::failed, why.toStdString()));
	editor.present_tour_session(session, std::nullopt);
	expect(show->text() == QStringLiteral("Show the run"), "the notice's way is to the run");
	expect(!editor.tour_agent()->transcript()->isVisibleTo(&editor), "the transcript is never shown by itself");
	show->click();
	expect(opened == 0 && editor.tour_agent()->transcript()->isVisibleTo(&editor), "Show the run opens the tour session's transcript, not the Agent tab");
	expect(editor.tour_agent()->transcript_text().contains(why), "the transcript says how the run ended, in the same words");
	expect(notice->isVisible(), "following it leaves the notice until it is dismissed or the request is made again");

	// AC3. Dismiss hides it; making the request again clears it.
	child<QToolButton>(*notice, "tourAgentFailureDismiss")->click();
	expect(!notice->isVisible() && editor.agent_failure_text(core::writing_hook::problem).isEmpty(), "Dismiss hides it");
	editor.show_agent_failure(core::writing_hook::problem, why);
	expect(notice->isVisible(), "a second failure shows again");
	magic->click();
	expect(asked == 2 && !notice->isVisible(), "making the request again clears it");
	editor.show_agent_failure(core::writing_hook::problem, why);
	editor.clear_agent_failure(core::writing_hook::problem);
	expect(!notice->isVisible(), "a success clears it");

	// The outline's request, beside Suggest an outline.
	editor.show_stops();
	QApplication::processEvents();
	auto* const suggest = child<QPushButton>(editor, "tourSuggestOutline");
	suggest->click();
	editor.show_agent_failure(core::writing_hook::outline, qt::agent_tab_view::run_failure_words(application::agent_run_outcome::timeout, QStringLiteral("Claude Code")));
	QApplication::processEvents();
	auto* const outline = editor.agent_failure_notice(core::writing_hook::outline);
	expect(outline->isVisible() && editor.agent_failure_text(core::writing_hook::outline).startsWith(QStringLiteral("Suggest an outline failed. ")), "Suggest an outline's failure is said beside it");
	expect(outline->parentWidget() == suggest->parentWidget(), "with Suggest an outline");

	QApplication::setPalette(dark_palette(QApplication::palette()));
	QApplication::processEvents();
	expect(child<QPushButton>(*outline, "tourAgentFailureOpen")->palette().color(QPalette::Active, QPalette::ButtonText) == qt::agent_accent(editor.palette()), "the accent follows the palette");
	render(editor, prefix, "outline-failure-dark");
	suggest->click();
	expect(!outline->isVisible(), "asking again clears it");

	std::cout << "review-tour-agent-failure:pass\n";
	return EXIT_SUCCESS;
}
