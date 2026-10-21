// RFC-BPR-0089 M1 and M2: the Commits tab and the filter chip present the
// host's projection, reconcile rows by commit, and dispatch gestures naming
// commits; the generated row and the file menus offer the core's gestures.
#include "bpr/application/commit_filter_projection.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/generated_files_view.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QTreeWidget>

#include <cstdlib>
#include <filesystem>
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
			projection.FilteredFiles  = 12U;
			projection.CommitFiltered = true;
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

	// RFC-BPR-0089 M2.
	void test_bulk_label_and_hide_files()
	{
		auto model   = qt::commit_filter_list_model{ };
		auto rows    = owner(core::filter_commit_state::hidden).Rows;
		rows[0].Bulk = true;
		rows[1].Bulk = true;
		model.apply(rows);
		expect(qt::commit_filter_delegate::tag(model.index(0)) == QStringLiteral("Bulk"), "a bulk commit is labelled");
		expect(qt::commit_filter_delegate::tag(model.index(1)) == QStringLiteral("Hidden, bulk"), "the bulk label sits beside the state");
		expect(model.index(0).data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("bulk")), "and is spoken");
		expect(qt::commit_filter_delegate::tag(model.index(2)).isEmpty(), "a smaller commit has no label");

		auto tab = qt::commit_filter_tab{ };
		auto hid = std::vector<std::vector<std::string>>{ };
		tab.present(owner());
		tab.list()->selectionModel()->select(tab.model()->index(1), QItemSelectionModel::ClearAndSelect);
		auto       menu   = std::unique_ptr<QMenu>{ tab.make_menu(nullptr) };
		const auto action = [&menu](const QString& aName) { return menu->findChild<QAction*>(aName); };
		expect(action(QStringLiteral("commitFilterHideFiles")) != nullptr && !action(QStringLiteral("commitFilterHideFiles"))->isEnabled(),
			   "Hide files these commits changed waits for a host to take it");
		tab.set_file_filter_dispatch([&hid](const std::vector<std::string>& aCommits) { hid.push_back(aCommits); });
		menu.reset(tab.make_menu(nullptr));
		expect(action(QStringLiteral("commitFilterHideFiles"))->isEnabled(), "Hide files these commits changed is offered once the trees are read");
		action(QStringLiteral("commitFilterHideFiles"))->trigger();
		expect(hid == std::vector<std::vector<std::string>>{ { std::string(40U, '2') } }, "it dispatches the selected commits' full IDs");
		auto loading  = owner();
		loading.Trees = application::commit_filter_trees::loading;
		for (auto& entry : loading.Rows) {
			entry.Files.reset();
		}
		tab.present(loading);
		tab.list()->selectionModel()->select(tab.model()->index(1), QItemSelectionModel::ClearAndSelect);
		menu.reset(tab.make_menu(nullptr));
		expect(!action(QStringLiteral("commitFilterHideFiles"))->isEnabled(), "not before the commits' files are known");
	}

	void test_file_filter_chip_and_counts()
	{
		auto chip                    = qt::commit_filter_chip{ true };
		auto projection              = owner();
		projection.FileFilterPaths   = 3U;
		projection.FileFilteredFiles = 3U;
		projection.FilteredFiles     = 2U;
		projection.ComparisonFiles   = 5U;
		chip.present(projection);
		expect(!chip.isHidden() && chip.summary()->text() == QStringLiteral("File filter on")
				   && chip.detail()->text() == QStringLiteral("2 of 5 files, 3 hidden by file filters"),
			   "a file filter alone shows the chip and says how many files it hides");
		projection.Generated = { { .Path = "gen/a.c", .Reason = core::generated_reason::attribute_set, .Generated = true, .ReviewAnyway = false, .Additions = 4U, .Deletions = 1U, .Gestures = { } } };
		expect(qt::commit_filter_files_text(projection) == QStringLiteral("2 of 5 files, 3 hidden by file filters, 1 generated set aside"),
			   "the count names every reason files are left out");
		auto aside            = owner();
		aside.FilteredFiles   = 4U;
		aside.ComparisonFiles = 5U;
		aside.Generated       = projection.Generated;
		chip.present(aside);
		expect(chip.isHidden(), "generated files set aside alone are not a filter: no chip");
	}

	auto generated_projection() -> application::commit_filter_projection
	{
		auto projection               = owner();
		projection.Generated          = { { .Path = "gen/a.c", .Reason = core::generated_reason::attribute_set, .Generated = true, .ReviewAnyway = false, .Additions = 120U, .Deletions = 7U, .Gestures = { core::generated_gesture::review_anyway, core::generated_gesture::show_as_normal } },
										  { .Path = "package-lock.json", .Reason = core::generated_reason::marked_generated, .Generated = true, .ReviewAnyway = false, .Additions = 30U, .Deletions = 0U, .Gestures = { core::generated_gesture::review_anyway, core::generated_gesture::show_as_normal, core::generated_gesture::follow_attributes } } };
		projection.GeneratedAdditions = 150U;
		projection.GeneratedDeletions = 7U;
		projection.Marked             = { { .Path = "gen/b.c", .Reason = core::generated_reason::attribute_set, .Generated = true, .ReviewAnyway = true, .Additions = 0U, .Deletions = 0U, .Gestures = { core::generated_gesture::set_aside_again, core::generated_gesture::show_as_normal } } };
		projection.OrdinaryGestures   = { core::generated_gesture::treat_as_generated };
		return projection;
	}

	void test_generated_row()
	{
		auto row      = qt::generated_files_row{ };
		auto gestures = std::vector<application::generated_file_gesture>{ };
		row.set_gesture_dispatch([&gestures](const application::generated_file_gesture& aGesture) { gestures.push_back(aGesture); });
		row.present(owner());
		expect(row.isHidden(), "nothing set aside, no row");

		row.present(generated_projection());
		row.resize(360, row.sizeHint().height());
		row.show();
		settle();
		expect(!row.isHidden() && row.toggle()->text() == QStringLiteral("2 generated files hidden") && row.totals()->text() == QStringLiteral("+150 \u2212"
																																			   "7"),
			   "the row names how many generated files are hidden and their line totals");
		expect(!row.list()->isVisible() && row.list()->topLevelItemCount() == 2, "the list is closed until the reviewer opens it");
		row.toggle()->click();
		row.resize(360, row.sizeHint().height());
		settle();
		expect(row.open() && row.list()->isVisible() && row.list()->topLevelItem(1)->text(0) == QStringLiteral("package-lock.json")
				   && row.list()->topLevelItem(1)->text(1) == QStringLiteral("Treated as generated"),
			   "opened, it lists each file and why it is generated");

		auto       menu   = std::unique_ptr<QMenu>{ row.make_menu(1, nullptr) };
		const auto titles = [&menu] {
			auto names = QStringList{ };
			for (auto* action : menu->actions()) {
				if (!action->isSeparator()) {
					names << action->text();
				}
			}
			return names;
		};
		expect(titles() == QStringList{ QStringLiteral("Review anyway"), QStringLiteral("Show as normal"), QStringLiteral("Follow .gitattributes") },
			   "a file marked generated offers Review anyway, Show as normal and Follow .gitattributes");
		menu->actions().front()->trigger();
		expect(gestures.size() == 1U && gestures.back().Path == "package-lock.json" && gestures.back().Gesture == core::generated_gesture::review_anyway,
			   "Review anyway dispatches the file's path");
		expect(row.make_menu(7, nullptr) == nullptr, "no menu past the list");

		if (const auto* directory = std::getenv("BPR_RFC0089_RENDER_DIR"); directory != nullptr) {
			const auto path = std::filesystem::path{ directory } / ("generated-row-" + std::to_string(static_cast<int>(row.devicePixelRatioF() * 100.0)) + ".png");
			row.grab().save(QString::fromStdString(path.string()));
		}

		row.present(owner());
		expect(row.isHidden() && !row.list()->isVisible(), "once nothing is set aside the row hides");
	}

	void test_file_menus_offer_generated_gestures()
	{
		const auto projection = generated_projection();
		expect(qt::generated_gestures_for(projection, "gen/b.c") == std::vector{ core::generated_gesture::set_aside_again, core::generated_gesture::show_as_normal },
			   "a file reviewed anyway offers its own gestures");
		expect(qt::generated_gestures_for(projection, "src/ordinary.c") == std::vector{ core::generated_gesture::treat_as_generated },
			   "an ordinary file offers Treat as generated");
		auto menu     = QMenu{ };
		auto received = std::vector<application::generated_file_gesture>{ };
		menu.addAction(QStringLiteral("Open in New Tab"));
		qt::add_generated_actions(menu, "src/ordinary.c", qt::generated_gestures_for(projection, "src/ordinary.c"),
								  [&received](const application::generated_file_gesture& aGesture) { received.push_back(aGesture); });
		expect(menu.actions().size() == 3 && menu.actions()[1]->isSeparator() && menu.actions()[2]->text() == QStringLiteral("Treat as generated"),
			   "the gestures follow the menu's own actions after a separator");
		menu.actions()[2]->trigger();
		expect(received.size() == 1U && received.back().Gesture == core::generated_gesture::treat_as_generated && received.back().Path == "src/ordinary.c",
			   "Treat as generated dispatches the path");
		auto idle = QMenu{ };
		qt::add_generated_actions(idle, "src/ordinary.c", { core::generated_gesture::treat_as_generated }, { });
		expect(!idle.actions().front()->isEnabled(), "with no host to take it a gesture is shown but disabled");
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	auto application = QApplication{ aArgc, aArgv };
	test_model_reconciles_by_commit();
	test_tab_states_and_menu();
	test_chip();
	test_rows_wrap_and_tag();
	test_bulk_label_and_hide_files();
	test_file_filter_chip_and_counts();
	test_generated_row();
	test_file_menus_offer_generated_gestures();
	if (Failures != 0) {
		std::cerr << Failures << " failures\n";
		return EXIT_FAILURE;
	}
	std::cout << "commit-filter-view-tests:pass\n";
	return EXIT_SUCCESS;
}
