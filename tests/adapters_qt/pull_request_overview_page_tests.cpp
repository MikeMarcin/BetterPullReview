#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include <QTextEdit>

#include "bpr/application/pull_request_overview.hpp"

#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTreeView>
#include <QWheelEvent>

#include <algorithm>
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
					auto* read = page.findChild<QPushButton*>(QStringLiteral("overviewReadDescription"));
					auto* back = page.findChild<QPushButton*>(QStringLiteral("overviewReturnToFiles"));
					expect(read->isVisible() && !back->isVisible(), "overflow exposes the named reading action");
					expect(edit->viewport()->height() <= 4 * edit->fontMetrics().lineSpacing() + 2 * edit->document()->documentMargin(), "preview has a font-relative compact content budget with separate reader controls");
					expect(edit->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff && edit->verticalScrollBar()->value() == 0, "preview has no scrolling well");
					QWheelEvent previewWheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, { }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
					QApplication::sendEvent(edit->viewport(), &previewWheel);
					expect(edit->verticalScrollBar()->value() == 0, "wheel cannot scroll the compact preview");
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
					expect(back->isVisible() && !page.files_view()->isVisible() && edit->hasFocus(), "keyboard activation opens local reader and focuses text");
					expect(edit->maximumHeight() == QWIDGETSIZE_MAX && page.body_view()->height() > 650 && edit->viewport()->height() > page.height() * 2 / 3, "expanded reader fills available space without a height cap, including its source controls and fallback explanation");
					QKeyEvent pageDown{ QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier };
					QApplication::sendEvent(edit, &pageDown);
					expect(edit->verticalScrollBar()->value() > 0, "PageDown navigates the sole reader viewport");
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
		auto* read = page.findChild<QPushButton*>(QStringLiteral("overviewReadDescription"));
		expect(page.findChild<QLabel*>(QStringLiteral("overviewNoDescription"))->isVisible() && !read->isVisible() && !page.body_view()->isVisible(), "empty description has a compact explicit state");
		projected.Body = "Short 日本語 🦊 description.";
		page.present(projected);
		settle();
		expect(!read->isVisible() && page.body_view()->rendered_view()->height() < 3 * page.body_view()->fontMetrics().lineSpacing(), "short body uses its natural height without an expansion action");
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
		expect(!read->isVisible(), "widening recomputes preview overflow");
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
		auto* read = page.findChild<QPushButton*>(QStringLiteral("overviewReadDescription"));
		auto* back = page.findChild<QPushButton*>(QStringLiteral("overviewReturnToFiles"));
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
	test_compact_descriptions();
	test_retained_description_reader();
	test_body_edits_clamp_retained_selection();
	test_many_participants_scroll_without_compressing_or_overpainting();
	std::cout << "pull-request-overview-page-tests:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
