// RFC-BPR-0089 M1: the Commits tab and the filter chip present the host's
// projection, reconcile rows by commit, and dispatch gestures naming commits.
#include "bpr/application/commit_filter_projection.hpp"
#include "bpr/qt/commit_filter_view.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QStyleOptionViewItem>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

	auto row(const char aDigit, std::string aHeadline, const core::filter_commit_state aState = core::filter_commit_state::shown,
			 const core::filter_commit_role aRole = core::filter_commit_role::chain) -> application::commit_filter_row
	{
		auto value        = application::commit_filter_row{ };
		value.Commit      = std::string(40U, aDigit);
		value.ShortCommit = std::string(7U, aDigit);
		value.Headline    = std::move(aHeadline);
		value.Author      = "octo-author";
		value.Role        = aRole;
		value.State       = aState;
		value.Files       = 2U;
		return value;
	}

	auto owner(const core::filter_commit_state aSecond = core::filter_commit_state::shown) -> application::commit_filter_projection
	{
		auto projection            = application::commit_filter_projection{ };
		projection.Rows            = { row('1', "Change the generator input"), row('2', "Regenerate", aSecond), row('3', "Follow-ups") };
		projection.Spanned         = 3U;
		projection.Shown           = aSecond == core::filter_commit_state::hidden ? 2U : 3U;
		projection.Status          = aSecond == core::filter_commit_state::hidden ? core::commit_filter_status::active : core::commit_filter_status::inactive;
		projection.ComparisonFiles = 340U;
		projection.Trees           = application::commit_filter_trees::ready;
		if (aSecond == core::filter_commit_state::hidden) {
			projection.FilteredFiles = 12U;
		}
		return projection;
	}

	struct structure_counter
	{
		int Inserted = 0;
		int Removed  = 0;
		int Moved    = 0;
		int Reset    = 0;
		int Changed  = 0;
	};

	void test_model_reconciles_by_commit()
	{
		auto model   = qt::commit_filter_list_model{ };
		auto counter = structure_counter{ };
		QObject::connect(&model, &QAbstractItemModel::rowsInserted, [&counter] { ++counter.Inserted; });
		QObject::connect(&model, &QAbstractItemModel::rowsRemoved, [&counter] { ++counter.Removed; });
		QObject::connect(&model, &QAbstractItemModel::rowsMoved, [&counter] { ++counter.Moved; });
		QObject::connect(&model, &QAbstractItemModel::modelReset, [&counter] { ++counter.Reset; });
		QObject::connect(&model, &QAbstractItemModel::dataChanged, [&counter] { ++counter.Changed; });
		model.apply(owner().Rows);
		expect(model.rowCount() == 3 && counter.Inserted == 3 && counter.Reset == 0, "the first rows are inserted, not reset");
		const auto persistent = QPersistentModelIndex{ model.index(1) };
		model.apply(owner(core::filter_commit_state::hidden).Rows);
		expect(counter.Changed == 1 && counter.Inserted == 3 && persistent.isValid() && persistent.row() == 1
				   && model.data(persistent, qt::commit_filter_list_model::state_role).toInt() == static_cast<int>(core::filter_commit_state::hidden),
			   "hiding C2 changes only its row, which keeps its identity");
		auto pushed = owner().Rows;
		pushed.erase(pushed.begin() + 2);
		pushed.insert(pushed.begin(), row('4', "Amended follow-ups"));
		model.apply(pushed);
		expect(model.rowCount() == 3 && counter.Removed == 1 && counter.Reset == 0 && model.row_of(std::string(40U, '4')) == 0
				   && model.row_of(std::string(40U, '3')) == -1 && persistent.isValid(),
			   "a force push removes the gone row and inserts its replacement without a reset");
		expect(model.data(model.index(1), Qt::AccessibleTextRole).toString().contains(QStringLiteral("Change the generator input, commit 1111111, by octo-author, 2 files")),
			   "each row names itself to accessibility");
	}

	void test_tab_states_and_menu()
	{
		auto tab = qt::commit_filter_tab{ };
		tab.resize(360, 400);
		auto gestures = std::vector<application::commit_filter_gesture>{ };
		auto opened   = std::vector<std::string>{ };
		auto shown    = 0;
		tab.set_gesture_dispatch([&gestures](const application::commit_filter_gesture& aGesture) { gestures.push_back(aGesture); });
		tab.set_open_dispatch([&opened](const std::string& aCommit) { opened.push_back(aCommit); });
		tab.set_shown_dispatch([&shown] { ++shown; });
		tab.present(owner());
		expect(shown == 0, "the tab asks for nothing until it shows");
		tab.show();
		settle();
		expect(shown == 1, "showing the tab asks for the trees once");
		tab.hide();
		tab.show();
		settle();
		expect(shown == 1, "and only once");
		expect(tab.status()->text() == QStringLiteral("3 commits.") && !tab.clear_button()->isVisible() && tab.list()->isVisible(),
			   "with no filter the tab counts the commits and offers no Clear");

		tab.list()->selectionModel()->select(tab.model()->index(1), QItemSelectionModel::ClearAndSelect);
		auto       menu   = std::unique_ptr<QMenu>{ tab.make_menu(nullptr) };
		const auto action = [&menu](const QString& aName) { return menu->findChild<QAction*>(aName); };
		expect(action(QStringLiteral("commitFilterShowOnly"))->isEnabled() && action(QStringLiteral("commitFilterHide"))->isEnabled()
				   && action(QStringLiteral("commitFilterShow")) == nullptr && !action(QStringLiteral("commitFilterClearAll"))->isEnabled(),
			   "a shown commit offers Show only and Hide; Show selected appears only for a hidden one");
		action(QStringLiteral("commitFilterHide"))->trigger();
		expect(gestures.size() == 1U && gestures.back().Kind == core::commit_filter_operation_kind::hide
				   && gestures.back().Commits == std::vector<std::string>{ std::string(40U, '2') },
			   "Hide selected dispatches the selected commit's full ID");
		action(QStringLiteral("commitFilterCopy"))->trigger();
		expect(QGuiApplication::clipboard()->text() == QString::fromStdString(std::string(40U, '2')), "Copy commit ID copies the full ID");
		action(QStringLiteral("commitFilterOpen"))->trigger();
		expect(opened == std::vector<std::string>{ std::string(40U, '2') }, "Open on GitHub names the one selected commit");

		tab.present(owner(core::filter_commit_state::hidden));
		expect(tab.status()->text().startsWith(QStringLiteral("2 of 3 commits shown.\n12 of 340 files, filtered by commits."))
				   && tab.clear_button()->isVisible(),
			   "an active filter says what it shows and offers Clear all filters");
		tab.list()->selectionModel()->select(tab.model()->index(1), QItemSelectionModel::ClearAndSelect);
		menu.reset(tab.make_menu(nullptr));
		expect(action(QStringLiteral("commitFilterShow")) != nullptr && action(QStringLiteral("commitFilterShow"))->isEnabled()
				   && action(QStringLiteral("commitFilterClearAll"))->isEnabled(),
			   "a hidden commit offers Show selected, and Clear all filters is offered");
		tab.list()->selectionModel()->select(QItemSelection{ tab.model()->index(0), tab.model()->index(2) }, QItemSelectionModel::ClearAndSelect);
		menu.reset(tab.make_menu(nullptr));
		action(QStringLiteral("commitFilterShowOnly"))->trigger();
		expect(gestures.back().Kind == core::commit_filter_operation_kind::show_only && gestures.back().Commits.size() == 3U
				   && !action(QStringLiteral("commitFilterOpen"))->isEnabled(),
			   "a range selection acts on every selected commit, in list order; Open on GitHub needs one");
		tab.clear_button()->click();
		expect(gestures.back().Kind == core::commit_filter_operation_kind::clear && gestures.back().Commits.empty(), "Clear all filters dispatches clear");

		auto outside          = owner();
		outside.Rows[0].State = core::filter_commit_state::outside_comparison;
		tab.present(outside);
		tab.list()->selectionModel()->select(tab.model()->index(0), QItemSelectionModel::ClearAndSelect);
		menu.reset(tab.make_menu(nullptr));
		expect(!action(QStringLiteral("commitFilterShowOnly"))->isEnabled() && !action(QStringLiteral("commitFilterHide"))->isEnabled(),
			   "a commit outside the comparison cannot be shown or hidden");

		auto truncated        = application::commit_filter_projection{ };
		truncated.Unavailable = core::commit_filter_unavailable::truncated;
		tab.present(truncated);
		expect(tab.status()->text().contains(QStringLiteral("first 250 commits")) && !tab.list()->isVisible(), "a truncated list says why it has no filter");
		auto paused       = owner(core::filter_commit_state::hidden);
		paused.Status     = core::commit_filter_status::suspended;
		paused.Suspension = core::commit_filter_suspension::reversed;
		paused.FilteredFiles.reset();
		paused.Refusal = core::commit_filter_refusal::base_merge_alone;
		tab.present(paused);
		expect(tab.status()->text().contains(QStringLiteral("newer update to an older one")) && tab.status()->text().contains(QStringLiteral("on its own")),
			   "a paused filter and a refused gesture say why");
		tab.list()->selectionModel()->select(tab.model()->index(1), QItemSelectionModel::ClearAndSelect);
		menu.reset(tab.make_menu(nullptr));
		expect(!action(QStringLiteral("commitFilterHide"))->isEnabled() && action(QStringLiteral("commitFilterClearAll"))->isEnabled(),
			   "while paused no commit can be shown or hidden, but the filter can be cleared");
	}

	void test_chip()
	{
		auto review   = qt::commit_filter_chip{ true };
		auto overview = qt::commit_filter_chip{ false };
		auto cleared  = 0;
		auto opened   = 0;
		review.set_clear_dispatch([&cleared] { ++cleared; });
		review.set_commits_dispatch([&opened] { ++opened; });
		review.present(owner());
		overview.present(owner());
		expect(review.isHidden() && overview.isHidden(), "no filter, no chip");
		auto gone = owner();
		gone.Gone = { { .ShortCommit = "3333333", .Headline = "Follow-ups" } };
		review.present(gone);
		expect(!review.isHidden() && review.detail()->text().contains(QStringLiteral("3333333")), "a gone hidden commit is named on the chip");
		review.present(owner(core::filter_commit_state::hidden));
		overview.present(owner(core::filter_commit_state::hidden));
		expect(!review.isHidden() && review.summary()->text() == QStringLiteral("2 of 3 commits shown")
				   && review.detail()->text() == QStringLiteral("12 of 340 files, filtered by commits") && review.commits_button() != nullptr
				   && overview.commits_button() == nullptr,
			   "an active filter names itself; in Review the chip also opens the Commits tab");
		review.commits_button()->click();
		review.clear_button()->click();
		expect(opened == 1 && cleared == 1, "Commits and Clear all filters dispatch");
	}

	void test_rows_wrap_and_tag()
	{
		auto view  = QListView{ };
		auto model = qt::commit_filter_list_model{ };
		view.setModel(&model);
		view.resize(300, 400);
		auto rows        = owner(core::filter_commit_state::hidden).Rows;
		rows[2].Headline = std::string(400U, 'w').replace(100U, 1U, " ").replace(200U, 1U, " ").replace(300U, 1U, " ");
		model.apply(rows);
		auto delegate = qt::commit_filter_delegate{ };
		auto option   = QStyleOptionViewItem{ };
		option.initFrom(&view);
		option.widget       = &view;
		const auto shortRow = delegate.sizeHint(option, model.index(0));
		const auto longRow  = delegate.sizeHint(option, model.index(2));
		expect(longRow.height() > shortRow.height(), "a long headline wraps onto more lines rather than eliding");
		expect(qt::commit_filter_delegate::tag(model.index(1)) == QStringLiteral("Hidden") && qt::commit_filter_delegate::tag(model.index(0)).isEmpty(),
			   "a hidden row says so in words");
		rows[0].Role = core::filter_commit_role::base_merge;
		model.apply(rows);
		expect(qt::commit_filter_delegate::tag(model.index(0)) == QStringLiteral("Merge from base"), "a merge from the base branch is labelled");
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	auto application = QApplication{ aArgc, aArgv };
	test_model_reconciles_by_commit();
	test_tab_states_and_menu();
	test_chip();
	test_rows_wrap_and_tag();
	if (Failures != 0) {
		std::cerr << Failures << " failures\n";
		return EXIT_FAILURE;
	}
	std::cout << "commit-filter-view-tests:pass\n";
	return EXIT_SUCCESS;
}
