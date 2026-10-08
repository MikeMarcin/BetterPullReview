#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include <QTextEdit>

#include "../support/resize_cursor_hold_check.hpp"

#include "bpr/application/pull_request_overview.hpp"

#include <QAbstractScrollArea>
#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeView>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto row(
		const std::string&                              aPath,
		const application::overview_file_operation      aOperation,
		const application::overview_change_since_review aChanged,
		const std::optional<core::file_read_state>      aRead,
		const std::size_t                               aThreads    = 0U,
		const std::size_t                               aUnresolved = 0U,
		const std::optional<std::string>&               aNote       = std::nullopt)
		-> application::overview_file_row
	{
		const auto slash = aPath.rfind('/');
		return application::overview_file_row{
			.Path                  = aPath,
			.Folder                = slash == std::string::npos ? std::string{ } : aPath.substr(0U, slash),
			.Name                  = slash == std::string::npos ? aPath : aPath.substr(slash + 1U),
			.Operation             = aOperation,
			.Additions             = 12U,
			.Deletions             = aOperation == application::overview_file_operation::added ? 0U : 3U,
			.Changed               = aChanged,
			.Read                  = aRead,
			.ThreadCount           = aThreads,
			.UnresolvedThreadCount = aUnresolved,
			.Note                  = aNote,
		};
	}

	[[nodiscard]] auto projection() -> application::pull_request_overview_projection
	{
		auto projection                = application::pull_request_overview_projection{ };
		projection.Owner               = "EpicGames";
		projection.RepositoryName      = "UnrealEngine";
		projection.Number              = core::pull_request_number{ 14366U };
		projection.Title               = "Hair strand lighting fixes";
		projection.Body                = "Fixes three crashes.";
		projection.AuthorLogin         = "jiayin";
		projection.BaseRef             = "release";
		projection.HeadRef             = "hair-strand";
		projection.CommitCount         = 4U;
		projection.ReturnedCommitCount = 4U;
		projection.Files               = {
			row("Engine/Renderer/HairStrandsLighting.cpp", application::overview_file_operation::modified,
				application::overview_change_since_review::unchanged, core::file_read_state::seen, 3U, 1U),
			row("Engine/Renderer/HairStrandsVoxelization.cpp", application::overview_file_operation::modified,
				application::overview_change_since_review::changed, core::file_read_state::read, 2U, 0U,
				std::string{ "Re-read the atomic move" }),
			row("Engine/Shaders/HairStrandsDeepShadow.usf", application::overview_file_operation::added,
				application::overview_change_since_review::added, std::nullopt),
		};
		projection.Participants = {
			{ .Login = "jiayin", .Role = application::overview_participant_role::author, .Signoff = application::overview_signoff::uploaded },
			{ .Login = "mike", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::changes_requested, .Viewer = true },
			{ .Login = "rcaloca", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::approved },
		};
		projection.SinceLastReview = application::overview_since_last_review{
			.Known               = true,
			.ReviewedAt          = core::observed_utc_milliseconds{ 1'770'000'000'000 },
			.Uploads             = 2U,
			.FilesChanged        = 2U,
			.RepliesToMe         = 1U,
			.MyUnresolvedThreads = 2U,
		};
		projection.Checks    = { .Passed = 3U, .Skipped = 1U };
		projection.Progress  = { .FilesRead = 1U, .FilesTotal = 3U, .PrivateNotes = 1U, .DraftedComments = 1U };
		projection.Entry     = application::overview_entry_kind::continue_review;
		projection.EntryPath = "Engine/Renderer/HairStrandsVoxelization.cpp";
		return projection;
	}

	auto test_page_presents_the_projection_and_reports_its_gestures() -> void
	{
		qt::pull_request_overview_page page;
		page.resize(1'100, 700);
		page.show();
		QApplication::processEvents();
		expect(!page.presented() && !page.entry_button()->isEnabled()
				   && page.title_label()->text() == QStringLiteral("No pull request"),
			   "before a projection the page names nothing and offers no entry");

		auto entered   = std::vector<std::pair<application::overview_entry_kind, QString>>{ };
		auto activated = std::vector<QString>{ };
		page.set_entry_dispatch(
			[&entered](const application::overview_entry_kind aEntry, const QString& aPath) {
				entered.emplace_back(aEntry, aPath);
			});
		page.set_file_dispatch([&activated](const QString& aPath) { activated.push_back(aPath); });
		page.present(projection());
		QApplication::processEvents();

		expect(page.presented()
				   && page.title_label()->text() == QStringLiteral("Hair strand lighting fixes")
				   && page.summary_label()->text()
					   == QStringLiteral("jiayin wants to merge 4 commits into release from hair-strand.")
				   && page.body_view()->rendered_view()->toPlainText() == QStringLiteral("Fixes three crashes.")
				   && page.body_view()->isVisible(),
			   "the header carries the title, the author and branches, and the description");
		expect(page.files_heading()->text()
				   == QStringLiteral("Files: 3 in this pull request, 2 changed since my last review, 2 not read"),
			   "the files heading counts the files, the changed and the not read");

		auto* const model    = page.file_model();
		const auto  renderer = model->index(0, 0);
		expect(model->folder_count() == 2U && model->file_count() == 3U
				   && model->rowCount() == 2 && model->rowCount(renderer) == 2
				   && model->rowCount(model->index(1, 0)) == 1
				   && model->index(0, qt::overview_file_model::file_column).data().toString()
					   == QStringLiteral("Engine/Renderer")
				   && model->index(1, qt::overview_file_model::file_column).data().toString()
					   == QStringLiteral("Engine/Shaders"),
			   "[[BPR-1213]] files keep projection order under flat folder rows named by their whole path, and Engine, which holds no file, takes no row");
		const auto voxel = model->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"));
		expect(voxel.isValid() && voxel.parent().row() == 0 && voxel.row() == 1
				   && voxel.siblingAtColumn(qt::overview_file_model::operation_column).data().toString() == QStringLiteral("M")
				   && voxel.data().toString() == QStringLiteral("HairStrandsVoxelization.cpp")
				   && voxel.siblingAtColumn(qt::overview_file_model::added_column).data().toString() == QStringLiteral("+12")
				   && voxel.siblingAtColumn(qt::overview_file_model::removed_column).data().toString() == QStringLiteral("−3")
				   && voxel.siblingAtColumn(qt::overview_file_model::changed_column).data().toString() == QStringLiteral("changed")
				   && voxel.siblingAtColumn(qt::overview_file_model::read_column).data(Qt::AccessibleTextRole).toString() == QStringLiteral("read")
				   && voxel.siblingAtColumn(qt::overview_file_model::comments_column).data().toString() == QStringLiteral("2")
				   && voxel.siblingAtColumn(qt::overview_file_model::note_column).data().toString() == QStringLiteral("Re-read the atomic move")
				   && voxel.data(qt::overview_file_model::path_role).toString() == QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"),
			   "a file row answers every column of the accepted table");
		const auto lighting = model->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp"));
		const auto shadow   = model->index_for_path(QStringLiteral("Engine/Shaders/HairStrandsDeepShadow.usf"));
		expect(lighting.siblingAtColumn(qt::overview_file_model::comments_column).data().toString() == QStringLiteral("1 / 3")
				   && lighting.siblingAtColumn(qt::overview_file_model::changed_column).data().toString().isEmpty()
				   && shadow.siblingAtColumn(qt::overview_file_model::operation_column).data().toString() == QStringLiteral("A")
				   && shadow.siblingAtColumn(qt::overview_file_model::changed_column).data().toString() == QStringLiteral("new")
				   && shadow.siblingAtColumn(qt::overview_file_model::removed_column).data().toString().isEmpty()
				   && shadow.siblingAtColumn(qt::overview_file_model::read_column).data(Qt::AccessibleTextRole).toString() == QStringLiteral("unread"),
			   "unresolved counts lead the total, a new file says new, and no receipt reads unread");

		const auto participants = page.participant_rows();
		expect(participants.size() == 3U && participants[0]->login() == QStringLiteral("jiayin")
				   && participants[0]->standing_text() == QStringLiteral("uploaded")
				   && participants[1]->login() == QStringLiteral("mike")
				   && participants[1]->standing_text() == QStringLiteral("changes requested")
				   && participants[2]->standing_text() == QStringLiteral("approved")
				   && participants[1]->isVisible(),
			   "participants are rows with their login and standing");

		expect(page.since_heading()->text().startsWith(QStringLiteral("Since my last review on "))
				   && page.value_label(qt::overview_value::uploads)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::files_changed)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::new_comments)->text() == QStringLiteral("not available")
				   && page.value_label(qt::overview_value::replies_to_me)->text() == QStringLiteral("1")
				   && page.value_label(qt::overview_value::my_unresolved_threads)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::checks)->text() == QStringLiteral("3 passed, 1 skipped")
				   && page.value_label(qt::overview_value::files_read)->text() == QStringLiteral("1 of 3")
				   && page.value_label(qt::overview_value::private_notes)->text() == QStringLiteral("1")
				   && page.value_label(qt::overview_value::drafted_comments)->text() == QStringLiteral("1 unpublished"),
			   "the side column states what happened since my last review and my progress, "
			   "saying not available where the host cannot answer");
		expect(page.entry_button()->isEnabled()
				   && page.entry_button()->text() == QStringLiteral("Continue review")
				   && page.entry_button()->toolTip().contains(QStringLiteral("HairStrandsVoxelization.cpp")),
			   "the one action continues the review at the named file");

		page.entry_button()->click();
		QApplication::processEvents();
		expect(entered.size() == 1U
				   && entered.front().first == application::overview_entry_kind::continue_review
				   && entered.front().second == QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"),
			   "Continue review reports its semantic kind and entry path");
		page.files_view()->activated(lighting);
		QApplication::processEvents();
		expect(activated.size() == 1U
				   && activated.front() == QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp"),
			   "activating a file row reports its path");
		page.files_view()->activated(model->index(0, 0));
		QApplication::processEvents();
		expect(activated.size() == 1U, "activating a folder row reports nothing");

		auto unknown                     = projection();
		unknown.SinceLastReview          = { .Known = true };
		unknown.Files.front().Changed    = application::overview_change_since_review::unknown;
		unknown.Progress.DraftedComments = std::nullopt;
		unknown.Entry                    = application::overview_entry_kind::start_review;
		page.present(unknown);
		QApplication::processEvents();
		expect(page.files_heading()->text().contains(QStringLiteral("not yet resolved"))
				   && page.value_label(qt::overview_value::uploads)->text() == QStringLiteral("not retained")
				   && page.value_label(qt::overview_value::files_changed)->text() == QStringLiteral("not yet resolved")
				   && page.value_label(qt::overview_value::drafted_comments)->text() == QStringLiteral("not available")
				   && page.file_model()->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp")).siblingAtColumn(qt::overview_file_model::changed_column).data().toString()
					   == QStringLiteral("?")
				   && page.entry_button()->text() == QStringLiteral("Start review"),
			   "what the host cannot answer reads as not known, never as none");
		page.entry_button()->click();
		QApplication::processEvents();
		expect(entered.size() == 2U
				   && entered.back().first == application::overview_entry_kind::start_review
				   && entered.back().second == QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"),
			   "Start review remains distinct from Continue review at dispatch");

		auto partial                = projection();
		partial.CommitCount         = 9'311U;
		partial.ReturnedCommitCount = 250U;
		page.present(partial);
		expect(page.summary_label()->text().contains(
				   QStringLiteral("9311 commits (250 returned by GitHub)")),
			   "large totals do not label the returned commit slice as complete");

		page.clear();
		QApplication::processEvents();
		expect(!page.presented() && page.file_model()->file_count() == 0U
				   && !page.entry_button()->isEnabled() && page.participant_rows().empty(),
			   "clearing empties the page");
		page.hide();
	}

	void settle()
	{
		for (int pass = 0; pass < 5; ++pass) {
			QApplication::processEvents();
		}
	}

	void key(QTreeView& aView, const Qt::Key aKey)
	{
		QKeyEvent press{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QKeyEvent release{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aView, &press);
		QApplication::sendEvent(&aView, &release);
		settle();
	}

	void click(QTreeView& aView, const QPoint& aPoint)
	{
		const auto  global = QPointF{ aView.viewport()->mapToGlobal(aPoint) };
		QMouseEvent press{ QEvent::MouseButtonPress, QPointF{ aPoint }, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QMouseEvent release{ QEvent::MouseButtonRelease, QPointF{ aPoint }, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aView.viewport(), &press);
		QApplication::sendEvent(aView.viewport(), &release);
		settle();
	}

	/// \brief [[BPR-1269]]. Collapse all and expand all fold every folder row
	/// at once, keep each folder's files and their order, open nothing, and
	/// survive a refresh; a per-folder toggle still works afterwards.
	void test_collapse_all_and_expand_all_folders()
	{
		qt::pull_request_overview_page page;
		page.resize(1100, 700);
		auto projected = projection();
		projected.Files.push_back(row("README.md", application::overview_file_operation::modified,
									  application::overview_change_since_review::unchanged, std::nullopt));
		page.present(projected);
		page.show();
		settle();
		auto* const view       = page.files_view();
		auto* const model      = page.file_model();
		auto* const collapse   = page.collapse_folders_button();
		auto* const expand     = page.expand_folders_button();
		auto        dispatches = 0;
		page.set_file_dispatch([&](const QString&) { ++dispatches; });
		expect(collapse != nullptr && expand != nullptr && collapse->isVisible() && expand->isVisible()
				   && collapse->accessibleName() == QStringLiteral("Collapse all folders") && expand->accessibleName() == QStringLiteral("Expand all folders")
				   && collapse->toolTip() == collapse->accessibleName() && expand->toolTip() == expand->accessibleName()
				   && collapse->focusPolicy() == Qt::StrongFocus && expand->focusPolicy() == Qt::StrongFocus && !collapse->icon().isNull(),
			   "two named glyph buttons beside the heading take the keyboard");
		const auto folders = [model] {
			auto result = std::vector<QPersistentModelIndex>{ };
			for (auto folder = 0; folder < model->rowCount(); ++folder) {
				result.emplace_back(model->index(folder, 0));
			}
			return result;
		};
		const auto children = [model](const QModelIndex& aFolder) {
			auto paths = QStringList{ };
			for (auto child = 0; child < model->rowCount(aFolder); ++child) {
				paths.push_back(model->index(child, qt::overview_file_model::file_column, aFolder).data(qt::overview_file_model::path_role).toString());
			}
			return paths;
		};
		auto before = std::vector<QStringList>{ };
		for (const auto& folder : folders()) {
			before.push_back(children(folder));
		}
		const auto file = QPersistentModelIndex{ model->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp")) };
		view->setCurrentIndex(file);
		collapse->click();
		settle();
		auto index = std::size_t{ 0U };
		for (const auto& folder : folders()) {
			expect(!view->isExpanded(folder) && children(folder) == before[index++], "collapse all closes every folder and keeps its files in order");
		}
		expect(view->visualRect(file).isEmpty() && view->currentIndex().siblingAtColumn(0) == QModelIndex{ file.parent() } && dispatches == 0,
			   "the hidden current file leaves its folder current, and nothing opens");
		projected.Files.front().Read = core::file_read_state::read;
		projected.Files.push_back(row("Docs/start.md", application::overview_file_operation::added, application::overview_change_since_review::added, std::nullopt));
		page.present(projected);
		settle();
		const auto docs = QPersistentModelIndex{ model->index_for_path(QStringLiteral("Docs/start.md")).parent() };
		for (const auto& folder : folders()) {
			expect(folder == docs ? view->isExpanded(folder) : !view->isExpanded(folder),
				   "a refresh keeps what collapse all closed, and a new folder starts expanded");
		}
		view->setFocus();
		view->setCurrentIndex(file.parent());
		key(*view, Qt::Key_Right);
		expect(view->isExpanded(file.parent()) && !view->visualRect(file).isEmpty(), "a single folder still opens afterwards");
		expand->click();
		settle();
		for (const auto& folder : folders()) {
			expect(view->isExpanded(folder), "expand all opens every folder");
		}
		expect(dispatches == 0, "no folder gesture opens a file");
		page.clear();
		settle();
		expect(!collapse->isVisible() && !expand->isVisible(), "without folders the buttons are gone");
		page.present(projection());
		settle();
		for (const auto& folder : folders()) {
			expect(view->isExpanded(folder), "a replaced authority starts every folder expanded");
		}
	}

	/// \brief [[BPR-1213]]. The owner's flat folders: a folder row only where
	/// files are, named by its whole path; every file label at one left
	/// column; disclosure hides and shows only that folder's files.
	void test_folder_controls_preserve_identity_and_refresh_state()
	{
		qt::pull_request_overview_page page;
		page.resize(1100, 700);
		auto projected = projection();
		// A folder several empty levels below its parent, and a file at the root.
		projected.Files.push_back(row("Engine/Renderer/Private/Deep/HairStrandsCluster.cpp", application::overview_file_operation::modified,
									  application::overview_change_since_review::unchanged, std::nullopt));
		projected.Files.push_back(row("README.md", application::overview_file_operation::modified,
									  application::overview_change_since_review::unchanged, std::nullopt));
		page.present(projected);
		page.show();
		settle();
		auto* view       = page.files_view();
		auto* model      = page.file_model();
		auto  dispatches = 0;
		auto  resets     = 0;
		auto  insertions = 0;
		auto  removals   = 0;
		page.set_file_dispatch([&](const QString&) { ++dispatches; });
		QObject::connect(model, &QAbstractItemModel::modelReset, [&] { ++resets; });
		QObject::connect(model, &QAbstractItemModel::rowsInserted, [&] { ++insertions; });
		QObject::connect(model, &QAbstractItemModel::rowsRemoved, [&] { ++removals; });
		const auto file     = QPersistentModelIndex{ model->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp")) };
		const auto renderer = QPersistentModelIndex{ file.parent() };
		const auto shadow   = QPersistentModelIndex{ model->index_for_path(QStringLiteral("Engine/Shaders/HairStrandsDeepShadow.usf")) };
		const auto deep     = QPersistentModelIndex{ model->index_for_path(QStringLiteral("Engine/Renderer/Private/Deep/HairStrandsCluster.cpp")) };
		const auto readme   = QPersistentModelIndex{ model->index_for_path(QStringLiteral("README.md")) };
		const auto label    = [model](const int aRow) {
			return model->index(aRow, qt::overview_file_model::file_column).data().toString();
		};
		expect(model->rowCount() == 4 && label(0) == QStringLiteral("Engine/Renderer") && label(1) == QStringLiteral("Engine/Shaders")
				   && label(2) == QStringLiteral("Engine/Renderer/Private/Deep") && label(3) == QStringLiteral("/")
				   && !renderer.parent().isValid() && !deep.parent().parent().isValid() && readme.parent().row() == 3,
			   "one row per folder that holds files: no Engine, Private or Deep row, and a root file under /");
		expect(view->isExpanded(renderer) && !view->visualRect(file).isEmpty(), "new folders initially reveal their files");
		const auto left = [view](const QModelIndex& aIndex) {
			return view->visualRect(aIndex.siblingAtColumn(qt::overview_file_model::file_column)).left();
		};
		expect(view->indentation() == 0 && left(file) == left(shadow) && left(file) == left(deep) && left(file) == left(readme)
				   && left(file) == left(renderer) && left(renderer) == left(deep.parent()),
			   "every file label and every folder row starts at the same left column, with no indentation");
		auto bold = view->font();
		bold.setWeight(QFont::DemiBold);
		auto measured = QStyleOptionViewItem{ };
		measured.initFrom(view);
		const auto deepFolder = QModelIndex{ deep.parent() }.siblingAtColumn(qt::overview_file_model::file_column);
		expect(view->visualRect(deepFolder).width()
				   >= qt::overview_row_delegate::disclosure_rect(measured).width() + QFontMetrics{ bold }.horizontalAdvance(deepFolder.data().toString()),
			   "the longest folder path fits its cell beside the disclosure mark, unelided");
		auto* accessible = QAccessible::queryAccessibleInterface(view);
		auto* table      = accessible == nullptr ? nullptr : accessible->tableInterface();
		expect(table != nullptr, "native Overview tree exposes an accessible table");
		// Qt exposes a tree row's disclosure state through its first cell.
		auto* cell = table->cellAt(0, qt::overview_file_model::operation_column);
		expect(cell != nullptr && cell->text(QAccessible::Name).contains(QStringLiteral("Engine/Renderer"))
				   && cell->state().expandable && cell->state().expanded,
			   "folder accessibility identifies its whole path and expanded disclosure state");
		view->setFocus();
		view->setCurrentIndex(renderer);
		key(*view, Qt::Key_Left);
		expect(!view->isExpanded(renderer) && view->visualRect(file).isEmpty() && !view->visualRect(shadow).isEmpty()
				   && !view->visualRect(deep).isEmpty() && cell->state().expandable && !cell->state().expanded,
			   "keyboard collapse hides only this folder's own files, not a deeper folder's");
		projected.Files.front().Read = core::file_read_state::read;
		page.present(projected);
		settle();
		expect(!view->isExpanded(renderer) && view->currentIndex() == renderer && view->hasFocus()
				   && file.isValid() && resets == 0 && insertions == 0,
			   "metadata refresh retains collapse, focus and persistent file identity");
		projected.Files.insert(projected.Files.begin(), row("Docs/start.md", application::overview_file_operation::added, application::overview_change_since_review::added, std::nullopt));
		projected.Files.push_back(row("Engine/Tests/new.cpp", application::overview_file_operation::added,
									  application::overview_change_since_review::added, std::nullopt));
		page.present(projected);
		settle();
		expect(renderer.isValid() && renderer.row() == 1 && file.isValid() && !view->isExpanded(renderer)
				   && view->currentIndex() == renderer && view->visualRect(file).isEmpty() && insertions == 2 && resets == 0
				   && view->isExpanded(model->index_for_path(QStringLiteral("Engine/Tests/new.cpp")).parent()),
			   "structural refresh inserts new folders, expanded, without resetting surviving identities or collapse");
		key(*view, Qt::Key_Right);
		expect(view->isExpanded(renderer) && !view->visualRect(file).isEmpty()
				   && model->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp")) == file
				   && model->index(1, qt::overview_file_model::file_column, renderer).data().toString() == QStringLiteral("HairStrandsVoxelization.cpp"),
			   "reopening restores the same file identities and order");

		// The pointer: the folder cell's disclosure mark, then the folder row.
		view->setCurrentIndex(file);
		auto option = QStyleOptionViewItem{ };
		option.initFrom(view);
		option.rect           = view->visualRect(QModelIndex{ renderer }.siblingAtColumn(qt::overview_file_model::file_column));
		const auto disclosure = qt::overview_row_delegate::disclosure_rect(option);
		expect(disclosure.width() > 0 && option.rect.contains(disclosure), "the folder cell reserves its disclosure mark");
		click(*view, disclosure.center());
		expect(!view->isExpanded(renderer) && view->visualRect(file).isEmpty() && view->currentIndex().siblingAtColumn(0) == renderer
				   && dispatches == 0,
			   "clicking the disclosure hides the files and leaves the folder current without opening a file");
		click(*view, view->visualRect(QModelIndex{ renderer }.siblingAtColumn(qt::overview_file_model::note_column)).center());
		expect(view->isExpanded(renderer) && !view->visualRect(file).isEmpty() && dispatches == 0,
			   "clicking anywhere on the folder row shows its files again");
		view->doubleClicked(renderer);
		view->activated(renderer);
		settle();
		expect(dispatches == 0, "no folder gesture opens a file");

		projected.Files.erase(projected.Files.begin());
		std::ranges::reverse(projected.Files);
		page.present(projected);
		settle();
		expect(renderer.isValid() && file.isValid() && view->isExpanded(renderer) && removals == 1 && resets == 0 && dispatches == 0,
			   "removal and sibling reordering preserve surviving folders without model reset or navigation");
		view->setCurrentIndex(renderer);
		key(*view, Qt::Key_Left);
		page.clear();
		expect(!renderer.isValid() && !file.isValid(), "authority invalidation removes every retained folder and file identity");
		page.present(projection());
		settle();
		expect(view->isExpanded(model->index(0, 0)) && !view->currentIndex().isValid() && dispatches == 0,
			   "new authority starts expanded without inheriting the former collapse or selection");
		if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
			page.present(projected);
			settle();
			view->setExpanded(model->index_for_path(QStringLiteral("Engine/Tests/new.cpp")).parent(), false);
			settle();
			const QFileInfo target{ path };
			expect(page.grab().save(QStringLiteral("%1/%2-flat-folders.png").arg(target.path(), target.completeBaseName()), "PNG"),
				   "the flat folder render is saved for inspection");
		}
	}

	auto description(const int aSections) -> QString
	{
		auto result = QString{ };
		for (int section = 0; section < aSections; ++section) {
			result += QStringLiteral("## Section %1 — 日本語 🦊\n- [x] Literal <b>markup</b>\nWide code: compare(old_revision, new_revision, exact_repository_identity, selected_file_identity, retained_comparison_identity);\n| Case | Expected result |\n\n").arg(section);
		}
		return result + QStringLiteral("END OF ADMITTED BODY");
	}

	void expect_anchor(QPlainTextEdit* aEdit, const int aPosition, const std::source_location aLocation = std::source_location::current())
	{
		auto cursor = QTextCursor{ aEdit->document() };
		cursor.setPosition(aPosition);
		const auto rect = aEdit->cursorRect(cursor);
		if (rect.top() < 0 || rect.bottom() >= aEdit->viewport()->height()) {
			std::cerr << "anchor check line=" << aLocation.line() << " source=" << aPosition << " top=" << rect.top() << " bottom=" << rect.bottom() << " viewport=" << aEdit->viewport()->height() << " scroll=" << aEdit->verticalScrollBar()->value() << '/' << aEdit->verticalScrollBar()->maximum() << '\n';
		}
		expect(rect.top() >= 0 && rect.bottom() < aEdit->viewport()->height(), "retained source anchor is fully visible");
		expect(rect.top() <= rect.height() || aEdit->verticalScrollBar()->value() == aEdit->verticalScrollBar()->maximum(), "source anchor stays within one wrapped line or nearest document-end clamp");
	}

	void test_retained_description_reader()
	{
		for (const int width : { 800, 1400 }) {
			for (const int points : { 13, 20 }) {
				for (const int sections : { 20, 900 }) {
					qt::pull_request_overview_page page;
					page.resize(width, 800);
					auto font = page.font();
					font.setPointSize(points);
					page.setFont(font);
					auto       projected = projection();
					const auto source    = description(sections);
					projected.Body       = source.toStdString();
					page.present(projected);
					page.show();
					page.activateWindow();
					settle();
					page.body_view()->show_source(true);
					settle();
					auto* edit = page.body_view()->source_view();
					auto* read = page.findChild<QToolButton*>(QStringLiteral("overviewFullView"));
					auto* back = page.findChild<QToolButton*>(QStringLiteral("overviewFullView"));
					expect(read->isVisible() && !read->isCheckable(), "overflow exposes the named reading action");
					expect(edit->viewport()->height() > 4 * edit->fontMetrics().lineSpacing(), "normal summary has a larger shared allocation with separate reader controls");
					expect(edit->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded && edit->verticalScrollBar()->maximum() > 0, "normal summary scrolls the complete body");
					QWheelEvent previewWheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, { }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
					QApplication::sendEvent(edit->viewport(), &previewWheel);
					expect(edit->verticalScrollBar()->value() > 0 && page.files_view()->verticalScrollBar()->value() == 0, "wheel scrolls only the summary");
					int dispatched = 0;
					page.set_file_dispatch([&](const QString&) { ++dispatched; });
					page.set_entry_dispatch([&](auto, const QString&) { ++dispatched; });
					const auto file = page.file_model()->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp"));
					page.files_view()->setCurrentIndex(file);
					read->setFocus();
					QKeyEvent press{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
					QKeyEvent release{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
					QApplication::sendEvent(read, &press);
					QApplication::sendEvent(read, &release);
					settle();
					expect(back->isVisible() && !back->isChecked() && back->accessibleName() == QStringLiteral("Exit full view") && !page.files_view()->isVisible() && edit->hasFocus(), "keyboard activation opens local reader and focuses text");
					expect(edit->maximumHeight() == QWIDGETSIZE_MAX && page.body_view()->height() > 650 && edit->viewport()->height() > page.height() * 2 / 3, "expanded reader fills available space without a height cap, including its source controls and fallback explanation");
					QKeyEvent pageDown{ QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier };
					QApplication::sendEvent(edit, &pageDown);
					expect(edit->verticalScrollBar()->value() > 0, "PageDown navigates the sole reader viewport");
					// The reclaimed height can let one PageDown reach the end.
					// Exercise wheel movement from a place that still has room.
					edit->verticalScrollBar()->setValue(0);
					const auto  beforeWheel = edit->verticalScrollBar()->value();
					QWheelEvent readerWheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, { }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
					QApplication::sendEvent(edit->viewport(), &readerWheel);
					expect(edit->verticalScrollBar()->value() > beforeWheel, "synthetic wheel input moves the reader viewport");
					edit->selectAll();
					edit->copy();
					expect(QApplication::clipboard()->text() == source, "complete Unicode and literal text copies without shortening");
					auto cursor = edit->textCursor();
					cursor.clearSelection();
					cursor.setPosition(static_cast<int>(source.size() / 2));
					edit->setTextCursor(cursor);
					edit->centerCursor();
					settle();
					const int anchor = edit->cursorForPosition(QPoint{ 0, 0 }).position();
					cursor.setPosition(anchor + 31);
					cursor.setPosition(anchor + 7, QTextCursor::KeepAnchor);
					edit->setTextCursor(cursor);
					const auto selection = edit->textCursor();
					auto*      document  = edit->document();
					const auto revision  = document->revision();
					projected.Title += " refreshed";
					projected.Progress.FilesRead = 2;
					projected.Files.front().Read = core::file_read_state::read;
					const auto retainedFile      = QPersistentModelIndex{ file };
					int        resets            = 0;
					QObject::connect(page.file_model(), &QAbstractItemModel::modelReset, &page, [&] { ++resets; });
					projected.Participants.front().Signoff = application::overview_signoff::commented;
					page.present(projected);
					settle();
					expect(edit->document() == document && document->revision() == revision && edit->hasFocus(), "surrounding refresh keeps document revision and focus");
					expect(edit->textCursor().position() == selection.position() && edit->textCursor().anchor() == selection.anchor(), "unchanged refresh keeps directed selection");
					expect_anchor(edit, anchor);
					expect(retainedFile.isValid() && resets == 0, "receipt refresh retains Files indexes without resetting the model");
					back->click();
					settle();
					expect(page.files_view()->hasFocus() && page.files_view()->currentIndex() == file && dispatched == 0, "Return to Files preserves selection and dispatches nothing");
					page.resize(width == 800 ? 1400 : 800, 800);
					font.setPointSize(points == 13 ? 20 : 13);
					page.setFont(font);
					settle();
					read->click();
					settle();
					expect_anchor(edit, anchor);
					expect(edit->textCursor().position() == selection.position() && edit->textCursor().anchor() == selection.anchor(), "reopen after reflow keeps directed selection");
					page.resize(width, 800);
					font.setPointSize(points);
					page.setFont(font);
					settle();
					expect_anchor(edit, anchor);
					QEvent dpr{ QEvent::DevicePixelRatioChange };
					QApplication::sendEvent(edit, &dpr);
					settle();
					expect_anchor(edit, anchor);
					QEvent style{ QEvent::StyleChange };
					QApplication::sendEvent(edit, &style);
					settle();
					expect_anchor(edit, anchor);
					QKeyEvent tab{ QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier };
					QApplication::sendEvent(edit, &tab);
					settle();
					expect(page.isAncestorOf(QApplication::focusWidget()) && !edit->hasFocus(), "Shift+Tab leaves source for reader controls");
					back->setFocus();
					QKeyEvent forwardTab{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
					QApplication::sendEvent(back, &forwardTab);
					settle();
					expect(edit->hasFocus(), "Tab returns from fixed control to the reader");
					QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
					QApplication::sendEvent(back, &escape);
					settle();
					expect(page.files_view()->isVisible() && page.files_view()->hasFocus() && dispatched == 0, "local Escape returns to Files without semantic dispatch");
					read->click();
					settle();
					cursor = edit->textCursor();
					cursor.movePosition(QTextCursor::End);
					edit->setTextCursor(cursor);
					edit->ensureCursorVisible();
					settle();
					expect(edit->cursorRect().bottom() < edit->viewport()->height(), "last character is reachable");
					const auto endAnchor = edit->cursorForPosition(QPoint{ 0, 0 }).position();
					page.resize(1400, 800);
					font.setPointSize(13);
					page.setFont(font);
					settle();
					expect_anchor(edit, endAnchor);
					page.resize(800, 300);
					settle();
					expect(back->isVisible() && page.height() == 300 && edit->height() > 100, "short-height page retains a fixed return and usable reader");
					projected.Body = "Short edited body.";
					page.present(projected);
					settle();
					expect(edit->textCursor().position() == 0 && edit->textCursor().anchor() == 0, "changed body discards stale reading endpoints");
					page.clear();
					settle();
					expect(edit->toPlainText().isEmpty() && !back->isVisible() && !read->isVisible(), "authority clear empties text and local reader state");
					projected.Body = source.toStdString();
					page.present(projected);
					settle();
					read->click();
					settle();
					expect(edit->textCursor().position() == 0 && edit->verticalScrollBar()->value() == 0, "replacement starts afresh even with the old body");
				}
			}
		}
	}

	void test_compact_descriptions()
	{
		qt::pull_request_overview_page page;
		page.resize(800, 800);
		page.show();
		auto projected = projection();
		projected.Body.clear();
		page.present(projected);
		settle();
		auto* read = page.findChild<QToolButton*>(QStringLiteral("overviewFullView"));
		expect(page.findChild<QLabel*>(QStringLiteral("overviewNoDescription"))->isVisible() && !read->isVisible() && !page.body_view()->isVisible(), "empty description has a compact explicit state");
		projected.Body = "Short 日本語 🦊 description.";
		page.present(projected);
		settle();
		expect(read->isVisible() && page.body_view()->rendered_view()->height() < 3 * page.body_view()->fontMetrics().lineSpacing(), "short body uses its natural height with a reachable full-reader action");
		page.body_view()->rendered_view()->selectAll();
		page.body_view()->copy_source();
		expect(QApplication::clipboard()->text().toStdString() == projected.Body, "short body remains selectable and copyable");
		const auto narrowColumns = page.body_view()->rendered_view()->viewport()->width() / page.body_view()->fontMetrics().horizontalAdvance(QLatin1Char{ 'w' });
		projected.Body           = std::string(static_cast<std::size_t>(5 * narrowColumns), 'w');
		page.present(projected);
		settle();
		expect(read->isVisible(), "a long unbroken token wraps and overflows a narrow preview");
		page.resize(1400, 800);
		settle();
		expect(read->isVisible(), "full-reader action remains available after widening");
		const auto sentinel = std::string{ "\n日本語 🦊 END" };
		projected.Body      = std::string(262144 - sentinel.size(), 'x') + sentinel;
		page.present(projected);
		settle();
		read->click();
		settle();
		page.body_view()->rendered_view()->selectAll();
		page.body_view()->copy_source();
		expect(QApplication::clipboard()->text().toStdString() == projected.Body, "complete admitted byte-boundary text remains copyable");
	}

	// The control sits inside the reader frame, flush with the visible
	// viewport's top-right corner, so it is left of any vertical scroll bar.
	[[nodiscard]] auto in_reader_corner(const qt::pull_request_overview_page& aPage, const QToolButton& aControl) -> bool
	{
		auto*      reader   = aPage.body_view();
		auto*      area     = reader->source_visible() ? static_cast<QAbstractScrollArea*>(reader->source_view()) : reader->rendered_view();
		const auto corner   = area->viewport()->mapTo(&aPage, QPoint{ area->viewport()->width(), 0 });
		const auto control  = QRect{ aControl.mapTo(&aPage, QPoint{ 0, 0 }), aControl.size() };
		const auto readerAt = QRect{ reader->mapTo(&aPage, QPoint{ 0, 0 }), reader->size() };
		return control.right() + 1 == corner.x() && control.top() == corner.y() && readerAt.contains(control);
	}

	void test_resizable_summary()
	{
		qt::pull_request_overview_page page;
		page.resize(1000, 800);
		auto projected = projection();
		projected.Body = description(60).toStdString();
		page.present(projected);
		page.show();
		settle();
		auto* split = page.findChild<QSplitter*>(QStringLiteral("overviewSummarySplitter"));
		expect(split && split->sizes().size() == 2, "normal summary and Files share the native splitter");
		auto* fullView = page.findChild<QToolButton*>(QStringLiteral("overviewFullView"));
		auto* reader   = page.body_view();
		expect(fullView && fullView->accessibleName() == QStringLiteral("Enter full view") && fullView->isVisible() && in_reader_corner(page, *fullView),
			   "full-view control rides the reader's top-right corner without a separate action row");
		expect(reader->mapTo(&page, reader->rect().topRight()).x() == page.files_view()->mapTo(&page, page.files_view()->rect().topRight()).x(),
			   "reader keeps the full content width: its right edge meets the Files right edge, with no toggle column");
		{
			auto* const document = reader->rendered_view()->document();
			const auto  first    = document->begin().blockFormat().rightMargin();
			const auto  last     = document->lastBlock().blockFormat().rightMargin();
			expect(first >= fullView->width() && last < fullView->width(),
				   "only the lines beside the corner wrap short of it; later lines keep the full width");
		}
		fullView->click();
		settle();
		expect(fullView->isVisible() && fullView->accessibleName() == QStringLiteral("Exit full view") && in_reader_corner(page, *fullView),
			   "Exit full view stays at the reader's top-right corner in full view");
		fullView->click();
		settle();
		expect(fullView->accessibleName() == QStringLiteral("Enter full view") && in_reader_corner(page, *fullView) && page.files_view()->isVisible(),
			   "returning from full view puts the control back in the same corner");
		expect(!reader->findChild<QPushButton*>(QStringLiteral("markdownSourceToggle"))->isVisible()
				   && !reader->findChild<QPushButton*>(QStringLiteral("markdownCopySource"))->isVisible()
				   && reader->rendered_view()->height() == reader->height(),
			   "Overview gives source-action and destination-row space back to the reader");
		auto* grip = split->handle(1);
		expect(grip->isVisible() && grip->focusPolicy() == Qt::StrongFocus && grip->cursor().shape() == Qt::SplitVCursor, "grip is visible, focusable and has vertical resize cursor");
		expect(std::abs(split->sizes()[0] - split->sizes()[1]) <= 2, "long summary starts at half the shared room");
		int gestures = 0;
		page.set_summary_resize_dispatch([&](auto) { ++gestures; });
		grip->setFocus();
		const auto before = split->sizes()[0];
		QKeyEvent  down{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QApplication::sendEvent(grip, &down);
		settle();
		expect(split->sizes()[0] == before + page.body_view()->rendered_view()->fontMetrics().lineSpacing() && grip->hasFocus() && gestures == 1, "Down grows summary by one line without moving focus");
		auto* accessible = QAccessible::queryAccessibleInterface(grip);
		expect(accessible && accessible->text(QAccessible::Name) == QStringLiteral("Resize summary") && accessible->valueInterface() && accessible->valueInterface()->currentValue().toInt() == split->sizes()[0], "accessible resize grip reports current allocation");
		const auto chosen = page.summary_fraction();
		page.resize(1000, 300);
		settle();
		expect(page.height() == 300 && split->sizes()[0] > 0 && split->sizes()[1] > 0 && page.summary_fraction() == chosen, "short-height clamp keeps both panes and preferred fraction");
		page.resize(1000, 800);
		settle();
		expect(std::abs(split->sizes()[0] - before - page.body_view()->rendered_view()->fontMetrics().lineSpacing()) <= 2 && gestures == 1, "enlargement restores intent without another persistence gesture");
		const auto  local = QPointF{ grip->rect().center() };
		QMouseEvent press{ QEvent::MouseButtonPress, local, QPointF{ grip->mapToGlobal(local.toPoint()) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(grip, &press);
		QMouseEvent move{ QEvent::MouseMove, local + QPointF{ 0, 40 }, QPointF{ grip->mapToGlobal(local.toPoint()) + QPoint{ 0, 40 } }, Qt::NoButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(grip, &move);
		QMouseEvent release{ QEvent::MouseButtonRelease, local, QPointF{ grip->mapToGlobal(local.toPoint()) }, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(grip, &release);
		settle();
		expect(gestures > 1 && page.summary_fraction() != chosen, "pointer drag records a changed preferred fraction");
		projected.Body.clear();
		const auto retained = page.summary_fraction();
		page.present(projected);
		settle();
		expect(!grip->isVisible() && page.summary_fraction() == retained, "empty body hides grip without erasing preference");
		projected.Body = "Short description.";
		page.present(projected);
		settle();
		expect(grip->isVisible() && split->sizes()[0] > 150, "explicit preference gives a short body the chosen room");
		QMouseEvent doubleClick{ QEvent::MouseButtonDblClick, local, QPointF{ grip->mapToGlobal(local.toPoint()) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(grip, &doubleClick);
		settle();
		expect(!page.summary_fraction() && split->sizes()[0] < 150, "double-click resets to content-aware short sizing");
	}

	/// \brief [[BPR-1330]]. The owner, 2026-10-08: "The resize handle mouse
	/// cursor on the overview page below summary seems to flicker to the
	/// correct cursor and then return to the normal pointer like the drawer
	/// resize handle used to". The summary seam keeps its vertical-resize
	/// cursor while the pointer moves over it or rests under its tooltip.
	void test_summary_seam_holds_its_cursor()
	{
		qt::pull_request_overview_page page;
		page.resize(1000, 800);
		auto projected = projection();
		projected.Body = description(60).toStdString();
		page.present(projected);
		page.show();
		settle();
		auto* split = page.findChild<QSplitter*>(QStringLiteral("overviewSummarySplitter"));
		expect(split != nullptr && split->count() == 2 && split->handle(1)->isVisible(), "the summary seam is shown for a long description");
		expect(tests::resize_cursor_holds(*split->handle(1), Qt::SplitVCursor, "summary seam"),
			   "the summary seam shows the vertical-resize cursor on arrival, after every move over it and under its tooltip, and stops once the pointer leaves");
		page.hide();
	}

	void test_body_edits_clamp_retained_selection()
	{
		qt::pull_request_overview_page page;
		page.resize(800, 800);
		auto       projected = projection();
		const auto source    = description(100);
		projected.Body       = source.toStdString();
		page.present(projected);
		page.show();
		settle();
		auto* read = page.findChild<QToolButton*>(QStringLiteral("overviewFullView"));
		auto* back = page.findChild<QToolButton*>(QStringLiteral("overviewFullView"));
		page.body_view()->show_source(true);
		settle();
		auto* edit = page.body_view()->source_view();
		read->click();
		settle();
		auto selection = QTextCursor{ edit->document() };
		selection.setPosition(300);
		selection.setPosition(100, QTextCursor::KeepAnchor);
		edit->setTextCursor(selection);
		projected.Body = source.left(200).toStdString();
		page.present(projected);
		settle();
		expect(edit->textCursor().anchor() == 0 && edit->textCursor().position() == 0, "changed body clears the old selection instead of attaching it to new text");
		back->click();
		settle();
		projected.Body.clear();
		page.present(projected);
		settle();
		projected.Body = source.toStdString();
		page.present(projected);
		settle();
		read->click();
		settle();
		expect(edit->textCursor().position() == 0 && edit->textCursor().anchor() == 0 && edit->verticalScrollBar()->value() == 0, "edits clamp retained state even while the reader is collapsed");
		projected.Body.clear();
		page.present(projected);
		settle();
		expect(!back->isVisible() && page.files_view()->isVisible() && page.findChild<QLabel*>(QStringLiteral("overviewNoDescription"))->isVisible(), "removing an expanded body returns to compact empty description");
	}
	auto test_many_participants_scroll_without_compressing_or_overpainting() -> void
	{
		qt::pull_request_overview_page page;
		page.resize(900, 520);
		auto projected = projection();
		projected.Participants.clear();
		for (int index = 0; index < 64; ++index) {
			projected.Participants.push_back({ .Login   = "a-long-unbroken-participant-identifier-" + std::to_string(index),
											   .Role    = application::overview_participant_role::reviewer,
											   .Signoff = application::overview_signoff::changes_requested });
		}
		page.present(projected);
		page.show();
		settle();
		auto* scroll = page.findChild<QScrollArea*>(QStringLiteral("overviewSideScroll"));
		expect(scroll != nullptr && scroll->verticalScrollBar()->maximum() > 0,
			   "a large participant list has a real scroll range instead of compressing rows");
		const auto verifyRows = [&] {
			const auto rows = page.participant_rows();
			expect(rows.size() == 64, "all participant identities remain available");
			for (std::size_t index = 0; index < rows.size(); ++index) {
				const auto* row = rows[index];
				expect(row->height() >= row->heightForWidth(row->width())
						   && row->height() == std::max(28, row->fontMetrics().height() + 10)
						   && row->toolTip().contains(row->login())
						   && (index == 0 || rows[index - 1]->geometry().bottom() < row->geometry().top()),
					   "participants occupy one measured line without overlap and preserve full identity in the tooltip");
			}
			scroll->ensureWidgetVisible(rows.back());
			settle();
			const auto last = rows.back()->mapTo(scroll->viewport(), QPoint{ 0, 0 });
			expect(last.y() >= 0 && last.y() + rows.back()->height() <= scroll->viewport()->height(),
				   "the final participant is reachable in the same bounded sidebar");
		};
		verifyRows();
		auto larger = page.font();
		larger.setPointSizeF(larger.pointSizeF() + 4);
		page.setFont(larger);
		settle();
		verifyRows();
		page.resize(700, 400);
		settle();
		verifyRows();
		scroll->ensureWidgetVisible(page.entry_button());
		settle();
		expect(scroll->viewport()->rect().contains(page.entry_button()->mapTo(scroll->viewport(), QPoint{ 1, 1 })),
			   "review entry remains reachable after the complete participant list");
	}
	class observed_overview_delegate final : public QStyledItemDelegate
	{
	public:
		observed_overview_delegate(QAbstractItemDelegate& aDelegate, QObject* aParent)
			: QStyledItemDelegate{ aParent }
			, Delegate{ aDelegate }
		{ }
		auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
		{
			const auto path = aIndex.data(qt::overview_file_model::path_role).toString();
			if (!path.isEmpty()) {
				Paths.insert(path);
			}
			return Delegate.sizeHint(aOption, aIndex);
		}
		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
		{
			Delegate.paint(aPainter, aOption, aIndex);
		}
		mutable std::set<QString> Paths;

	private:
		QAbstractItemDelegate& Delegate;
	};

	void test_column_sizing_measures_visible_files()
	{
		qt::pull_request_overview_page page;
		page.resize(1200, 800);
		auto projected = projection();
		projected.Files.clear();
		for (auto index = 0; index < 1000; ++index) {
			projected.Files.push_back(row("src/file_" + std::to_string(index) + ".cpp",
										  application::overview_file_operation::modified,
										  application::overview_change_since_review::unchanged, std::nullopt));
		}
		projected.Progress.FilesTotal = projected.Files.size();
		auto* const view              = page.files_view();
		auto* const delegate          = new observed_overview_delegate{ *view->itemDelegate(), view };
		view->setItemDelegate(delegate);
		page.present(projected);
		page.show();
		settle();
		expect(!delegate->Paths.empty() && delegate->Paths.size() < 100U,
			   "initial Overview sizes columns from visible files, not the complete thousand-file table");
		expect(page.file_model()->file_count() == 1000U, "column sampling retains every exact file row");
		const auto last = page.file_model()->index_for_path(QStringLiteral("src/file_999.cpp"));
		view->scrollTo(last, QAbstractItemView::PositionAtBottom);
		delegate->Paths.clear();
		view->header()->resizeSections(QHeaderView::ResizeToContents);
		settle();
		expect(view->viewport()->rect().intersects(view->visualRect(last))
				   && delegate->Paths.contains(QStringLiteral("src/file_999.cpp"))
				   && delegate->Paths.size() < 100U,
			   "scrolling sizes the newly visible last file without measuring the whole review");
	}

	void test_cached_catalogue_availability_and_navigation()
	{
		qt::pull_request_overview_page page;
		page.resize(1200, 800);
		page.show();
		bool opened = false;
		page.set_file_dispatch([&](const QString&) { opened = true; });
		// The nested projection names its type for GCC 15's nested-aggregate
		// miscompile ([[BPR-0816]], [[BPR-1262]]).
		application::github_cached_overview_projection cached{ .Overview = application::github_loading_overview_projection{ .Title = "Historical review" }, .BaseOid = std::string(40, '1'), .HeadOid = std::string(40, '2') };
		page.present_cached(cached);
		auto* heading = page.findChild<QLabel*>(QStringLiteral("overviewFilesHeading"));
		expect(heading->text().contains(QStringLiteral("catalogue unavailable")), "missing catalogue is explicitly unavailable");
		auto* threadsHeading = page.findChild<QLabel*>(QStringLiteral("overviewCachedThreadsHeading"));
		auto* threads        = page.findChild<QPlainTextEdit*>(QStringLiteral("overviewCachedThreads"));
		expect(threadsHeading->text().contains(QStringLiteral("unavailable")) && threads->isHidden(), "unobserved thread collection never implies empty");
		cached.ReviewThreads.emplace();
		page.present_cached(cached);
		expect(threadsHeading->text().contains(QStringLiteral("0 observed")), "observed empty thread collection is explicit");
		cached.ReviewThreads->push_back({ .Id = "T1", .Path = "src/new.cpp", .Line = 7, .OriginalLine = 5, .Side = core::cached_thread_side::right, .Resolved = true, .Outdated = true, .Comments = { { .Id = "C1", .AuthorLogin = "author", .Body = "<b>literal root</b>" }, { .Id = "C2", .ReplyTo = "C1", .AuthorLogin = "reader", .Body = "literal reply" } } });
		cached.FileCatalogue.emplace();
		page.present_cached(cached);
		expect(heading->text().contains(QStringLiteral("0 files observed")), "empty observed catalogue is different from unavailable");
		cached.FileCatalogue->push_back({ "src/new.cpp", "src/old.cpp", core::cached_file_change::renamed, 12, 3 });
		page.present_cached(cached);
		settle();
		expect(threads->isVisible() && threads->isReadOnly() && threads->toPlainText().contains(QStringLiteral("<b>literal root</b>"))
				   && threads->toPlainText().contains(QStringLiteral("Reply · reader")) && threads->toPlainText().contains(QStringLiteral("was resolved · was outdated"))
				   && threads->toPlainText().contains(QStringLiteral("line 7 · original line 5")) && threadsHeading->text().contains(QStringLiteral("freshness unknown")),
			   "historical thread bodies, replies and inert anchors are literal and explicitly stale");
		const auto index = page.file_model()->index_for_path(QStringLiteral("src/new.cpp"));
		expect(index.isValid() && index.data(Qt::ToolTipRole).toString().contains(QStringLiteral("src/old.cpp")), "cached rename preserves both exact paths");
		expect(page.file_model()->row_at(index)->Operation == application::overview_file_operation::renamed, "cached operation preserved");
		for (int column = qt::overview_file_model::changed_column; column < qt::overview_file_model::column_count; ++column) {
			expect(page.files_view()->isColumnHidden(column), "unknown cached workflow counts are hidden");
		}
		QMetaObject::invokeMethod(page.files_view(), "activated", Qt::DirectConnection, Q_ARG(QModelIndex, index));
		expect(!opened && !page.presented() && !page.entry_button()->isEnabled(), "historical rows cannot enter the source workbench");
		auto* discussion = page.findChild<QPlainTextEdit*>(QStringLiteral("overviewCachedDiscussion"));
		expect(discussion && discussion->toPlainText().contains(QStringLiteral("General comments — unavailable")), "missing discussion remains explicitly unavailable");
		cached.GeneralComments.emplace();
		cached.ReviewSummaries.emplace();
		page.present_cached(cached);
		expect(discussion->toPlainText().contains(QStringLiteral("General comments — 0 observed")) && discussion->toPlainText().contains(QStringLiteral("Review summaries — 0 observed")), "observed empty general comments and reviews are distinct");
		bool historicalOpened = false;
		page.set_cached_source_dispatch([&](const QString& aPath) { historicalOpened = aPath == QStringLiteral("src/new.cpp"); });
		QMetaObject::invokeMethod(page.files_view(), "activated", Qt::DirectConnection, Q_ARG(QModelIndex, page.file_model()->index_for_path(QStringLiteral("src/new.cpp"))));
		expect(historicalOpened && !opened, "cached file activation uses historical ingress only");
		application::cached_review_source_projection source{ .Path = "src/new.cpp", .BaseOid = cached.BaseOid, .HeadOid = cached.HeadOid, .Status = application::cached_source_status::available, .BaseText = std::string{ }, .HeadText = "<literal> source" };
		page.present_cached_source(source);
		auto* sourceText = page.findChild<QPlainTextEdit*>(QStringLiteral("overviewCachedSource"));
		expect(sourceText && sourceText->isVisible() && sourceText->isReadOnly() && sourceText->toPlainText().contains(QStringLiteral("<literal> source")) && sourceText->toPlainText().contains(QStringLiteral("BASE — observed source")), "historical exact source and empty blob are literal text");
		auto foreign     = source;
		foreign.HeadOid  = std::string(40, '9');
		foreign.HeadText = "FOREIGN";
		page.present_cached_source(foreign);
		expect(!sourceText->toPlainText().contains(QStringLiteral("FOREIGN")), "foreign revision source cannot replace historical reader");
		source.HeadText = std::string(1, static_cast<char>(0xff));
		page.present_cached_source(source);
		expect(sourceText->toPlainText().contains(QStringLiteral("encoding cannot be displayed")), "invalid UTF-8 source is explicit instead of replacement-character content");
		page.present(projection());
		expect(threads->isHidden() && threads->toPlainText().isEmpty() && threadsHeading->isHidden(), "fresh projection removes cached thread content and labels");
		expect(sourceText->isHidden() && sourceText->toPlainText().isEmpty() && discussion->isHidden() && discussion->toPlainText().isEmpty(), "fresh projection erases historical source and discussion");
		for (int column = qt::overview_file_model::changed_column; column < qt::overview_file_model::column_count; ++column) {
			expect(!page.files_view()->isColumnHidden(column), "fresh server view restores current workflow columns");
		}
		expect(page.findChild<QLabel*>(QStringLiteral("overviewSummary"))->toolTip().isEmpty() && heading->toolTip().isEmpty(), "reconnect clears stale revision descriptions");
		const auto fresh = page.file_model()->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp"));
		QMetaObject::invokeMethod(page.files_view(), "activated", Qt::DirectConnection, Q_ARG(QModelIndex, fresh));
		expect(opened, "fresh server catalogue enables normal source navigation");
	}

	void test_history_publication_does_not_touch_file_rows()
	{
		auto page     = qt::pull_request_overview_page{ };
		auto expected = projection();
		page.present(expected);
		auto  fileSignals = 0U;
		auto* model       = page.file_model();
		QObject::connect(model, &QAbstractItemModel::modelReset, [&] { ++fileSignals; });
		QObject::connect(model, &QAbstractItemModel::dataChanged, [&] { ++fileSignals; });
		QObject::connect(model, &QAbstractItemModel::rowsInserted, [&] { ++fileSignals; });
		QObject::connect(model, &QAbstractItemModel::rowsRemoved, [&] { ++fileSignals; });
		QObject::connect(model, &QAbstractItemModel::rowsMoved, [&] { ++fileSignals; });
		for (const auto history : { application::overview_history_update{ .Known = true, .ReviewedAt = core::observed_utc_milliseconds{ 1'780'000'000'000 }, .Uploads = 7U }, application::overview_history_update{ .Known = true }, application::overview_history_update{ } }) {
			page.apply_history(history);
			expected.SinceLastReview.Known      = history.Known;
			expected.SinceLastReview.ReviewedAt = history.ReviewedAt;
			expected.SinceLastReview.Uploads    = history.Uploads;
			auto complete                       = qt::pull_request_overview_page{ };
			complete.present(expected);
			expect(page.projection() == expected && page.since_heading()->text() == complete.since_heading()->text()
					   && page.value_label(qt::overview_value::uploads)->text() == complete.value_label(qt::overview_value::uploads)->text(),
				   "history-only presentation equals full header and preserves other retained facts");
		}
		expect(fileSignals == 0U, "epoch history changes touch no file rows or models");
	}

} // namespace

namespace {
	// ---- [[BPR-1089]]: label chips, the count switch and the drill-down ----

	struct unit_spec
	{
		std::string                         Id;
		std::optional<core::feedback_label> Label;
		core::feedback_resolution           State = core::feedback_resolution::unresolved;
		std::string                         Path;
		/// A root whose label is not known.
		bool Unreadable = false;
	};

	[[nodiscard]] auto feedback_key() -> core::cached_review_key
	{
		auto key         = core::cached_review_key{ };
		key.Authority[0] = 1U;
		key.Profile      = core::host_profile_id{ 1U };
		key.Host         = core::git_host_id{ 1U };
		key.Account      = core::github_account_id{ 1U };
		key.Locator      = { "EpicGames", "UnrealEngine" };
		key.Number       = core::pull_request_number{ 14366U };
		return key;
	}

	[[nodiscard]] auto population(const std::vector<unit_spec>& aUnits) -> core::feedback_population
	{
		auto set   = core::feedback_annotation_set{ };
		set.Scope  = feedback_key();
		set.Epoch  = 1U;
		auto facts = std::vector<core::feedback_conversation_facts>{ };
		for (const auto& unit : aUnits) {
			auto conversation   = core::feedback_conversation{ };
			conversation.Id     = unit.Id;
			conversation.Root   = "r";
			conversation.Status = unit.Unreadable ? core::feedback_root_status::unclassified : unit.Label ? core::feedback_root_status::labelled
																										  : core::feedback_root_status::plain;
			conversation.Label  = unit.Unreadable ? std::nullopt : unit.Label;
			set.Conversations.push_back(std::move(conversation));
			auto fact       = core::feedback_conversation_facts{ };
			fact.Id         = unit.Id;
			fact.Resolution = unit.State;
			if (!unit.Path.empty()) {
				fact.Anchor.Status = core::feedback_anchor_status::file;
				fact.Anchor.Path   = unit.Path;
			}
			facts.push_back(std::move(fact));
		}
		std::ranges::sort(set.Conversations, { }, &core::feedback_conversation::Id);
		auto built = core::build_feedback_population(set, facts, core::feedback_collection::complete);
		expect(built.Refusal == core::feedback_population_refusal::none, "a test population is admitted");
		return std::move(built.Population);
	}

	/// The host's answer for one request, made with the core's own tallies.
	[[nodiscard]] auto feedback_for(const core::feedback_population& aPopulation, const application::overview_feedback_request& aRequest,
									const std::vector<std::string>& aFiles) -> application::overview_feedback_projection
	{
		const auto columns = core::labels_of(aRequest.Columns);
		const auto counts  = [&](core::feedback_counter_scope aWhere) {
			return core::project_feedback_counters(core::tally_feedback_counters(aPopulation, aWhere), columns, aRequest.Mode);
		};
		auto result    = application::overview_feedback_projection{ };
		result.Request = aRequest;
		auto folders   = std::set<std::string>{ };
		for (const auto& file : aFiles) {
			auto row   = application::overview_feedback_row{ };
			row.Kind   = application::overview_feedback_row_kind::file;
			row.Path   = file;
			row.Counts = counts({ .Kind = core::feedback_counter_scope_kind::file, .Path = file });
			result.Files.push_back(std::move(row));
			const auto slash = file.rfind('/');
			folders.insert(slash == std::string::npos ? std::string{ } : file.substr(0U, slash));
		}
		for (const auto& folder : folders) {
			auto row   = application::overview_feedback_row{ };
			row.Kind   = application::overview_feedback_row_kind::folder;
			row.Path   = folder;
			row.Counts = counts({ .Kind = core::feedback_counter_scope_kind::directory, .Path = folder });
			result.Folders.push_back(std::move(row));
		}
		result.Remainder.Kind     = application::overview_feedback_row_kind::remainder;
		result.Remainder.Counts   = counts({ .Kind = core::feedback_counter_scope_kind::unattributed });
		result.PullRequest.Kind   = application::overview_feedback_row_kind::pull_request;
		result.PullRequest.Counts = counts({ });
		return result;
	}

	const auto lighting  = std::string{ "Engine/Renderer/HairStrandsLighting.cpp" };
	const auto voxel     = std::string{ "Engine/Renderer/HairStrandsVoxelization.cpp" };
	const auto deep      = std::string{ "Engine/Shaders/HairStrandsDeepShadow.usf" };
	const auto all_files = std::vector<std::string>{ lighting, voxel, deep };

	[[nodiscard]] auto mixed_review() -> core::feedback_population
	{
		using core::feedback_label;
		using core::feedback_resolution;
		return population({
			{ .Id = "review-thread:a", .Label = feedback_label::issue, .State = feedback_resolution::unresolved, .Path = lighting },
			{ .Id = "review-thread:b", .Label = feedback_label::issue, .State = feedback_resolution::resolved, .Path = lighting },
			{ .Id = "review-thread:c", .Label = feedback_label::question, .State = feedback_resolution::unresolved, .Path = lighting },
			{ .Id = "review-thread:d", .Label = std::nullopt, .State = feedback_resolution::unresolved, .Path = lighting },
			{ .Id = "review-thread:e", .Label = feedback_label::nitpick, .State = feedback_resolution::unresolved, .Path = voxel },
			{ .Id = "review-thread:f", .Label = feedback_label::issue, .State = feedback_resolution::unknown, .Path = voxel },
			{ .Id = "review-thread:g", .Label = feedback_label::praise, .State = feedback_resolution::unresolved, .Path = { } },
			{ .Id = "issue-comment:h", .Label = feedback_label::note, .State = feedback_resolution::not_applicable, .Path = { } },
		});
	}

	[[nodiscard]] auto request_for(std::vector<core::feedback_label> aColumns, core::feedback_resolution_mode aMode) -> application::overview_feedback_request
	{
		auto request    = application::overview_feedback_request{ };
		request.Columns = *core::make_feedback_label_columns(aColumns);
		request.Mode    = aMode;
		return request;
	}

	[[nodiscard]] auto cell(qt::pull_request_overview_page& aPage, const std::string& aPath) -> QModelIndex
	{
		return aPage.file_model()->index_for_path(QString::fromStdString(aPath)).siblingAtColumn(qt::overview_file_model::comments_column);
	}

	/// The selected design draws a chip's count as its digits beside the
	/// mark: in Open, in the open amber, with no fill behind them, on file and
	/// folder rows alike. True when the count's box holds that ink, but less
	/// of it than a filled pill would.
	[[nodiscard]] auto count_is_amber_text(qt::pull_request_overview_page& aPage, const qt::overview_comment_chip& aChip) -> bool
	{
		auto* const viewport = aPage.files_view()->viewport();
		const auto  image    = viewport->grab().toImage();
		const auto  scale    = image.devicePixelRatio();
		const auto  open     = aPage.colours().States.Open;
		const auto  amberish = [&](const QColor aColour) {
			return std::abs(aColour.red() - open.red()) + std::abs(aColour.green() - open.green()) + std::abs(aColour.blue() - open.blue()) < 60;
		};
		const auto count  = QRectF{ aChip.Box.left() + 16.0 + 3.0, aChip.Box.top(), aChip.Box.width() - 19.0, aChip.Box.height() };
		auto       amber  = 0;
		auto       pixels = 0;
		for (auto y = count.top(); y < count.bottom(); y += 0.5) {
			for (auto x = count.left(); x < count.right(); x += 0.5) {
				amber += amberish(image.pixelColor(QPoint{ static_cast<int>(x * scale), static_cast<int>(y * scale) })) ? 1 : 0;
				++pixels;
			}
		}
		// Digits ink a small share of their box; a filled pill most of it.
		return amber > 0 && amber * 3 < pixels;
	}

	[[nodiscard]] auto chips_of(qt::pull_request_overview_page& aPage, const QModelIndex& aCell) -> std::vector<qt::overview_comment_chip>
	{
		const auto* counts = aPage.file_model()->feedback_at(aCell);
		expect(counts != nullptr, "the cell has counts");
		return qt::overview_comment_chips(*counts, aPage.files_view()->visualRect(aCell), aPage.files_view()->font());
	}

	void click_at(QWidget& aWidget, const QPointF aPoint)
	{
		QMouseEvent press{ QEvent::MouseButtonPress, aPoint, aWidget.mapToGlobal(aPoint), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QMouseEvent release{ QEvent::MouseButtonRelease, aPoint, aWidget.mapToGlobal(aPoint), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		settle();
	}

	/// [[BPR-1089]]. A modified file's operation mark reads in light and dark
	/// themes, and a selected row carries no dark square (owner, rejecting
	/// overview-keyboard-v001: "This black square is weird looking on a
	/// highlighted row"). The mark was Mid filled with a Base letter: in the
	/// dark theme #131519 under #0D1117, about 1.05:1.
	void test_modified_mark_reads_on_every_row()
	{
		const auto contrast = [](const QColor& aOne, const QColor& aTwo) {
			const auto channel = [](const double aValue) { return aValue <= 0.03928 ? aValue / 12.92 : std::pow((aValue + 0.055) / 1.055, 2.4); };
			const auto lum     = [&](const QColor& aColour) { return 0.2126 * channel(aColour.redF()) + 0.7152 * channel(aColour.greenF()) + 0.0722 * channel(aColour.blueF()); };
			const auto one     = lum(aOne);
			const auto two     = lum(aTwo);
			return (std::max(one, two) + 0.05) / (std::min(one, two) + 0.05);
		};
		const auto luminance = [](const QColor& aColour) { return 0.2126 * aColour.redF() + 0.7152 * aColour.greenF() + 0.0722 * aColour.blueF(); };
		const auto distance  = [](const QColor& aOne, const QColor& aTwo) {
			return std::abs(aOne.red() - aTwo.red()) + std::abs(aOne.green() - aTwo.green()) + std::abs(aOne.blue() - aTwo.blue());
		};
		struct theme
		{
			const char* Name;
			QColor      Base, Text, Window, Mid, Highlight, HighlightedText;
		};
		for (const auto& look : { theme{ "dark", QColor{ 0x0D1117 }, QColor{ 0xE6EDF3 }, QColor{ 0x1C2026 }, QColor{ 0x131519 }, QColor{ 0x28518C }, QColor{ Qt::white } },
								  theme{ "light", QColor{ Qt::white }, QColor{ 0x1F2328 }, QColor{ 0xF6F7F9 }, QColor{ 0xA0A0A0 }, QColor{ 0x0969DA }, QColor{ Qt::white } },
								  theme{ "high contrast", QColor{ Qt::white }, QColor{ Qt::black }, QColor{ Qt::white }, QColor{ 0x808080 }, QColor{ Qt::black }, QColor{ Qt::white } } }) {
			const auto                     name = std::string{ look.Name };
			qt::pull_request_overview_page page;
			auto                           palette = page.palette();
			palette.setColor(QPalette::Base, look.Base);
			palette.setColor(QPalette::Text, look.Text);
			palette.setColor(QPalette::Window, look.Window);
			palette.setColor(QPalette::Mid, look.Mid);
			palette.setColor(QPalette::Highlight, look.Highlight);
			palette.setColor(QPalette::HighlightedText, look.HighlightedText);
			page.setPalette(palette);
			page.resize(1'300, 760);
			page.present(projection());
			page.show();
			settle();
			auto* const view     = page.files_view();
			const auto  model    = page.file_model();
			const auto  voxelRow = model->index_for_path(QString::fromStdString(voxel));
			const auto  lightRow = model->index_for_path(QString::fromStdString(lighting));
			view->setCurrentIndex(voxelRow);
			settle();
			const auto plain = qt::overview_operation_mark_colours(application::overview_file_operation::modified, view->palette(), page.colours(), false);
			expect(contrast(plain.Letter, plain.Fill) >= 4.5, name + ": a modified mark's letter reads on its tile, at least 4.5:1");
			expect(contrast(plain.Outline, look.Base) >= 1.4, name + ": its outline shows on the row's ground");
			const auto chosen = qt::overview_operation_mark_colours(application::overview_file_operation::modified, view->palette(), page.colours(), true);
			expect(!chosen.Fill.isValid() && contrast(chosen.Letter, look.Highlight) >= 4.5, name + ": on a selected row it is the selection's own text, with no tile");

			const auto image = view->viewport()->grab().toImage();
			const auto scale = image.devicePixelRatio();
			const auto boxOf = [&](const QModelIndex& aRow) {
				const auto rect = view->visualRect(aRow.siblingAtColumn(qt::overview_file_model::operation_column));
				return QRectF{ rect.left() + 6.0, rect.center().y() - 7.0, 14.0, 14.0 };
			};
			auto dark   = 0;
			auto letter = 0;
			auto tile   = 0;
			for (const auto& [rowIndex, selected] : { std::pair{ voxelRow, true }, std::pair{ lightRow, false } }) {
				const auto box = boxOf(rowIndex);
				// Half-pixel steps, counted in whole numbers.
				for (auto row = 0; row < static_cast<int>(box.height() * 2.0); ++row) {
					for (auto column = 0; column < static_cast<int>(box.width() * 2.0); ++column) {
						const auto x      = box.left() + column * 0.5;
						const auto y      = box.top() + row * 0.5;
						const auto colour = image.pixelColor(QPoint{ static_cast<int>(x * scale), static_cast<int>(y * scale) });
						if (selected) {
							dark += luminance(colour) < luminance(look.Highlight) * 0.6 ? 1 : 0;
							letter += distance(colour, chosen.Letter) < 40 ? 1 : 0;
						} else {
							tile += distance(colour, plain.Fill) < 12 ? 1 : 0;
						}
					}
				}
			}
			expect(dark == 0, name + ": no dark square on the selected row's modified mark");
			expect(letter > 8, name + ": the selected row's M is drawn in the selection's text colour");
			expect(tile > 100, name + ": an unselected modified mark is drawn on its quiet tile");
			page.hide();
		}
	}

	/// Pixels of the selection's text colour in a band just outside a chip,
	/// where its keyboard ring is drawn and nothing else is.
	[[nodiscard]] auto ring_pixels(qt::pull_request_overview_page& aPage, const qt::overview_comment_chip& aChip) -> int
	{
		auto* const viewport = aPage.files_view()->viewport();
		const auto  image    = viewport->grab().toImage();
		const auto  scale    = image.devicePixelRatio();
		const auto  ring     = aPage.files_view()->palette().color(QPalette::Active, QPalette::HighlightedText);
		const auto  ground   = aPage.files_view()->palette().color(QPalette::Active, QPalette::Highlight);
		const auto  ringish  = [&](const QColor aColour) {
			const auto distance = [](const QColor aOne, const QColor aTwo) {
				return std::abs(aOne.red() - aTwo.red()) + std::abs(aOne.green() - aTwo.green()) + std::abs(aOne.blue() - aTwo.blue());
			};
			return distance(aColour, ring) * 2 < distance(aColour, ground);
		};
		// The band stays inside the row: a 16 px chip in a 22 px row.
		const auto outer = aChip.Box.adjusted(-4.0, -2.5, 4.0, 2.5);
		const auto inner = aChip.Box.adjusted(-1.5, -1.0, 1.5, 1.0);
		auto       count = 0;
		for (auto row = 0; row < static_cast<int>(outer.height() * 2.0); ++row) {
			for (auto column = 0; column < static_cast<int>(outer.width() * 2.0); ++column) {
				const auto x = outer.left() + column * 0.5;
				const auto y = outer.top() + row * 0.5;
				if (!inner.contains(QPointF{ x, y }) && ringish(image.pixelColor(QPoint{ static_cast<int>(x * scale), static_cast<int>(y * scale) }))) {
					++count;
				}
			}
		}
		return count;
	}

	/// No chips, a typical subset and all twelve each show a disjoint
	/// breakdown with Other once; All shows the breakdown; unknown is never a
	/// clean zero; the ? chip, the remainder row and the header follow; the
	/// description's geometry does not move.
	void test_label_chips_show_the_selected_breakdown()
	{
		qt::pull_request_overview_page page;
		page.resize(1'300, 760);
		auto asked = std::vector<application::overview_feedback_request>{ };
		page.set_feedback_dispatch([&](const application::overview_feedback_request& aRequest) { asked.push_back(aRequest); });
		page.present(projection());
		page.show();
		settle();
		const auto bodyBefore = page.body_view()->geometry();
		expect(asked.size() == 1U && asked.back().Columns.Count == 0U && asked.back().Mode == core::feedback_resolution_mode::unresolved && !asked.back().Sort,
			   "presenting a pull request asks once for no chips, Open, in file order");
		expect(!page.open_counts_button()->isVisible() && !page.uncertain_button()->isVisible(), "no switch before the host answers");
		auto* const model  = page.file_model();
		auto        resets = 0;
		QObject::connect(model, &QAbstractItemModel::modelReset, [&] { ++resets; });
		const auto review = mixed_review();

		// No chips: the speech bubble counts every open conversation.
		page.present_feedback(feedback_for(review, asked.back(), all_files));
		settle();
		expect(page.open_counts_button()->isVisible() && page.open_counts_button()->isChecked() && page.all_counts_button()->isVisible()
				   && page.label_key_button()->isVisible() && !page.open_counts_button()->icon().isNull(),
			   "the answer shows Open, with its round open mark, and All, and the Label key");
		auto chips = chips_of(page, cell(page, lighting));
		expect(chips.size() == 1U && chips[0].Kind == qt::overview_comment_chip::kind::other && chips[0].Count == 3U,
			   "no chips: one bubble counting the three open conversations on HairStrandsLighting.cpp");
		expect(cell(page, lighting).data().toString() == QStringLiteral("Comments 3"), "the cell's text names the count");
		expect(page.files_view()->columnWidth(qt::overview_file_model::comments_column) == qt::overview_comments_width, "the Comments column has its fixed width");
		expect(model->headerData(qt::overview_file_model::comments_column, Qt::Horizontal, Qt::DisplayRole).toString() == QStringLiteral("Comments ▾"), "the header offers its menu");

		// A typical subset: issue and question, Other once.
		page.set_label_columns(*core::make_feedback_label_columns(std::vector{ core::feedback_label::issue, core::feedback_label::question }));
		expect(asked.size() == 2U && asked.back().Columns.Count == 2U, "choosing chips asks again");
		page.present_feedback(feedback_for(review, asked.back(), all_files));
		settle();
		chips = chips_of(page, cell(page, lighting));
		expect(chips.size() == 3U && chips[0].Label == core::feedback_label::issue && chips[0].Count == 1U && chips[1].Label == core::feedback_label::question
				   && chips[1].Count == 1U && chips[2].Kind == qt::overview_comment_chip::kind::other && chips[2].Count == 1U,
			   "issue 1, question 1 and Other 1 (the plain thread): disjoint, the resolved issue in none");
		expect(cell(page, lighting).data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 1, question 1, Other 1 open conversations"),
			   "the cell's accessible text names label, count, unit and mode");
		const auto lightingFolder = model->index_for_path(QString::fromStdString(lighting)).parent().siblingAtColumn(qt::overview_file_model::comments_column);
		page.files_view()->clearSelection();
		settle();
		expect(count_is_amber_text(page, chips[0]) && count_is_amber_text(page, chips_of(page, lightingFolder).front()),
			   "in Open, a count is amber digits with no fill, on a file row and its folder row alike");
		// The ? mark's quiet ink stays readable: at least 4.5:1, 7:1 on a
		// high-contrast palette (black on white).
		const auto contrast = [](const QColor& aOne, const QColor& aTwo) {
			const auto channel = [](const double aValue) { return aValue <= 0.03928 ? aValue / 12.92 : std::pow((aValue + 0.055) / 1.055, 2.4); };
			const auto lum     = [&](const QColor& aColour) { return 0.2126 * channel(aColour.redF()) + 0.7152 * channel(aColour.greenF()) + 0.0722 * channel(aColour.blueF()); };
			const auto one     = lum(aOne);
			const auto two     = lum(aTwo);
			return (std::max(one, two) + 0.05) / (std::min(one, two) + 0.05);
		};
		auto stark = page.palette();
		stark.setColor(QPalette::Text, Qt::black);
		stark.setColor(QPalette::Base, Qt::white);
		expect(contrast(qt::feedback_state_faint(page.palette()), page.palette().color(QPalette::Base)) >= 4.5
				   && contrast(qt::feedback_state_faint(stark), Qt::white) >= 7.0,
			   "the ? mark's ink is quiet but at least 4.5:1, and 7:1 in high contrast");
		const auto voxelChips = chips_of(page, cell(page, voxel));
		expect(voxelChips.size() == 2U && voxelChips[0].Kind == qt::overview_comment_chip::kind::other && voxelChips[1].Kind == qt::overview_comment_chip::kind::uncertain,
			   "an issue of unknown state is not counted as open, and the cell says so with its ? mark rather than look clear");
		expect(cell(page, voxel).data(Qt::AccessibleTextRole).toString() == QStringLiteral("Other 1 open conversations; 1 whose state is unknown"),
			   "the unknown resolution is in the accessible text, in plain words");
		const auto uncertainTip = page.uncertain_button()->toolTip();
		expect(page.uncertain_button()->isVisible() && page.uncertain_button()->text() == QStringLiteral("2")
				   && uncertainTip.startsWith(QStringLiteral("Not counted: 2 conversations\n")) && uncertainTip.contains(QStringLiteral("• 1 whose state is unknown"))
				   && uncertainTip.contains(QStringLiteral("• 1 that can't be resolved")),
			   "the ? chip beside the switch counts the unknown issue and the comment that can't be resolved, a reason a line");
		expect(chips_of(page, cell(page, deep)).empty() && cell(page, deep).data().toString().isEmpty(), "a complete zero is blank, and only a complete zero");
		const auto remainder = model->remainder_index(qt::overview_file_model::file_column);
		expect(remainder.isValid() && remainder.row() == model->rowCount() - 1 && remainder.data().toString() == QStringLiteral("Not on a file")
				   && !remainder.data(qt::overview_file_model::folder_role).toBool() && model->folder_count() == 2U,
			   "a last row, Not on a file, holds the unanchored and pull request conversations and is no folder");
		const auto* rest = model->feedback_at(remainder.siblingAtColumn(qt::overview_file_model::comments_column));
		expect(rest != nullptr && rest->Catchall == 1U && rest->Uncertain == 1U, "Not on a file: the open praise in Other, the comment not resolvable");
		const auto folder = model->index_for_path(QString::fromStdString(lighting)).parent().siblingAtColumn(qt::overview_file_model::comments_column);
		expect(model->feedback_at(folder) != nullptr && model->feedback_at(folder)->Included == 4U, "the Engine/Renderer folder row counts its two files' open conversations");

		// Only a conversation of unknown state: Open counts none, and the cell
		// still is not blank.
		page.present_feedback(feedback_for(population({ { .Id = "review-thread:u", .Label = core::feedback_label::issue, .State = core::feedback_resolution::unknown, .Path = lighting } }),
										   asked.back(), all_files));
		settle();
		chips = chips_of(page, cell(page, lighting));
		expect(chips.size() == 1U && chips[0].Kind == qt::overview_comment_chip::kind::uncertain && cell(page, lighting).data().toString() == QStringLiteral("? 1"),
			   "zero open beside one of unknown state shows ?, never a blank clean zero");

		// All twelve: chips fold into +n inside the fixed column.
		auto twelve = std::vector<core::feedback_label>{ };
		auto many   = std::vector<unit_spec>{ };
		for (std::size_t label = 0U; label < core::feedback_label_count; ++label) {
			twelve.push_back(static_cast<core::feedback_label>(label));
			many.push_back({ .Id = "review-thread:m" + std::to_string(label), .Label = static_cast<core::feedback_label>(label), .State = core::feedback_resolution::unresolved, .Path = lighting });
		}
		many.push_back({ .Id = "review-thread:plain", .Label = std::nullopt, .State = core::feedback_resolution::unresolved, .Path = lighting });
		page.set_label_columns(*core::make_feedback_label_columns(twelve));
		page.present_feedback(feedback_for(population(many), asked.back(), all_files));
		settle();
		chips           = chips_of(page, cell(page, lighting));
		const auto more = std::ranges::find(chips, qt::overview_comment_chip::kind::more, &qt::overview_comment_chip::Kind);
		expect(more != chips.end() && more->Count + static_cast<std::size_t>(more - chips.begin()) == 13U && more->Folded.back().Kind == qt::overview_comment_chip::kind::other,
			   "all twelve and Other: the chips that fit, then +n naming the rest, Other last");
		const auto cellRect = page.files_view()->visualRect(cell(page, lighting));
		expect(std::ranges::all_of(chips, [&](const qt::overview_comment_chip& aChip) { return cellRect.contains(aChip.Box.toAlignedRect()); }), "every chip stays inside the fixed column");
		auto* const menu = page.make_comments_menu(cell(page, lighting));
		expect(menu != nullptr && std::ranges::count_if(menu->actions(), [](QAction* aAction) { return aAction->isEnabled() && aAction->menu() == nullptr && !aAction->isSeparator(); }) == 13,
			   "the cell's menu reaches all twelve chips and Other");
		delete menu;

		// All: the counts show their breakdown.
		page.set_label_columns(*core::make_feedback_label_columns(std::vector{ core::feedback_label::issue }));
		page.all_counts_button()->click();
		expect(asked.back().Mode == core::feedback_resolution_mode::all && page.all_counts_button()->isChecked(), "All asks for every conversation");
		page.present_feedback(feedback_for(review, asked.back(), all_files));
		settle();
		chips = chips_of(page, cell(page, lighting));
		expect(chips[0].Label == core::feedback_label::issue && chips[0].Count == 2U && chips[0].States == core::feedback_state_counts{ 1U, 1U, 0U, 0U },
			   "in All the issue chip counts two, one open and one resolved");
		expect(cell(page, lighting).data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 2 (1 open, 1 resolved), Other 2 (2 open) conversations"),
			   "and each count says its breakdown");

		// A stale answer, for a request no longer current, is dropped.
		page.present_feedback(feedback_for(review, request_for({ }, core::feedback_resolution_mode::unresolved), all_files));
		expect(model->feedback()->Request.Mode == core::feedback_resolution_mode::all, "an answer to an earlier request is dropped");
		expect(resets == 0, "no count, chip or mode change resets the model");
		expect(page.body_view()->geometry() == bodyBefore, "the description keeps its geometry");

		// The Label key lists all twelve marks with their codes; Escape closes it.
		page.show_label_key();
		settle();
		auto* const key  = page.label_key();
		auto* const list = key != nullptr ? key->findChild<QListWidget*>(QStringLiteral("overviewLabelKeyList")) : nullptr;
		expect(key != nullptr && key->isVisible() && list != nullptr && list->count() == 1 + 12 + 1 + 2 + 4 + 1 + 1 + 4 && list->item(4)->text().contains(QStringLiteral(":is:"))
				   && list->item(4)->data(Qt::AccessibleTextRole).toString().startsWith(QStringLiteral("issue, shortcode is")),
			   "the Label key names every mark and its shortcode, the state marks and the keys");
		expect(list->item(14)->text() == QStringLiteral("Other labels") && list->item(21)->text() == QStringLiteral("Keys")
				   && list->item(22)->text().startsWith(QStringLiteral("Tab: from a row onto its chips")),
			   "the Label key calls the bubble with dots Other labels, and names the keys that reach a chip");
		for (auto row = 0; row < list->count(); ++row) {
			for (const auto role : { Qt::DisplayRole, Qt::AccessibleTextRole, Qt::ToolTipRole }) {
				const auto words = list->item(row)->data(role).toString();
				expect(!words.contains(QStringLiteral("chip of their own")) && !words.contains(QStringLiteral("unreadable")) && !words.contains(QStringLiteral("no chip chosen")),
					   "no Label key entry says 'without a chip of their own' or other jargon");
			}
		}
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(key, &escape);
		settle();
		expect(!key->isVisible(), "Escape closes the Label key");
	}

	/// Pointer, keyboard and menu activation each name exactly the chip's row
	/// and number; a chip opens no file; Enter on a row still reads the file;
	/// Left and Right stay the tree's; Tab steps onto a row's chips with a
	/// ring and a tooltip; the current cell and chip survive a refresh and a
	/// sort.
	void test_chip_activation_names_exactly_its_count()
	{
		qt::pull_request_overview_page page;
		page.resize(1'300, 760);
		auto asked   = std::vector<application::overview_feedback_request>{ };
		auto drilled = std::vector<application::overview_drill_down>{ };
		auto opened  = std::vector<QString>{ };
		page.set_feedback_dispatch([&](const application::overview_feedback_request& aRequest) { asked.push_back(aRequest); });
		page.set_drill_down_dispatch([&](const application::overview_drill_down& aDown) { drilled.push_back(aDown); });
		page.set_file_dispatch([&](const QString& aPath) { opened.push_back(aPath); });
		page.present(projection());
		page.set_label_columns(*core::make_feedback_label_columns(std::vector{ core::feedback_label::issue, core::feedback_label::question }));
		page.show();
		settle();
		const auto review = mixed_review();
		page.present_feedback(feedback_for(review, asked.back(), all_files));
		settle();
		auto* const view  = page.files_view();
		auto* const model = page.file_model();

		// A click on the issue chip.
		auto chips = chips_of(page, cell(page, lighting));
		click_at(*view->viewport(), chips[0].Box.center());
		expect(drilled.size() == 1U && drilled[0].Row == application::overview_feedback_row_kind::file && drilled[0].Path == lighting
				   && drilled[0].Target == core::feedback_count_target{ .Kind = core::feedback_count_kind::dedicated, .Label = core::feedback_label::issue },
			   "a click on the issue chip asks for exactly that file's issue count");
		expect(opened.empty() && view->currentIndex() == cell(page, lighting) && !page.focused_chip(), "the chip opens no file, its row becomes current, and the pointer leaves no ring");
		click_at(*view->viewport(), chips[2].Box.center());
		expect(drilled.back().Target.Kind == core::feedback_count_kind::catchall, "a click on Other asks for the catch-all");

		// Keyboard (owner, rejecting overview-keyboard-v001: nothing showed that
		// Left and Right did more than fold folders). Left and Right stay the
		// tree's; Tab steps from a row onto its chips, with a ring and the
		// chip's tooltip; Enter or Space lists a chip; Escape returns.
		page.activateWindow();
		view->setFocus();
		settle();
		const auto voxelRow = model->index_for_path(QString::fromStdString(voxel));
		view->setCurrentIndex(voxelRow);
		key(*view, Qt::Key_Right);
		expect(view->currentIndex() == voxelRow && !page.focused_chip(), "Right on a file row is the tree's: it moves to no other cell and onto no chip");
		const auto ringBefore = ring_pixels(page, chips_of(page, cell(page, voxel)).front());
		key(*view, Qt::Key_Tab);
		auto focused = page.focused_chip();
		expect(view->hasFocus() && focused && focused->Kind == qt::overview_comment_chip::kind::other, "Tab steps from the row onto its first chip, Other, and keeps focus in the files");
		auto* tip = qobject_cast<QLabel*>(qt::anchored_tooltip());
		expect(tip != nullptr && tip->isVisible() && tip->text().startsWith(QStringLiteral("Other labels: 1 open conversation here.\nLabels not ticked in Settings' Overview chips"))
				   && !tip->text().contains(QStringLiteral("chip of their own")) && tip->text().contains(QStringLiteral("press Enter")),
			   "the chip's tooltip comes up beside it, in plain words, naming Enter");
		expect(ringBefore == 0 && ring_pixels(page, *focused) > 12, "the chip the keyboard is on wears a ring in the selection's text colour; none before Tab");
		key(*view, Qt::Key_Tab);
		focused = page.focused_chip();
		tip     = qobject_cast<QLabel*>(qt::anchored_tooltip());
		expect(focused && focused->Kind == qt::overview_comment_chip::kind::uncertain && tip != nullptr
				   && tip->text().startsWith(QStringLiteral("Not counted: 1 conversation\n• 1 whose state is unknown")),
			   "Tab moves on to the ? chip, whose tooltip gives its reason in plain words");
		key(*view, Qt::Key_Left);
		expect(page.focused_chip() && page.focused_chip()->Kind == qt::overview_comment_chip::kind::other && view->currentIndex() == voxelRow,
			   "once on a chip, Left moves along the chips, not the cells");
		key(*view, Qt::Key_Right);
		expect(page.focused_chip() && page.focused_chip()->Kind == qt::overview_comment_chip::kind::uncertain, "and Right back");
		key(*view, Qt::Key_Return);
		expect(drilled.back().Path == voxel && drilled.back().Target.Kind == core::feedback_count_kind::uncertain && opened.empty(),
			   "Enter on the ? chip asks for exactly that file's uncertain conversations, and opens no file");
		key(*view, Qt::Key_Escape);
		expect(!page.focused_chip() && view->currentIndex() == voxelRow && ring_pixels(page, chips_of(page, cell(page, voxel)).front()) == 0,
			   "Escape returns to the row and the ring goes");
		key(*view, Qt::Key_Return);
		expect(opened.size() == 1U && opened[0] == QString::fromStdString(voxel), "Enter on the row reads the file");
		key(*view, Qt::Key_Tab);
		key(*view, Qt::Key_Space);
		expect(drilled.back().Path == voxel && drilled.back().Target.Kind == core::feedback_count_kind::catchall, "Space on Other asks for the catch-all");
		QKeyEvent backtab{ QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier };
		QApplication::sendEvent(view, &backtab);
		settle();
		expect(!page.focused_chip() && view->hasFocus(), "Shift+Tab from the first chip returns to the row");
		key(*view, Qt::Key_Tab);
		key(*view, Qt::Key_Up);
		expect(!page.focused_chip() && view->currentIndex().siblingAtColumn(0) != voxelRow.siblingAtColumn(0), "another row leaves the chip");
		key(*view, Qt::Key_Down);
		expect(!page.focused_chip() && view->currentIndex().siblingAtColumn(0) == voxelRow.siblingAtColumn(0), "and coming back lands on the row, not the chip it left");
		key(*view, Qt::Key_Tab);
		key(*view, Qt::Key_Tab);
		key(*view, Qt::Key_Tab);
		expect(!page.focused_chip() && !view->hasFocus(), "Tab past the last chip leaves the files, as Tab always does");
		view->setFocus();
		settle();
		const auto folder = voxelRow.parent().siblingAtColumn(qt::overview_file_model::comments_column);
		view->setCurrentIndex(folder);
		key(*view, Qt::Key_Left);
		expect(!view->isExpanded(folder.siblingAtColumn(0)) && !page.focused_chip(), "on a folder row Left folds it, from any cell");
		key(*view, Qt::Key_Right);
		expect(view->isExpanded(folder.siblingAtColumn(0)), "and Right unfolds it");
		expect(opened.size() == 1U, "no key but Enter on a file row opened a file");

		// The remainder and the ? chip beside the switch.
		auto* const restMenu = page.make_comments_menu(model->remainder_index());
		expect(restMenu != nullptr, "Not on a file has a menu");
		restMenu->actions().front()->trigger();
		delete restMenu;
		expect(drilled.back().Row == application::overview_feedback_row_kind::remainder && drilled.back().Target.Kind == core::feedback_count_kind::catchall,
			   "Not on a file's entries ask for the remainder row");
		page.uncertain_button()->click();
		expect(drilled.back().Row == application::overview_feedback_row_kind::pull_request && drilled.back().Target.Kind == core::feedback_count_kind::uncertain,
			   "the ? chip asks for every uncertain conversation of the pull request");

		// Focus across a refresh: the current cell and its chip stay, nothing
		// resets.
		view->setCurrentIndex(cell(page, lighting));
		key(*view, Qt::Key_Tab);
		expect(page.focused_chip() && page.focused_chip()->Label == core::feedback_label::issue, "Tab onto lighting's issue chip");
		auto resets = 0;
		auto moves  = 0;
		QObject::connect(model, &QAbstractItemModel::modelReset, [&] { ++resets; });
		QObject::connect(model, &QAbstractItemModel::rowsMoved, [&] { ++moves; });
		auto refreshed = review;
		refreshed.Units.pop_back();
		page.present_feedback(feedback_for(refreshed, asked.back(), all_files));
		settle();
		expect(view->currentIndex() == cell(page, lighting) && resets == 0 && moves == 0, "a refresh keeps the current Comments cell and resets nothing");
		expect(page.focused_chip() && page.focused_chip()->Label == core::feedback_label::issue, "and the keyboard stays on the issue chip");

		// Sorting by a label: the menu asks, the answer's order moves rows and
		// keeps the current cell.
		auto* const sort       = page.make_sort_menu();
		const auto  sortItems  = sort->actions();
		const auto  byQuestion = std::ranges::find_if(sortItems, [](QAction* aAction) { return aAction->text() == QStringLiteral("question"); });
		expect(byQuestion != sortItems.end(), "the sort menu offers each chosen label");
		(*byQuestion)->trigger();
		delete sort;
		expect(asked.back().Sort == core::feedback_count_target{ .Kind = core::feedback_count_kind::dedicated, .Label = core::feedback_label::question },
			   "choosing a label asks for that sort");
		auto sorted  = feedback_for(refreshed, asked.back(), all_files);
		sorted.Order = { deep, voxel, lighting };
		page.present_feedback(sorted);
		settle();
		expect(model->index_for_path(QString::fromStdString(voxel)).row() == 0 && model->index_for_path(QString::fromStdString(lighting)).row() == 1
				   && view->currentIndex() == cell(page, lighting) && resets == 0,
			   "the rows take the answer's order by move, and the current cell follows its file");
		expect(page.focused_chip() && page.focused_chip()->Label == core::feedback_label::issue, "the chip the keyboard is on follows its row too");
		expect(model->headerData(qt::overview_file_model::comments_column, Qt::Horizontal, Qt::DisplayRole).toString() == QStringLiteral("Comments ↓ ▾")
				   && model->headerData(qt::overview_file_model::comments_column, Qt::Horizontal, Qt::ToolTipRole).toString().contains(QStringLiteral("Sorted by question")),
			   "the header says it is sorted, and by what");

		// Another pull request starts again from the Settings default.
		page.choose_mode(core::feedback_resolution_mode::unresolved);
		const auto before = asked.size();
		page.set_default_mode(core::feedback_resolution_mode::all);
		expect(asked.size() == before && page.open_counts_button()->isChecked(), "a new Settings default leaves a view whose mode was chosen as it is");
		auto other   = projection();
		other.Number = core::pull_request_number{ 1U };
		page.present(other);
		settle();
		expect(asked.back().Mode == core::feedback_resolution_mode::all && !asked.back().Sort && !model->feedback(), "another pull request starts at the default, unsorted, with no counts");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   scale = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();
	if (scale > 0) {
		expect(qFuzzyCompare(application.devicePixelRatio(), scale), "the requested display scale is effective");
	}
	test_history_publication_does_not_touch_file_rows();
	test_cached_catalogue_availability_and_navigation();
	test_column_sizing_measures_visible_files();
	test_page_presents_the_projection_and_reports_its_gestures();
	test_folder_controls_preserve_identity_and_refresh_state();
	test_collapse_all_and_expand_all_folders();
	test_compact_descriptions();
	test_resizable_summary();
	test_summary_seam_holds_its_cursor();
	test_retained_description_reader();
	test_body_edits_clamp_retained_selection();
	test_many_participants_scroll_without_compressing_or_overpainting();
	test_label_chips_show_the_selected_breakdown();
	test_chip_activation_names_exactly_its_count();
	test_modified_mark_reads_on_every_row();
	std::cout << "pull-request-overview-page-tests:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
