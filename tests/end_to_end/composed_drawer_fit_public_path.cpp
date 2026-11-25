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
///   running over a pending list, and, after the pull request's head moves,
///   with the card of a transcript replay that read the earlier head and the
///   card of a question refused because work is in the way of the move;
/// - the Tour tab's route, and its chat column about a stop: nothing asked
///   yet, answered, a turn running over a pending list, a suggested rewrite
///   of the stop, the whole tour's chat, and a chat whose session read the
///   earlier head.
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
/// - no two buttons lie over one another.
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
/// same binary draws what the application's own window draws.
/// `BPR_COMPOSED_DRAWER_REPORT`, when set, makes the run report every fault
/// and finish, for a before-and-after record, instead of failing on them.
///
///     composed_drawer_fit_public_path FIXTURE_AGENT

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_pending_list.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"
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
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

	/// \brief Where a control is drawn in \p aSurface. A native push button's
	/// widget is larger than its face, so two such widgets may overlap where
	/// their faces do not.
	[[nodiscard]] auto face_in(const QWidget& aControl, const QWidget& aSurface) -> QRect
	{
		auto face = aControl.rect();
		if (const auto* const push = qobject_cast<const QPushButton*>(&aControl); push != nullptr) {
			auto option = QStyleOptionButton{ };
			option.initFrom(push);
			if (const auto item = push->style()->subElementRect(QStyle::SE_PushButtonLayoutItem, &option, push); item.isValid()) {
				face = item;
			}
		}
		return face.translated(aControl.mapTo(&aSurface, QPoint{ 0, 0 }));
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
				const auto* const tool  = qobject_cast<const QToolButton*>(button);
				const auto        words = !button->text().isEmpty() && (tool == nullptr || tool->toolButtonStyle() != Qt::ToolButtonIconOnly);
				// A button that wraps its words needs its longest word.
				if (words && button->width() < button->minimumSizeHint().width()) {
					found.push_back(QStringLiteral("%1 is %2 wide, its words need %3").arg(describe(*button)).arg(button->width()).arg(button->minimumSizeHint().width()));
				}
				if (words && button->hasHeightForWidth() && button->height() < button->heightForWidth(button->width())) {
					found.push_back(QStringLiteral("%1 is %2 tall, its words need %3").arg(describe(*button)).arg(button->height()).arg(button->heightForWidth(button->width())));
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
					found.push_back(QStringLiteral("%1 lies over %2").arg(describe(*one), describe(*two)));
				}
			}
		}
		return found;
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
			settle();
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

	/// Drafts the tour on the fixture agent and accepts it whole.
	void draft_tour(qt::workbench_window& aWindow)
	{
		aWindow.open_review_tour();
		auto* const editor = aWindow.tour_editor();
		auto* const tour   = editor->tour_agent();
		wait([&] { return editor->shown().Draft && !editor->shown().Loading && tour->draft_button()->isEnabled(); }, "the tour is ready to draft");
		tour->draft_button()->click();
		wait([&] {
			const auto& runs = tour->session().Runs;
			return !runs.empty() && runs.back().Request == core::tour_agent_request::draft_tour && runs.back().State != core::tour_agent_run_state::running;
		},
			 "the draft ends");
		expect(tour->session().Runs.back().State == core::tour_agent_run_state::answered, "the fixture agent drafts the tour: " + tour->transcript_text().toStdString());
		wait([&] { return tour->accept_all_button()->isVisible() || (editor->shown().Draft && editor->shown().Draft->Stops.size() >= 2U); }, "the draft is proposed");
		if (tour->accept_all_button()->isVisible()) {
			tour->accept_all_button()->click();
		}
		wait([&] { return editor->shown().Draft && editor->shown().Draft->Stops.size() >= 2U && !editor->shown().Saving; }, "the draft's two stops are the tour's");
	}

	[[nodiscard]] auto tour_running(const qt::tour_agent_panel& aPanel) -> bool
	{
		return std::ranges::any_of(aPanel.session().Runs, [](const core::tour_agent_run& aRun) { return aRun.State == core::tour_agent_run_state::running; });
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	// A slow turn of the fixture agent runs about eight seconds here: long
	// enough to drag the drawer through its widths while it runs.
	qputenv("BPR_FIXTURE_AGENT_SLOW_STEP_MS", "400");
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
		if (const auto root = qEnvironmentVariable("BPR_COMPOSED_DRAWER_RENDER_ROOT"); !root.isEmpty()) {
			run.RenderFolder = QStringLiteral("%1/%2-%3").arg(root, QGuiApplication::platformName(), QApplication::style()->name().toLower());
			expect(QDir{ }.mkpath(run.RenderFolder), "the render folder exists");
			const auto scheme = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QStringLiteral("dark") : QStringLiteral("light");
			run.RenderPrefix  = QStringLiteral("%1-%2-%3-").arg(qEnvironmentVariable("BPR_COMPOSED_DRAWER_RENDER_TAG", QStringLiteral("drawer")), scheme).arg(qRound(qApp->devicePixelRatio() * 100.0));
		}
		auto options                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.ClaudeAgentExecutable = agent;
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
		desktop                     = access::start_desktop(app::detail::github_public_path_fixture::returning_review_v1, std::move(options), [&](app::profile_desktop_startup_result aResult) {
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
		ask(QStringLiteral("Review this pull request"));
		wait([&] { return quiet() && tab->turn_count() >= 2U; }, "the review is answered");
		run.sweep_drawer("agent-report");
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

		// ---- The Tour tab and its chat column --------------------------------
		draft_tour(*window);
		window->tour_editor()->show_stops();
		settle();
		expect(window->tray_tabs()->currentIndex() == 1, "the Tour tab shows the route");
		run.sweep_drawer("tour-route");
		const auto stop  = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = editor->shown().Draft->Stops.front().Id };
		const auto whole = core::tour_thread{ };
		editor->go_to_stop(stop.Stop);
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
		// Enhance: the stop's explanation rewritten, offered as a suggestion
		// with its choices.
		const auto free = [&] {
			chat->composer()->setPlainText(QStringLiteral("x"));
			wait([&] { return chat->send_button()->accessibleName() == QStringLiteral("Send") && chat->send_button()->isEnabled(); }, "the tour agent is free");
			chat->composer()->clear();
		};
		free();
		expect(chat->can_update() && chat->request_update(), "Update this stop from this chat is asked");
		wait([&] { return asked(stop) == 5 && !tour_running(*panel) && chat->findChild<QPushButton*>(QStringLiteral("tourSideChatAccept")) != nullptr; }, "the update proposes the stop's new explanation");
		run.sweep_chat("tour-stop-chat-suggestion");
		free();
		expect(chat->enhance(stop, std::nullopt), "Enhance is sent for the stop");
		settle_for(6000);
		run.sweep_chat("tour-stop-enhanced");
		// The whole tour's chat: an answer, then a proposed revision.
		editor->open_side_chat(whole);
		wait([&] { return chat->current() == std::optional{ whole }; }, "the whole tour's chat opens");
		sends(QStringLiteral("Is the tour in a good order?"));
		wait([&] { return asked(whole) == 1 && !tour_running(*panel); }, "the whole tour's question is answered");
		run.sweep_chat("tour-whole-chat-answered");
		sends(QStringLiteral("Drop stop 2"));
		wait([&] { return asked(whole) == 2 && !tour_running(*panel); }, "the whole tour's revision is proposed");
		settle();
		run.sweep_chat("tour-whole-chat-revision");

		// ---- After the pull request's head moves ----------------------------
		window->open_agent_question();
		const auto mode      = [&] { return tab->findChild<QLabel*>(QStringLiteral("agentContinuationMode")); };
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
		auto* const replay = tab->findChild<QAction*>(QStringLiteral("agentTranscriptReplay"));
		expect(replay != nullptr, "History offers a transcript replay");
		replay->trigger();
		auto replayed = std::string{ };
		wait([&] { replayed = selected(); return replayed != seeded && mode()->text().startsWith(QStringLiteral("Transcript replay")) && tab->ask_button()->isEnabled(); }, "the transcript replay opens");
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
		expect(tab->banner_second_button()->text() == QStringLiteral("Continue at ") + earlierHead, "the card offers to continue at the head it read: " + tab->banner_second_button()->text().toStdString());
		run.sweep_drawer("agent-pinned-offered");
		// The session with a checkout follows the head; work in its checkout
		// is in the way of the move, and the question is refused with the
		// ways on.
		open(seeded);
		{
			auto written = 0;
			auto error   = std::error_code{ };
			for (auto item = std::filesystem::recursive_directory_iterator{ access::agent_invocation_evidence(*run.Owner).WorkspaceRoot, error }; !error && item != std::filesystem::recursive_directory_iterator{ }; item.increment(error)) {
				if (item->path().filename() == ".git") {
					item.disable_recursion_pending();
					auto output = std::ofstream{ item->path().parent_path() / "course-edit.txt", std::ios::trunc };
					output << "the reviewer's own notes\n";
					written += output ? 1 : 0;
				}
			}
			expect(written > 0, "the session's checkout takes the reviewer's own file");
		}
		ask(QStringLiteral("What changed in this push?"));
		wait([&] { return quiet() && tab->state() == qt::agent_tab_state::stopped && tab->banner()->isVisible(); }, "the question is refused: work is in the way of the move");
		expect(std::ranges::count_if(tab->banner_reconcile_buttons(), [&](const QPushButton* aButton) { return aButton->isVisibleTo(tab->banner()); }) >= 2, "the card offers the ways to reconcile: " + tab->banner_text().toStdString());
		run.sweep_drawer("agent-pinned-reconcile");
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
		run.sweep_chat("tour-stop-chat-head-moved");

		for (const auto& fault : run.Faults) {
			std::cerr << "FAULT: " << fault << '\n';
		}
		std::cout << "composed-drawer-fit: " << run.States.size() << " states, " << run.Faults.size() << " faults, dpr " << qApp->devicePixelRatio() << ", " << QGuiApplication::platformName().toStdString() << "/"
				  << QApplication::style()->name().toStdString() << '\n';
		window->findChild<QAction*>(QStringLiteral("quitProfilesAction"))->trigger();
		wait([&] { return desktop->is_quiescent(); }, "Quit drains every session");
		if (!run.Faults.empty() && !report) {
			std::cerr << run.Faults.size() << " states of the right drawer pan sideways or cut a control off\n";
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
