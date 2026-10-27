// [[BPR-1782]]. Publish tour reads as working from the press until GitHub
// answers, as Compose review's Publish does: Compose review's waiting panel
// names what the captured plan sends, the controls that would change it are
// disabled, Stop stays, and the answer is said with Resume and Check status.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
namespace {
	using namespace bpr;
	using phase = application::tour_publication_phase;
	void expect(bool aPass, const char* aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	template <typename Widget>
	auto child(qt::review_tour_editor& aEditor, const char* aName) -> Widget*
	{
		auto* const value = aEditor.findChild<Widget*>(QString::fromUtf8(aName));
		expect(value != nullptr, aName);
		return value;
	}
	void save(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aSuffix)
	{
		if (aPrefix.empty()) {
			return;
		}
		QApplication::processEvents();
		expect(aEditor.grab().save(QString::fromStdString(aPrefix + aSuffix)), "render saved");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication           application{ argc, argv };
	const auto             prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };
	qt::review_tour_editor editor;
	editor.resize(1180, 760);
	editor.set_mode(qt::tour_editor_mode::page);
	editor.present({ .Draft = std::make_shared<const core::review_tour>(tests::review_tour_fixture()) });
	editor.show();
	QApplication::processEvents();

	auto* const publish  = child<QPushButton>(editor, "tourPublish");
	auto* const preview  = child<QPushButton>(editor, "tourPreparePublication");
	auto* const stop     = child<QPushButton>(editor, "tourStop");
	auto* const resume   = child<QPushButton>(editor, "tourResume");
	auto* const check    = child<QPushButton>(editor, "tourReconcile");
	auto* const mode     = child<QComboBox>(editor, "tourPublicationMode");
	auto* const describe = child<QCheckBox>(editor, "tourDescriptionEdit");
	auto* const status   = child<QLabel>(editor, "tourPublicationStatus");
	auto* const panel    = child<QWidget>(editor, "tourPublicationWorking");
	auto* const title    = child<QLabel>(editor, "tourPublicationWorkingTitle");
	auto* const stages   = child<QLabel>(editor, "tourPublicationWorkingStages");
	auto* const said     = child<QLabel>(editor, "tourPublicationWorkingStatus");
	auto* const link     = child<QLabel>(editor, "tourPublicationLink");

	// [[BPR-1787]]. The brief goes in the description unless the author says otherwise.
	expect(describe->isChecked() && describe->toolTip().contains(QStringLiteral("Problem and Solution go in the PR description")) && describe->toolTip().contains(QStringLiteral("Off: Problem and Solution go in the tour comment")), "Update PR description starts on and says where the brief goes");

	// Admitted: a previewed plan, Publish ready, nothing in the panel.
	editor.present_publication({ .Status = "Preview ready.", .Markdown = "## Tour\n\nWhat GitHub will show.", .Publish = true, .Reconcile = true });
	QApplication::processEvents();
	expect(publish->text() == QStringLiteral("Publish tour") && publish->isEnabled(), "admitted: Publish tour can be pressed");
	expect(preview->isEnabled() && mode->isEnabled() && describe->isEnabled() && !stop->isEnabled(), "admitted: the plan's choices are open, Stop is not");
	expect(panel->isHidden() && !status->isHidden() && status->text() == QStringLiteral("Preview ready."), "admitted: the status line says where it stands");

	// In flight: the press until GitHub answers.
	editor.present_publication({ .Status   = "Stop keeps what GitHub already has. Nothing will be sent twice.",
								 .Markdown = "## Tour\n\nWhat GitHub will show.",
								 .Phase    = phase::publishing,
								 .Stages   = { "The tour comment", "Stop 1, “Why”", "The description link" },
								 .Stop     = true });
	QApplication::processEvents();
	expect(publish->text() == QStringLiteral("Publishing…") && !publish->isEnabled(), "in flight: Publish reads as working and cannot be pressed again");
	expect(!preview->isEnabled() && !mode->isEnabled() && !describe->isEnabled(), "in flight: nothing that would change what is sent can be used");
	expect(stop->isEnabled() && !resume->isEnabled() && !check->isEnabled(), "in flight: Stop stays available");
	expect(!panel->isHidden() && status->isHidden(), "in flight: Compose review's waiting panel says it; the status line steps aside");
	expect(title->text() == QStringLiteral("Publishing the tour to GitHub…"), "in flight: the panel's title");
	expect(stages->text() == QStringLiteral("Sending, in order:\nThe tour comment\nStop 1, “Why”\nThe description link") && !stages->isHidden(), "in flight: the plan's own stages, in order");
	expect(said->text().startsWith(QStringLiteral("Stop keeps")) && panel->accessibleName() == title->text(), "in flight: what is safe, and an accessible name");
	save(editor, prefix, "-in-flight.png");

	// Stopping: still in flight, the status says so inside the panel.
	editor.present_publication({ .Status = "Stopping. Confirmed receipts are retained; check status before Resume.", .Phase = phase::publishing, .Stages = { "The tour comment" }, .Stop = true });
	expect(said->text().startsWith(QStringLiteral("Stopping.")) && stop->isEnabled() && !publish->isEnabled(), "stopping: said in the panel, Stop still live");

	// Success: published, with where GitHub shows it.
	editor.present_publication({ .Status = "Published: one COMMENT review with verified tour links.", .Markdown = "## Tour\n\nObserved.", .Phase = phase::published, .Link = "https://github.com/octo-org/review-repository/pull/17#pullrequestreview-501", .Reconcile = true });
	QApplication::processEvents();
	expect(publish->text() == QStringLiteral("Published") && !publish->isEnabled(), "success: Publish says Published and cannot repost");
	expect(panel->isHidden() && !status->isHidden() && status->text().startsWith(QStringLiteral("Published:")), "success: said in the status line");
	expect(!link->isHidden() && link->text() == QStringLiteral("On GitHub: https://github.com/octo-org/review-repository/pull/17#pullrequestreview-501"), "success: the way to see it");
	expect(preview->isEnabled() && mode->isEnabled() && describe->isEnabled() && !stop->isEnabled() && check->isEnabled(), "success: controls return; Check status stays");
	save(editor, prefix, "-done.png");

	// Failure: why, with Resume and Check status.
	editor.present_publication({ .Status = "Partially published compatibility comments. Completed identities are retained. Preview before Resume.", .Phase = phase::attention, .Resume = true, .Reconcile = true });
	QApplication::processEvents();
	expect(publish->text() == QStringLiteral("Publish tour") && !publish->isEnabled(), "failure: Publish returns to its name, not pressable");
	expect(!panel->isHidden() && title->text() == QStringLiteral("Publishing didn't finish") && stages->isHidden(), "failure: the panel says it did not finish");
	expect(said->text().startsWith(QStringLiteral("Partially published")) && status->isHidden() && link->isHidden(), "failure: why, once");
	expect(resume->isEnabled() && check->isEnabled() && !stop->isEnabled(), "failure: Resume and Check status recover");
	save(editor, prefix, "-failed.png");

	std::cout << "review-tour-publication-working:pass\n";
	return EXIT_SUCCESS;
}
