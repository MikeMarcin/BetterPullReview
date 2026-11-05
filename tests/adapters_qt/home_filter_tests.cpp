// [[BPR-1865]]. Home's filter as Qt presents it: typing filters every list,
// the matched characters are marked, the panels with nothing collapse away,
// the keyboard moves through the results and opens one, Escape clears and
// then leaves, and typing in a list goes to the field. The core ranks; these
// courses check that the list shows what it ranked.
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/review_landing_model.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPixmap>
#include <QStyleOptionViewItem>
#include <QThread>
#include <QVBoxLayout>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using bucket = core::attention_bucket;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle(const int aMilliseconds = 200)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		do {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
			QThread::msleep(5);
		} while (timer.elapsed() < aMilliseconds);
	}

	[[nodiscard]] auto item(const std::int64_t aNumber, const bucket aBucket, std::string aTitle, std::string aRepository = "octo-org/app", std::string aAuthor = "contributor-a")
		-> core::queue_item
	{
		auto result = core::queue_item{
			.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = std::move(aRepository), .Number = aNumber },
			.Membership = { .Role = core::github_role::reviewer },
			.Bucket     = aBucket,
		};
		result.Facts = core::queue_item_facts{ .Title = std::move(aTitle), .Author = std::move(aAuthor) };
		return result;
	}

	[[nodiscard]] auto queue() -> std::vector<core::queue_item>
	{
		return core::ordered_queue(std::vector{
			item(4021, bucket::initial_review_requested, "Fix login redirect loop", "octo-org/app", "alice"),
			item(4022, bucket::changed_since_my_review, "Speed up the discovery cache", "octo-org/app", "bob"),
			item(123, bucket::observed_with_new_activity, "Login page polish", "acme-tools/build", "carol"),
			item(1230, bucket::quiet, "Résumé parsing for Zoë's café", "acme-tools/build", "dave"),
			item(77, bucket::quiet, "Teach the queue to wait", "octo-org/notes", "erin"),
		});
	}

	[[nodiscard]] auto recent_item(const QString& aIdentity, const QString& aTitle) -> QListWidgetItem*
	{
		auto* const row = new QListWidgetItem{ aIdentity };
		row->setData(qt::home_row_role::identity, aIdentity);
		row->setData(qt::home_row_role::title, aTitle);
		row->setData(qt::home_row_role::why, QStringLiteral("Viewed."));
		row->setData(qt::home_row_role::state, -1);
		row->setData(qt::home_row_role::icon_action, QStringLiteral("Open on GitHub"));
		return row;
	}

	void send_key(QWidget& aWidget, const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier, const QString& aText = { })
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers, aText };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers, aText };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}

	[[nodiscard]] auto marks(const QModelIndex& aIndex, const int aRole) -> std::vector<QPoint>
	{
		auto result = std::vector<QPoint>{ };
		for (const auto& value : aIndex.data(aRole).toList()) {
			result.push_back(value.toPoint());
		}
		return result;
	}

	/// \brief The field over the list, wired as the window wires them.
	struct filter_fixture
	{
		filter_fixture()
		{
			Page         = new QWidget{ };
			auto* column = new QVBoxLayout{ Page };
			column->setContentsMargins(0, 12, 0, 0);
			column->setSpacing(0);
			Field = new qt::home_filter_field{ Page };
			List  = new qt::home_list{ Filter, Page };
			column->addWidget(Field);
			column->addWidget(List, 1);
			List->set_state_colours(qt::home_state_colours_for(qt::appearance_profile{ }));
			Field->set_shortcut(QKeySequence{ QKeySequence::Find });
			Field->set_query_observer([this](const QString& aQuery) {
				List->set_filter(aQuery);
				Field->set_count(List->filter_matches());
			});
			Field->set_step_dispatch([this](const int aStep) { return List->step_filter(aStep); });
			Field->set_open_dispatch([this](const bool aNewWindow) { List->open_filter_result(aNewWindow); });
			Field->set_leave_dispatch([this] { List->focus_filter_result(); });
			List->set_filter_typing_dispatch([this](const QString& aText) { Field->take_typing(aText); });
			List->set_open_dispatch([this](const core::queue_item& aItem) { Opened.push_back(aItem.Ref.Number); });
			List->set_new_window_dispatch([this](const core::queue_item& aItem) { NewWindow.push_back(aItem.Ref.Number); });
			List->set_recent_open_dispatch([this](const int aRow) { RecentOpened.push_back(aRow); });
			List->set_view_observer([this] { ++ViewChanges; });
			Source.apply(queue());
			List->set_queue_status(true, true);
			Page->resize(760, 980);
			Page->show();
			Page->activateWindow();
			settle();
		}

		filter_fixture(const filter_fixture&)                    = delete;
		auto operator=(const filter_fixture&) -> filter_fixture& = delete;
		filter_fixture(filter_fixture&&)                         = delete;
		auto operator=(filter_fixture&&) -> filter_fixture&      = delete;

		~filter_fixture()
		{
			delete Page;
		}

		void type(const QString& aQuery)
		{
			Field->line_edit()->setText(aQuery);
			settle(60);
		}

		/// The identities of every result, in reading order.
		[[nodiscard]] auto results() const -> QStringList
		{
			auto identities = QStringList{ };
			for (const auto group : { qt::home_group::needs_you, qt::home_group::my_pull_requests, qt::home_group::waiting_on_others, qt::home_group::following }) {
				auto* const view = List->state_view(group);
				for (auto row = 0; view != nullptr && List->state_panel(group)->isVisible() && row < view->model()->rowCount(); ++row) {
					identities.push_back(view->model()->index(row, 0).data(qt::home_row_role::identity).toString());
				}
			}
			return identities;
		}

		[[nodiscard]] auto result_identity() const -> QString
		{
			return List->filter_result().data(qt::home_row_role::identity).toString();
		}

		qt::review_landing_model        Source;
		qt::review_landing_filter_model Filter{ Source };
		QWidget*                        Page  = nullptr;
		qt::home_filter_field*          Field = nullptr;
		qt::home_list*                  List  = nullptr;
		std::vector<std::int64_t>       Opened;
		std::vector<std::int64_t>       NewWindow;
		std::vector<int>                RecentOpened;
		int                             ViewChanges = 0;
	};

	void save(QWidget& aWidget, const QString& aPath, const QString& aSuffix)
	{
		if (aPath.isEmpty()) {
			return;
		}
		auto path = aPath;
		path.replace(QStringLiteral(".png"), QStringLiteral("-%1.png").arg(aSuffix));
		expect(aWidget.grab().save(path), "the render is written");
	}

	void test_typing_filters_and_panels_without_matches_go(const QString& aRender)
	{
		filter_fixture home;
		save(*home.Page, aRender, QStringLiteral("rest"));
		const auto all = home.results();
		expect(all.size() == 5, "five rows before filtering");
		expect(home.Field->shortcut_label()->isVisible() && !home.Field->count_label()->isVisible(), "at rest the field shows its key, not a count");

		home.type(QStringLiteral("login"));
		expect(home.results() == QStringList{ QStringLiteral("acme-tools/build#123"), QStringLiteral("octo-org/app#4021") }
				   || home.results() == QStringList{ QStringLiteral("octo-org/app#4021"), QStringLiteral("acme-tools/build#123") },
			   "only the two rows with login in them are shown: " + home.results().join(QLatin1Char{ ',' }).toStdString());
		expect(home.List->filter_matches() == 2 && home.Field->count_label()->isVisible() && home.Field->count_label()->text() == QStringLiteral("2 matches")
				   && !home.Field->shortcut_label()->isVisible(),
			   "the field says how many rows it shows, where its key stood");
		for (const auto group : { qt::home_group::needs_you, qt::home_group::my_pull_requests, qt::home_group::waiting_on_others, qt::home_group::following }) {
			auto* const panel = home.List->state_panel(group);
			expect(panel->isVisible() == (home.List->state_model(group)->rowCount() > 0), "a panel is shown exactly while it holds a match");
			if (panel->isVisible()) {
				expect(panel->count() == home.List->state_model(group)->rowCount(), "a panel counts its matches");
			}
		}
		save(*home.Page, aRender, QStringLiteral("login"));

		home.type(QString{ });
		expect(home.results().size() == 5 && home.List->filter_matches() == 0 && !home.Field->count_label()->isVisible(), "clearing it shows every row again");
	}

	void test_exact_number_is_the_first_result()
	{
		filter_fixture home;
		home.type(QStringLiteral("123"));
		expect(home.result_identity() == QStringLiteral("acme-tools/build#123"), "the exact number is the current result: " + home.result_identity().toStdString());
		home.type(QStringLiteral("#12"));
		expect(home.result_identity() == QStringLiteral("acme-tools/build#123"), "#12: the nearer number first");
		auto* const view = home.List->filter_result_view();
		expect(view != nullptr && view->model()->rowCount() >= 1, "the result is in a shown list");
		home.type(QStringLiteral("acme"));
		auto* const following = home.List->state_view(qt::home_group::following);
		expect(following->model()->rowCount() == 2, "both acme-tools rows are found");
		home.type(QStringLiteral("acme 1230"));
		expect(home.results() == QStringList{ QStringLiteral("acme-tools/build#1230") }, "a repository and a number together");
	}

	void test_matched_characters_are_marked(const QString& aRender)
	{
		filter_fixture home;
		home.type(QStringLiteral("login"));
		auto* const view  = home.List->filter_result_view();
		const auto  index = home.List->filter_result();
		expect(view != nullptr && index.isValid(), "a result");
		const auto identity = index.data(qt::home_row_role::identity).toString();
		const auto title    = marks(index, qt::home_row_role::title_marks);
		if (identity == QStringLiteral("octo-org/app#4021")) {
			expect(title == std::vector{ QPoint{ 4, 5 } }, "\"login\" is marked where it stands in \"Fix login redirect loop\"");
		} else {
			expect(title == std::vector{ QPoint{ 0, 5 } }, "\"Login\" is marked at the start of \"Login page polish\"");
		}
		auto option = QStyleOptionViewItem{ };
		option.initFrom(view);
		option.rect      = view->visualRect(index);
		option.font      = view->font();
		option.widget    = view;
		const auto rects = home.List->row_delegate()->mark_rects(option, index);
		expect(rects.size() == 1U && option.rect.contains(rects.front()), "one mark, drawn inside its row");

		home.type(QStringLiteral("cafe"));
		const auto cafe = home.List->filter_result();
		expect(cafe.data(qt::home_row_role::identity).toString() == QStringLiteral("acme-tools/build#1230"), "café is found by cafe");
		const auto start = static_cast<int>(QStringLiteral("Résumé parsing for Zoë's ").size());
		expect(marks(cafe, qt::home_row_role::title_marks) == std::vector{ QPoint{ start, 4 } }, "the mark is in characters, past é and ë");

		home.type(QStringLiteral("carol"));
		const auto author = home.List->filter_result();
		expect(marks(author, qt::home_row_role::author_marks) == std::vector{ QPoint{ 0, 5 } } && marks(author, qt::home_row_role::title_marks).empty(), "the author's login is marked");
		home.type(QStringLiteral("build#123"));
		const auto both = home.List->filter_result();
		const auto repo = static_cast<int>(QStringLiteral("acme-tools/").size());
		expect(marks(both, qt::home_row_role::identity_marks) == std::vector{ QPoint{ repo, 5 }, QPoint{ repo + 6, 3 } }, "the repository's name and the number are marked in owner/name#123");
		save(*home.Page, aRender, QStringLiteral("marks"));

		home.type(QString{ });
		expect(!index.isValid() || !home.List->state_view(qt::home_group::needs_you)->model()->index(0, 0).data(qt::home_row_role::title_marks).isValid(), "no marks while nothing is filtered");
	}

	void test_the_keyboard_moves_through_results_and_opens_one()
	{
		filter_fixture home;
		home.Field->focus_field();
		settle(30);
		home.type(QStringLiteral("o"));
		const auto results = home.List->filter_matches();
		expect(results >= 3, "several results for one letter");
		const auto first = home.result_identity();
		send_key(*home.Field->line_edit(), Qt::Key_Down);
		const auto second = home.result_identity();
		expect(second != first && home.Field->line_edit()->hasFocus(), "Down moves the current result and the keyboard stays in the field");
		send_key(*home.Field->line_edit(), Qt::Key_Up);
		expect(home.result_identity() == first, "Up moves it back");
		send_key(*home.Field->line_edit(), Qt::Key_Up);
		expect(home.result_identity() == first, "and stops at the first result");
		send_key(*home.Field->line_edit(), Qt::Key_Return);
		expect(home.Opened.size() == 1U && QStringLiteral("#%1").arg(home.Opened.front()) == first.mid(first.indexOf(QLatin1Char{ '#' })), "Return opens the current result");
		send_key(*home.Field->line_edit(), Qt::Key_Return, Qt::ControlModifier);
		expect(home.NewWindow.size() == 1U && home.NewWindow.front() == home.Opened.front(), "Command-Return opens it in a new window");
		home.type(QStringLiteral("zzzz"));
		send_key(*home.Field->line_edit(), Qt::Key_Return);
		expect(home.Opened.size() == 1U, "Return opens nothing while nothing matches");
	}

	void test_escape_clears_and_then_leaves()
	{
		filter_fixture home;
		home.Field->focus_field();
		settle(30);
		home.type(QStringLiteral("login"));
		send_key(*home.Field->line_edit(), Qt::Key_Escape);
		settle(30);
		expect(home.Field->query().isEmpty() && home.results().size() == 5 && home.Field->line_edit()->hasFocus(), "the first Escape clears the filter and keeps the keyboard");
		send_key(*home.Field->line_edit(), Qt::Key_Escape);
		settle(30);
		auto* const focused = QApplication::focusWidget();
		expect(qobject_cast<QListView*>(focused) != nullptr && home.List->isAncestorOf(focused), "the second leaves the field for a row of the list");
	}

	void test_typing_in_a_list_goes_to_the_field()
	{
		filter_fixture home;
		home.List->focus_first_row();
		settle(30);
		auto* const list = qobject_cast<QListView*>(QApplication::focusWidget());
		expect(list != nullptr, "a row has the keyboard");
		send_key(*list, Qt::Key_L, Qt::NoModifier, QStringLiteral("l"));
		settle(30);
		expect(home.Field->line_edit()->hasFocus() && home.Field->query() == QStringLiteral("l"), "a character typed in a list starts the filter with it");
		home.type(QString{ });
		home.List->focus_first_row();
		settle(30);
		send_key(*qobject_cast<QListView*>(QApplication::focusWidget()), Qt::Key_Slash, Qt::NoModifier, QStringLiteral("/"));
		settle(30);
		expect(home.Field->line_edit()->hasFocus() && home.Field->query().isEmpty(), "\"/\" only takes the keyboard to the field");
		home.List->focus_first_row();
		settle(30);
		auto* const again = qobject_cast<QListView*>(QApplication::focusWidget());
		send_key(*again, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
		send_key(*again, Qt::Key_L, Qt::ControlModifier, QStringLiteral("l"));
		expect(again->hasFocus() && home.Field->query().isEmpty(), "Space and a modified key stay the list's");
	}

	void test_collapsed_panels_open_while_filtering_and_come_back()
	{
		filter_fixture home;
		auto* const    following = home.List->state_panel(qt::home_group::following);
		following->set_collapsed(true);
		settle(30);
		const auto views = home.ViewChanges;
		home.type(QStringLiteral("acme"));
		expect(following->isVisible() && !following->collapsed(), "a folded panel with matches is held open while the filter runs");
		home.type(QString{ });
		expect(following->collapsed(), "and is folded again once the filter is cleared");
		expect(home.ViewChanges == views, "holding it open is not a change to the remembered view");
	}

	void test_nothing_matches_is_one_sentence()
	{
		filter_fixture home;
		home.type(QStringLiteral("zzzz"));
		auto* const sentence = home.List->filter_sentence();
		expect(sentence->isVisible() && sentence->text().contains(QStringLiteral("“zzzz”")) && sentence->text().contains(QStringLiteral("Esc")),
			   "nothing matching is one sentence that names the query and the way out");
		expect(home.Field->count_label()->text() == QStringLiteral("No matches"), "and the field says so");
		for (const auto group : { qt::home_group::needs_you, qt::home_group::my_pull_requests, qt::home_group::waiting_on_others, qt::home_group::following }) {
			expect(!home.List->state_panel(group)->isVisible(), "no empty panel is drawn");
		}
		expect(!home.List->empty_sentence()->isVisible(), "the empty queue's sentence is not a match");
		home.List->set_filter_elsewhere(1);
		expect(!sentence->isVisible(), "a match elsewhere on Home (the Active review card) keeps the sentence away");
		home.List->set_filter_elsewhere(0);
		home.type(QString{ });
		expect(!sentence->isVisible(), "clearing the filter takes it away");
	}

	void test_recently_viewed_is_filtered_and_opened()
	{
		filter_fixture home;
		auto* const    recent = new qt::home_recent_view{ };
		recent->addItem(recent_item(QStringLiteral("microsoft/terminal#42"), QStringLiteral("Fix the conpty resize race")));
		recent->addItem(recent_item(QStringLiteral("microsoft/terminal#77"), QStringLiteral("Teach the pane splitter about tabs")));
		home.List->adopt_recent(*recent);
		home.List->set_recent(2, QStringLiteral("2 viewed"));
		settle(60);
		home.type(QStringLiteral("conpty"));
		expect(!recent->isRowHidden(0) && recent->isRowHidden(1) && home.List->recent_panel()->isVisible() && home.List->recent_panel()->count() == 1,
			   "Recently viewed keeps only the row that matches, and counts it");
		expect(home.List->filter_result_view() == recent && home.List->filter_result().row() == 0, "the best match may be a Recently viewed row");
		expect(marks(recent->model()->index(0, 0), qt::home_row_role::title_marks) == std::vector{ QPoint{ 8, 6 } }, "its match is marked too");
		send_key(*home.Field->line_edit(), Qt::Key_Return);
		expect(home.RecentOpened == std::vector{ 0 }, "Return opens it as a Recently viewed row");
		home.type(QStringLiteral("zzzz"));
		expect(!home.List->recent_panel()->isVisible(), "Recently viewed goes when nothing in it matches");
		home.type(QStringLiteral("77"));
		expect(home.List->recent_panel()->isVisible() && !recent->isRowHidden(1) && home.result_identity() == QStringLiteral("octo-org/notes#77"),
			   "a number finds its row in every list; equal matches keep Home's order, the queue first: " + home.result_identity().toStdString());
		home.type(QString{ });
		expect(!recent->isRowHidden(0) && !recent->isRowHidden(1) && home.List->recent_panel()->count() == 2, "clearing shows every recent row again");
	}

	void test_a_large_list_filters_at_once()
	{
		filter_fixture home;
		auto           rows = std::vector<core::queue_item>{ };
		for (auto number = 1; number <= 1'500; ++number) {
			rows.push_back(item(number, number % 3 == 0 ? bucket::initial_review_requested : bucket::quiet,
								"Pull request " + std::to_string(number) + (number % 7 == 0 ? " about caching" : " about parsing"),
								number % 2 == 0 ? "octo-org/app" : "acme-tools/build", "contributor-" + std::to_string(number % 11)));
		}
		home.Source.apply(core::ordered_queue(rows));
		settle(300);
		auto timer = QElapsedTimer{ };
		timer.start();
		home.Field->line_edit()->setText(QStringLiteral("cach"));
		const auto filtered = timer.elapsed();
		settle(100);
		expect(home.List->filter_matches() == 1'500 / 7, "every row about caching, and no other");
		timer.restart();
		home.Field->line_edit()->setText(QStringLiteral("1499"));
		const auto number = timer.elapsed();
		settle(100);
		expect(home.result_identity() == QStringLiteral("acme-tools/build#1499"), "the exact number among 1,500 rows");
		// Timings are printed, never asserted: a Debug build's are not a measure.
		std::cout << "home-filter-tests:timing rows=1500 word=" << filtered << "ms number=" << number << "ms\n";
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   render = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString{ };
	test_typing_filters_and_panels_without_matches_go(render);
	test_exact_number_is_the_first_result();
	test_matched_characters_are_marked(render);
	test_the_keyboard_moves_through_results_and_opens_one();
	test_escape_clears_and_then_leaves();
	test_typing_in_a_list_goes_to_the_field();
	test_collapsed_panels_open_while_filtering_and_come_back();
	test_nothing_matches_is_one_sentence();
	test_recently_viewed_is_filtered_and_opened();
	test_a_large_list_filters_at_once();
	std::cout << "home-filter-tests:pass\n";
	return EXIT_SUCCESS;
}
