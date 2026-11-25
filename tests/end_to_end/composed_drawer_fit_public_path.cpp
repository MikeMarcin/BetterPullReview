/// \file
/// \brief The right drawer of the composed window never pans sideways and
/// never cuts a control off, in every state its courses reach.
///
/// \details Owner, attended review 2026-11-25, of the tour chat at its
/// narrowest: "No, doesnt look like your screenshots.", and of the card of a
/// session that read an earlier head: "This made the drawer have a horizontal
/// scroll which should have been disallowed by our tests."
///
/// Both looks had been drawn and measured in a window nothing composed: the
/// tour chat as a window of its own, 300 pixels wide, and the Agent tab in a
/// workbench window with no application behind it. The application's window
/// puts the tour chat in the Tour tab's second column, beside a tour that
/// keeps 340 pixels, in a drawer whose handle a person drags; the tour's own
/// agent sets the chat's prompts, code font and notices. A course over the
/// parts alone could pass while the window a person opens did not.
///
/// This course is the application: the desktop composition on
/// returning-review-v1, signed in, with the Fixture agent, driven through the
/// window's own gestures. It walks the right drawer through its states:
///
/// - Comments, with the pull request's threads;
/// - the Agent tab ready, answered, with a review's report, with a turn
///   running over a pending list, with the report of a run that stopped
///   before answering and with that run's log shown, and, after the pull
///   request's head moves, with the card of a transcript replay that read the
///   earlier head, the card of a question refused because its session's
///   workspace was not moved, and the new session that card's New session
///   opens, with the refused question running in it and then answered;
/// - the Tour tab with nothing drafted, while Draft Tour runs, with the
///   draft's proposal waiting (each proposed stop with its choices, before
///   any is applied), with a proposed text being edited, with Reset tour
///   asked and done, and with one proposed stop added while the other waits;
/// - the Tour tab's route, a stop being written and its removal asked, and
///   its chat column: about a stop with nothing asked yet, answered, a turn
///   running over a pending list, a suggested rewrite of the stop, that
///   suggestion being edited, and the stop's Enhance done outside the chat;
///   about the whole tour as its own chat toggle opens it after Draft Tour,
///   answered, and with a proposed revision; that revision's parts waiting on
///   the stops they change (a new explanation, read and edited, and a
///   removal); and about a stop whose session read the earlier head.
///
/// It also holds the drawer to what its cards promise. A suggested prompt
/// waits, greyed, with what to choose first as its tooltip, while the card
/// asks which revision to go on with, before a question and after a refused
/// one, and a question typed meanwhile stays in the field. That card's New
/// session opens an empty session at the head under review and sends the
/// refused question there; the session it left keeps its transcript in
/// History. A run that stopped reports on the question field with Resend and
/// its log. The first question of the whole tour's chat after Draft Tour, and
/// of a stop's chat after its Enhance, starts from that answer, and the chat
/// says so until it is asked (the Fixture agent's reply says it was given the
/// answer). A suggestion in a tour chat offers a pencil, a trash and Accept.
///
/// A state is what the window shows between two gestures, not only where a
/// flow ends. The owner, 2026-11-25, of the proposal Draft Tour leaves before
/// Apply all, its buttons cut to "dit tex": "It still looks exactly the same
/// this must be an offscreen vs on screen render issue". It was not: this
/// course's render of that state, drawn off screen, is the window on screen
/// to the pixel. The course had pressed Apply all on its way to the route, so
/// that state was neither measured nor drawn.
///
/// Finding cards no report holds join it: an answer that is not a review brings
/// them under it in the Agent tab, open and then one dismissed.
///
/// In each state the drawer is dragged by its handle to 280, 300, 340 and 420
/// pixels. The chat column is the Tour tab's second: there the drawer is
/// first dragged as narrow as it goes and then the column itself is given
/// those widths (it is never narrower than its own minimum). At every width:
///
/// - no scroll area in the drawer shows a horizontal scroll bar, has sideways
///   range (a trackpad pans a scroll area whose bar is turned off) or holds
///   content wider than its viewport;
/// - every button and label shown lies inside the drawer and inside the
///   scrolled content it belongs to;
/// - no button is narrower than its words, and no label that does not wrap or
///   shorten its words is narrower than them;
/// - no two buttons lie over one another;
/// - no button's face and no text field is cut by a part that holds it: a
///   row laid out shorter than its buttons, or a card still as tall as the
///   words a field replaced, clips them where no scrolling brings them back;
/// - nothing lies over what a scroll area scrolls: no label or button that is
///   not the area's own is drawn inside its viewport;
/// - a chat's transcript, which keeps itself at its end, draws its top edge
///   exactly while turns are scrolled under it, so a turn cut at the top is
///   seen to go on above and never reads as covered by the header; where the
///   cut falls depends on every height above it, so it cannot be laid out
///   away. At its start the transcript draws none;
/// - the Agent tab's header is its one row: its transcript starts directly
///   under it, and what a session says of itself (that it was carried over,
///   or cannot be picked up) is a notice on the question field;
/// - no label, button or menu item says the words RFC-BPR-0130 replaced:
///   "Native continuation", "Transcript replay", "Native:", "Private history
///   and runs".
///
/// Owner, attended review 2026-11-25, of a question refused after a push, in
/// a 280 pixel drawer: its first line was "partly hidden under the Native
/// continuation line". Nothing lay over it. The transcript held the session's
/// earlier turns too, some 1,700 pixels more than its viewport, and was at its
/// end, so the viewport's top fell where it fell: inside that question's
/// bubble, directly under a line of words that stood at the drawer's edge in
/// the transcript's own ink with nothing drawn between. The owner
/// then, of that line: "I thought we were getting rid of 'Native
/// continuation' altogether".
///
/// A code block is code: it keeps its lines and pans inside its own frame
/// until its Wrap is pressed. The block itself is held to the rules above;
/// only the text inside its frame may be wider than the frame.
///
/// CTest runs the course at 100% and 200%. `BPR_COMPOSED_DRAWER_RENDER_ROOT`,
/// when set, receives the drawer as drawn in each state at each width, in a
/// folder named for the platform and style that drew it, as
/// `<tag>-<scheme>-<scale>-<state>-<width>.png` (`BPR_COMPOSED_DRAWER_RENDER_TAG`,
/// else "drawer"), and the whole window at each state's narrowest width as
/// `…-window.png`. Run under the native look (`BPR_NATIVE_LOOK=1`, Cocoa) the
/// same binary draws what the application's own window draws: the window is
/// told it is the active one, as the window a person looks at is, and run
/// without `QT_SCALE_FACTOR` it draws at the display's own scale.
/// `BPR_COMPOSED_DRAWER_REPORT`, when set, makes the run report every fault
/// and finish, for a before-and-after record, instead of failing on them.
///
///     composed_drawer_fit_public_path FIXTURE_AGENT

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_pending_list.hpp"
#include "bpr/qt/agent_session_menu.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/chat_history_menu.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/stick_to_bottom.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/transcript_edge.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	/// Lets the window run for `aMilliseconds`: posted layout requests, the
	/// chats' own deferred fitting and the splitter's width publications.
	void settle_for(const int aMilliseconds)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			// A widget a refresh replaced is gone, as it is under the
			// application's own event loop.
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents(QEventLoop::AllEvents, 10);
		}
	}

	void settle() { settle_for(60); }

	[[nodiscard]] auto describe(const QWidget& aWidget) -> QString
	{
		auto words = QString::fromLatin1(aWidget.metaObject()->className());
		if (!aWidget.objectName().isEmpty()) {
			words += QStringLiteral(" '%1'").arg(aWidget.objectName());
		}
		if (const auto* const button = qobject_cast<const QAbstractButton*>(&aWidget); button != nullptr && !button->text().isEmpty()) {
			words += QStringLiteral(" \"%1\"").arg(button->text().left(40));
		}
		if (const auto* const label = qobject_cast<const QLabel*>(&aWidget); label != nullptr && !label->text().isEmpty()) {
			words += QStringLiteral(" \"%1\"").arg(label->text().left(40));
		}
		return words;
	}

	/// \brief Whether \p aArea is the text inside a code block's frame, or a
	/// field holding code: code keeps its lines, so it alone may be wider than
	/// its frame.
	[[nodiscard]] auto shows_code(const QAbstractScrollArea& aArea) -> bool
	{
		for (const auto* parent = static_cast<const QWidget*>(&aArea); parent != nullptr; parent = parent->parentWidget()) {
			const auto name = parent->objectName();
			if (name == QStringLiteral("markdownCodeBlock") || name == QStringLiteral("threadDraftChangeRows")) {
				return true;
			}
		}
		return false;
	}

	/// \brief Every way \p aSurface pans sideways: a horizontal scroll bar
	/// shown, sideways range, or scrolled content wider than its viewport.
	[[nodiscard]] auto pans_sideways(const QWidget& aSurface) -> QStringList
	{
		auto found = QStringList{ };
		for (const auto* const area : aSurface.findChildren<QAbstractScrollArea*>()) {
			if (!area->isVisibleTo(&aSurface) || area->viewport()->width() <= 0 || shows_code(*area)) {
				continue;
			}
			const auto* const bar = area->horizontalScrollBar();
			if (bar->isVisible()) {
				found.push_back(QStringLiteral("%1 shows a horizontal scroll bar").arg(describe(*area)));
			}
			if (bar->maximum() > bar->minimum()) {
				found.push_back(QStringLiteral("%1 pans %2 px sideways").arg(describe(*area)).arg(bar->maximum() - bar->minimum()));
			}
			const auto* const scroll = qobject_cast<const QScrollArea*>(area);
			if (scroll == nullptr || scroll->widget() == nullptr || !scroll->widget()->isVisible() || scroll->widget()->width() <= scroll->viewport()->width()) {
				continue;
			}
			// Name what forces the width, innermost first, so the failure
			// says what to wrap or shorten.
			const auto* const content = scroll->widget();
			const auto        reach   = [content](const QWidget& aPart) { return aPart.mapTo(content, QPoint{ 0, 0 }).x() + std::max(aPart.minimumWidth(), aPart.minimumSizeHint().width()); };
			const auto        limit   = scroll->viewport()->width();
			auto              past    = QStringList{ };
			for (const auto* const child : content->findChildren<QWidget*>()) {
				if (!child->isVisible() || reach(*child) <= limit || past.size() >= 6) {
					continue;
				}
				const auto inner = std::ranges::any_of(child->findChildren<QWidget*>(), [&](const QWidget* aInner) { return aInner->isVisible() && reach(*aInner) > limit; });
				if (!inner) {
					past.push_back(QStringLiteral("%1 reaches %2").arg(describe(*child)).arg(reach(*child)));
				}
			}
			found.push_back(QStringLiteral("%1's content is %2 wide in a %3 px viewport (%4)").arg(describe(*area)).arg(content->width()).arg(limit).arg(past.join(QStringLiteral(", "))));
		}
		return found;
	}

	/// \brief What bounds a control sideways: the content of the nearest
	/// scroll area between it and the surface, or the surface.
	[[nodiscard]] auto bound_of(const QWidget& aChild, const QWidget& aSurface) -> const QWidget*
	{
		for (const auto* parent = aChild.parentWidget(); parent != nullptr && parent != &aSurface; parent = parent->parentWidget()) {
			const auto* const owner = parent->parentWidget();
			if (const auto* const scroll = qobject_cast<const QScrollArea*>(owner != nullptr ? owner->parentWidget() : nullptr); scroll != nullptr && scroll->widget() == parent) {
				return parent;
			}
			if (const auto* const area = qobject_cast<const QAbstractScrollArea*>(owner); area != nullptr && area->viewport() == parent) {
				return parent;
			}
		}
		return &aSurface;
	}

	/// \brief Where a control is drawn in \p aSurface. A push button the
	/// platform style draws is larger than its face (the macOS style keeps
	/// room round a bezel), so two such widgets may overlap where their faces
	/// do not. A quiet chip paints its whole rect: laid out by a bezel it
	/// does not have, it is drawn over its neighbour.
	[[nodiscard]] auto face_of(const QWidget& aControl) -> QRect
	{
		auto              face  = aControl.rect();
		const auto* const quiet = dynamic_cast<const qt::quiet_push_button*>(&aControl);
		const auto        chip  = quiet != nullptr && !quiet->text().isEmpty() && !quiet->isDefault() && quiet->menu() == nullptr;
		if (const auto* const push = qobject_cast<const QPushButton*>(&aControl); push != nullptr && !chip) {
			auto option = QStyleOptionButton{ };
			option.initFrom(push);
			if (const auto item = push->style()->subElementRect(QStyle::SE_PushButtonLayoutItem, &option, push); item.isValid()) {
				face = item;
			}
		}
		return face;
	}

	[[nodiscard]] auto face_in(const QWidget& aControl, const QWidget& aSurface) -> QRect
	{
		return face_of(aControl).translated(aControl.mapTo(&aSurface, QPoint{ 0, 0 }));
	}

	/// \brief The part between \p aControl and what scrolls it that cuts its
	/// face, if one does. A viewport clips what can be scrolled back into
	/// view; any other part that is smaller than what it holds cuts it for
	/// good (a row given the height of a bezel its chips do not have).
	[[nodiscard]] auto cut_by(const QWidget& aControl, const QWidget& aSurface) -> const QWidget*
	{
		for (const auto* part = aControl.parentWidget(); part != nullptr && part != &aSurface; part = part->parentWidget()) {
			if (const auto* const area = qobject_cast<const QAbstractScrollArea*>(part->parentWidget()); area != nullptr && area->viewport() == part) {
				return nullptr;
			}
			if (!part->rect().contains(face_of(aControl).translated(aControl.mapTo(part, QPoint{ 0, 0 })))) {
				return part;
			}
		}
		return nullptr;
	}

	/// \brief Every button and label \p aSurface shows that is cut off, too
	/// narrow for its words, or lying over another button.
	[[nodiscard]] auto cut_off(const QWidget& aSurface) -> QStringList
	{
		auto found   = QStringList{ };
		auto buttons = std::vector<const QAbstractButton*>{ };
		for (const auto* const child : aSurface.findChildren<QWidget*>()) {
			const auto* const button = qobject_cast<const QAbstractButton*>(child);
			const auto* const label  = qobject_cast<const QLabel*>(child);
			if ((button == nullptr && label == nullptr) || !child->isVisibleTo(&aSurface) || child->width() <= 0 || child->visibleRegion().isEmpty()) {
				continue;
			}
			// A tooltip a part of the drawer raised (the destination of the
			// link Tab reached) is a window of its own, placed on the screen:
			// the drawer does not lay it out and cannot cut it.
			if (child->window() != aSurface.window()) {
				continue;
			}
			if (const auto* const area = qobject_cast<const QAbstractScrollArea*>(child->parentWidget() != nullptr ? child->parentWidget()->parentWidget() : nullptr); area != nullptr && shows_code(*area)) {
				continue;
			}
			// In the drawer, and in the scrolled content it belongs to.
			const auto inDrawer = QRect{ child->mapTo(&aSurface, QPoint{ 0, 0 }), child->size() };
			if (inDrawer.left() < 0 || inDrawer.right() >= aSurface.width()) {
				found.push_back(QStringLiteral("%1 spans %2..%3 of the drawer's %4").arg(describe(*child)).arg(inDrawer.left()).arg(inDrawer.right() + 1).arg(aSurface.width()));
			} else if (const auto* const bound = bound_of(*child, aSurface); bound != &aSurface) {
				const auto left = child->mapTo(bound, QPoint{ 0, 0 }).x();
				if (left < 0 || left + child->width() > bound->width()) {
					found.push_back(QStringLiteral("%1 spans %2..%3 of %4's %5").arg(describe(*child)).arg(left).arg(left + child->width()).arg(describe(*bound)).arg(bound->width()));
				}
			}
			if (button != nullptr) {
				const auto* const tool = qobject_cast<const QToolButton*>(button);
				// A tool button that shows words or an arrow is one of the
				// profile's own (a quiet chip, a disclosure): a plain one is
				// the platform style's to draw, which under the macOS style
				// is a bezel in the system's grey whatever the profile.
				if (tool != nullptr && (tool->arrowType() != Qt::NoArrow || (!tool->text().isEmpty() && tool->toolButtonStyle() != Qt::ToolButtonIconOnly))
					&& dynamic_cast<const qt::quiet_tool_button*>(tool) == nullptr && dynamic_cast<const qt::disclosure_button*>(tool) == nullptr) {
					found.push_back(QStringLiteral("%1 is drawn by the platform style, not as the profile's quiet chip").arg(describe(*tool)));
				}
				const auto words = !button->text().isEmpty() && (tool == nullptr || tool->toolButtonStyle() != Qt::ToolButtonIconOnly);
				// A button that wraps its words needs its longest word.
				if (words && button->width() < button->minimumSizeHint().width()) {
					found.push_back(QStringLiteral("%1 is %2 wide, its words need %3").arg(describe(*button)).arg(button->width()).arg(button->minimumSizeHint().width()));
				}
				if (words && button->hasHeightForWidth() && button->height() < button->heightForWidth(button->width())) {
					found.push_back(QStringLiteral("%1 is %2 tall, its words need %3").arg(describe(*button)).arg(button->height()).arg(button->heightForWidth(button->width())));
				}
				if (const auto* const part = cut_by(*button, aSurface); part != nullptr) {
					const auto face = face_of(*button).translated(button->mapTo(part, QPoint{ 0, 0 }));
					found.push_back(QStringLiteral("%1's face (%2,%3 %4x%5) is cut by %6, which is %7x%8")
										.arg(describe(*button))
										.arg(face.left())
										.arg(face.top())
										.arg(face.width())
										.arg(face.height())
										.arg(describe(*part))
										.arg(part->width())
										.arg(part->height()));
				}
				buttons.push_back(button);
			}
			// A label that neither wraps nor shortens its words (its size
			// policy then lets it be any width) shows them whole.
			if (label != nullptr && !label->wordWrap() && !label->text().isEmpty() && label->pixmap().isNull() && label->sizePolicy().horizontalPolicy() != QSizePolicy::Ignored
				&& label->width() < label->minimumSizeHint().width()) {
				found.push_back(QStringLiteral("%1 is %2 wide, its words need %3").arg(describe(*label)).arg(label->width()).arg(label->minimumSizeHint().width()));
			}
		}
		// A field of text is whole inside what holds it, too.
		for (const auto* const field : aSurface.findChildren<QPlainTextEdit*>()) {
			if (!field->isVisibleTo(&aSurface) || field->visibleRegion().isEmpty()) {
				continue;
			}
			if (const auto* const part = cut_by(*field, aSurface); part != nullptr) {
				found.push_back(QStringLiteral("%1 (%2x%3) is cut by %4, which is %5x%6").arg(describe(*field)).arg(field->width()).arg(field->height()).arg(describe(*part)).arg(part->width()).arg(part->height()));
			}
		}
		for (std::size_t first = 0U; first < buttons.size(); ++first) {
			for (std::size_t second = first + 1U; second < buttons.size(); ++second) {
				const auto* const one = buttons[first];
				const auto* const two = buttons[second];
				if (one->isAncestorOf(two) || two->isAncestorOf(one)) {
					continue;
				}
				// Only what is drawn of each: one scrolled under a pinned bar
				// is clipped there, not over it.
				const auto shown = [&aSurface](const QAbstractButton& aButton) {
					return face_in(aButton, aSurface).intersected(aButton.visibleRegion().boundingRect().translated(aButton.mapTo(&aSurface, QPoint{ 0, 0 })));
				};
				if (shown(*one).intersects(shown(*two))) {
					const auto box = [](const QRect& aRect) { return QStringLiteral("(%1,%2 %3x%4)").arg(aRect.left()).arg(aRect.top()).arg(aRect.width()).arg(aRect.height()); };
					found.push_back(QStringLiteral("%1 %2 lies over %3 %4").arg(describe(*one), box(shown(*one)), describe(*two), box(shown(*two))));
				}
			}
		}
		return found;
	}

	/// \brief Where \p aPart is drawn in \p aSurface: its rect, less what the
	/// parts that hold it clip.
	[[nodiscard]] auto drawn_in(const QWidget& aPart, const QWidget& aSurface) -> QRect
	{
		auto drawn = QRect{ aPart.mapTo(&aSurface, QPoint{ 0, 0 }), aPart.size() };
		for (const auto* parent = aPart.parentWidget(); parent != nullptr && parent != &aSurface; parent = parent->parentWidget()) {
			drawn &= QRect{ parent->mapTo(&aSurface, QPoint{ 0, 0 }), parent->size() };
		}
		return drawn & aSurface.rect();
	}

	/// \brief Every way what \p aSurface scrolls is covered from above, or
	/// reads as covered: a label or button that is not a scroll area's own
	/// drawn inside its viewport; a chat's transcript with a turn cut at its
	/// top and no edge drawn on its first row, or with none cut and one
	/// drawn; and anything standing between the Agent tab's header row and
	/// its transcript.
	/// \p aEdges, when given, counts the transcripts found off their start
	/// with their edge drawn.
	[[nodiscard]] auto covered_from_above(const QWidget& aSurface, std::size_t* const aEdges = nullptr) -> QStringList
	{
		auto found = QStringList{ };
		for (auto* const area : aSurface.findChildren<QScrollArea*>()) {
			if (!area->isVisibleTo(&aSurface) || shows_code(*area)) {
				continue;
			}
			const auto view = drawn_in(*area->viewport(), aSurface);
			if (view.isEmpty()) {
				continue;
			}
			for (const auto* const child : aSurface.findChildren<QWidget*>()) {
				const auto* const label = qobject_cast<const QLabel*>(child);
				const auto        words = label != nullptr && (!label->text().isEmpty() || !label->pixmap().isNull());
				// A tooltip a part raised is a window of its own, placed on the
				// screen and not in the drawer.
				if ((qobject_cast<const QAbstractButton*>(child) == nullptr && !words) || child->window() != aSurface.window() || !child->isVisibleTo(&aSurface) || area->isAncestorOf(child)) {
					continue;
				}
				if (const auto over = drawn_in(*child, aSurface) & view; !over.isEmpty()) {
					found.push_back(QStringLiteral("%1 lies over %2x%3 of what %4 scrolls").arg(describe(*child)).arg(over.width()).arg(over.height()).arg(describe(*area)));
				}
			}
			const auto* const edge = qt::transcript_edge::of(*area);
			if (area->widget() == nullptr) {
				continue;
			}
			if (edge == nullptr) {
				// A transcript keeps itself at its end, so it is the scroll
				// area whose first turn is cut at the top as a matter of course.
				if (qt::stick_to_bottom::of(*area) != nullptr) {
					found.push_back(QStringLiteral("%1 keeps itself at its end and has no edge to draw over a turn cut at its top").arg(describe(*area)));
				}
				continue;
			}
			// Whether anything the transcript holds is cut at the viewport's
			// top or gone above it. The blank margin over its first turn is
			// not: a transcript a pixel taller than its viewport cuts nothing.
			const auto* const bar   = area->verticalScrollBar();
			const auto        under = bar->value() - bar->minimum();
			const auto        row   = QRect{ area->viewport()->geometry().topLeft(), QSize{ area->viewport()->width(), 1 } };
			const auto        above = std::ranges::any_of(area->widget()->children(), [area](const QObject* const aChild) {
				const auto* const part = qobject_cast<const QWidget*>(aChild);
				return part != nullptr && !part->isHidden() && part->height() > 0 && part->mapTo(area->viewport(), QPoint{ 0, 0 }).y() < 0;
			});
			if (!above) {
				if (edge->isVisible()) {
					found.push_back(QStringLiteral("%1 cuts nothing at its top (%2 px off its start) and draws an edge over its first turn").arg(describe(*area)).arg(under));
				}
				continue;
			}
			if (!edge->isVisible() || edge->geometry() != row) {
				found.push_back(QStringLiteral("%1 is %2 px off its start with no edge on its viewport's first row: the turn cut there reads as covered by what is above it").arg(describe(*area)).arg(under));
				continue;
			}
			// Drawn, not only placed: the row is the edge's colour.
			const auto image  = area->grab(row).toImage().convertToFormat(QImage::Format_ARGB32);
			const auto colour = qt::transcript_edge::colour(edge->palette()).rgb();
			auto       same   = 0;
			for (int x = 0; x < image.width(); ++x) {
				same += image.pixel(x, 0) == colour ? 1 : 0;
			}
			if (same * 10 < image.width() * 9) {
				found.push_back(QStringLiteral("%1's edge is drawn on %2 of its first row's %3 pixels").arg(describe(*area)).arg(same).arg(image.width()));
			} else if (aEdges != nullptr) {
				++*aEdges;
			}
		}
		// The Agent tab's header is its one row; its transcript starts under it.
		const auto* const row    = aSurface.findChild<QWidget*>(QStringLiteral("agentContextRow"));
		const auto* const thread = aSurface.findChild<QScrollArea*>(QStringLiteral("agentConversationScroll"));
		if (row != nullptr && thread != nullptr && row->isVisibleTo(&aSurface) && thread->isVisibleTo(&aSurface)) {
			if (const auto between = thread->mapTo(&aSurface, QPoint{ 0, 0 }).y() - row->mapTo(&aSurface, QPoint{ 0, row->height() }).y(); between != 0) {
				found.push_back(QStringLiteral("%1 px stand between the Agent tab's header row and its transcript").arg(between));
			}
		}
		return found;
	}

	/// \brief Everything \p aSurface shows, or one of its menus offers, that
	/// says words RFC-BPR-0130 replaced (AC03: "In no state does a chat show
	/// "Native continuation", "Transcript replay" or "Native:""), in its text
	/// or its tooltip.
	[[nodiscard]] auto internal_words(const QWidget& aSurface) -> QStringList
	{
		auto       found = QStringList{ };
		const auto said  = [&found](const QString& aWho, const QString& aText) {
			for (const auto* const old : { "Native continuation", "Transcript replay", "Native:", "Private history and runs" }) {
				if (aText.contains(QString::fromLatin1(old))) {
					found.push_back(QStringLiteral("%1 says \"%2\"").arg(aWho, QString::fromLatin1(old)));
				}
			}
		};
		for (const auto* const child : aSurface.findChildren<QWidget*>()) {
			if (!child->isVisibleTo(&aSurface)) {
				continue;
			}
			if (const auto* const label = qobject_cast<const QLabel*>(child); label != nullptr) {
				said(describe(*label), label->text() + QLatin1Char{ '\n' } + label->toolTip());
			} else if (const auto* const button = qobject_cast<const QAbstractButton*>(child); button != nullptr) {
				said(describe(*button), button->text() + QLatin1Char{ '\n' } + button->toolTip());
			}
		}
		// A menu is read whether or not it is open.
		for (const auto* const menu : aSurface.findChildren<QMenu*>()) {
			for (const auto* const action : menu->actions()) {
				said(QStringLiteral("the item \"%1\" of %2").arg(action->text().left(40), describe(*menu)), action->text() + QLatin1Char{ '\n' } + action->toolTip());
			}
		}
		return found;
	}

	/// \brief `covered_from_above` fails on what it claims to catch.
	void check_what_covers()
	{
		QWidget surface;
		surface.resize(300, 300);
		auto* const header = new QWidget{ &surface };
		header->setObjectName(QStringLiteral("agentContextRow"));
		header->setGeometry(0, 0, 300, 40);
		auto* const title = new QLabel{ QStringLiteral("The session's title"), header };
		title->setGeometry(10, 8, 280, 20);
		auto* const scroll = new QScrollArea{ &surface };
		scroll->setObjectName(QStringLiteral("agentConversationScroll"));
		scroll->setGeometry(0, 40, 300, 200);
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const content = new QWidget{ };
		content->setMinimumHeight(260);
		scroll->setWidget(content);
		auto* const turn = new QLabel{ QStringLiteral("What changed in this push?"), content };
		turn->setGeometry(12, 8, 200, 20);
		surface.setAttribute(Qt::WA_DontShowOnScreen);
		surface.show();
		settle();
		const auto said = [&surface](std::size_t* const aEdges = nullptr) { return covered_from_above(surface, aEdges).join(QStringLiteral("; ")); };
		expect(said().isEmpty(), "a scroll area under its header, at its start, is covered by nothing: " + said().toStdString());
		// A transcript at its end with nothing drawn at its top: what the
		// owner saw as the question hidden under the line above it.
		qt::stick_to_bottom::attach(*scroll).pin();
		settle();
		expect(scroll->verticalScrollBar()->value() == 60 && said().contains(QStringLiteral("has no edge to draw")), "negative control: a transcript that keeps its end and has no edge is caught: " + said().toStdString());
		auto& edge  = qt::transcript_edge::attach(*scroll);
		auto  edges = std::size_t{ 0U };
		settle();
		expect(said(&edges).isEmpty() && edges == 1U, "and with its edge drawn it is not: " + said().toStdString());
		edge.hide();
		expect(said().contains(QStringLiteral("60 px off its start with no edge")), "negative control: a transcript off its start whose edge is not shown is caught: " + said().toStdString());
		edge.show();
		edge.setGeometry(0, 0, 150, 1);
		expect(said().contains(QStringLiteral("with no edge on its viewport's first row")), "negative control: an edge that does not span the first row is caught");
		scroll->verticalScrollBar()->setValue(0);
		settle();
		expect(said().isEmpty(), "at its start the transcript draws no edge, and nothing is found: " + said().toStdString());
		edge.show();
		expect(said().contains(QStringLiteral("cuts nothing at its top (0 px off its start) and draws an edge")), "negative control: an edge over the first turn of a transcript at its start is caught");
		// Off its start by less than the room over its first turn: nothing is
		// cut, so an edge there would be a line over a whole first turn.
		scroll->verticalScrollBar()->setValue(8);
		settle();
		edge.show();
		expect(said().contains(QStringLiteral("cuts nothing at its top (8 px off its start)")), "negative control: an edge with only blank room scrolled under it is caught");
		scroll->verticalScrollBar()->setValue(9);
		settle();
		edge.hide();
		expect(said().contains(QStringLiteral("9 px off its start with no edge")), "negative control: one pixel of the first turn cut with no edge is caught");
		scroll->verticalScrollBar()->setValue(0);
		settle();
		edge.hide();
		// Words laid where the transcript scrolls.
		const auto words = [&surface] { return internal_words(surface).join(QStringLiteral("; ")); };
		expect(words().isEmpty(), "a title and a question are no internal words: " + words().toStdString());
		auto* const mode = new QLabel{ QStringLiteral("Native continuation"), &surface };
		mode->setGeometry(0, 30, 300, 20);
		mode->show();
		settle();
		expect(said().contains(QStringLiteral("lies over 300x10 of what")), "negative control: a label drawn over a scroll area's viewport is caught: " + said().toStdString());
		expect(words().contains(QStringLiteral("says \"Native continuation\"")), "negative control: a line that says Native continuation is caught: " + words().toStdString());
		// The line the owner saw: under the header's row, over the transcript.
		scroll->setGeometry(0, 60, 300, 180);
		mode->setGeometry(0, 40, 300, 20);
		settle();
		expect(said() == QStringLiteral("20 px stand between the Agent tab's header row and its transcript"), "negative control: a line between the header's row and the transcript is caught: " + said().toStdString());
		mode->setText(QStringLiteral("Carried over from a saved transcript."));
		mode->setToolTip(QStringLiteral("Transcript replay keeps 48 KiB."));
		expect(words().contains(QStringLiteral("says \"Transcript replay\"")), "negative control: a tooltip that says Transcript replay is caught");
		mode->setToolTip({ });
		auto* const menu = new QMenu{ &surface };
		menu->addAction(QStringLiteral("Private history and runs"));
		expect(words().contains(QStringLiteral("says \"Private history and runs\"")), "negative control: a menu item that says Private history and runs is caught, though the menu is closed");
		menu->clear();
		// The notice on the question field, under the transcript.
		scroll->setGeometry(0, 40, 300, 180);
		mode->setGeometry(10, 224, 280, 20);
		settle();
		expect(said().isEmpty() && words().isEmpty(), "and a notice in the accepted words under the transcript is neither: " + said().toStdString() + words().toStdString());
	}

	/// \brief The checks themselves fail on what they claim to catch.
	void check_the_checks()
	{
		QWidget surface;
		surface.resize(300, 200);
		auto* const scroll = new QScrollArea{ &surface };
		scroll->setGeometry(0, 0, 300, 100);
		scroll->setWidgetResizable(true);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const content = new QWidget{ };
		content->setMinimumWidth(420);
		scroll->setWidget(content);
		auto* const past = new QPushButton{ QStringLiteral("Continue against the old revision"), &surface };
		past->setGeometry(220, 120, 200, 30);
		surface.setAttribute(Qt::WA_DontShowOnScreen);
		surface.show();
		settle();
		expect(!pans_sideways(surface).isEmpty(), "negative control: content wider than its viewport pans though the bar is off");
		expect(!cut_off(surface).isEmpty(), "negative control: a button past the drawer's edge is caught");
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		settle();
		expect(pans_sideways(surface).size() >= 2, "negative control: a horizontal scroll bar is caught");
		content->setMinimumWidth(0);
		past->setGeometry(10, 120, 20, 30);
		settle();
		expect(pans_sideways(surface).isEmpty(), "content that fits does not pan");
		expect(!cut_off(surface).isEmpty(), "negative control: a button narrower than its words is caught");
		past->setGeometry(10, 120, 280, 30);
		auto* const over = new QPushButton{ QStringLiteral("Start a new session"), &surface };
		over->setGeometry(100, 120, 180, 30);
		over->show();
		settle();
		expect(!cut_off(surface).isEmpty(), "negative control: a button lying over another is caught");
		over->setGeometry(10, 160, 280, 30);
		settle();
		expect(cut_off(surface).isEmpty(), "and the same parts with room fit: " + cut_off(surface).join(QStringLiteral("; ")).toStdString());
		auto* const plain = new QToolButton{ &surface };
		plain->setText(QStringLiteral("Worked for 4 s"));
		plain->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		plain->setArrowType(Qt::RightArrow);
		plain->setGeometry(10, 60, 200, 30);
		plain->show();
		settle();
		expect(!cut_off(surface).isEmpty(), "negative control: a plain tool button with an arrow, which the platform style draws, is caught");
		plain->hide();
		settle();
		expect(cut_off(surface).isEmpty(), "and without it nothing is: " + cut_off(surface).join(QStringLiteral("; ")).toStdString());
		auto* const row = new QWidget{ &surface };
		row->setGeometry(10, 60, 280, 20);
		auto* const chip = new qt::quiet_push_button{ QStringLiteral("Keep"), row };
		chip->setGeometry(0, 0, 60, 30);
		row->show();
		settle();
		expect(cut_off(surface).join(QStringLiteral("; ")).contains(QStringLiteral("is cut by")), "negative control: a chip taller than the row that holds it is caught");
		chip->setGeometry(0, 0, 60, 20);
		settle();
		expect(cut_off(surface).isEmpty(), "and a chip its row holds whole is not: " + cut_off(surface).join(QStringLiteral("; ")).toStdString());
		auto* const frame = new QWidget{ &surface };
		frame->setGeometry(10, 90, 280, 40);
		auto* const field = new QPlainTextEdit{ frame };
		field->setGeometry(0, 0, 280, 70);
		frame->show();
		settle();
		expect(cut_off(surface).join(QStringLiteral("; ")).contains(QStringLiteral("QPlainTextEdit")), "negative control: a text field taller than the frame that holds it is caught");
		check_what_covers();
	}

	/// The course: one window of the composed application and what it found.
	struct course
	{
		app::github_application* Owner  = nullptr;
		qt::workbench_window*    Window = nullptr;
		QString                  RenderFolder;
		QString                  RenderPrefix;
		std::vector<std::string> Faults;
		std::vector<std::string> States;
		/// How often a chat's transcript was judged off its start with its
		/// edge drawn.
		std::size_t EdgesDrawn = 0U;
		/// The part of the drawer the state being judged is about, if it
		/// could be scrolled out of view.
		QPointer<QWidget> Shown;
		/// While this file stands beside the course's own copy of the fixture
		/// agent, the agent's turns wait.
		QString HoldFile;

		/// Holds the fixture agent's next turn open until `release_turns`: a
		/// state that only shows while a turn runs is judged at every width
		/// however long that takes.
		void hold_turns() const
		{
			auto file = QFile{ HoldFile };
			expect(file.open(QIODevice::WriteOnly), "the course holds the fixture agent's turn");
		}
		void release_turns() const { expect(!QFile::exists(HoldFile) || QFile::remove(HoldFile), "the course lets the fixture agent's turn go on"); }

		[[nodiscard]] auto drawer() const -> QWidget* { return Window->conversations_drawer(); }
		[[nodiscard]] auto chat() const -> qt::tour_side_chat* { return Window->tour_editor()->side_chat(); }

		/// Drags the drawer's handle so the drawer is \p aWidth wide, or as
		/// near as its bounds allow; answers the width it has.
		auto drag_drawer(const int aWidth) const -> int
		{
			auto* const splitter = Window->central_splitter();
			const auto  index    = splitter->indexOf(drawer());
			expect(index > 0, "the right drawer is in the window's splitter");
			for (auto round = 0; round < 3 && drawer()->width() != aWidth; ++round) {
				splitter->move_handle(index, splitter->handle(index)->x() + drawer()->width() - aWidth);
				settle();
			}
			return drawer()->width();
		}

		void render(QWidget& aWidget, const QString& aName) const
		{
			if (!RenderFolder.isEmpty()) {
				expect(aWidget.grab().save(RenderFolder + QLatin1Char{ '/' } + RenderPrefix + aName + QStringLiteral(".png"), "PNG"), "the render is saved: " + aName.toStdString());
			}
		}

		void judge(const std::string& aState, const int aWidth, const bool aWindowToo)
		{
			// The window a person looks at is the active one: a default
			// button wears its accent and a selection its strong colour. The
			// platform never makes a window active that is kept off screen,
			// so Qt is told it is.
			if (!Window->isActiveWindow()) {
				QT_WARNING_PUSH
				QT_WARNING_DISABLE_DEPRECATED
				QApplication::setActiveWindow(Window);
				QT_WARNING_POP
			}
			settle();
			// What the state is about, scrolled to as it is read.
			if (Shown != nullptr) {
				for (auto* parent = Shown->parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
					if (auto* const area = qobject_cast<QScrollArea*>(parent); area != nullptr) {
						area->ensureWidgetVisible(Shown, 0, 12);
						break;
					}
				}
				settle();
			}
			const auto name = QString::fromStdString(aState) + QLatin1Char{ '-' } + QString::number(aWidth);
			render(*drawer(), name);
			if (aWindowToo) {
				render(*Window, name + QStringLiteral("-window"));
			}
			const auto where = aState + " at " + std::to_string(aWidth) + " (drawer " + std::to_string(drawer()->width()) + ")";
			if (const auto panning = pans_sideways(*drawer()); !panning.isEmpty()) {
				Faults.push_back(where + ": pans sideways: " + panning.join(QStringLiteral("; ")).toStdString());
			}
			if (const auto cut = cut_off(*drawer()); !cut.isEmpty()) {
				Faults.push_back(where + ": cut off: " + cut.join(QStringLiteral("; ")).toStdString());
			}
			if (const auto covered = covered_from_above(*drawer(), &EdgesDrawn); !covered.isEmpty()) {
				Faults.push_back(where + ": covered from above: " + covered.join(QStringLiteral("; ")).toStdString());
			}
			if (const auto old = internal_words(*drawer()); !old.isEmpty()) {
				Faults.push_back(where + ": internal words: " + old.join(QStringLiteral("; ")).toStdString());
			}
		}

		/// The window made as narrow as it goes, which squeezes the drawer if
		/// anything does, judged, and made its own size again.
		void judge_in_the_narrowest_window(const std::string& aState)
		{
			const auto size = Window->size();
			Window->resize(std::max(Window->minimumWidth(), Window->minimumSizeHint().width()), size.height());
			settle();
			judge(aState + "-narrowest-window", drawer()->width(), true);
			Window->resize(size);
			settle();
		}

		/// A state of a drawer tab: the drawer dragged to each width.
		void sweep_drawer(const std::string& aState)
		{
			States.push_back(aState);
			auto first = true;
			for (const auto width : { 280, 300, 340, 420 }) {
				const auto got = drag_drawer(width);
				expect(got == width, aState + ": the drawer is dragged to " + std::to_string(width) + " (" + std::to_string(got) + ")");
				judge(aState, width, first);
				first = false;
			}
			drag_drawer(280);
			judge_in_the_narrowest_window(aState);
		}

		/// A state of the Tour tab's chat column: the drawer as narrow as it
		/// goes with the column as the tab splits it, then the column at
		/// each width it can have.
		void sweep_chat(const std::string& aState)
		{
			States.push_back(aState);
			auto* const split = Window->findChild<QSplitter*>(QStringLiteral("workbenchTourChatSplit"));
			expect(split != nullptr && chat()->isVisibleTo(drawer()), aState + ": the chat is the Tour tab's second column");
			drag_drawer(0);
			judge(aState + "-narrowest-drawer", chat()->width(), true);
			auto judged = std::vector<int>{ };
			for (const auto width : { 280, 300, 340, 420 }) {
				const auto tour = split->widget(0)->minimumWidth();
				drag_drawer(tour + split->handleWidth() + width);
				split->setSizes({ split->width() - split->handleWidth() - width, width });
				settle();
				const auto got = chat()->width();
				if (std::ranges::find(judged, got) != judged.end()) {
					continue;
				}
				judged.push_back(got);
				expect(got >= width && got <= width + 60, aState + ": the chat column is given " + std::to_string(width) + " (" + std::to_string(got) + " in a drawer of " + std::to_string(drawer()->width()) + ")");
				judge(aState, got, false);
			}
			drag_drawer(0);
			judge_in_the_narrowest_window(aState);
		}
	};

	void press_return(QPlainTextEdit& aComposer, const QString& aText)
	{
		aComposer.setPlainText(aText);
		QApplication::processEvents();
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(&aComposer, &press);
		QApplication::processEvents();
	}

	void open_review(qt::workbench_window& aWindow)
	{
		if (!aWindow.github_open_action()->isEnabled()) {
			aWindow.github_connect_action()->trigger();
		}
		wait([&] { return aWindow.github_open_action()->isEnabled(); }, "connected");
		aWindow.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		aWindow.home_open_action()->trigger();
		wait([&] { return aWindow.review_ask_button()->isVisible() && aWindow.github_open_action()->isEnabled(); }, "the window shows the pull request");
	}

	void agent_ready(qt::workbench_window& aWindow)
	{
		aWindow.open_agent_question();
		auto*      tab   = aWindow.agent_tab();
		const auto ready = [tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; };
		wait([tab] { return tab->setup_readiness() != qt::agent_setup_readiness::checking; }, "the agent setup looks for agents");
		// The Fixture agent session saves its entry as it starts.
		static_cast<void>(e2e::wait_until_plain(ready, std::chrono::milliseconds{ 15000 }));
		if (!ready()) {
			aWindow.open_agent_setup();
			const auto check = [&aWindow] {
				auto* const button = aWindow.agent_setup()->findChild<QPushButton*>(QStringLiteral("agentSetupCheck"));
				return button != nullptr && button->isVisible() && button->isEnabled() ? button : nullptr;
			};
			wait([&] { return check() != nullptr; }, "the setup offers Check it works on the fixture agent");
			check()->click();
			wait(ready, "Check it works passes on the fixture agent");
			aWindow.open_agent_question();
		}
		auto* const selector = aWindow.agent_panel()->profile_selector();
		if (const auto index = selector->findText(QStringLiteral("Fixture agent"), Qt::MatchStartsWith); index >= 0) {
			selector->setCurrentIndex(index);
		}
	}

	[[nodiscard]] auto tour_running(const qt::tour_agent_panel& aPanel) -> bool
	{
		return std::ranges::any_of(aPanel.session().Runs, [](const core::tour_agent_run& aRun) { return aRun.State == core::tour_agent_run_state::running; });
	}

	/// The button named \p aName that \p aRoot shows now: a part laid out
	/// again leaves its old buttons hidden until they are deleted.
	[[nodiscard]] auto shown_button(const QWidget& aRoot, const QString& aName) -> QAbstractButton*
	{
		for (auto* const button : aRoot.findChildren<QAbstractButton*>(aName)) {
			if (button->isVisibleTo(&aRoot)) {
				return button;
			}
		}
		return nullptr;
	}

	/// Whether \p aRoot shows the field a proposed text's source is written in.
	[[nodiscard]] auto shows_proposal_source(const QWidget& aRoot) -> bool
	{
		return std::ranges::any_of(aRoot.findChildren<QPlainTextEdit*>(QStringLiteral("tourProposalText")), [&aRoot](const QPlainTextEdit* aText) { return aText->isVisibleTo(&aRoot); });
	}

	/// Takes the keyboard off whatever the course's own gestures left it on.
	///
	/// \details The course presses buttons and fills fields by program. That
	/// leaves the keyboard where no pointer would: in the Agent tab's composer
	/// as the Tour tab opens, or on a tool the next state disables. Qt then
	/// hands it to the next button as if Tab had been pressed, and that button
	/// wears a keyboard focus ring no person driving with the pointer sees.
	/// Where the window itself gives the keyboard to a control (a question's
	/// safe answer, a field opened to write in) it is left there.
	void leave_no_keyboard_behind()
	{
		if (auto* const held = QApplication::focusWidget(); held != nullptr) {
			held->clearFocus();
		}
	}

	/// Drafts the tour on the fixture agent and adds its two stops, judging
	/// the Tour tab in each state a person passes on the way: nothing drafted,
	/// the draft running, its proposal waiting, a proposed text being edited,
	/// Reset tour asked and done, and one stop added while the other waits.
	void draft_tour(course& aRun)
	{
		auto& window = *aRun.Window;
		leave_no_keyboard_behind();
		window.open_review_tour();
		auto* const editor = window.tour_editor();
		auto* const tour   = editor->tour_agent();
		auto* const drawer = aRun.drawer();
		const auto  shows  = [drawer](const QWidget* const aPart) { return aPart != nullptr && aPart->isVisibleTo(drawer); };
		wait([&] { return editor->shown().Draft && !editor->shown().Loading && tour->draft_button()->isEnabled(); }, "the tour is ready to draft");
		leave_no_keyboard_behind();
		aRun.sweep_drawer("tour-empty");
		// The draft, held open while the drawer is dragged through its widths.
		aRun.hold_turns();
		tour->draft_button()->click();
		wait([&] { return tour_running(*tour) && tour->draft_button()->text() == QStringLiteral("Stop drafting"); }, "the draft runs");
		aRun.sweep_drawer("tour-drafting");
		expect(tour_running(*tour) && tour->draft_button()->text() == QStringLiteral("Stop drafting"), "the draft still ran while the drawer was dragged");
		aRun.release_turns();
		wait([&] {
			const auto& runs = tour->session().Runs;
			return !runs.empty() && runs.back().Request == core::tour_agent_request::draft_tour && runs.back().State != core::tour_agent_run_state::running;
		},
			 "the draft ends");
		expect(tour->session().Runs.back().State == core::tour_agent_run_state::answered, "the fixture agent drafts the tour: " + tour->transcript_text().toStdString());
		// The proposal, before any of it is applied: two proposed stops wait
		// in the tour, each with its own choices, under Apply all.
		const auto proposed = [&] {
			return shows(tour->accept_all_button()) && shows(tour->accept_button(0U)) && shows(tour->accept_button(1U)) && shows(tour->edit_button(0U)) && shown_button(*drawer, QStringLiteral("tourProposalDiscard")) != nullptr;
		};
		wait(proposed, "the draft's two stops are proposed, each with its choices, under Apply all");
		aRun.sweep_drawer("tour-proposal");
		// Its text edited: the proposed explanation's Markdown in a field.
		tour->edit_button(0U)->click();
		wait([&] { return shows(tour->proposal_editor(0U)); }, "a proposed stop's text opens as its Markdown source");
		aRun.sweep_drawer("tour-proposal-editing");
		tour->edit_button(0U)->click();
		wait([&] { return !shows(tour->proposal_editor(0U)); }, "and shows as it will read again");
		// Reset tour asks first, and then offers its undo.
		expect(tour->reset_button()->isEnabled(), "Reset tour is offered over a waiting proposal: " + tour->reset_button()->toolTip().toStdString());
		tour->reset_button()->click();
		wait([&] { return shows(tour->reset_confirm()) && shows(tour->reset_confirm_button()); }, "Reset tour asks first");
		aRun.sweep_drawer("tour-reset-asked");
		tour->reset_confirm_button()->click();
		wait([&] { return shows(tour->reset_done()) && shows(tour->undo_reset_button()) && tour->accept_button(0U) == nullptr; }, "the tour is reset and Undo reset is offered");
		aRun.sweep_drawer("tour-reset-done");
		tour->undo_reset_button()->click();
		wait(proposed, "Undo reset brings the proposal back");
		// One proposed stop added: it is the tour's, and asks for its Accept,
		// while the other still waits. Apply all is for two or more.
		tour->accept_button(0U)->click();
		wait([&] { return editor->shown().Draft && editor->shown().Draft->Stops.size() == 1U && !editor->shown().Saving && tour->accept_button(0U) == nullptr && shows(tour->accept_button(1U)); },
			 "the first proposed stop is the tour's and the second still waits");
		aRun.sweep_drawer("tour-proposal-one-added");
		tour->accept_button(1U)->click();
		wait([&] { return editor->shown().Draft && editor->shown().Draft->Stops.size() >= 2U && !editor->shown().Saving; }, "the draft's two stops are the tour's");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	// A slow turn of the fixture agent ("Take your time") runs about twenty
	// seconds: long enough to drag the drawer through its widths while it
	// runs. The application hands an agent only the environment names its
	// profile allows, so the agent's own pace cannot be set from here; a turn
	// that must outlast a look is held open instead (`course::hold_turns`).
	QApplication qtApplication{ argc, argv };
	if (argc < 2) {
		std::cerr << "usage: composed_drawer_fit_public_path FIXTURE_AGENT\n";
		return EXIT_FAILURE;
	}
	const auto    agent = std::filesystem::path{ argv[1] };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	auto search = e2e::stub_claude_search(std::filesystem::path{ data.path().toStdString() } / "fixture-agent-search", agent);
	if (!search) {
		std::cerr << "fixture agent search refused\n";
		return EXIT_FAILURE;
	}
	const auto                            report = qEnvironmentVariableIsSet("BPR_COMPOSED_DRAWER_REPORT");
	auto                                  run    = course{ };
	std::unique_ptr<app::profile_desktop> desktop;
	try {
		check_the_checks();
		// Beside the course's own copy of the agent, in the course's own root.
		run.HoldFile = data.path() + QStringLiteral("/fixture-agent-search/hold-turns");
		if (const auto root = qEnvironmentVariable("BPR_COMPOSED_DRAWER_RENDER_ROOT"); !root.isEmpty()) {
			run.RenderFolder = QStringLiteral("%1/%2-%3").arg(root, QGuiApplication::platformName(), QApplication::style()->name().toLower());
			expect(QDir{ }.mkpath(run.RenderFolder), "the render folder exists");
			const auto scheme = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QStringLiteral("dark") : QStringLiteral("light");
			run.RenderPrefix  = QStringLiteral("%1-%2-%3-").arg(qEnvironmentVariable("BPR_COMPOSED_DRAWER_RENDER_TAG", QStringLiteral("drawer")), scheme).arg(qRound(qApp->devicePixelRatio() * 100.0));
		}
		auto options = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		// The course's own copy, which it alone can hold: a file beside the
		// built agent would hold every course that runs it.
		options.ClaudeAgentExecutable = std::filesystem::path{ data.path().toStdString() } / "fixture-agent-search" / ("claude" + agent.extension().string());
		options.ClaudeSearch          = search;
		// As an owner's look runs: the Fixture agent is a saved agent entry.
		options.FixtureAgentSession = true;
		// Nothing of this desktop reaches the screen when the course draws
		// natively: no menu bar item, and a notification is counted, not shown.
		// The fixture's first head, with its next push known to the server:
		// the course moves the pull request's head to it later.
		options.EpochPhase                       = app::detail::github_epoch_fixture_phase::seed;
		options.HiddenAttendedWindow             = true;
		options.CaptureSystemNotificationDisplay = true;
		auto terminal                            = false;
		desktop                                  = access::start_desktop(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](app::profile_desktop_startup_result aResult) {
			terminal = true;
			if (aResult) {
				run.Owner = *aResult;
			}
		});
		wait([&] { return terminal; }, "the profile owner starts");
		expect(run.Owner != nullptr, "the profile owner is admitted");
		run.Owner->show();
		wait([&] { return run.Owner->windows().size() == 1U; }, "the window is admitted");
		auto* const window = run.Owner->window(run.Owner->windows().front());
		run.Window         = window;
		window->resize(1440, 960);
		open_review(*window);
		// As an owner's look opens: Review, on the renderer, with the
		// comparison and both drawers.
		auto step  = app::detail::course_modality_step{ };
		step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true, .ConversationsDrawer = true };
		wait([&] { return access::enter_course_modality(*run.Owner, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
		expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and both drawers");
		const auto path = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return window->go_to_file_line(path, 12U, false) && window->active_document_path() == path; }, "the renderer opens at a line");
		agent_ready(*window);
		auto* const tab    = window->agent_tab();
		auto* const editor = window->tour_editor();
		auto* const chat   = editor->side_chat();
		auto* const panel  = editor->tour_agent();
		const auto  quiet  = [&] { return access::agent_invocation_evidence(*run.Owner).Quiescent && tab->state() != qt::agent_tab_state::working; };
		const auto  ask    = [&](const QString& aQuestion) {
			const auto turns = tab->turn_count();
			// A send before the window has lent the session the review's
			// authority is refused and said; it is sent again.
			auto asking = QElapsedTimer{ };
			asking.start();
			while (tab->turn_count() == turns && tab->state() != qt::agent_tab_state::working && asking.elapsed() < 30000) {
				window->side_chat()->composer()->setPlainText(aQuestion);
				QApplication::processEvents();
				window->side_chat()->send_action()->trigger();
				static_cast<void>(e2e::wait_until_plain([&] { return tab->turn_count() > turns || tab->state() == qt::agent_tab_state::working; }, std::chrono::milliseconds{ 1500 }));
			}
			expect(tab->turn_count() > turns || tab->state() == qt::agent_tab_state::working, "the question is sent: " + aQuestion.toStdString());
		};

		// ---- Comments ------------------------------------------------------
		window->tray_tabs()->setCurrentIndex(0);
		settle();
		run.sweep_drawer("comments");

		// ---- The Agent tab -------------------------------------------------
		window->open_agent_question();
		settle();
		run.sweep_drawer("agent-ready");
		ask(QStringLiteral("What does this change do?"));
		wait([&] { return quiet() && tab->turn_count() >= 1U; }, "the first question is answered");
		run.sweep_drawer("agent-answered");
		// The keyboard on the answer's link. Tab from the answer's words
		// reaches the place the answer names, which wears the window's focus
		// ring; none of its words are selected.
		{
			auto* const answer = tab->turn_answer_view(0U);
			expect(answer != nullptr && answer->rendered_view()->isVisibleTo(run.drawer()), "the first answer reads in the Agent tab");
			auto* const reader = answer->rendered_view();
			reader->setFocus(Qt::TabFocusReason);
			tests::window_system_key(*reader, Qt::Key_Tab);
			settle();
			const auto on = [answer] { return answer->keyboard_link().toStdString(); };
			expect(reader->hasFocus() && on() == "src/engine/renderer.cpp:12", "Tab reaches the place the answer names: the keyboard is on \"" + on() + "\"");
			expect(!reader->textCursor().hasSelection(), "and selects none of its words: \"" + reader->textCursor().selectedText().toStdString() + "\" is selected");
			run.Shown = answer;
			run.sweep_drawer("agent-answer-link-keyboard");
			run.Shown = nullptr;
			expect(reader->hasFocus() && on() == "src/engine/renderer.cpp:12" && !reader->textCursor().hasSelection(), "the keyboard stays on the link while the drawer is dragged; it is on \"" + on() + "\"");
			leave_no_keyboard_behind();
		}
		ask(QStringLiteral("Review this pull request"));
		wait([&] { return quiet() && tab->turn_count() >= 2U; }, "the review is answered");
		run.sweep_drawer("agent-report");
		// Finding cards no report holds. An answer that is not a review
		// brings its findings as cards under it in the Agent tab itself, each
		// with Draft, Dismiss and Ask follow-up in a row. Three buttons side
		// by side were wider than a 280 pixel drawer leaves a card, and no
		// state of this course showed one: a review's findings are its
		// report's. Open, then one dismissed and saying so beside its Undo.
		{
			const auto turn = tab->turn_count();
			ask(QStringLiteral("Any findings worth a comment?"));
			wait([&] { return quiet() && tab->turn_count() > turn && tab->turn_cards(turn).size() == 3U; }, "an answer with findings brings three finding cards under it");
			const auto shown = [&run](const QWidget* const aCard) { return aCard != nullptr && aCard->isVisibleTo(run.drawer()); };
			for (const auto* const card : tab->turn_cards(turn)) {
				expect(shown(card) && shown(card->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"))) && shown(card->findChild<QPushButton*>(QStringLiteral("agentFindingDismiss")))
						   && shown(card->findChild<QPushButton*>(QStringLiteral("agentFindingFollowUp"))),
					   "each finding is an open card in the Agent tab, with Draft, Dismiss and Ask follow-up");
			}
			run.Shown = tab->turn_cards(turn).front();
			run.sweep_drawer("agent-finding-cards-open");
			auto* const second = tab->turn_cards(turn).at(1U);
			second->findChild<QPushButton*>(QStringLiteral("agentFindingDismiss"))->click();
			settle();
			expect(shown(second->findChild<QPushButton*>(QStringLiteral("agentFindingUndo"))) && shown(second->findChild<QLabel*>(QStringLiteral("agentFindingState"))), "a dismissed card says so beside its Undo");
			run.Shown = second;
			run.sweep_drawer("agent-finding-card-dismissed");
			run.Shown = nullptr;
			leave_no_keyboard_behind();
		}
		ask(QStringLiteral("Take your time and read the renderer."));
		wait([&] { return tab->state() == qt::agent_tab_state::working; }, "the slow turn runs");
		const auto longWords = QStringLiteral("Explain why the renderer clamps the resize before the next frame and whether the shaders under src/engine/shaders/post still compile");
		press_return(*window->side_chat()->composer(), longWords);
		press_return(*window->side_chat()->composer(), QStringLiteral("And is src/engine/shaders/post/a-rather-long-directory-name/bloom.frag still read?"));
		wait([&] { return tab->queue_list()->isVisible(); }, "two follow-ups wait in the Agent tab's pending list");
		window->side_chat()->composer()->setPlainText(QStringLiteral("One more, typed while it runs"));
		run.sweep_drawer("agent-queue");
		expect(tab->state() == qt::agent_tab_state::working, "the slow turn still ran while the drawer was dragged");
		window->side_chat()->composer()->clear();
		wait([&] { return quiet() && !tab->queue_list()->isVisible(); }, "the queued follow-ups go and are answered");
		// A run that stops for a reason of its own: the agent's process ends
		// before it answers. Its report stands on the question field, under
		// the stopped turn, with Resend and Show the log; then the log shown.
		{
			const auto answered = tab->turn_count();
			ask(QStringLiteral("FIXTURE_CRASH What happens when the agent stops early?"));
			wait([&] { return quiet() && tab->state() == qt::agent_tab_state::stopped && tab->banner()->isVisibleTo(run.drawer()) && tab->banner_first_button()->isVisibleTo(run.drawer()); }, "the agent stops before answering and the tab says so");
			auto* const log = tab->findChild<QWidget*>(QStringLiteral("agentStateLog"));
			expect(tab->banner_first_button()->text() == QStringLiteral("Resend") && tab->banner_second_button()->text() == QStringLiteral("Show the log") && log != nullptr && !log->isVisibleTo(run.drawer()),
				   "the stopped card offers Resend and Show the log: " + tab->banner_text().toStdString());
			expect(tab->banner_docked(), "the stopped card stands on the question field, in view wherever the thread is scrolled");
			expect(std::ranges::all_of(tab->suggestion_buttons(), [](const QPushButton* aPrompt) { return aPrompt->isEnabled(); }), "a run that stopped for a reason of its own leaves the suggested prompts to press");
			run.Shown = tab->banner();
			run.sweep_drawer("agent-stopped");
			tab->banner_second_button()->click();
			wait([&] { return log->isVisibleTo(run.drawer()) && tab->banner_second_button()->text() == QStringLiteral("Hide the log"); }, "Show the log shows what the agent wrote");
			run.Shown = tab->banner();
			run.sweep_drawer("agent-stopped-log");
			run.Shown = nullptr;
			expect(tab->banner_first_button()->isVisibleTo(run.drawer()) && tab->banner_first_button()->isEnabled(), "Resend stays to press beside the log");
			tab->banner_second_button()->click();
			wait([&] { return !log->isVisibleTo(run.drawer()); }, "Hide the log folds it again");
			// The stopped turn deleted: its report goes with it.
			auto* const remove = tab->turn_delete_button(tab->turn_count() - 1U);
			expect(remove != nullptr && remove->isVisibleTo(run.drawer()), "the stopped turn offers Delete");
			remove->click();
			wait([&] { return tab->turn_count() == answered && !tab->banner()->isVisibleTo(run.drawer()); }, "the stopped turn is deleted and its report goes");
			leave_no_keyboard_behind();
		}

		// ---- The Tour tab and its chat column --------------------------------
		draft_tour(run);
		window->tour_editor()->show_stops();
		settle();
		expect(window->tray_tabs()->currentIndex() == 1, "the Tour tab shows the route");
		run.sweep_drawer("tour-route");
		const auto stop  = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = editor->shown().Draft->Stops.front().Id };
		const auto whole = core::tour_thread{ };
		const auto shows = [&run](const QWidget* const aPart) { return aPart != nullptr && aPart->isVisibleTo(run.drawer()); };
		editor->go_to_stop(stop.Stop);
		// The stop written: its title and explanation in fields, with the
		// card's tools; then Remove stop, which asks first.
		editor->begin_stop_edit();
		wait([&] { return editor->stop_view().Mode == core::tour_stop_view_mode::edit && shows(editor->stop_body()) && shows(editor->stop_remove_button()); }, "the pencil opens the stop's fields");
		run.sweep_drawer("tour-stop-editing");
		editor->stop_remove_button()->click();
		wait([&] { return shown_button(*run.drawer(), QStringLiteral("tourStopRemoveKeep")) != nullptr && shown_button(*run.drawer(), QStringLiteral("tourStopRemoveYes")) != nullptr; }, "Remove stop asks first");
		run.sweep_drawer("tour-stop-remove-asked");
		shown_button(*run.drawer(), QStringLiteral("tourStopRemoveKeep"))->click();
		editor->cancel_stop_edit();
		wait([&] { return editor->stop_view().Mode != core::tour_stop_view_mode::edit && editor->shown().Draft->Stops.size() == 2U; }, "the stop is kept as it was");
		editor->open_side_chat(stop);
		wait([&] { return chat->isVisible() && chat->column_open(); }, "the stop's chat opens beside the tour");
		run.sweep_chat("tour-stop-chat-new");
		const auto sends = [&](const QString& aText) {
			chat->composer()->setPlainText(aText);
			wait([&] { return chat->send_button()->accessibleName() == QStringLiteral("Send") && chat->send_button()->isEnabled(); }, "the tour agent is free: Send sends");
			press_return(*chat->composer(), aText);
		};
		const auto asked = [&](const core::tour_thread& aThread) {
			return std::ranges::count_if(panel->session().Runs, [&](const core::tour_agent_run& aRun) { return aRun.Request == core::tour_agent_request::ask && aRun.Thread == aThread; });
		};
		sends(QStringLiteral("Why does the renderer clear before it presents?"));
		wait([&] { return asked(stop) == 1 && !tour_running(*panel); }, "the stop's question is answered");
		run.sweep_chat("tour-stop-chat-answered");
		sends(QStringLiteral("Take your time: what does this stop show?"));
		wait([&] { return tour_running(*panel) && asked(stop) == 2; }, "the stop's slow question runs");
		press_return(*chat->composer(), longWords);
		press_return(*chat->composer(), QStringLiteral("Third: anything else?"));
		wait([&] { return chat->pending_list()->isVisible(); }, "two follow-ups wait in the stop's pending list");
		chat->composer()->setPlainText(QStringLiteral("Fourth, typed while it runs"));
		run.sweep_chat("tour-stop-chat-queue");
		expect(tour_running(*panel), "the stop's slow turn still ran while the column was dragged");
		chat->composer()->clear();
		wait([&] { return asked(stop) == 4 && !tour_running(*panel); }, "the stop's queued follow-ups go and are answered");
		// Update this stop from this chat: the stop's explanation rewritten,
		// offered as a suggestion with its choices.
		const auto agentIsFree = [&] {
			chat->composer()->setPlainText(QStringLiteral("x"));
			wait([&] { return chat->send_button()->accessibleName() == QStringLiteral("Send") && chat->send_button()->isEnabled(); }, "the tour agent is free");
			chat->composer()->clear();
		};
		agentIsFree();
		expect(chat->can_update() && chat->request_update(), "Update this stop from this chat is asked");
		wait([&] { return asked(stop) == 5 && !tour_running(*panel) && chat->findChild<QPushButton*>(QStringLiteral("tourSideChatAccept")) != nullptr; }, "the update proposes the stop's new explanation");
		// The suggestion's choices as a proposed part's are in the tour: a
		// pencil and a trash, named in their tooltips, and Accept in words at
		// the row's right end. No Ask more: it opened the chat the card is in.
		{
			const auto* const accept = shown_button(*chat, QStringLiteral("tourSideChatAccept"));
			const auto* const pencil = dynamic_cast<const qt::quiet_tool_button*>(shown_button(*chat, QStringLiteral("tourSideChatEdit")));
			const auto* const trash  = dynamic_cast<const qt::quiet_tool_button*>(shown_button(*chat, QStringLiteral("tourSideChatDiscard")));
			expect(accept != nullptr && accept->text() == QStringLiteral("Accept"), "the suggestion's Accept is in words");
			expect(pencil != nullptr && pencil->text().isEmpty() && !pencil->icon().isNull() && pencil->accessibleName() == QStringLiteral("Edit text") && pencil->toolTip().startsWith(QStringLiteral("Edit text\n")), "Edit text is the pencil, named in its tooltip");
			expect(trash != nullptr && trash->text().isEmpty() && !trash->icon().isNull() && trash->accessibleName() == QStringLiteral("Discard") && trash->toolTip().startsWith(QStringLiteral("Discard\n")), "Discard is the trash, named in its tooltip");
			expect(shown_button(*chat, QStringLiteral("tourSideChatAskMore")) == nullptr, "the suggestion has no Ask more");
			const auto at = [&](const QAbstractButton* const aButton) { return QRect{ aButton->mapTo(chat, QPoint{ 0, 0 }), aButton->size() }; };
			expect(at(pencil).right() < at(trash).left() && at(trash).right() < at(accept).left() && at(pencil).center().y() > at(accept).top() && at(pencil).center().y() < at(accept).bottom(), "the pencil, the trash and Accept stand on one row, Accept at its right");
		}
		run.sweep_chat("tour-stop-chat-suggestion");
		// Edit text on the suggestion: its source in a field on the card.
		expect(shown_button(*chat, QStringLiteral("tourSideChatEdit")) != nullptr, "the suggestion offers Edit text");
		shown_button(*chat, QStringLiteral("tourSideChatEdit"))->click();
		wait([&] {
			const auto* const edit  = shown_button(*chat, QStringLiteral("tourSideChatEdit"));
			const auto        texts = chat->findChildren<QPlainTextEdit*>(QStringLiteral("tourSideChatProposalText"));
			return edit != nullptr && std::ranges::any_of(texts, [&](const QPlainTextEdit* aText) { return aText->isVisibleTo(chat); });
		},
			 "Edit text shows the suggestion's source");
		run.sweep_chat("tour-stop-chat-suggestion-editing");
		shown_button(*chat, QStringLiteral("tourSideChatEdit"))->click();
		wait([&] {
			const auto texts = chat->findChildren<QPlainTextEdit*>(QStringLiteral("tourSideChatProposalText"));
			return shown_button(*chat, QStringLiteral("tourSideChatEdit")) != nullptr && std::ranges::none_of(texts, [&](const QPlainTextEdit* aText) { return aText->isVisibleTo(chat); });
		},
			 "and shows as it will read again");
		const auto replied = [&](const core::tour_thread& aThread) {
			const auto& runs  = panel->session().Runs;
			const auto  found = std::ranges::find_if(runs.rbegin(), runs.rend(), [&](const core::tour_agent_run& aRun) { return aRun.Request == core::tour_agent_request::ask && aRun.Thread == aThread; });
			return found != runs.rend() ? found->Reply : std::string{ };
		};
		const auto carried = [&] {
			const auto* const line = chat->findChild<QLabel*>(QStringLiteral("tourCarriedAnswer"));
			return line != nullptr && line->isVisibleTo(chat) ? line->text() : QString{ };
		};
		// Enhance on the stop: a fresh run, outside the chat's conversation.
		// Its suggestion waits on the stop, and the chat's next question
		// starts from it without a button to ask for that.
		agentIsFree();
		expect(shown_button(*run.drawer(), QStringLiteral("tourProposalAccept")) == nullptr, "nothing waits on the stop's card before its Enhance");
		expect(chat->enhance(stop, std::nullopt), "Enhance is asked of the stop");
		wait([&] { return shown_button(*run.drawer(), QStringLiteral("tourProposalAccept")) != nullptr; }, "the Enhance's suggestion waits on the stop's card");
		agentIsFree();
		expect(carried() == QStringLiteral("Your next question starts from the Enhance suggestion."), "the chat says its next question starts from the Enhance's suggestion: " + carried().toStdString());
		run.sweep_chat("tour-stop-chat-enhanced");
		sends(QStringLiteral("Why is that wording better?"));
		wait([&] { return asked(stop) == 6 && !tour_running(*panel); }, "the question after the Enhance is answered");
		expect(replied(stop).find("I started from the earlier answer") != std::string::npos, "the question after the Enhance carried the Enhance's answer: " + replied(stop));
		expect(carried().isEmpty(), "and once it is carried the chat says no more of it: " + carried().toStdString());
		// The whole tour's chat, opened by the tour's own chat toggle: nothing
		// asked yet, and its first question starts from Draft Tour's answer.
		expect(panel->chat_button() != nullptr && panel->chat_button()->isVisibleTo(run.drawer()), "the tour shows its own chat toggle");
		panel->chat_button()->click();
		wait([&] { return chat->isVisibleTo(run.drawer()) && chat->current() == std::optional{ whole }; }, "the tour's chat toggle opens the whole tour's chat");
		expect(carried() == QStringLiteral("Your next question starts from the draft's answer."), "the whole tour's chat says its next question starts from the draft's answer: " + carried().toStdString());
		run.sweep_chat("tour-whole-chat-new");
		sends(QStringLiteral("Is the tour in a good order?"));
		wait([&] { return asked(whole) == 1 && !tour_running(*panel); }, "the whole tour's question is answered");
		expect(replied(whole).find("I started from the earlier answer") != std::string::npos, "the whole tour's first question carried Draft Tour's answer: " + replied(whole));
		expect(carried().isEmpty(), "and once it is carried the chat says no more of it: " + carried().toStdString());
		run.sweep_chat("tour-whole-chat-answered");
		sends(QStringLiteral("Rewrite stop 1 and Drop stop 2"));
		wait([&] { return asked(whole) == 2 && !tour_running(*panel); }, "the whole tour's revision is proposed");
		settle();
		run.sweep_chat("tour-whole-chat-revision");
		// Review each: every part of the revision waits on the card of the
		// stop it changes, with its own choices. Judged with the chat closed,
		// where the card is at its narrowest.
		expect(chat->revision_review() != nullptr && chat->revision_review()->isVisibleTo(chat), "the revision offers Review each");
		chat->revision_review()->click();
		const auto second  = editor->shown().Draft->Stops.back().Id;
		const auto primary = [&](const QString& aWords) {
			auto* const button = shown_button(*run.drawer(), QStringLiteral("tourProposalAccept"));
			return button != nullptr && button->text() == aWords && shown_button(*run.drawer(), QStringLiteral("tourProposalDiscard")) != nullptr ? button : nullptr;
		};
		chat->close_button()->click();
		wait([&] { return !chat->isVisibleTo(run.drawer()); }, "the chat closes");
		// The first stop's new explanation, where the stop's own Accept was.
		editor->go_to_stop(stop.Stop);
		wait([&] { return primary(QStringLiteral("Accept")) != nullptr && shown_button(*run.drawer(), QStringLiteral("tourProposalEdit")) != nullptr; }, "the proposed explanation waits on its stop's card");
		run.Shown = primary(QStringLiteral("Accept"));
		run.sweep_drawer("tour-revision-text-on-stop");
		shown_button(*run.drawer(), QStringLiteral("tourProposalEdit"))->click();
		wait([&] { return shows_proposal_source(*run.drawer()); }, "the proposed explanation opens as its Markdown source on the card");
		run.Shown = primary(QStringLiteral("Accept"));
		run.sweep_drawer("tour-revision-text-editing-on-stop");
		shown_button(*run.drawer(), QStringLiteral("tourProposalEdit"))->click();
		wait([&] { return !shows_proposal_source(*run.drawer()); }, "and shows as it will read again");
		// The second stop's removal.
		editor->go_to_stop(second);
		wait([&] { return primary(QStringLiteral("Remove")) != nullptr; }, "the proposed removal waits on its stop's card with Remove and Discard");
		run.Shown = primary(QStringLiteral("Remove"));
		run.sweep_drawer("tour-revision-removal-on-stop");
		run.Shown = nullptr;
		// The whole tour's chat again, as a person leaves it for the Agent tab.
		editor->open_side_chat(whole);
		wait([&] { return chat->isVisibleTo(run.drawer()) && chat->current() == std::optional{ whole }; }, "the whole tour's chat opens again");

		// ---- After the pull request's head moves ----------------------------
		window->open_agent_question();
		const auto mode      = [&] { return tab->session_notice(); };
		const auto historyIn = [&] { return mode() != nullptr && !tab->ask_button()->property("conversationLoading").toBool(); };
		const auto selected  = [&] {
			auto id = std::string{ };
			QMetaObject::invokeMethod(tab->history_button()->menu(), "aboutToShow");
			static_cast<void>(e2e::wait_until_plain([&] { id = tab->session_menu()->selected(); return !id.empty() && historyIn(); }, e2e::default_agent_budget));
			return id;
		};
		const auto open = [&](const std::string& aId) {
			QMetaObject::invokeMethod(tab->history_button()->menu(), "aboutToShow");
			wait([&] { const auto ids = tab->session_menu()->ids(); return historyIn() && std::ranges::find(ids, aId) != ids.end(); }, "History lists the session");
			tab->session_menu()->open(aId);
			wait([&] { return historyIn() && tab->session_menu()->selected() == aId && tab->state() != qt::agent_tab_state::working; }, "the session opens");
		};
		const auto seeded = selected();
		expect(!seeded.empty(), "the Agent tab's session is in History");
		// A transcript replay of it: a conversation with no checkout of its
		// own, which stays at the head it read.
		auto* const replay = tab->findChild<QAction*>(QStringLiteral("agentCarryOver"));
		expect(replay != nullptr && replay->text() == QStringLiteral("Carry this conversation over to Fixture agent"), "History offers to carry the conversation over to the agent the tab asks");
		replay->trigger();
		auto replayed = std::string{ };
		wait([&] { replayed = selected(); return replayed != seeded && mode()->text().startsWith(QStringLiteral("Carried over from a saved transcript.")) && tab->ask_button()->isEnabled(); }, "the carried-over session opens");
		const auto earlierHead = window->side_chat()->head_oid_text().left(7);
		// The author pushes; Refresh reads the pull request at its new head.
		const auto pinned = access::agent_chat_evidence(*run.Owner).Observed;
		expect(pinned.has_value(), "the chat knows the head it reads");
		window->github_refresh_action()->trigger();
		wait([&] {
			const auto current = access::agent_chat_evidence(*run.Owner).Observed;
			return current && current->HeadOid != pinned->HeadOid;
		},
			 "the pull request's head moves");
		wait([&] { return quiet() && access::review_ledger_settled(*run.Owner) && access::review_ledger_available(*run.Owner) == std::optional{ true }; }, "the moved head's review settles");
		{
			auto last   = std::optional<core::comparison_pair>{ };
			auto stable = QElapsedTimer{ };
			wait([&] {
				const auto pairs = access::tab_comparison_evidence(*run.Owner);
				if (!pairs.Accepted || pairs.Accepted != pairs.Window || pairs.Accepted != last) {
					last = pairs.Accepted;
					stable.start();
					return false;
				}
				return stable.elapsed() >= 1000;
			},
				 "the moved head's comparison settles");
		}
		open(seeded);
		open(replayed);
		wait([&] { return tab->state() == qt::agent_tab_state::head_moved && tab->banner()->isVisible() && tab->banner_first_button()->isVisible() && tab->banner_second_button()->isVisible(); },
			 "the replay that read the earlier head offers its card");
		expect(tab->banner_first_button()->text() == QStringLiteral("Continue at ") + earlierHead, "the card offers to continue at the head it read: " + tab->banner_first_button()->text().toStdString());
		expect(mode()->isVisibleTo(tab) && mode()->text() == QStringLiteral("Carried over from a saved transcript. Fixture agent has the questions and answers, not what the earlier agent read."),
			   "the carried-over session says so on the question field, naming the agent: " + mode()->text().toStdString());
		// While the card asks which revision to go on with, no suggested
		// prompt can be pressed, and each says what to choose first.
		const auto promptsWait = [&](const QString& aWhy) {
			const auto prompts = tab->suggestion_buttons();
			return !prompts.empty() && std::ranges::all_of(prompts, [&](const QPushButton* aPrompt) { return aPrompt->isVisibleTo(run.drawer()) && !aPrompt->isEnabled() && aPrompt->toolTip() == aWhy; });
		};
		expect(promptsWait(QStringLiteral("Choose Continue at %1 or New session first.").arg(earlierHead)),
			   "the suggested prompts wait on the card's choice and say so: " + (tab->suggestion_buttons().empty() ? std::string{ "none shown" } : tab->suggestion_buttons().front()->toolTip().toStdString()));
		run.Shown = tab->banner();
		run.sweep_drawer("agent-pinned-offered");
		// The session with a checkout follows the head on its next question.
		// The tour's agent holds the pull request's checkout, so the move is
		// refused and the stopped card offers the two ways on.
		open(seeded);
		// The replay's card goes as the session's own conversation comes in.
		// A question sent while that card still shows is the replay's to
		// answer, and is not sent: the course waits for the card to go.
		static_cast<void>(e2e::wait_until_plain([&] { return tab->state() != qt::agent_tab_state::head_moved; }, std::chrono::milliseconds{ 10000 }));
		ask(QStringLiteral("What changed in this push?"));
		wait([&] { return quiet() && tab->state() == qt::agent_tab_state::stopped && tab->banner()->isVisible(); }, "the question is refused: the session's workspace was not moved");
		expect(tab->banner_first_button()->isVisibleTo(tab->banner()) && tab->banner_second_button()->isVisibleTo(tab->banner()) && tab->banner_first_button()->text() == QStringLiteral("Continue at ") + earlierHead
				   && tab->banner_second_button()->text() == QStringLiteral("New session"),
			   "the stopped card offers to continue at the earlier head or a new session: " + tab->banner_text().toStdString());
		// The refused turn names those two by the words on the card's chips.
		{
			const auto said = tab->turn_answer(tab->turn_count() - 1U);
			expect(said.contains(QStringLiteral(" Choose Continue at %1 or New session. No question was sent.").arg(earlierHead)) && !said.contains(QStringLiteral("tart a new session")),
				   "the refused turn names the card's two choices as the card does: " + said.toStdString());
		}
		// A session that goes on in the ordinary way says nothing of it; the
		// one before it said that it was carried over.
		expect(mode()->isHidden() && mode()->text().isEmpty(), "the refused session, which was not carried over, has no notice: " + mode()->text().toStdString());
		// The card waits here too, so the prompts do.
		expect(promptsWait(QStringLiteral("Choose Continue at %1 or New session first.").arg(earlierHead)),
			   "after a refused question the suggested prompts wait on the card's choice too: " + (tab->suggestion_buttons().empty() ? std::string{ "none shown" } : tab->suggestion_buttons().front()->toolTip().toStdString()));
		run.sweep_drawer("agent-pinned-refused");
		// And a question typed meanwhile is not sent: it stays in the field,
		// which says why.
		{
			const auto turns = tab->turn_count();
			window->side_chat()->composer()->setPlainText(QStringLiteral("And is the shader still read?"));
			tab->ask_button()->click();
			settle_for(400);
			expect(tab->turn_count() == turns && tab->state() == qt::agent_tab_state::stopped && tab->send_refusal_text() == QStringLiteral("Not sent. The pull request has moved on. Choose Continue or New session, then send.")
					   && window->side_chat()->composer()->toPlainText() == QStringLiteral("And is the shader still read?"),
				   "a question typed while the card waits is not sent and stays in the field: " + tab->send_refusal_text().toStdString());
			run.sweep_drawer("agent-pinned-refused-typed");
			window->side_chat()->composer()->clear();
		}
		// New session on the card: an empty session at the head under review,
		// shown at once, and the kept message sent there. Not the old
		// transcript with one more turn under it.
		{
			const auto refusedTurns = tab->turn_count();
			expect(refusedTurns > 1U, "the refused session has a transcript to leave behind");
			run.hold_turns();
			tab->banner_second_button()->click();
			wait([&] { return tab->state() == qt::agent_tab_state::working && tab->turn_count() == 1U; }, "New session sends the kept message");
			expect(tab->turn_question(0U) == QStringLiteral("What changed in this push?"), "the new session's first turn is the kept message: " + tab->turn_question(0U).toStdString());
			expect(tab->session_menu()->selected() != seeded && !tab->session_menu()->selected().empty(), "History's tick is on the new session");
			expect(!window->side_chat()->head_oid_text().contains(earlierHead), "the new session reads the head under review: " + window->side_chat()->head_oid_text().toStdString());
			expect(!tab->banner()->isVisibleTo(run.drawer()), "and the card that asked is gone");
			run.Shown = nullptr;
			run.sweep_drawer("agent-new-session-kept-sent");
			expect(tab->state() == qt::agent_tab_state::working, "the kept message's turn still ran while the drawer was dragged");
			run.release_turns();
			wait([&] { return quiet(); }, "the kept message's turn ends");
			expect(tab->state() == qt::agent_tab_state::idle && tab->turn_count() == 1U && tab->turn_answer(0U).contains(QStringLiteral("Fixture answer")), "the kept message is answered in the new session: " + tab->turn_answer(0U).toStdString());
			run.sweep_drawer("agent-new-session-answered");
			// The session left behind is in History with its turns.
			open(seeded);
			expect(tab->turn_count() >= 2U, "the session left behind keeps its transcript in History");
		}
		// The stop's chat, whose session read the earlier head.
		window->open_review_tour();
		editor->go_to_stop(stop.Stop);
		editor->open_side_chat(stop);
		wait([&] { return chat->isVisible() && chat->current() == std::optional{ stop }; }, "the stop's chat opens after the head moved");
		QMetaObject::invokeMethod(chat->history_button()->menu(), "aboutToShow");
		wait([&] {
			const auto* const old = chat->findChild<QToolButton*>(QStringLiteral("tourContinueOldRevision"));
			return old != nullptr && old->isVisibleTo(chat) && !chat->history_button()->property("loading").toBool();
		},
			 "the stop's chat says its session read the earlier head");
		expect(chat->new_session_action()->isEnabled(), "History offers New session in a chat that read the earlier head");
		run.sweep_chat("tour-stop-chat-head-moved");

		const auto reached = std::vector<std::string>{ "comments",
													   "agent-ready",
													   "agent-answered",
													   "agent-answer-link-keyboard",
													   "agent-report",
													   "agent-finding-cards-open",
													   "agent-finding-card-dismissed",
													   "agent-queue",
													   "agent-stopped",
													   "agent-stopped-log",
													   "tour-empty",
													   "tour-drafting",
													   "tour-proposal",
													   "tour-proposal-editing",
													   "tour-reset-asked",
													   "tour-reset-done",
													   "tour-proposal-one-added",
													   "tour-route",
													   "tour-stop-editing",
													   "tour-stop-remove-asked",
													   "tour-stop-chat-new",
													   "tour-stop-chat-answered",
													   "tour-stop-chat-queue",
													   "tour-stop-chat-suggestion",
													   "tour-stop-chat-suggestion-editing",
													   "tour-stop-chat-enhanced",
													   "tour-whole-chat-new",
													   "tour-whole-chat-answered",
													   "tour-whole-chat-revision",
													   "tour-revision-text-on-stop",
													   "tour-revision-text-editing-on-stop",
													   "tour-revision-removal-on-stop",
													   "agent-pinned-offered",
													   "agent-pinned-refused",
													   "agent-pinned-refused-typed",
													   "agent-new-session-kept-sent",
													   "agent-new-session-answered",
													   "tour-stop-chat-head-moved" };
		expect(run.States == reached, "every state of the course is reached, in order (" + std::to_string(run.States.size()) + " of " + std::to_string(reached.size()) + ")");
		expect(run.EdgesDrawn > 0U, "the course judged a chat's transcript off its start, with its edge drawn");
		for (const auto& fault : run.Faults) {
			std::cerr << "FAULT: " << fault << '\n';
		}
		std::cout << "composed-drawer-fit: " << run.States.size() << " states, " << run.Faults.size() << " faults, " << run.EdgesDrawn << " transcript edges drawn, dpr " << qApp->devicePixelRatio() << ", " << QGuiApplication::platformName().toStdString() << "/"
				  << QApplication::style()->name().toStdString() << '\n';
		window->findChild<QAction*>(QStringLiteral("quitProfilesAction"))->trigger();
		wait([&] { return desktop->is_quiescent(); }, "Quit drains every session");
		if (!run.Faults.empty() && !report) {
			std::cerr << run.Faults.size() << " states of the right drawer pan sideways, cut a control off, cover what they scroll or say internal words\n";
			return EXIT_FAILURE;
		}
		std::cout << (run.Faults.empty() ? "composed-drawer-fit:pass\n" : "composed-drawer-fit:reported\n");
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "composed-drawer-fit: " << aError.what() << '\n';
		for (const auto& fault : run.Faults) {
			std::cerr << "FAULT: " << fault << '\n';
		}
		if (run.Window != nullptr) {
			const auto* const tab = run.Window->agent_tab();
			std::cerr << "status: " << run.Window->status_text().toStdString() << "\nagent tab: state " << static_cast<int>(tab->state()) << ", " << tab->turn_count() << " turns, card: " << tab->banner_text().toStdString()
					  << "\ntour chat: " << run.Window->tour_editor()->side_chat()->chat_text().toStdString() << '\n';
			for (const auto& made : run.Window->tour_editor()->tour_agent()->session().Runs) {
				std::cerr << "tour run request=" << static_cast<int>(made.Request) << " thread=" << static_cast<int>(made.Thread.Kind) << ":" << made.Thread.Stop.value() << " state=" << static_cast<int>(made.State)
						  << " question=" << made.Question << " reply=" << made.Reply << '\n';
			}
		}
		return EXIT_FAILURE;
	}
}
