#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/overview_tour_summary.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// [[BPR-1587]] [[BPR-1588]]. RFC-BPR-0085's shared drawer parts and the
// Overview's Status drawer: the parts in the four built-in themes, the
// principles audit with a negative control per rule, and the drawer's resize,
// collapse and adopt behaviour.

namespace {
	using namespace bpr;

	auto Failures = 0;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			++Failures;
			std::cerr << "FAIL: " << aMessage << '\n';
		}
	}

	void settle()
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto close_to(const QColor& aOne, const QColor& aOther, const int aSlack = 6) -> bool
	{
		return std::abs(aOne.red() - aOther.red()) <= aSlack && std::abs(aOne.green() - aOther.green()) <= aSlack
			&& std::abs(aOne.blue() - aOther.blue()) <= aSlack;
	}

	void save(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_DRAWER_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(aWidget.grab().save(QStringLiteral("%1/%2.png").arg(root, aName), "PNG"), "a render is written for inspection");
	}

	/// The parts as one drawer would assemble them.
	struct sample
	{
		QWidget                 Host;
		qt::drawer_count_pill*  Pill    = nullptr;
		qt::drawer_header*      Header  = nullptr;
		qt::drawer_card*        Closed  = nullptr;
		qt::drawer_card*        Draft   = nullptr;
		qt::drawer_card*        Reading = nullptr;
		qt::drawer_card*        Writing = nullptr;
		qt::drawer_empty_state* Empty   = nullptr;
		QAction                 NewNote{ QStringLiteral("New note") };
	};

	void build(sample& aSample, const QColor& aWriting)
	{
		aSample.Host.setAutoFillBackground(true);
		auto* const column = new QVBoxLayout{ &aSample.Host };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);
		aSample.Header   = new qt::drawer_header{ &aSample.Host };
		auto* const tabs = new QTabBar{ aSample.Header };
		qt::configure_drawer_tabs(*tabs);
		tabs->addTab(QStringLiteral("Comments"));
		tabs->addTab(QStringLiteral("Notes"));
		tabs->addTab(QStringLiteral("Tour"));
		aSample.Pill = new qt::drawer_count_pill{ tabs };
		aSample.Pill->set_count(2U);
		tabs->setTabButton(0, QTabBar::RightSide, aSample.Pill);
		auto* const tools = new QWidget{ aSample.Header };
		auto* const row   = new QHBoxLayout{ tools };
		row->setContentsMargins(0, 0, 0, 0);
		row->addWidget(qt::make_drawer_tool(tools, qt::drawer_icon(qt::drawer_glyph::tour, aSample.Host.palette()), QStringLiteral("Prepare review tour"), QStringLiteral("sampleTour")));
		row->addWidget(qt::make_drawer_tool(tools, qt::drawer_icon(qt::drawer_glyph::hide_right, aSample.Host.palette()), QStringLiteral("Hide"), QStringLiteral("sampleHide")));
		aSample.Header->set_parts(tabs, tools);
		column->addWidget(aSample.Header);
		const auto card = [&aSample, column, &aWriting](const qt::drawer_card_state aState, const qt::drawer_ownership aOwnership, const QString& aText) {
			auto* const made = new qt::drawer_card{ &aSample.Host };
			made->set_state(aState);
			made->set_ownership(aOwnership);
			made->set_colours({ .Writing = aWriting });
			auto* const inside = new QVBoxLayout{ made };
			inside->addWidget(new QLabel{ aText, made });
			made->setMinimumHeight(64);
			column->addWidget(made);
			return made;
		};
		aSample.Closed  = card(qt::drawer_card_state::closed, qt::drawer_ownership::published, QStringLiteral("A published thread"));
		aSample.Draft   = card(qt::drawer_card_state::closed, qt::drawer_ownership::unpublished, QStringLiteral("My unpublished draft"));
		aSample.Reading = card(qt::drawer_card_state::reading, qt::drawer_ownership::published, QStringLiteral("Open where you read"));
		aSample.Reading->set_notch(20);
		aSample.Writing = card(qt::drawer_card_state::writing, qt::drawer_ownership::private_note, QStringLiteral("A private note being written"));
		aSample.Empty   = new qt::drawer_empty_state{ qt::drawer_icon(qt::drawer_glyph::page, aSample.Host.palette()), QStringLiteral("No notes on this file yet."), &aSample.NewNote, &aSample.Host };
		column->addWidget(aSample.Empty, 1);
	}

	void test_the_parts_in_every_theme()
	{
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto name    = QString::fromUtf8(core::stable_name(profile.variant()).data());
			const auto writing = profile.color(core::appearance_role::state_unread);
			sample     parts;
			profile.apply_to(parts.Host);
			build(parts, writing);
			parts.Host.resize(375, 560);
			parts.Host.show();
			settle();
			const auto image = parts.Host.grab().toImage();
			const auto base  = parts.Host.palette().color(QPalette::Active, QPalette::Base);
			const auto dpr   = image.devicePixelRatio();
			const auto at    = [&image, dpr](const QWidget& aWidget, const QPoint aPoint) {
				const auto spot = aWidget.mapTo(aWidget.window(), aPoint);
				return image.pixelColor(static_cast<int>(spot.x() * dpr), static_cast<int>(spot.y() * dpr));
			};
			expect(close_to(at(*parts.Reading, QPoint{ parts.Reading->width() / 2, 4 }), base), name.toStdString() + ": an open card is the palette's Base");
			expect(close_to(at(*parts.Reading, QPoint{ 1, parts.Reading->height() / 2 }), QColor{ 0xD2, 0x99, 0x22 }, 40),
				   name.toStdString() + ": the reading card's edge is the amber");
			expect(!close_to(at(*parts.Closed, QPoint{ parts.Closed->width() / 2, 4 }), base, 1) || base == parts.Host.palette().color(QPalette::Window),
				   name.toStdString() + ": a closed card paints no panel until hovered");
			expect(parts.Pill->isVisible() && parts.Pill->text() == QStringLiteral("2"), name.toStdString() + ": the count pill shows its count");
			expect(parts.Empty->sentence() == QStringLiteral("No notes on this file yet.") && parts.Empty->action_button() != nullptr
					   && parts.Empty->action_button()->text() == QStringLiteral("New note"),
				   name.toStdString() + ": the empty state is one sentence and one action");
			const auto violations = qt::drawer_principle_violations(parts.Host);
			expect(violations.isEmpty(), name.toStdString() + ": the parts keep every principle: " + violations.join(QStringLiteral("; ")).toStdString());
			save(parts.Host, QStringLiteral("drawer-parts-%1").arg(name));
			parts.Host.hide();
		}
	}

	void test_the_header_wraps_its_tools_rather_than_clipping_a_tab()
	{
		sample parts;
		build(parts, QColor{ 0x2F, 0x6F, 0xD6 });
		parts.Host.resize(375, 400);
		parts.Host.show();
		settle();
		expect(!parts.Header->wrapped(), "at 375 pixels the tools share the tab row");
		parts.Host.resize(150, 400);
		settle();
		expect(parts.Header->wrapped(), "narrow, the tools move to their own row");
		parts.Pill->set_count(0U);
		expect(!parts.Pill->isVisible(), "a zero count hides the pill");
	}

	void test_the_audit_fails_each_broken_principle()
	{
		{
			auto  host  = QWidget{ };
			auto* row   = new QVBoxLayout{ &host };
			auto* tabs  = new QTabBar{ &host };
			auto* pages = new QStackedWidget{ &host };
			row->addWidget(tabs);
			row->addWidget(pages);
			auto* page = new QWidget{ pages };
			auto* nest = new QTabWidget{ page };
			nest->setObjectName(QStringLiteral("nested"));
			pages->addWidget(page);
			const auto found = qt::drawer_principle_violations(host);
			expect(found.join(QString{ }).contains(QStringLiteral("nested is a tab row inside another tab's page")), "negative control: tabs inside a tab are refused");
			expect(qt::drawer_principle_violations(host, { .NestedTabs = false }).isEmpty(), "the nested-tab rule can be left out");
		}
		{
			auto host = QWidget{ };
			host.setObjectName(QStringLiteral("stack"));
			auto* column = new QVBoxLayout{ &host };
			for (const auto* const text : { "Send", "Add context", "Start a new session" }) {
				column->addWidget(new QPushButton{ QString::fromUtf8(text), &host });
			}
			expect(qt::drawer_principle_violations(host).contains(QStringLiteral("stack stacks three or more buttons in a column")),
				   "negative control: a column of stacked buttons is refused");
		}
		{
			auto  host   = QWidget{ };
			auto* column = new QVBoxLayout{ &host };
			auto* label  = new QLabel{ QStringLiteral("x"), &host };
			label->setObjectName(QStringLiteral("tinted"));
			label->setStyleSheet(QStringLiteral("color: #ff0000"));
			column->addWidget(label);
			expect(qt::drawer_principle_violations(host).contains(QStringLiteral("tinted has a style sheet")),
				   "negative control: a style sheet, a colour outside the palette roles, is refused");
		}
		{
			auto  host   = QWidget{ };
			auto* column = new QVBoxLayout{ &host };
			for (const auto* const text : { "not available", "—", "0" }) {
				auto* label = new QLabel{ QString::fromUtf8(text), &host };
				label->setObjectName(QStringLiteral("value"));
				column->addWidget(label);
			}
			host.show();
			settle();
			expect(qt::drawer_principle_violations(host).size() == 3, "negative control: rows saying not available, — or 0 are refused");
			expect(qt::drawer_principle_violations(host, { .EmptyValueRows = false }).isEmpty(), "the empty-row rule can be left out");
		}
	}

	struct status_fixture
	{
		QWidget                                           Window;
		qt::overview_status_splitter*                     Split  = nullptr;
		qt::overview_status_drawer*                       Drawer = nullptr;
		std::vector<application::workbench_status_drawer> Reported;
		QAction                                           Github{ QStringLiteral("Open on GitHub") };
	};

	void make(status_fixture& aFixture)
	{
		auto* const row = new QHBoxLayout{ &aFixture.Window };
		row->setContentsMargins(0, 0, 0, 0);
		aFixture.Split      = new qt::overview_status_splitter{ &aFixture.Window };
		auto* const main    = new QLabel{ QStringLiteral("Overview"), aFixture.Split };
		auto* const content = new QScrollArea{ };
		content->setWidget(new QLabel{ QStringLiteral("Merge, participants and checks") });
		aFixture.Drawer = new qt::overview_status_drawer{ content, aFixture.Split };
		aFixture.Split->addWidget(main);
		aFixture.Split->addWidget(aFixture.Drawer);
		aFixture.Split->setStretchFactor(0, 1);
		aFixture.Split->setStretchFactor(1, 0);
		QObject::connect(aFixture.Split, &QSplitter::splitterMoved, aFixture.Drawer, [&aFixture](int, int) { aFixture.Drawer->observe_drag(aFixture.Drawer->width()); });
		aFixture.Drawer->set_change_dispatch([&aFixture](const application::workbench_status_drawer& aDrawer) { aFixture.Reported.push_back(aDrawer); });
		aFixture.Drawer->set_tools(&aFixture.Github);
		row->addWidget(aFixture.Split);
		aFixture.Window.resize(1200, 700);
		aFixture.Window.show();
		settle();
	}

	void drag(QSplitterHandle& aHandle, const int aBy)
	{
		const auto start = QPointF{ aHandle.width() / 2.0, aHandle.height() / 2.0 };
		const auto end   = start + QPointF{ static_cast<qreal>(aBy), 0.0 };
		auto       press = QMouseEvent{ QEvent::MouseButtonPress, start, aHandle.mapToGlobal(start), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aHandle, &press);
		auto move = QMouseEvent{ QEvent::MouseMove, end, aHandle.mapToGlobal(end), Qt::NoButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aHandle, &move);
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, end, aHandle.mapToGlobal(end), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aHandle, &release);
		settle();
	}

	void test_status_is_a_real_drawer()
	{
		status_fixture fixture;
		make(fixture);
		auto& drawer = *fixture.Drawer;
		expect(drawer.width() == static_cast<int>(application::default_status_drawer_width) && fixture.Reported.empty(),
			   "Status opens at 340 pixels and reports nothing it was not asked");
		// RFC-BPR-0089: Commits sits beside Status. [[BPR-1781]]: then
		// Comments, Notes, Tour and Agent; Prepare review tour is the Tour
		// tab's own action, not an icon on the row.
		expect(drawer.tabs()->count() == 6 && drawer.tabs()->accessibleTabName(0) == QStringLiteral("Status") && drawer.tabs()->accessibleTabName(1) == QStringLiteral("Commits")
				   && drawer.findChild<QToolButton*>(QStringLiteral("overviewPrepareTour")) == nullptr && drawer.github_button()->isVisible() && drawer.hide_button()->isVisible()
				   && !drawer.rail()->isVisible(),
			   "its header holds Status and Commits, with Open on GitHub and Hide at the right");
		expect(drawer.content()->isVisible() && !drawer.commits()->isVisible(), "Status shows first");
		drawer.tabs()->setCurrentIndex(1);
		settle();
		expect(drawer.commits()->isVisible() && !drawer.content()->isVisible(), "the Commits tab shows the commits in Status's place");
		drawer.tabs()->setCurrentIndex(0);
		settle();
		expect(qt::drawer_principle_violations(drawer, { .EmptyValueRows = false }).isEmpty(), "the Status drawer's chrome keeps the principles");
		auto* const handle = fixture.Split->handle(1);
		expect(handle != nullptr && handle->cursor().shape() == Qt::SplitHCursor, "its seam holds the resize cursor");
		drag(*handle, -80);
		expect(!fixture.Reported.empty() && fixture.Reported.back().Width == std::optional<std::uint32_t>{ 420U }
				   && fixture.Reported.back().State == core::workbench_drawer_state::expanded,
			   "dragging the seam 80 pixels left reports a 420-pixel drawer");
		drag(*handle, -600);
		expect(drawer.width() == static_cast<int>(application::maximum_status_drawer_width)
				   && fixture.Reported.back().Width == std::optional<std::uint32_t>{ application::maximum_status_drawer_width },
			   "a drag past 560 stops at 560");
		drag(*handle, 800);
		expect(drawer.width() == static_cast<int>(application::minimum_status_drawer_width)
				   && fixture.Reported.back().Width == std::optional<std::uint32_t>{ application::minimum_status_drawer_width },
			   "a drag past 260 stops at 260");
		const auto reported = fixture.Reported.size();
		drawer.hide_button()->click();
		settle();
		expect(drawer.rail()->isVisible() && drawer.width() == qt::workbench_drawer_rail::width_pixels && fixture.Reported.size() == reported + 1U
				   && fixture.Reported.back().State == core::workbench_drawer_state::collapsed && !handle->isEnabled(),
			   "Hide leaves the rail, reports collapsed and disables the seam");
		drawer.rail()->click();
		settle();
		expect(!drawer.rail()->isVisible() && drawer.width() == static_cast<int>(application::minimum_status_drawer_width)
				   && fixture.Reported.back().State == core::workbench_drawer_state::expanded && handle->isEnabled(),
			   "the rail brings it back at the width it had");
		const auto before = fixture.Reported.size();
		drawer.adopt({ .Width = 500U, .State = core::workbench_drawer_state::collapsed });
		settle();
		expect(drawer.rail()->isVisible() && fixture.Reported.size() == before, "adopting the host's value applies it and reports nothing");
		drawer.adopt({ .Width = 500U, .State = core::workbench_drawer_state::expanded });
		settle();
		expect(drawer.width() == 500 && fixture.Reported.size() == before, "an adopted width is presented");
		drawer.adopt({ .Width = 9000U, .State = core::workbench_drawer_state::expanded });
		settle();
		expect(drawer.width() == 500, "an out-of-bounds value is not adopted");
		expect(drawer.rail()->focusPolicy() == Qt::StrongFocus && drawer.tabs()->focusPolicy() == Qt::StrongFocus
				   && drawer.hide_button()->focusPolicy() == Qt::StrongFocus,
			   "its tab, tools and rail are keyboard stops");
		fixture.Github.setEnabled(false);
		expect(!drawer.github_button()->isEnabled(), "the GitHub tool follows its action");
		save(fixture.Window, QStringLiteral("status-drawer"));
	}

	/// [[BPR-1781]]. Overview's drawer holds the conversation tabs: six,
	/// the Tour tab only while there is a tour to show, all on one row.
	void test_overview_drawer_has_the_conversation_tabs()
	{
		status_fixture fixture;
		make(fixture);
		auto&       drawer = *fixture.Drawer;
		auto* const tabs   = drawer.tabs();
		const auto  names  = QStringList{ QStringLiteral("Status"), QStringLiteral("Commits"), QStringLiteral("Comments"), QStringLiteral("Notes"), QStringLiteral("Tour"), QStringLiteral("Agent") };
		for (auto index = 0; index < 6; ++index) {
			expect(tabs->accessibleTabName(index) == names[index], "tab " + std::to_string(index) + " is " + names[index].toStdString());
		}
		expect(!drawer.tour_tab_shown() && !tabs->isTabVisible(4), "with no tour to show there is no Tour tab");
		auto observed = std::vector<qt::overview_drawer_tab>{ };
		drawer.set_tab_observer([&observed](const qt::overview_drawer_tab aTab) { observed.push_back(aTab); });
		for (const auto tab : { qt::overview_drawer_tab::comments, qt::overview_drawer_tab::notes, qt::overview_drawer_tab::agent }) {
			auto* const holder = drawer.holder(tab);
			expect(holder != nullptr && holder->layout() != nullptr, "Comments, Notes and Agent are holders a borrowed panel goes in");
			auto* const borrowed = new QLabel{ QStringLiteral("borrowed"), holder };
			holder->layout()->addWidget(borrowed);
			drawer.show_tab(tab);
			settle();
			expect(drawer.current_tab() == tab && borrowed->isVisible() && observed.back() == tab, "show_tab opens the holder and says so");
		}
		expect(drawer.holder(qt::overview_drawer_tab::status) == nullptr && drawer.holder(qt::overview_drawer_tab::tour) == nullptr, "Status and Tour borrow nothing");
		// The row fits its width. Native font metrics decide whether all names
		// fit or the other tabs give way to glyphs, with the open tab named.
		auto width = 0;
		for (auto index = 0; index < tabs->count(); ++index) {
			if (tabs->isTabVisible(index)) {
				width += tabs->tabRect(index).width();
			}
		}
		expect(width <= drawer.header()->width(), "five tabs fit the header at the default width");
		expect(tabs->tabText(tabs->currentIndex()) == QStringLiteral("Agent"), "the open tab is named at the default width");
		const auto compact = tabs->tabText(0).isEmpty();
		for (auto index = 0; index < names.size(); ++index) {
			if (!tabs->isTabVisible(index)) {
				continue;
			}
			const auto named    = !compact || index == tabs->currentIndex();
			const auto hasGlyph = !tabs->tabIcon(index).isNull();
			expect(tabs->tabText(index) == (named ? names[index] : QString{ }) && hasGlyph == compact,
				   "the fitted row shows every name or the open name and every glyph");
			if (!named) {
				expect(tabs->tabToolTip(index).startsWith(names[index] + QLatin1Char{ '\n' }), "a tab showing only its glyph says its name first");
			}
		}
		// A hidden Tour tab cannot be opened; shown, it can, and hiding it
		// while open goes back to Status.
		drawer.show_tab(qt::overview_drawer_tab::tour);
		expect(drawer.current_tab() == qt::overview_drawer_tab::agent, "a hidden Tour tab is not opened");
		drawer.set_tour_tab_shown(true);
		drawer.show_tab(qt::overview_drawer_tab::tour);
		settle();
		expect(drawer.current_tab() == qt::overview_drawer_tab::tour && drawer.tour_summary()->isVisible(), "the Tour tab shows its summary");
		drawer.set_tour_tab_shown(false);
		settle();
		expect(drawer.current_tab() == qt::overview_drawer_tab::status, "hiding the open Tour tab gives way to Status");
		// Collapsed, show_tab expands it as the rail does.
		drawer.hide_button()->click();
		settle();
		drawer.show_tab(qt::overview_drawer_tab::comments);
		settle();
		expect(!drawer.rail()->isVisible() && drawer.current_tab() == qt::overview_drawer_tab::comments, "a collapsed drawer opens on the tab asked for");
		expect(qt::drawer_principle_violations(drawer, { .EmptyValueRows = false }).isEmpty(), "six tabs keep the principles");
		drawer.resize(560, drawer.height());
		fixture.Split->setSizes({ 640, 560 });
		settle();
		save(fixture.Window, QStringLiteral("overview-drawer-six-tabs"));
	}

	[[nodiscard]] auto stop(const std::uint64_t aId, const char* aTitle, const core::tour_preparation aPreparation) -> core::overview_tour_stop
	{
		return { .Id = core::tour_stop_id{ aId }, .Title = aTitle, .Preparation = aPreparation };
	}

	/// [[BPR-1781]]. The Tour tab presents and routes: a writer's brief and
	/// stops with readiness, a reader's brief and titles, Prepare for an
	/// author with none; its one button reports a gesture and does nothing else.
	void test_overview_tour_summary()
	{
		auto        window  = QWidget{ };
		auto* const column  = new QVBoxLayout{ &window };
		auto* const summary = new qt::overview_tour_summary{ &window };
		column->addWidget(summary);
		window.resize(360, 640);
		window.show();
		settle();
		auto gestures = std::vector<qt::overview_tour_gesture>{ };
		summary->set_dispatch([&gestures](const qt::overview_tour_gesture aGesture) { gestures.push_back(aGesture); });
		summary->present({ .Kind = core::overview_tour_kind::prepare });
		settle();
		expect(summary->action()->isVisible() && summary->action()->text() == QStringLiteral("Prepare review tour") && summary->stop_titles().isEmpty()
				   && !summary->state_label()->isVisible(),
			   "an author with no tour is offered Prepare review tour, labelled");
		summary->action()->click();
		expect(gestures == std::vector{ qt::overview_tour_gesture::prepare }, "Prepare reports its gesture");
		save(window, QStringLiteral("overview-tour-prepare"));

		auto writer  = core::overview_tour{ .Kind = core::overview_tour_kind::writer, .Problem = "Startup reads every file twice.", .Solution = "" };
		writer.Stops = { stop(1, "The cache", core::tour_preparation::ready), stop(2, "", core::tour_preparation::editing), stop(3, "Invalidation", core::tour_preparation::needs_attention),
						 stop(4, "Tests", core::tour_preparation::suggested) };
		summary->present(writer);
		settle();
		expect(summary->stop_titles() == QStringList{ QStringLiteral("The cache"), QStringLiteral("Untitled stop"), QStringLiteral("Invalidation"), QStringLiteral("Tests") },
			   "a writer sees every stop, in route order");
		expect(summary->stop_readiness() == QStringList{ QStringLiteral("Ready"), QStringLiteral("Editing"), QStringLiteral("Needs attention"), QStringLiteral("Suggested") },
			   "each with its readiness as the route shows it");
		expect(summary->state_label()->text().startsWith(QStringLiteral("A private draft")), "a private draft says so");
		auto* const solution = summary->findChild<QLabel*>(QStringLiteral("overviewTourSolution"));
		expect(solution != nullptr && solution->isVisible() && solution->text() == QStringLiteral("Not written yet"), "a writer sees an empty field as still to write");
		expect(summary->action()->text() == QStringLiteral("Continue in Review"), "and continues in Review");
		auto* const firstRow = summary->findChild<QWidget*>(QStringLiteral("overviewTourStop"));
		summary->present(writer);
		expect(summary->findChild<QWidget*>(QStringLiteral("overviewTourStop")) == firstRow, "the same route presented again keeps its rows");
		summary->action()->click();
		expect(gestures.back() == qt::overview_tour_gesture::continue_tour, "Continue in Review reports its gesture");
		save(window, QStringLiteral("overview-tour-writer"));
		writer.Published = true;
		summary->present(writer);
		expect(summary->state_label()->text().startsWith(QStringLiteral("Published")), "a published tour says so");

		auto reader      = core::overview_tour{ .Kind = core::overview_tour_kind::reader, .Problem = "Startup reads every file twice.", .Solution = "Read once and cache." };
		reader.Stops     = { stop(1, "The cache", core::tour_preparation::ready), stop(2, "Invalidation", core::tour_preparation::needs_attention) };
		reader.Published = true;
		summary->present(reader);
		settle();
		expect(summary->stop_titles().size() == 2 && summary->stop_readiness().isEmpty(), "a reader sees the stop titles without readiness");
		expect(summary->action()->text() == QStringLiteral("Start tour") && summary->action()->isEnabled(), "and starts the tour");
		summary->action()->click();
		expect(gestures.back() == qt::overview_tour_gesture::start_tour, "Start tour reports its gesture");
		save(window, QStringLiteral("overview-tour-reader"));
		reader.Solution = "";
		summary->present(reader);
		expect(!solution->isVisible(), "a reader sees only what was written");
		expect(qt::drawer_principle_violations(*summary).isEmpty(), "the Tour tab keeps the principles");

		summary->present({ });
		expect(!summary->action()->isVisible() && summary->stop_titles().isEmpty(), "no tour presents nothing");
		// The palette's change recolours the readiness words.
		summary->present(writer);
		auto dark = window.palette();
		dark.setColor(QPalette::Window, QColor{ 0x20, 0x20, 0x20 });
		window.setPalette(dark);
		settle();
		auto* const ready = summary->findChild<QLabel*>(QStringLiteral("overviewTourStopReadiness"));
		expect(ready != nullptr && ready->palette().color(QPalette::WindowText) == QColor{ 0x3F, 0xB9, 0x50 }, "a dark palette gives the dark ready green");
	}
} // namespace

