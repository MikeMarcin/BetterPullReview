// The Markdown readers mark the link the keyboard is on with the window's
// focus ring, never by selecting the link's words, and a press of the
// pointer on a link is the link's: one click follows it whatever the reader
// has selected. The owner, 2026-11-25, of a report link that came up
// selected: "I cant actually click go to draft properly until I deselect it
// and then click it again." The report's page was given the ring then; here
// is the same for every reader of Markdown, each in the surface that holds
// it:
//
// - a comment's body, in place in its open thread;
// - a report that does not follow the convention, drawn as its Markdown;
// - a code box (a permalink's lines), whose header's file and commit are
//   links.
//
// Keys and clicks go through the window system.

#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/application/markdown_blob_snippet_host.hpp"
#include "bpr/core/markdown_navigation.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QStyle>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	using conversation = application::github_conversation_projection;

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	/// Only an active window's focus widget has focus, and a window kept off
	/// screen is never made active.
	void make_active(QWidget& aWindow)
	{
		QT_WARNING_PUSH
		QT_WARNING_DISABLE_DEPRECATED
		QApplication::setActiveWindow(&aWindow);
		QT_WARNING_POP
		settle();
	}

	struct link_span
	{
		int     First = 0;
		int     Last  = 0;
		QString Words;
	};

	/// The reader's links in reading order; one written in several runs is one.
	[[nodiscard]] auto links_of(const QTextDocument& aDocument) -> std::vector<link_span>
	{
		auto links = std::vector<link_span>{ };
		auto hrefs = std::vector<QString>{ };
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			for (auto run = block.begin(); !run.atEnd(); ++run) {
				const auto part = run.fragment();
				const auto href = part.charFormat().anchorHref();
				if (href.isEmpty()) {
					continue;
				}
				if (!links.empty() && links.back().Last == part.position() && hrefs.back() == href) {
					links.back().Last = part.position() + part.length();
					links.back().Words += part.text();
					continue;
				}
				links.push_back({ .First = part.position(), .Last = part.position() + part.length(), .Words = part.text() });
				hrefs.push_back(href);
			}
		}
		return links;
	}

	[[nodiscard]] auto colour_gap(const QColor& aOne, const QColor& aOther) -> int
	{
		return std::abs(aOne.red() - aOther.red()) + std::abs(aOne.green() - aOther.green()) + std::abs(aOne.blue() - aOther.blue());
	}

	/// A point inside the words of a link.
	[[nodiscard]] auto point_in(const QTextEdit& aText, const link_span& aLink) -> QPointF
	{
		const auto boxes = qt::text_focus_boxes(aText, aLink.First, aLink.Last);
		expect(!boxes.empty(), "the link's words stand in the reader");
		return boxes.front().center();
	}

	/// What the accessibility bridge was last told to say.
	QString said; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

	void hear(QAccessibleEvent* const aEvent)
	{
		if (aEvent->type() == QAccessible::Announcement) {
			said = static_cast<QAccessibleAnnouncementEvent*>(aEvent)->message();
		}
	}

	/// `BPR_MARKDOWN_LINK_RENDER_DIR`, when set, receives each reader's window
	/// with the keyboard on a link, in a folder named for the platform and
	/// style that drew it.
	void render(QWidget& aWindow, const std::string& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MARKDOWN_LINK_RENDER_DIR");
		if (root.isEmpty()) {
			return;
		}
		const auto folder = QStringLiteral("%1/%2-%3").arg(root, QGuiApplication::platformName(), QApplication::style()->name().toLower());
		expect(QDir{ }.mkpath(folder), "the render folder exists");
		const auto file = QString::fromStdString(aName).replace(QLatin1Char{ ' ' }, QLatin1Char{ '-' }).remove(QLatin1Char{ '\'' });
		expect(aWindow.grab().save(QStringLiteral("%1/%2-%3.png").arg(folder, file).arg(qRound(aWindow.devicePixelRatioF() * 100.0)), "PNG"), "the render is saved: " + aName);
	}

	struct reader
	{
		std::string             Name;
		qt::markdown_body_view* View = nullptr;
		/// How many links the reader's holder has been asked to follow.
		std::function<int()> Followed;
	};

	void check(const reader& aReader)
	{
		const auto  name = aReader.Name + ": ";
		auto* const text = aReader.View->rendered_view();
		expect(text != nullptr && text->isVisible(), name + "the reader shows its rendered text");
		const auto links = links_of(*text->document());
		expect(links.size() >= 2U, name + "the reader holds two links");
		const auto& first    = links[0];
		const auto& second   = links[1];
		const auto  selected = [text] { return text->textCursor().selectedText().toStdString(); };
		const auto  on       = [&aReader] { return aReader.View->keyboard_link().toStdString(); };
		const auto  grab     = [text] {
			settle();
			return text->viewport()->grab().toImage();
		};
		const auto click = [text](const link_span& aLink) { tests::window_system_clicks(*text->viewport(), point_in(*text, aLink), 1); };
		expect(!text->textCursor().hasSelection() && on().empty(), name + "shown, the reader selects nothing and the keyboard is on no link");

		// The keyboard. Tab reaches the first link: it wears the window's
		// focus ring, a screen reader hears its words, and none are selected.
		text->setFocus(Qt::TabFocusReason);
		settle();
		expect(text->hasFocus(), name + "the reader takes the keyboard");
		said.clear();
		tests::window_system_key(*text, Qt::Key_Tab);
		expect(text->hasFocus() && on() == first.Words.toStdString(), name + "Tab reaches the first link, \"" + first.Words.toStdString() + "\"; the keyboard is on \"" + on() + "\"");
		expect(!text->textCursor().hasSelection(), name + "Tab selects no text; \"" + selected() + "\" is selected");
		expect(said == first.Words, name + "a screen reader hears the link Tab reached: \"" + said.toStdString() + "\"");
		const auto ringed = grab();
		render(*text->window(), aReader.Name + " ringed");
		// The ring shows while the reader holds the keyboard, and only then:
		// without the keyboard the link is drawn as any link is.
		text->clearFocus();
		const auto plain = grab();
		text->setFocus(Qt::OtherFocusReason);
		expect(grab() == ringed && on() == first.Words.toStdString(), name + "with the keyboard back the ring is on the same link");
		const auto ring  = qt::focus_ring_colour(text->palette(), QPalette::Base);
		const auto scale = ringed.devicePixelRatio();
		const auto boxes = qt::text_focus_boxes(*text, first.First, first.Last);
		// The ring stands no more than a few pixels outside its words: in
		// device pixels, as far as the ring's width and four pixels more.
		const auto reach = static_cast<int>(std::ceil((qt::focus_ring_width + 4.0) * scale));
		const auto band  = [scale, reach](const QRectF& aWords) {
			return QRectF{ aWords.left() * scale, aWords.top() * scale, aWords.width() * scale, aWords.height() * scale }.toAlignedRect().adjusted(-reach, -reach, reach, reach);
		};
		struct side
		{
			const char* Name;
			QPointF     Middle;
			QPoint      Out;
		};
		for (const auto& words : boxes) {
			// On each side of the words, at the side's middle, the ring's
			// own colour is drawn where the reader's ground was: a little
			// outside the words, or at the reader's edge where the words
			// stand at it.
			const auto centre = words.center();
			for (const auto& [which, middle, out] : { side{ "left", { words.left(), centre.y() }, { -1, 0 } }, side{ "right", { words.right(), centre.y() }, { 1, 0 } },
													  side{ "top", { centre.x(), words.top() }, { 0, -1 } }, side{ "bottom", { centre.x(), words.bottom() }, { 0, 1 } } }) {
				auto found = false;
				for (auto step = -reach / 2; step <= reach && !found; ++step) {
					const auto at = (middle * scale).toPoint() + out * step;
					found         = ringed.rect().contains(at) && colour_gap(ringed.pixelColor(at), ring) <= 12 && colour_gap(plain.pixelColor(at), ring) > 60;
				}
				expect(found, name + "the keyboard's link wears the window's focus ring on its " + which + " side (ring " + ring.name().toStdString() + ")");
			}
		}
		// The ring is all that changed: nothing is drawn as selected.
		for (auto y = 0; y < ringed.height(); ++y) {
			for (auto x = 0; x < ringed.width(); ++x) {
				if (ringed.pixel(x, y) == plain.pixel(x, y)) {
					continue;
				}
				const auto inside = std::ranges::any_of(boxes, [&](const QRectF& aWords) { return band(aWords).contains(x, y); });
				expect(inside, name + "with the keyboard on a link only its ring is drawn: the pixel at " + std::to_string(x) + "," + std::to_string(y) + " changed too");
			}
		}
		// Return follows the link the keyboard is on, which stays there.
		auto followed = aReader.Followed();
		tests::window_system_key(*text, Qt::Key_Return);
		expect(aReader.Followed() == followed + 1, name + "Return follows the keyboard's link: followed " + std::to_string(aReader.Followed() - followed) + " times");
		expect(on() == first.Words.toStdString() && !text->textCursor().hasSelection(), name + "and the keyboard stays on it, with nothing selected");
		// Tab goes on to the next link, Shift+Tab back; the ring goes along.
		tests::window_system_key(*text, Qt::Key_Tab);
		expect(on() == second.Words.toStdString() && !text->textCursor().hasSelection(), name + "Tab goes on to \"" + second.Words.toStdString() + "\" and selects nothing; the keyboard is on \"" + on() + "\"");
		const auto moved = grab();
		for (const auto& words : boxes) {
			const auto area = band(words).intersected(moved.rect());
			if (std::ranges::none_of(qt::text_focus_boxes(*text, second.First, second.Last), [&](const QRectF& aOther) { return band(aOther).intersects(area); })) {
				expect(moved.copy(area) == plain.copy(area), name + "the link the keyboard left wears no ring");
			}
		}
		tests::window_system_key(*text, Qt::Key_Backtab, Qt::ShiftModifier);
		expect(on() == first.Words.toStdString() && grab() == ringed, name + "Shift+Tab goes back to the first link, in its ring");
		// An arrow key is the caret's: the keyboard is on no link again.
		tests::window_system_key(*text, Qt::Key_Right);
		expect(on().empty() && !text->textCursor().hasSelection(), name + "an arrow key takes the keyboard off the link and selects nothing");

		// The pointer. One click follows a link, selects nothing and leaves
		// the keyboard on no link.
		followed = aReader.Followed();
		click(first);
		expect(aReader.Followed() == followed + 1, name + "one click follows a link: followed " + std::to_string(aReader.Followed() - followed) + " times");
		expect(!text->textCursor().hasSelection() && on().empty(), name + "the click selects nothing; \"" + selected() + "\" is selected");
		// The owner's screen: the link's own words selected. The first click
		// on it follows it, with no click anywhere else before it.
		auto words = QTextCursor{ text->document() };
		words.setPosition(first.First);
		words.setPosition(first.Last, QTextCursor::KeepAnchor);
		text->setTextCursor(words);
		expect(text->textCursor().selectedText() == first.Words, name + "the link's own words are selected");
		click(first);
		expect(aReader.Followed() == followed + 2, name + "with its own words selected, the first click on the link follows it: followed " + std::to_string(aReader.Followed() - followed - 1) + " times");
		expect(text->textCursor().selectedText() == first.Words, name + "and the click leaves the selection as it was");
		click(first);
		expect(aReader.Followed() == followed + 3, name + "and so does the next click");
		// Whatever the reader has selected, a link takes one click.
		text->selectAll();
		const auto whole = selected();
		expect(!whole.empty(), name + "the whole body is selected");
		click(second);
		click(first);
		expect(aReader.Followed() == followed + 5, name + "with the whole body selected, one click each follows two links: followed " + std::to_string(aReader.Followed() - followed - 3) + " times");
		expect(selected() == whole, name + "a click on a link changes no selection");
		// The ring ends at the pointer's press, and the click still follows.
		auto caret = text->textCursor();
		caret.movePosition(QTextCursor::Start);
		text->setTextCursor(caret);
		tests::window_system_key(*text, Qt::Key_Tab);
		expect(!on().empty(), name + "Tab puts the keyboard on a link again");
		const auto ringedLink = links_of(*text->document())[on() == first.Words.toStdString() ? 0U : 1U];
		click(ringedLink);
		expect(aReader.Followed() == followed + 6 && on().empty() && !text->textCursor().hasSelection(), name + "one click on the ringed link follows it, ends the ring and selects nothing");
		// A press that leaves its link follows nothing and selects nothing.
		const auto from = point_in(*text, first);
		tests::window_system_mouse(*text->viewport(), from, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
		tests::window_system_mouse(*text->viewport(), from + QPointF{ 0.0, 400.0 }, QEvent::MouseMove, Qt::LeftButton);
		tests::window_system_mouse(*text->viewport(), from + QPointF{ 0.0, 400.0 }, QEvent::MouseButtonRelease, Qt::NoButton);
		expect(aReader.Followed() == followed + 6, name + "a press dragged off its link follows nothing");
		expect(!text->textCursor().hasSelection(), name + "and it drags no selection out of the link; \"" + selected() + "\" is selected");
		// A double click is one click of the link, not two.
		tests::window_system_clicks(*text->viewport(), point_in(*text, first), 2);
		expect(aReader.Followed() == followed + 7, name + "a double click follows its link once: followed " + std::to_string(aReader.Followed() - followed - 6) + " times");
		expect(!text->textCursor().hasSelection(), name + "and selects none of its words; \"" + selected() + "\" is selected");
		std::cout << "markdown-link-mark: " << aReader.Name << " (" << links.size() << " links, scale " << scale << ")\n";
	}

	[[nodiscard]] auto thread(const std::string& aId) -> conversation
	{
		auto made               = conversation{ };
		made.Id                 = aId;
		made.Kind               = application::github_conversation_kind::review_thread;
		made.Path               = "src/engine/renderer.cpp";
		made.Line               = 12U;
		made.DiffSide           = application::github_conversation_diff_side::right;
		made.AnchorAvailability = application::github_conversation_anchor_availability::current;
		return made;
	}

	[[nodiscard]] auto post(const std::string& aId, const std::string& aParent, const std::string& aBody) -> conversation
	{
		auto made        = conversation{ };
		made.Id          = aId;
		made.ParentId    = aParent;
		made.Kind        = application::github_conversation_kind::review_comment;
		made.AuthorLogin = "jiayin";
		made.Body        = aBody;
		made.Path        = "src/engine/renderer.cpp";
		made.Line        = 12U;
		return made;
	}

	/// A comment's body, in place in its open thread. Its first link starts
	/// the post, where the reader has no room beside or above its words.
	void check_a_comment()
	{
		auto view = qt::conversation_cards_view{ };
		view.resize(480, 700);
		view.show();
		make_active(view);
		auto followed = 0;
		view.set_link_dispatch([&followed](const std::string&, const std::string&) { ++followed; });
		view.adopt({ thread("t"), post("p", "t", "[The resize rule](https://example.test/resize) says to clamp both sizes before the next frame. The swap chain asserts on a zero extent, as [its notes](https://example.test/swap) say.") });
		view.open_thread("t");
		settle();
		auto* const body = dynamic_cast<qt::markdown_body_view*>(view.open_card()->findChild<QWidget*>(QStringLiteral("threadPostReader")));
		expect(body != nullptr && body->in_place(), "the comment reads in place through the shared reader");
		check({ .Name = "a comment's body", .View = body, .Followed = [&followed] { return followed; } });
	}

	/// A report that does not follow the convention is drawn as its Markdown.
	void check_a_report_as_markdown()
	{
		auto view = qt::agent_report_view{ };
		view.resize(900, 700);
		view.show();
		make_active(view);
		auto followed = 0;
		view.set_link_dispatch([&followed](const QString&) { ++followed; });
		view.present({ .Turn     = 1U,
					   .Title    = QStringLiteral("Notes on #5"),
					   .Markdown = QStringLiteral("The move is clean and every include resolves. The packaging script still collects shaders from the old folder: see [the build log](https://example.test/log) and "
												  "[the plan](https://example.test/plan) before merging.\n"),
					   .Question = QStringLiteral("What should I look at?"),
					   .Byline   = { } });
		settle();
		expect(!view.dressed_shown(), "a report without the convention's sections is drawn as its Markdown");
		auto* const body = dynamic_cast<qt::markdown_body_view*>(view.findChild<QWidget*>(QStringLiteral("agentReportBody")));
		expect(body != nullptr && body->isVisible() && !view.dressed_view()->isVisible(), "the report's Markdown reader shows it");
		check({ .Name = "a report drawn as Markdown", .View = body, .Followed = [&followed] { return followed; } });
	}

	/// A code box: a permalink's lines under a header whose file and commit
	/// are links.
	void check_a_code_box()
	{
		const auto standing = std::string{ "https://github.com/octo-org/widgets/blob/0123456789abcdef0123456789abcdef01234567/src/frame.cpp#L3-L5" };
		auto       view     = qt::markdown_body_view{ };
		view.resize(640, 420);
		view.show();
		make_active(view);
		auto followed = 0;
		view.set_link_dispatch([&followed](const std::string&) { ++followed; });
		view.set_permalink_repository(core::markdown_repository{ .Host = "github.com", .Owner = "octo-org", .Repository = "widgets" });
		view.set_permalink_dispatch([](const std::vector<application::markdown_blob_snippet_request>&) { });
		view.set_body("The width is clamped here:\n\n" + standing + "\n", "code-box:1");
		const auto permalink = core::recognize_markdown_blob_permalink(standing);
		expect(permalink.has_value(), "the body's address is a blob permalink");
		view.apply_permalink_snippet({ .Permalink = *permalink,
									   .Lines     = std::make_shared<const std::vector<std::string>>(std::vector<std::string>{ "int clamp_width(int aWidth, int aMaximum)", "{", "\treturn std::clamp(aWidth, 0, aMaximum);" }) });
		settle();
		expect(view.permalink_snippets_shown() == 1U, "the permalink reads as its code box");
		check({ .Name = "a code box", .View = &view, .Followed = [&followed] { return followed; } });
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	auto       application = QApplication{ aArgc, aArgv };
	const auto previous    = QAccessible::installUpdateHandler(hear);
	check_a_comment();
	check_a_report_as_markdown();
	check_a_code_box();
	static_cast<void>(QAccessible::installUpdateHandler(previous));
	std::cout << "markdown-link-mark-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
