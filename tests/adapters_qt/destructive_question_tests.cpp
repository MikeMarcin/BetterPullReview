#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLabel>
#include <QPointer>
#include <QTextEdit>
#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>

// [[BPR-1345]] v003. The one question asked before something in the compose
// flow is deleted (owner, delete-comment v002 and discard-review v002): its
// structure, its keys, the comment in its box, and its red in every theme.
namespace {
	using namespace bpr;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "destructive-question:fail " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void settle()
	{
		for (auto turn = 0; turn < 4; ++turn) {
			QApplication::processEvents();
		}
	}
	/// Red: a hue within 25 degrees of pure red, and plainly saturated.
	[[nodiscard]] auto red(const QColor& aColour) -> bool
	{
		const auto hue = aColour.hslHue();
		return aColour.hslSaturationF() > 0.35 && (hue <= 25 || hue >= 335);
	}
	[[nodiscard]] auto below(const QWidget& aUpper, const QWidget& aLower, const QWidget& aIn) -> bool
	{
		return aUpper.mapTo(&aIn, QPoint{ 0, aUpper.height() }).y() <= aLower.mapTo(&aIn, QPoint{ }).y();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   renders = argc == 2 ? QString::fromLocal8Bit(argv[1]) : QString{ };
	if (!renders.isEmpty()) {
		QDir{ }.mkpath(renders);
	}
	QWidget host;
	host.resize(900, 640);
	host.show();
	settle();
	auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	// Structure: the title, the close mark, the muted words ending as the
	// owner asked, Cancel then the deleting button.
	{
		QPointer<qt::destructive_question> question = new qt::destructive_question{ host, QStringLiteral("deleteCommentConfirmation"), QStringLiteral("Delete this comment from your review?"),
																					QStringLiteral("This cannot be undone."), QStringLiteral("Delete comment") };
		question->set_subject(QStringLiteral("renderer.cpp"), qt::comment_lines(core::diff_side::right, 3U, 2U), "nitpick: these two lines could be **one**.\n\nSee `scale`.", "pending:7:2",
							  qt::default_feedback_mark_theme(host.palette()), code);
		auto accepted = 0;
		auto rejected = 0;
		QObject::connect(question, &QDialog::accepted, [&] { ++accepted; });
		QObject::connect(question, &QDialog::rejected, [&] { ++rejected; });
		question->open();
		settle();
		expect(question->isVisible() && question->windowModality() == Qt::WindowModal && question->testAttribute(Qt::WA_DeleteOnClose)
				   && (question->windowFlags() & Qt::FramelessWindowHint) != 0,
			   "a window-modal, frameless panel deleted on close");
		expect(question->question() == QStringLiteral("Delete this comment from your review?") && question->windowTitle() == question->question()
				   && question->accessibleName() == question->question() && question->title_label()->font().bold(),
			   "the question is the bold title, the window's title and its accessible name");
		expect(question->detail() == QStringLiteral("This cannot be undone.") && !question->detail().contains(QStringLiteral("computer")),
			   "the words say only that it cannot be undone (owner: \"We don't need to say 'it is deleted from this computer'\")");
		expect(question->location() == QStringLiteral("renderer.cpp, lines 3–4") && question->location_label()->text() == QStringLiteral("renderer.cpp")
				   && question->location_label()->font().weight() >= QFont::DemiBold,
			   "the location is its own line: the file, then its lines");
		auto* const box = question->subject_box();
		expect(box != nullptr && box->isVisible() && question->subject_view() != nullptr && box->isAncestorOf(question->subject_view()),
			   "the comment is in its own box");
		expect(below(*question->location_label(), *box, *question) && below(*question->title_label(), *question->location_label(), *question)
				   && below(*box, *question->detail_label(), *question) && !box->isAncestorOf(question->detail_label()) && !box->isAncestorOf(question->title_label()),
			   "title, location, the comment's box, then the question's words: the comment starts below the location and apart from the dialog's text");
		const auto  rendered = question->subject_view()->rendered_view()->toPlainText();
		auto* const mark     = question->subject_mark();
		expect(rendered.contains(QStringLiteral("these two lines could be one.")) && !rendered.contains(QStringLiteral("**")) && question->subject_view()->in_place()
				   && question->subject_view()->header_length() == QStringLiteral("nitpick:").size() && mark != nullptr && mark->isVisible() && !mark->pixmap().isNull()
				   && mark->accessibleName().contains(QStringLiteral("nitpick")) && box->isAncestorOf(mark)
				   && mark->mapTo(question.data(), QPoint{ mark->width(), 0 }).x() <= question->subject_view()->mapTo(question.data(), QPoint{ }).x(),
			   "the comment is rendered as the drawer reads it: its label's mark in the left gutter, its header set apart, the Markdown rendered, not its source");
		auto* const close = question->close_button();
		expect(close != nullptr && close->isVisible() && close->accessibleName() == QStringLiteral("Close") && close->mapTo(question.data(), QPoint{ }).y() < box->mapTo(question.data(), QPoint{ }).y()
				   && close->mapTo(question.data(), QPoint{ }).x() > question->title_label()->mapTo(question.data(), QPoint{ }).x(),
			   "a close mark at the top right, named");
		auto* const cancel  = question->cancel_button();
		auto* const confirm = question->confirm_button();
		expect(cancel->isDefault() && QApplication::focusWidget() == cancel && cancel->role() == qt::question_button_role::neutral,
			   "Cancel is the default and takes the focus");
		expect(confirm->role() == qt::question_button_role::destructive && confirm->property("questionRole").toString() == QStringLiteral("destructive")
				   && confirm->accessibleName() == QStringLiteral("Delete comment") && !confirm->accessibleDescription().isEmpty() && !confirm->isDefault()
				   && cancel->mapTo(question.data(), QPoint{ }).x() < confirm->mapTo(question.data(), QPoint{ }).x()
				   && cancel->mapTo(question.data(), QPoint{ }).y() == confirm->mapTo(question.data(), QPoint{ }).y(),
			   "the deleting button is named for what it does, at the right of Cancel on one row");
		expect(question->accessibleDescription().contains(QStringLiteral("renderer.cpp, lines 3–4")) && question->accessibleDescription().contains(QStringLiteral("could be one")),
			   "an assistive reader hears where the comment is and what it says");
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(question.data(), &escape);
		settle();
		expect(rejected == 1 && accepted == 0, "Esc cancels");
	}
	{
		auto* const question = new qt::destructive_question{ host, QStringLiteral("q"), QStringLiteral("Delete this comment?"), QStringLiteral("This cannot be undone."), QStringLiteral("Delete comment") };
		auto        accepted = 0;
		auto        rejected = 0;
		QObject::connect(question, &QDialog::accepted, [&] { ++accepted; });
		QObject::connect(question, &QDialog::rejected, [&] { ++rejected; });
		question->open();
		settle();
		expect(!question->subject_box()->isVisible() && question->subject_view() == nullptr, "with no subject, no box");
		QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(question->cancel_button(), &enter);
		settle();
		expect(rejected == 1 && accepted == 0, "Return on the focused default keeps it");
	}
	{
		auto* const question = new qt::destructive_question{ host, QStringLiteral("q"), QStringLiteral("Delete this comment?"), QStringLiteral("This cannot be undone."), QStringLiteral("Delete comment") };
		auto        accepted = 0;
		auto        rejected = 0;
		QObject::connect(question, &QDialog::accepted, [&] { ++accepted; });
		QObject::connect(question, &QDialog::rejected, [&] { ++rejected; });
		question->open();
		settle();
		question->close_button()->click();
		settle();
		expect(rejected == 1 && accepted == 0, "the close mark cancels");
	}
	{
		auto* const question = new qt::destructive_question{ host, QStringLiteral("q"), QStringLiteral("Discard your review and its 2 pending comments?"), QStringLiteral("This cannot be undone."), QStringLiteral("Discard review") };
		auto        accepted = 0;
		QObject::connect(question, &QDialog::accepted, [&] { ++accepted; });
		question->open();
		settle();
		question->confirm_button()->click();
		settle();
		expect(accepted == 1, "the deleting button confirms");
	}
	expect(qt::comment_lines(core::diff_side::left, 4U, std::nullopt) == QStringLiteral("base line 5") && qt::comment_lines(core::diff_side::right, 430U, std::nullopt) == QStringLiteral("line 431"),
		   "lines are said as the pending rows say them");

	// The red, in every theme: the deleting button's words red on a red
	// tint and readable (4.5:1, 7:1 in high contrast), Cancel's neutral; the
	// muted words readable; Discard review's red words readable on the page.
	const auto modes = std::array{ std::pair{ core::appearance_mode::light, "light" }, std::pair{ core::appearance_mode::dark, "dark" },
								   std::pair{ core::appearance_mode::high_contrast_light, "high-contrast-light" },
								   std::pair{ core::appearance_mode::high_contrast_dark, "high-contrast-dark" } };
	for (const auto& [mode, name] : modes) {
		QWidget themed;
		themed.resize(900, 640);
		qt::appearance_profile profile;
		static_cast<void>(profile.select_mode(mode));
		profile.apply_to(themed);
		themed.show();
		settle();
		auto* const question = new qt::destructive_question{ themed, QStringLiteral("deleteCommentConfirmation"), QStringLiteral("Delete this comment from your review?"),
															 QStringLiteral("This cannot be undone."), QStringLiteral("Delete comment") };
		question->set_subject(QStringLiteral("renderer.cpp"), qt::comment_lines(core::diff_side::right, 3U, 2U), "nitpick: these two lines could be one.", "pending:7:2",
							  qt::default_feedback_mark_theme(themed.palette()), code);
		question->open();
		settle();
		const auto colours = question->colours();
		const auto ratio   = colours.HighContrast ? 7.0 : 4.5;
		const auto text    = themed.palette().color(QPalette::Active, QPalette::WindowText);
		if (mode == core::appearance_mode::high_contrast_light || mode == core::appearance_mode::high_contrast_dark) {
			expect(colours.HighContrast, "a high-contrast theme is read as one");
		}
		expect(question->palette().color(QPalette::Window) == themed.palette().color(QPalette::Window), "the question takes its window's theme");
		expect(red(colours.Danger) && colours.DangerTint.red() > colours.DangerTint.green() && colours.DangerTint.red() > colours.DangerTint.blue(),
			   "the deleting button is red words on a red tint");
		expect(qt::destructive_contrast(colours.Danger, colours.DangerTint) >= ratio, "its words read on its tint");
		expect(qt::destructive_contrast(colours.Muted, colours.Surface) >= ratio && qt::destructive_contrast(colours.Ink, colours.Surface) >= ratio,
			   "the muted words and the title read on the panel");
		expect(colours.Ink == text && !red(colours.Neutral), "Cancel is neutral, in the text's ink");
		expect(qt::destructive_contrast(colours.Edge, colours.Surface) > 1.1, "the panel has a visible edge");
		qt::danger_tool_button discard{ &themed };
		discard.setText(QStringLiteral("Discard review"));
		expect(red(discard.ink()) && qt::destructive_contrast(discard.ink(), themed.palette().color(QPalette::Active, QPalette::Window)) >= ratio,
			   "Discard review's words are red and read on the page");
		if (!renders.isEmpty()) {
			expect(question->grab().save(renders + QStringLiteral("/delete-comment-") + QString::fromLatin1(name) + QStringLiteral(".png")), "render saved");
			auto* const discardQuestion = new qt::destructive_question{ themed, QStringLiteral("discardReviewConfirmation"), QStringLiteral("Discard your review and its 2 pending comments?"),
																		QStringLiteral("This cannot be undone."), QStringLiteral("Discard review") };
			discardQuestion->open();
			settle();
			expect(discardQuestion->grab().save(renders + QStringLiteral("/discard-review-") + QString::fromLatin1(name) + QStringLiteral(".png")), "render saved");
			discardQuestion->reject();
		}
		question->reject();
		settle();
	}
	std::cout << "destructive-question:pass scale=" << host.devicePixelRatioF() << '\n';
	return EXIT_SUCCESS;
}