namespace {
	/// \brief WCAG relative luminance of a colour.
	[[nodiscard]] auto luminance(const QColor& aColour) -> double
	{
		const auto channel = [](const double aValue) { return aValue <= 0.03928 ? aValue / 12.92 : std::pow((aValue + 0.055) / 1.055, 2.4); };
		return (0.2126 * channel(aColour.redF())) + (0.7152 * channel(aColour.greenF())) + (0.0722 * channel(aColour.blueF()));
	}

	[[nodiscard]] auto contrast(const QColor& aOne, const QColor& aOther) -> double
	{
		const auto one   = luminance(aOne);
		const auto other = luminance(aOther);
		return (std::max(one, other) + 0.05) / (std::min(one, other) + 0.05);
	}

	/// [[BPR-1589]]. A section header reads in every theme, opens by key and
	/// mouse, and its chevron says which; a meter draws its proportions.
	void test_section_header_and_meter_in_every_theme()
	{
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto name = std::string{ core::stable_name(profile.variant()) };
			QWidget    host;
			profile.apply_to(host);
			host.setAutoFillBackground(true);
			auto* const column = new QVBoxLayout{ &host };
			auto* const header = new qt::drawer_section_header{ &host };
			header->setText(QStringLiteral("Needs attention"));
			header->set_count(QStringLiteral("4"));
			auto* const meter = new qt::drawer_meter{ &host };
			const auto  green = profile.color(core::appearance_role::state_accepted);
			const auto  red   = profile.color(core::appearance_role::state_defect);
			meter->set_segments({ { 3.0, green }, { 1.0, red } }, 8.0);
			column->addWidget(header);
			column->addWidget(meter);
			column->addStretch(1);
			host.resize(320, 120);
			host.show();
			settle();
			expect(header->sizeHint().width() <= header->width() && header->minimumSizeHint().width() < header->sizeHint().width(), name + ": the header fits its title and count, and shortens in a narrow drawer");
			expect(header->arrowType() == Qt::RightArrow && !header->isChecked(), name + ": a section opens closed");
			header->setFocus(Qt::OtherFocusReason);
			auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
			auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
			QApplication::sendEvent(header, &press);
			QApplication::sendEvent(header, &release);
			settle();
			expect(header->isChecked() && header->arrowType() == Qt::DownArrow, name + ": Space opens it and the chevron points down");
			const auto image   = host.grab().toImage();
			const auto dpr     = image.devicePixelRatio();
			const auto ground  = host.palette().color(QPalette::Active, QPalette::Window);
			auto       darkest = 1.0;
			const auto top     = header->mapTo(&host, QPoint{ 0, 0 });
			for (int y = top.y() + 2; y < top.y() + header->height() - 2; ++y) {
				for (int x = top.x() + 4; x < top.x() + 80; ++x) {
					const auto pixel = image.pixelColor(static_cast<int>(x * dpr), static_cast<int>(y * dpr));
					darkest          = std::max(darkest, contrast(pixel, ground));
				}
			}
			expect(darkest >= 4.5, name + ": the title reads at 4.5:1 or more (" + std::to_string(darkest) + ")");
			const auto at = [&](const double aFraction) {
				const auto spot = meter->mapTo(&host, QPoint{ static_cast<int>(meter->width() * aFraction), meter->height() / 2 });
				return image.pixelColor(static_cast<int>(spot.x() * dpr), static_cast<int>(spot.y() * dpr));
			};
			expect(close_to(at(0.2), green, 12) && close_to(at(0.45), red, 12) && !close_to(at(0.8), green, 12) && !close_to(at(0.8), red, 12),
				   name + ": the meter draws three eighths green, one eighth red and the rest as track");
			const auto violations = qt::drawer_principle_violations(host);
			expect(violations.isEmpty(), name + ": the section parts keep every principle: " + violations.join(QStringLiteral("; ")).toStdString());
			save(host, QStringLiteral("drawer-section-%1").arg(QString::fromStdString(name)));
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_parts_in_every_theme();
	test_the_header_wraps_its_tools_rather_than_clipping_a_tab();
	test_the_audit_fails_each_broken_principle();
	test_status_is_a_real_drawer();
	test_overview_drawer_has_the_conversation_tabs();
	test_overview_tour_summary();
	test_section_header_and_meter_in_every_theme();
	if (Failures != 0) {
		std::cerr << Failures << " failure(s)\n";
		return EXIT_FAILURE;
	}
	std::cout << "drawer-parts-tests:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString()
			  << " style=" << QApplication::style()->name().toStdString() << " dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
