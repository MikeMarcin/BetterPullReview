#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/identity_avatar.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/reviewer_picker.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include <QTextEdit>

#include "../support/resize_cursor_hold_check.hpp"

#include "bpr/application/pull_request_overview.hpp"
#include "bpr/core/file_progress_mark.hpp"

#include <QAbstractScrollArea>
#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRect>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyleHints>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
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
			.Path      = aPath,
			.Folder    = slash == std::string::npos ? std::string{ } : aPath.substr(0U, slash),
			.Name      = slash == std::string::npos ? aPath : aPath.substr(slash + 1U),
			.Operation = aOperation,
			.Additions = 12U,
			.Deletions = aOperation == application::overview_file_operation::added ? 0U : 3U,
			.Changed   = aChanged,
			.Read      = aRead,
			// As the projection decides it for a receipt with no acceptance or
			// exclusion: the read progression.
			.Mark                  = aRead.has_value() ? std::optional{ static_cast<core::file_progress_mark>(static_cast<int>(*aRead)) } : std::nullopt,
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
		expect(!page.presented() && !page.entry_path()
				   && page.title_label()->text() == QStringLiteral("No pull request"),
			   "before a projection the page names nothing and offers no entry");
		// [[BPR-1588]]. The owner's decision D1: no Start review button.
		expect(page.findChild<QPushButton*>(QStringLiteral("overviewEntryButton")) == nullptr,
			   "the Overview has no Start review or Continue review button");

		auto activated = std::vector<QString>{ };
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
				   && !participants[1]->isVisible(),
			   "participants are rows with their login and standing, in People's details");
		page.status_section_header(qt::overview_status_section::people)->click();
		QApplication::processEvents();
		expect(participants[1]->isVisible(), "[[BPR-1589]] People opens in place to the roster");
		const auto rowShown = [&page](const qt::overview_value aValue) { return !page.value_label(aValue)->parentWidget()->isHidden(); };

		expect(page.since_heading()->text().startsWith(QStringLiteral("Since my last review on "))
				   && page.value_label(qt::overview_value::uploads)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::files_changed)->text() == QStringLiteral("2")
				   && !rowShown(qt::overview_value::new_comments)
				   && page.value_label(qt::overview_value::replies_to_me)->text() == QStringLiteral("1")
				   && page.value_label(qt::overview_value::my_unresolved_threads)->text() == QStringLiteral("2")
				   && page.value_label(qt::overview_value::checks)->text() == QStringLiteral("3 passed, 1 skipped")
				   && page.value_label(qt::overview_value::files_read)->text() == QStringLiteral("1 of 3")
				   && page.value_label(qt::overview_value::private_notes)->text() == QStringLiteral("1")
				   && page.value_label(qt::overview_value::drafted_comments)->text() == QStringLiteral("1"),
			   "[[BPR-1589]] the Status drawer states what happened since my last review and my progress, "
			   "and draws no row where the host cannot answer");
		expect(page.entry_path() == QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"),
			   "the first entry into Review lands at the first unseen change's file");
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
				   && !rowShown(qt::overview_value::uploads)
				   && page.value_label(qt::overview_value::files_changed)->text() == QStringLiteral("not yet resolved") && rowShown(qt::overview_value::files_changed)
				   && !rowShown(qt::overview_value::drafted_comments)
				   && page.file_model()->index_for_path(QStringLiteral("Engine/Renderer/HairStrandsLighting.cpp")).siblingAtColumn(qt::overview_file_model::changed_column).data().toString()
					   == QStringLiteral("?")
				   && page.entry_path() == QStringLiteral("Engine/Renderer/HairStrandsVoxelization.cpp"),
			   "what is still resolving says so; what the host cannot answer is not drawn, never as none");

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
				   && !page.entry_path() && page.participant_rows().empty(),
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

	/// \brief [[BPR-1677]]. Sends one press and release of `aKey`, the
	/// modifiers included, to the files.
	void key(QTreeView& aView, const QKeyCombination aKey)
	{
		QKeyEvent press{ QEvent::KeyPress, aKey.key(), aKey.keyboardModifiers() };
		QKeyEvent release{ QEvent::KeyRelease, aKey.key(), aKey.keyboardModifiers() };
		QApplication::sendEvent(&aView, &press);
		QApplication::sendEvent(&aView, &release);
		settle();
	}

	/// \brief The rows the tree shows, top to bottom.
	[[nodiscard]] auto rows_shown(const QTreeView& aView) -> std::vector<QModelIndex>
	{
		auto rows = std::vector<QModelIndex>{ };
		for (auto at = aView.model()->index(0, 0); at.isValid(); at = aView.indexBelow(at)) {
			rows.push_back(at);
		}
		return rows;
	}

	/// \brief [[BPR-1677]]. The owner, 2026-10-20: "in the overview page on
	/// the file listing i want cmd+up and down to go to the top and bottom of
	/// the files list". The document start and end (Command-Up and
	/// Command-Down on macOS, Ctrl+Home and Ctrl+End elsewhere) make the top
	/// or bottom shown row current, selected and in view, keep the keyboard on
	/// the files, tell accessibility which row it is, and open nothing.
	void test_document_keys_go_to_the_top_and_bottom_of_the_files()
	{
		const auto keys = qt::shortcut_keys(qt::shortcut_command::files_ends);
#if defined(Q_OS_MACOS)
		expect(keys == QList{ QKeySequence{ Qt::CTRL | Qt::Key_Up }, QKeySequence{ Qt::CTRL | Qt::Key_Down } }, "the catalogue names Command-Up and Command-Down");
#else
		expect(keys == QList{ QKeySequence{ Qt::CTRL | Qt::Key_Home }, QKeySequence{ Qt::CTRL | Qt::Key_End } }, "the catalogue names Ctrl+Home and Ctrl+End");
#endif
		expect(qt::shortcut(qt::shortcut_command::files_ends).Binding == qt::shortcut_binding::read
				   && qt::shortcut(qt::shortcut_command::files_ends).Place == std::string_view{ "In Overview's files" },
			   "the files read the keys themselves, and the page says where");
		const auto top    = keys.at(0)[0];
		const auto bottom = keys.at(1)[0];
		const auto edge   = [](const QKeyCombination aKey) {
			return qt::document_edge_for(QKeyEvent{ QEvent::KeyPress, aKey.key(), aKey.keyboardModifiers() });
		};
		expect(edge(top) == qt::document_edge::start && edge(bottom) == qt::document_edge::end
				   && !edge(QKeyCombination{ Qt::Key_Down }).has_value() && !edge(QKeyCombination{ Qt::AltModifier, Qt::Key_Down }).has_value()
				   && !edge(QKeyCombination{ Qt::ControlModifier | Qt::AltModifier, Qt::Key_Down }).has_value(),
			   "the catalogue's keys are the document ends, and Down, Next change and Next file are not");
#if defined(Q_OS_MACOS)
		// macOS marks the arrow keys as keypad keys.
		expect(edge(QKeyCombination{ Qt::ControlModifier | Qt::KeypadModifier, Qt::Key_Up }) == qt::document_edge::start
				   && edge(QKeyCombination{ Qt::ControlModifier | Qt::KeypadModifier, Qt::Key_Down }) == qt::document_edge::end,
			   "Command-Up and Command-Down as macOS delivers them");
#endif

		qt::pull_request_overview_page page;
		page.resize(1100, 520);
		auto projected = projection();
		// Enough files that the list scrolls, and a file at the root, which
		// the last folder row, /, holds.
		for (auto file = 0; file < 40; ++file) {
			projected.Files.push_back(row("Engine/Plugins/Plugin" + std::to_string(file) + ".cpp", application::overview_file_operation::modified,
										  application::overview_change_since_review::unchanged, std::nullopt));
		}
		projected.Files.push_back(row("README.md", application::overview_file_operation::modified,
									  application::overview_change_since_review::unchanged, std::nullopt));
		page.present(projected);
		page.show();
		settle();
		auto* const view       = page.files_view();
		auto* const model      = page.file_model();
		auto* const scroll     = view->verticalScrollBar();
		auto        dispatches = 0;
		page.set_file_dispatch([&](const QString&) { ++dispatches; });
		expect(scroll->maximum() > scroll->minimum(), "the files scroll");
		auto* const accessible = QAccessible::queryAccessibleInterface(view);
		auto* const table      = accessible == nullptr ? nullptr : accessible->tableInterface();
		expect(table != nullptr, "the files are an accessible table");
		expect(view->accessibleDescription().contains(QStringLiteral("%1 and %2 go to the top and bottom row").arg(qt::spoken_key(keys.at(0)), qt::spoken_key(keys.at(1)))),
			   "the table's description names both keys as a screen reader speaks them");
		const auto told = [table](const int aShownRow) {
			auto* const cell = table->cellAt(aShownRow, qt::overview_file_model::file_column);
			return cell != nullptr && cell->state().selected && cell->state().focused;
		};

		const auto middle = model->index_for_path(QStringLiteral("Engine/Plugins/Plugin20.cpp"));
		view->setFocus();
		view->setCurrentIndex(middle.siblingAtColumn(qt::overview_file_model::file_column));
		view->scrollTo(middle);
		settle();
		expect(scroll->value() > scroll->minimum() && scroll->value() < scroll->maximum(), "the course starts in the middle of the files");

		key(*view, bottom);
		auto shown = rows_shown(*view);
		expect(view->currentIndex().siblingAtColumn(0) == shown.back() && view->currentIndex().data(qt::overview_file_model::path_role).toString() == QStringLiteral("README.md")
				   && view->currentIndex().column() == qt::overview_file_model::file_column,
			   "the end makes the last file current, in the same column");
		expect(view->selectionModel()->isRowSelected(shown.back().row(), shown.back().parent()) && view->selectionModel()->selectedRows().size() == 1,
			   "and selects that one row");
		expect(scroll->value() == scroll->maximum() && view->viewport()->rect().contains(view->visualRect(view->currentIndex())),
			   "and scrolls the list to its bottom, the row in view");
		expect(view->hasFocus() && told(static_cast<int>(shown.size()) - 1) && dispatches == 0,
			   "the keyboard stays on the files, accessibility hears the row, and nothing opens");

		key(*view, top);
		expect(view->currentIndex().siblingAtColumn(0) == model->index(0, 0) && view->currentIndex().column() == qt::overview_file_model::file_column
				   && view->selectionModel()->isRowSelected(0, { }) && view->selectionModel()->selectedRows().size() == 1,
			   "the start makes the top row current and selected");
		expect(scroll->value() == scroll->minimum() && view->hasFocus() && told(0) && dispatches == 0,
			   "and scrolls the list to its top, the keyboard and accessibility with it, opening nothing");
		key(*view, top);
		expect(view->currentIndex().siblingAtColumn(0) == model->index(0, 0) && scroll->value() == scroll->minimum(), "at the top the start stays there");

		// A closed last folder's files are not shown: its row is the bottom.
		const auto root = QPersistentModelIndex{ model->index_for_path(QStringLiteral("README.md")).parent() };
		view->setExpanded(root, false);
		settle();
		key(*view, bottom);
		shown = rows_shown(*view);
		expect(view->currentIndex().siblingAtColumn(0) == QModelIndex{ root } && shown.back() == QModelIndex{ root } && scroll->value() == scroll->maximum()
				   && dispatches == 0,
			   "with the last folder closed the end goes to its row");
		key(*view, Qt::Key_Up);
		expect(view->currentIndex().siblingAtColumn(0) == shown.at(shown.size() - 2U), "Up still steps one row");

		page.clear();
		settle();
		key(*view, top);
		key(*view, bottom);
		expect(!view->currentIndex().isValid() && dispatches == 0, "with no files the keys do nothing");
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
		expect(view->visualRect(deepFolder).width() >= QFontMetrics{ bold }.horizontalAdvance(deepFolder.data().toString()),
			   "the longest folder path fits its cell unelided; its file count is in the row's first cell");
		expect(view->visualRect(QModelIndex{ deep.parent() }.siblingAtColumn(qt::overview_file_model::operation_column)).width()
				   >= qt::overview_row_delegate::disclosure_rect(measured).width() + qt::overview_row_delegate::count_rect(measured, deep.parent()).width(),
			   "the folder row's first cell holds its disclosure mark and its count's slot");
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
		option.rect           = view->visualRect(QModelIndex{ renderer }.siblingAtColumn(qt::overview_file_model::operation_column));
		const auto disclosure = qt::overview_row_delegate::disclosure_rect(option);
		expect(disclosure.width() > 0 && option.rect.contains(disclosure), "the folder row's first cell reserves its disclosure mark");
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

	// [[BPR-1774]]. Writes the page as it is now under the directory a
	// render run names (BPR_DESCRIPTION_EDIT_RENDER_DIR); nothing otherwise.
	void render_description_edit(QWidget& aPage, const char* const aName)
	{
		const auto directory = qEnvironmentVariable("BPR_DESCRIPTION_EDIT_RENDER_DIR");
		if (!directory.isEmpty()) {
			aPage.grab().save(directory + QLatin1Char{ '/' } + QString::fromUtf8(aName));
		}
	}

	/// [[BPR-1804]]. The page with the picker drawn where it opened, for the
	/// owner's look; nothing is written unless the directory is named.
	void render_reviewer_picker(qt::pull_request_overview_page& aPage, const char* const aName)
	{
		const auto directory = qEnvironmentVariable("BPR_REVIEWER_PICKER_RENDER_DIR");
		if (directory.isEmpty()) {
			return;
		}
		auto image = aPage.grab().toImage();
		if (auto* const picker = aPage.reviewer_picker_popup(); picker != nullptr && picker->isVisible()) {
			auto painter = QPainter{ &image };
			painter.drawPixmap(aPage.mapFromGlobal(picker->mapToGlobal(QPoint{ })), picker->grab());
		}
		image.save(directory + QLatin1Char{ '/' } + QString::fromUtf8(aName));
	}

	/// [[BPR-1804]]. One key pressed and released on `aWidget`.
	void press(QWidget* const aWidget, const int aKey, const QString& aText = { })
	{
		auto down = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier, aText };
		QCoreApplication::sendEvent(aWidget, &down);
		auto up = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier, aText };
		QCoreApplication::sendEvent(aWidget, &up);
	}

	[[nodiscard]] auto reviewer_answer() -> core::reviewer_search_answer
	{
		return {
			.Author    = "octo-author",
			.Suggested = { { .NodeId = "U_sam", .Login = "samira-o", .Name = "Samira Ortega" }, { .NodeId = "U_bea", .Login = "bea", .Name = "Beatriz Nunes" } },
			.Matches   = { { .NodeId = "U_ana", .Login = "ana-lima", .Name = "Ana Lima" }, { .NodeId = "U_rui", .Login = "rui", .Name = "" }, { .NodeId = "U_ola", .Login = "ola-dev", .Name = "Ola Nordmann" } },
			.Requested = { "rui" },
			.Reviewed  = { "bea" },
		};
	}

	// [[BPR-1804]]. Request review appears in People only when offered and
	// sends only an open gesture; the picker shows the core's rows with
	// GitHub's suggestions first under their heading and each standing in a
	// word and in its accessible text; the keyboard moves past headings and
	// chooses; Escape and a withdrawn offer close it; the page performs
	// nothing itself.
	void test_reviewers_are_requested_from_people()
	{
		qt::pull_request_overview_page page;
		page.resize(1100, 760);
		page.show();
		page.present(projection());
		page.status_section_header(qt::overview_status_section::people)->setChecked(true);
		settle();
		auto* const button = page.request_review_button();
		expect(button != nullptr && !button->isVisible() && !page.reviewer_requests_offered(), "Request review is not offered until the composition offers it");
		auto gestures = std::vector<qt::reviewer_picker_gesture>{ };
		page.set_reviewer_picker_dispatch([&gestures](const qt::reviewer_picker_gesture& aGesture) { gestures.push_back(aGesture); });
		page.set_reviewer_requests_offered(true);
		settle();
		expect(button->isVisible() && button->text() == QStringLiteral("Request review") && button->accessibleName() == QStringLiteral("Request review"),
			   "Request review shows in People, named for what it does");
		render_reviewer_picker(page, "people-request-review-offered-light.png");
		button->click();
		expect(gestures.size() == 1U && gestures.back().Kind == qt::reviewer_picker_gesture::kind::open && page.reviewer_picker_popup() == nullptr,
			   "pressing it only asks the composition to open the picker");

		auto state = core::open_reviewer_picker({ }).State;
		page.present_reviewer_picker(state);
		settle();
		auto* const picker = page.reviewer_picker_popup();
		expect(picker != nullptr && picker->isVisible() && picker->status()->text() == QStringLiteral("Searching…") && picker->search_field()->hasFocus(),
			   "the picker opens under the button, focused in its search, and says it is searching");
		render_reviewer_picker(page, "people-reviewer-picker-searching-light.png");

		state = core::accept_reviewer_search(state, 1U, reviewer_answer());
		page.present_reviewer_picker(state);
		settle();
		expect(picker->listed_logins() == QStringList{ "samira-o", "bea", "ana-lima", "rui", "ola-dev" }, "suggestions lead, then everyone else, in the core's order");
		auto* const list = picker->people();
		expect(list->item(0)->text() == QStringLiteral("Suggested") && list->item(3)->text() == QStringLiteral("Everyone who can review") && list->count() == 7,
			   "the two groups have their headings");
		expect(list->item(2)->data(Qt::AccessibleTextRole).toString() == QStringLiteral("bea, Beatriz Nunes, suggested, reviewed, choose to ask again")
				   && list->item(5)->data(Qt::AccessibleTextRole).toString() == QStringLiteral("rui, review requested")
				   && list->item(4)->data(Qt::AccessibleTextRole).toString() == QStringLiteral("ana-lima, Ana Lima, can be asked to review"),
			   "each row says who they are and where they stand to assistive technology");
		expect(picker->status()->isHidden() && list->accessibleName() == QStringLiteral("People who can review"), "a full list needs no status line");

		gestures.clear();
		press(picker->search_field(), Qt::Key_Down);
		expect(list->currentRow() == 1, "Down from the search moves to the first person, past the heading");
		press(picker->search_field(), Qt::Key_Down);
		press(picker->search_field(), Qt::Key_Down);
		expect(list->currentRow() == 4, "and on past the next heading");
		press(picker->search_field(), Qt::Key_Up);
		expect(list->currentRow() == 2, "Up comes back past it");
		press(picker->search_field(), Qt::Key_Return);
		expect(gestures.size() == 1U && gestures.back().Kind == qt::reviewer_picker_gesture::kind::choose && gestures.back().Text == "bea", "Return chooses the highlighted person");
		render_reviewer_picker(page, "people-reviewer-picker-list-light.png");

		press(picker->search_field(), Qt::Key_A, QStringLiteral("a"));
		press(picker->search_field(), Qt::Key_N, QStringLiteral("n"));
		expect(gestures.size() == 3U && gestures.back().Kind == qt::reviewer_picker_gesture::kind::query && gestures.back().Text == "an", "typing hands the composition the text");

		auto chose = core::choose_reviewer(core::type_reviewer_query(state, "ana").State, "ana-lima");
		page.present_reviewer_picker(chose.State);
		settle();
		expect(picker->listed_logins() == QStringList{ "ana-lima" } && list->item(0)->data(Qt::AccessibleTextRole).toString().endsWith(QStringLiteral("asking GitHub now"))
				   && picker->status()->text() == QStringLiteral("Asking GitHub for a review from ana-lima…"),
			   "a choice waiting on GitHub is said on its row and in the line");
		auto done = core::finish_reviewer_request(chose.State, 1U, { .Requested = { "rui", "ana-lima" }, .Written = true });
		page.present_reviewer_picker(done);
		settle();
		expect(picker->status()->text() == QStringLiteral("Requested a review from ana-lima.") && picker->status()->accessibleName() == picker->status()->text(),
			   "GitHub's answer is said, and named for assistive technology");
		render_reviewer_picker(page, "people-reviewer-picker-requested-light.png");
		auto dropped = core::finish_reviewer_request(core::choose_reviewer(core::type_reviewer_query(state, "ola").State, "ola-dev").State, 1U, { .Requested = { "rui" }, .Written = true });
		expect(qt::reviewer_picker::status_text(dropped) == QStringLiteral("GitHub did not add ola-dev. Only people with access to this repository can review it."),
			   "someone GitHub did not add is said plainly");
		auto nobody = core::accept_reviewer_search(core::type_reviewer_query(state, "zz").State, 2U, { });
		expect(qt::reviewer_picker::status_text(nobody) == QStringLiteral("No one who can review matches \u201Czz\u201D."), "an empty answer says what to change");

		gestures.clear();
		press(picker->search_field(), Qt::Key_Escape);
		settle();
		expect(!picker->isVisible() && gestures.size() == 1U && gestures.back().Kind == qt::reviewer_picker_gesture::kind::close, "Escape closes the picker and says so");
		page.present_reviewer_picker(core::close_reviewer_picker(done));
		expect(gestures.size() == 1U, "a close the core asked for is not reported back");

		page.present_reviewer_picker(core::open_reviewer_picker(core::close_reviewer_picker(done)).State);
		settle();
		expect(picker->isVisible(), "it opens again");
		page.set_reviewer_requests_offered(false);
		settle();
		expect(!picker->isVisible() && !button->isVisible() && gestures.back().Kind == qt::reviewer_picker_gesture::kind::close, "a withdrawn offer closes the picker and hides the button");
	}

	// [[BPR-1774]]. Edit is offered when the composition says so and not
	// otherwise; the editor opens on the body's exact bytes in the reader's
	// place; Save hands the composition the original and the text and sends
	// nothing itself; Cancel and another pull request close it; a conflict
	// keeps the typed text until the newer one is asked for.
	void test_the_description_is_edited_in_place()
	{
		qt::pull_request_overview_page page;
		page.resize(1100, 760);
		page.show();
		auto projected = projection();
		projected.Body = "Fixes three crashes.\r\n\r\n- One\r\n- Two\r\n";
		page.present(projected);
		settle();
		auto* const edit     = page.description_edit_button();
		auto* const fullView = page.findChild<QToolButton*>(QStringLiteral("overviewFullView"));
		expect(edit != nullptr && !edit->isVisible() && !page.begin_description_edit(), "Edit is not offered until the composition offers it");
		auto saves = std::vector<qt::overview_description_save>{ };
		page.set_description_save_dispatch([&saves](const qt::overview_description_save& aSave) { saves.push_back(aSave); });
		page.set_description_editable(true);
		settle();
		expect(edit->isVisible() && fullView->isVisible() && in_reader_corner(page, *fullView) && edit->accessibleName() == QStringLiteral("Edit description")
				   && edit->geometry().right() < fullView->geometry().left() && edit->mapTo(&page, QPoint{ }).y() == fullView->mapTo(&page, QPoint{ }).y(),
			   "Edit sits in the reader's corner beside Full view, named for what it does");
		// [[BPR-1774]] owner's look: the pencil and its tooltip are enough; the
		// read-only description shows no caret.
		expect(edit->toolButtonStyle() == Qt::ToolButtonIconOnly && edit->toolTip() == QStringLiteral("Edit description") && page.body_view()->rendered_view()->cursorWidth() == 0
				   && page.body_view()->rendered_view()->isReadOnly(),
			   "Edit stays the pencil with its tooltip and the read-only description shows no caret");
		render_description_edit(page, "overview-edit-offered-light.png");

		edit->click();
		settle();
		auto* const editor = page.description_editor();
		expect(page.description_editing() && editor->isVisible() && !page.body_view()->isVisible() && editor->raw() == projected.Body && page.description_original() == projected.Body
				   && editor->hasFocus() && !edit->isVisible() && saves.empty(),
			   "Edit opens the Markdown editor in the reader's place on the body's exact bytes, focused, and sends nothing");
		auto cursor = editor->textCursor();
		cursor.movePosition(QTextCursor::End);
		editor->setTextCursor(cursor);
		editor->insertPlainText(QStringLiteral("- Three\n"));
		settle();
		expect(editor->raw() == projected.Body + "- Three\n", "typed text keeps the untouched bytes, CRLF included");
		render_description_edit(page, "overview-edit-open-light.png");
		page.description_save_button()->click();
		expect(saves.size() == 1U && saves.back().Original == projected.Body && saves.back().Body == projected.Body + "- Three\n", "Save hands the composition the original and the text, once");
		page.present_description_status(QStringLiteral("Saving the description to GitHub…"), true);
		expect(editor->isReadOnly() && !page.description_save_button()->isEnabled() && !page.description_cancel_button()->isEnabled()
				   && page.description_status()->text() == QStringLiteral("Saving the description to GitHub…"),
			   "while GitHub is asked the text and Save are held");
		page.description_save_button()->click();
		auto ctrlReturn = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier };
		QApplication::sendEvent(editor, &ctrlReturn);
		expect(saves.size() == 1U, "a held Save sends nothing more");

		// A conflict: the text stays until GitHub's newer one is asked for.
		page.present_description_status(QStringLiteral("Someone changed the description on GitHub since you began. Nothing was saved."));
		page.offer_newer_description("Newer from GitHub.\r\n");
		settle();
		expect(!editor->isReadOnly() && page.description_newer_button()->isVisible() && editor->raw() == projected.Body + "- Three\n", "a conflict keeps the typed text and offers the newer one");
		render_description_edit(page, "overview-edit-conflict-light.png");
		page.description_newer_button()->click();
		settle();
		expect(editor->raw() == "Newer from GitHub.\r\n" && page.description_original() == "Newer from GitHub.\r\n" && !page.description_newer_button()->isVisible(),
			   "taking the offer edits from GitHub's newer text, which is now the original");
		page.description_save_button()->click();
		expect(saves.size() == 2U && saves.back().Original == "Newer from GitHub.\r\n", "the next Save names the newer original");

		// A new read of the same pull request keeps the edit; Cancel ends it.
		auto refreshed = projected;
		refreshed.Body = "Changed under the editor.";
		page.present(refreshed);
		settle();
		expect(page.description_editing() && editor->raw() == "Newer from GitHub.\r\n" && !page.body_view()->isVisible(), "a refresh of the same pull request keeps the editor and its text");
		page.description_cancel_button()->click();
		settle();
		expect(!page.description_editing() && !editor->isVisible() && page.body_view()->isVisible() && edit->isVisible() && saves.size() == 2U,
			   "Cancel closes the editor and sends nothing");

		// Escape cancels; another pull request closes; withdrawal hides Edit.
		page.begin_description_edit();
		settle();
		auto escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(editor, &escape);
		settle();
		expect(!page.description_editing(), "Escape cancels the edit");
		page.begin_description_edit();
		auto other   = refreshed;
		other.Number = core::pull_request_number{ 1U };
		page.present(other);
		settle();
		expect(!page.description_editing() && page.body_view()->isVisible(), "another pull request ends the edit");
		page.set_description_editable(false);
		settle();
		expect(!edit->isVisible() && in_reader_corner(page, *fullView) && !page.begin_description_edit(), "withdrawn, Edit is gone and Full view keeps its corner");

		// No description: the author is offered Add a description.
		other.Body.clear();
		page.present(other);
		page.set_description_editable(true);
		settle();
		auto* const add = page.description_add_button();
		expect(add->isVisible() && page.findChild<QLabel*>(QStringLiteral("overviewNoDescription"))->isVisible() && !edit->isVisible(), "an empty description offers Add a description");
		add->click();
		settle();
		expect(page.description_editing() && editor->raw().empty() && !add->isVisible(), "Add a description opens the editor on nothing");
		page.close_description_editor();
		page.clear();
		settle();
		expect(!page.description_editable() && !add->isVisible(), "an invalidated page withdraws Edit");
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
		// [[BPR-1589]]. The roster is People's details.
		page.status_section_header(qt::overview_status_section::people)->setChecked(true);
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
		auto* const checks = page.findChild<QWidget*>(QStringLiteral("overviewChecksDisclosure"));
		scroll->ensureWidgetVisible(checks);
		settle();
		expect(scroll->viewport()->rect().contains(checks->mapTo(scroll->viewport(), QPoint{ 1, 1 })),
			   "check details remain reachable after the complete participant list");
	}
	/// \brief [[BPR-1363]]. Every column's left edge and width, in viewport
	/// coordinates.
	[[nodiscard]] auto column_geometry(const QTreeView& aView) -> std::vector<std::pair<int, int>>
	{
		auto geometry = std::vector<std::pair<int, int>>{ };
		for (auto column = 0; column < qt::overview_file_model::column_count; ++column) {
			geometry.emplace_back(aView.header()->sectionViewportPosition(column), aView.header()->sectionSize(column));
		}
		return geometry;
	}

	[[nodiscard]] auto describe(const std::vector<std::pair<int, int>>& aGeometry) -> std::string
	{
		auto text = std::string{ };
		for (const auto& [left, width] : aGeometry) {
			text += ' ' + std::to_string(left) + '+' + std::to_string(width);
		}
		return text;
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

	/// \brief A thousand-file review sizes its columns from the whole tree,
	/// yet takes the delegate's measure of only a bounded few cells, so the
	/// first Overview show stays quick on the GUI lane ([[BPR-1363]]: the
	/// widest name fits before it is ever scrolled into view, and scrolling
	/// moves no column).
	void test_column_sizing_measures_visible_files()
	{
		qt::pull_request_overview_page page;
		page.resize(1200, 800);
		auto projected = projection();
		projected.Files.clear();
		for (auto index = 0; index < 1000; ++index) {
			projected.Files.push_back(row(index == 999 ? "src/a_much_longer_file_name_than_any_other_999.cpp" : "src/file_" + std::to_string(index) + ".cpp",
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
			   "initial Overview measures a bounded few cells, not the complete thousand-file table");
		expect(page.file_model()->file_count() == 1000U, "column sizing retains every exact file row");
		const auto last = page.file_model()->index_for_path(QStringLiteral("src/a_much_longer_file_name_than_any_other_999.cpp"));
		expect(!view->viewport()->rect().intersects(view->visualRect(last))
				   && view->sizeHintForIndex(last).width() <= view->header()->sectionSize(qt::overview_file_model::file_column),
			   "the widest file name fits its column before it is ever shown");
		const auto before = column_geometry(*view);
		delegate->Paths.clear();
		view->scrollTo(last, QAbstractItemView::PositionAtBottom);
		settle();
		expect(view->viewport()->rect().intersects(view->visualRect(last)) && column_geometry(*view) == before && delegate->Paths.size() < 100U,
			   "scrolling to the last file moves no column and measures nothing more of the review");
	}

	/// \brief [[BPR-1363]]. The owner's nested-folders pull request
	/// (`nested-folders-v1`), file for file.
	[[nodiscard]] auto nested_folders_projection() -> application::pull_request_overview_projection
	{
		using operation = application::overview_file_operation;
		auto projected  = projection();
		projected.Files.clear();
		struct entry final
		{
			const char*   Path;
			operation     Operation;
			std::uint64_t Additions;
			std::uint64_t Deletions;
		};
		for (const auto& source : std::vector<entry>{
				 { "README.md", operation::modified, 2U, 1U },
				 { "docs/api/reference.md", operation::modified, 1U, 0U },
				 { "docs/api/v2/endpoints.md", operation::added, 4U, 0U },
				 { "docs/guide.md", operation::modified, 1U, 0U },
				 { "src/engine/renderer.cpp", operation::modified, 7U, 0U },
				 { "src/engine/renderer.h", operation::modified, 5U, 0U },
				 { "src/engine/shaders/basic.frag", operation::modified, 2U, 1U },
				 { "src/engine/shaders/basic.vert", operation::modified, 2U, 1U },
				 { "src/engine/shaders/post/bloom.frag", operation::added, 5U, 0U },
				 { "src/engine/shaders/post/tonemap.frag", operation::added, 5U, 0U },
				 { "src/main.cpp", operation::modified, 1U, 0U },
				 { "src/ui/widgets/button.cpp", operation::modified, 1U, 1U },
				 { "src/ui/widgets/button.h", operation::modified, 1U, 0U },
				 { "src/ui/widgets/slider.cpp", operation::added, 6U, 0U },
				 { "tests/engine/renderer_tests.cpp", operation::modified, 1U, 0U },
				 { "tests/ui/widgets/button_tests.cpp", operation::added, 11U, 0U },
				 { "tools/build/scripts/package.py", operation::modified, 2U, 1U },
			 }) {
			auto file      = row(source.Path, source.Operation, application::overview_change_since_review::unchanged, core::file_read_state::unread);
			file.Additions = source.Additions;
			file.Deletions = source.Deletions;
			projected.Files.push_back(std::move(file));
		}
		projected.Progress.FilesTotal = projected.Files.size();
		projected.Progress.FilesRead  = 0U;
		return projected;
	}

	/// \brief [[BPR-1363]]. The owner, 2026-10-09: "I dont like that opening
	/// this disclosure shift the column widths". Opening or closing a folder,
	/// all of them, a refresh, or a scroll never moves a column or changes its
	/// width; the File column holds every file and folder of the whole tree,
	/// shown or not; and a width the user drags stays.
	void test_folder_disclosure_never_moves_a_column()
	{
		qt::pull_request_overview_page page;
		page.resize(1100, 640);
		const auto projected = nested_folders_projection();
		page.present(projected);
		page.show();
		settle();
		auto* const view   = page.files_view();
		auto* const model  = page.file_model();
		auto* const header = view->header();
		page.collapse_folders_button()->click();
		settle();
		const auto closed = column_geometry(*view);
		const auto same   = [&](const std::string_view aStep) {
			settle();
			const auto now = column_geometry(*view);
			if (now != closed) {
				std::cerr << "columns before:" << describe(closed) << "\ncolumns after: " << describe(now) << '\n';
			}
			expect(now == closed, aStep);
		};
		auto folders = std::vector<QPersistentModelIndex>{ };
		for (auto folder = 0; folder < model->rowCount(); ++folder) {
			if (model->index(folder, 0).data(qt::overview_file_model::folder_role).toBool()) {
				folders.emplace_back(model->index(folder, 0));
			}
		}
		expect(folders.size() == 12U, "the nested-folders pull request has twelve folder rows");
		// The owner's step: open docs/api alone, then close it.
		const auto api = QPersistentModelIndex{ model->index_for_path(QStringLiteral("docs/api/reference.md")).parent() };
		view->setExpanded(api, true);
		same("opening docs/api moves no column and changes no width");
		view->setExpanded(api, false);
		same("closing docs/api again moves no column");
		for (const auto& folder : folders) {
			view->setExpanded(folder, true);
			same("opening each folder in turn moves no column");
		}
		const auto scrolls = view->verticalScrollBar()->maximum() > 0;
		expect(scrolls, "with every folder open the table scrolls, so a scroll bar can come and go");
		view->scrollToBottom();
		same("scrolling to the last file moves no column");
		for (const auto& folder : folders) {
			view->setExpanded(folder, false);
			same("closing each folder in turn moves no column");
		}
		page.expand_folders_button()->click();
		same("expand all moves no column");
		page.collapse_folders_button()->click();
		same("collapse all moves no column");
		page.present(projected);
		same("a refresh of the same pull request moves no column");

		// Every cell of the whole tree fits its column, whether its folder is
		// open or not, at the delegate's own measure.
		for (const auto& folder : folders) {
			for (auto column = 0; column < qt::overview_file_model::column_count; ++column) {
				if (column == qt::overview_file_model::note_column || column == qt::overview_file_model::comments_column) {
					continue;
				}
				expect(view->sizeHintForIndex(folder.sibling(folder.row(), column)).width() <= header->sectionSize(column),
					   "every folder cell fits its column");
				for (auto file = 0; file < model->rowCount(folder); ++file) {
					expect(view->sizeHintForIndex(model->index(file, column, folder)).width() <= header->sectionSize(column),
						   "every file cell fits its column, its folder closed");
				}
			}
		}

		// A width the user drags on the File column's edge stays.
		const auto fileColumn = qt::overview_file_model::file_column;
		const auto edge       = header->sectionViewportPosition(fileColumn) + header->sectionSize(fileColumn) - 1;
		const auto dragged    = header->sectionSize(fileColumn) + 40;
		const auto at         = [&](const int aX) { return QPointF{ static_cast<qreal>(aX), header->height() / 2.0 }; };
		const auto send       = [&](const QEvent::Type aType, const int aX, const Qt::MouseButtons aButtons) {
			QMouseEvent event{ aType, at(aX), QPointF{ header->viewport()->mapToGlobal(at(aX)) }, Qt::LeftButton, aButtons, Qt::NoModifier };
			QApplication::sendEvent(header->viewport(), &event);
		};
		send(QEvent::MouseMove, edge, Qt::NoButton);
		send(QEvent::MouseButtonPress, edge, Qt::LeftButton);
		send(QEvent::MouseMove, edge + 20, Qt::LeftButton);
		send(QEvent::MouseMove, edge + 40, Qt::LeftButton);
		send(QEvent::MouseButtonRelease, edge + 40, Qt::NoButton);
		settle();
		expect(header->sectionSize(fileColumn) == dragged, "a drag on the File column's edge widens it");
		const auto draggedGeometry = column_geometry(*view);
		expect(draggedGeometry[qt::overview_file_model::added_column].first == closed[qt::overview_file_model::added_column].first + 40,
			   "the columns after it move with the drag");
		page.expand_folders_button()->click();
		settle();
		page.collapse_folders_button()->click();
		settle();
		page.present(projected);
		settle();
		expect(column_geometry(*view) == draggedGeometry, "the dragged width stays through opening, closing and a refresh");
	}

	/// \brief [[BPR-1363]]. The leftmost and rightmost device pixels of a
	/// row band that differ from the band's commonest colour, its ground, in
	/// logical coordinates; nothing when the band is bare.
	[[nodiscard]] auto ink_span(const QImage& aImage, const QRect& aBand) -> std::optional<std::pair<qreal, qreal>>
	{
		const auto ratio  = aImage.devicePixelRatio();
		const auto top    = static_cast<int>(std::ceil(aBand.top() * ratio)) + 1;
		const auto bottom = static_cast<int>(std::floor((aBand.bottom() + 1) * ratio)) - 2;
		const auto left   = static_cast<int>(std::ceil(aBand.left() * ratio));
		const auto right  = std::min(aImage.width(), static_cast<int>(std::floor((aBand.right() + 1) * ratio))) - 1;
		auto       counts = std::map<QRgb, int>{ };
		for (auto y = top; y <= bottom; ++y) {
			for (auto x = left; x <= right; ++x) {
				++counts[aImage.pixel(x, y)];
			}
		}
		const auto ground = std::ranges::max_element(counts, { }, [](const auto& aEntry) { return aEntry.second; })->first;
		const auto inked  = [&](const int aX) {
			for (auto y = top; y <= bottom; ++y) {
				const auto pixel = aImage.pixel(aX, y);
				if (std::abs(qRed(pixel) - qRed(ground)) + std::abs(qGreen(pixel) - qGreen(ground)) + std::abs(qBlue(pixel) - qBlue(ground)) > 60) {
					return true;
				}
			}
			return false;
		};
		auto first = left;
		while (first <= right && !inked(first)) {
			++first;
		}
		if (first > right) {
			return std::nullopt;
		}
		auto last = right;
		while (!inked(last)) {
			--last;
		}
		return std::pair{ first / ratio, (last + 1) / ratio };
	}

	/// \brief [[BPR-1363]]. The owner, 2026-10-09: "The disclosure arrow not
	/// being the leftmost thing on the overview page for the folder rows is
	/// really bothering me." On every folder row the disclosure mark is the
	/// leftmost thing painted; v002, the folder's file count sits in its slot
	/// right after the mark and before the path; and the path starts where
	/// every file name starts.
	void test_folder_disclosure_is_the_leftmost_mark()
	{
		qt::pull_request_overview_page page;
		page.resize(1100, 640);
		page.present(nested_folders_projection());
		page.show();
		settle();
		auto* const view  = page.files_view();
		auto* const model = page.file_model();
		page.collapse_folders_button()->click();
		view->clearSelection();
		view->setCurrentIndex({ });
		settle();
		const auto image = view->viewport()->grab().toImage();
		auto       bold  = view->font();
		bold.setWeight(QFont::DemiBold);
		auto folders = 0;
		for (auto row = 0; row < model->rowCount(); ++row) {
			const auto folder = model->index(row, qt::overview_file_model::operation_column);
			if (!folder.data(qt::overview_file_model::folder_role).toBool()) {
				continue;
			}
			++folders;
			const auto path   = folder.siblingAtColumn(qt::overview_file_model::file_column);
			const auto count  = QString::number(folder.data(qt::overview_file_model::file_count_role).toULongLong());
			auto       option = QStyleOptionViewItem{ };
			option.initFrom(view);
			option.rect         = view->visualRect(folder);
			const auto arrow    = qt::overview_row_delegate::disclosure_rect(option);
			const auto rowBand  = QRect{ 0, option.rect.top(), view->viewport()->width(), option.rect.height() };
			const auto whole    = ink_span(image, rowBand);
			const auto inArrow  = ink_span(image, arrow);
			const auto pathCell = view->visualRect(path);
			const auto pathInk  = ink_span(image, pathCell);
			expect(whole && inArrow && pathInk, "a folder row paints its disclosure mark and its path");
			expect(whole->first == inArrow->first && whole->first < pathInk->first && inArrow->second <= pathCell.left() + 1,
				   "the disclosure mark is the leftmost thing painted on a folder row");
			expect(path.data().toString() == (folder.data(Qt::AccessibleTextRole).toString().section(QLatin1Char(','), 0, 0).mid(7))
					   && folder.data(Qt::AccessibleTextRole).toString().endsWith(QStringLiteral(", %1 files").arg(count)),
				   "the folder's accessible text still names its path and file count");
			expect(folder.data(Qt::DisplayRole).toString().isEmpty(), "the folder row's first cell has no display text; its count is painted");
			const auto slot     = qt::overview_row_delegate::count_rect(option, folder);
			const auto countInk = ink_span(image, slot);
			expect(slot.left() == arrow.right() + 1 && slot.right() < pathCell.left(), "the count's slot follows the disclosure mark and ends before the File column");
			expect(countInk && inArrow->second <= countInk->first && countInk->second <= pathInk->first,
				   "the folder's file count is drawn between its disclosure mark and its path");
		}
		expect(folders == 12, "every folder row was checked");
		const auto reference = model->index_for_path(QStringLiteral("docs/api/reference.md"));
		view->setExpanded(reference.parent(), true);
		settle();
		expect(view->visualRect(reference).left() == view->visualRect(reference.parent().siblingAtColumn(qt::overview_file_model::file_column)).left(),
			   "a file's name starts where its folder's path starts");
		if (const auto target = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !target.isEmpty()) {
			expect(page.grab().save(target, "PNG"), "the folder rows render is saved for inspection");
		}
	}

	/// \brief [[BPR-1363]] v002. The owner, session d827fdb7: "I would prefer
	/// if the number had a consistent column alignment, either put it as a
	/// column out to the right or move it to the left of the folder name."
	/// Every folder's count is right-aligned in one slot right after the
	/// disclosure mark, as wide as the largest count, at every path depth:
	/// the slots share one x, the counts' right edges line up, and the slot
	/// changes with the pull request's contents only.
	void test_folder_counts_share_one_column()
	{
		auto       projected = projection();
		const auto add       = [&](const std::string& aFolder, const std::size_t aFiles) {
			for (std::size_t file = 0; file < aFiles; ++file) {
				projected.Files.push_back(row(aFolder + "/file_" + std::to_string(file) + ".cpp", application::overview_file_operation::modified,
											  application::overview_change_since_review::unchanged, core::file_read_state::unread));
			}
		};
		projected.Files.clear();
		add("a", 1U);
		add("b/c/d/e", 12U);
		add("f/g", 120U);
		add("h/i/j", 7U);
		projected.Progress.FilesTotal = projected.Files.size();
		qt::pull_request_overview_page page;
		page.resize(1100, 640);
		page.present(projected);
		page.show();
		settle();
		auto* const view  = page.files_view();
		auto* const model = page.file_model();
		page.collapse_folders_button()->click();
		view->clearSelection();
		view->setCurrentIndex({ });
		settle();
		expect(model->largest_folder_file_count() == 120U, "the largest folder holds 120 files");
		const auto image  = view->viewport()->grab().toImage();
		auto       places = std::set<std::pair<int, int>>{ };
		auto       rights = std::vector<qreal>{ };
		auto       lefts  = std::map<std::size_t, qreal>{ };
		const auto digits = QFontMetrics{ view->font() }.horizontalAdvance(QStringLiteral("000"));
		for (auto row = 0; row < model->rowCount(); ++row) {
			const auto folder = model->index(row, qt::overview_file_model::operation_column);
			if (!folder.data(qt::overview_file_model::folder_role).toBool()) {
				continue;
			}
			auto option = QStyleOptionViewItem{ };
			option.initFrom(view);
			option.rect       = view->visualRect(folder);
			const auto arrow  = qt::overview_row_delegate::disclosure_rect(option);
			const auto slot   = qt::overview_row_delegate::count_rect(option, folder);
			const auto path   = view->visualRect(folder.siblingAtColumn(qt::overview_file_model::file_column));
			const auto whole  = ink_span(image, QRect{ 0, option.rect.top(), view->viewport()->width(), option.rect.height() });
			const auto inSlot = ink_span(image, slot);
			const auto name   = ink_span(image, path);
			places.emplace(slot.left(), slot.width());
			expect(whole && inSlot && name, "a folder row paints its mark, its count and its path");
			expect(whole->first < arrow.right() && whole->first < inSlot->first, "the disclosure mark is the leftmost thing on the row");
			expect(slot.left() == arrow.right() + 1 && inSlot->second <= name->first, "the count sits after the mark and before the name");
			expect(std::abs(slot.width() - digits) <= 1, "the slot is as wide as the largest count's three digits");
			rights.push_back(inSlot->second);
			lefts[folder.data(qt::overview_file_model::file_count_role).toULongLong()] = inSlot->first;
		}
		expect(rights.size() == 4U && places.size() == 1U, "the four folders' counts share one slot, at one x, at every path depth");
		const auto [low, high] = std::ranges::minmax(rights);
		expect(high - low <= 2.0, "the counts are right-aligned: their right edges line up");
		expect(lefts.at(120U) + 4.0 < lefts.at(12U) && lefts.at(12U) + 4.0 < lefts.at(1U), "a longer count reaches further left, not further right");

		// The slot follows the contents alone: opening a folder moves nothing,
		// and the File column starts where it did.
		const auto closed = column_geometry(*view);
		view->setExpanded(model->index_for_path(QStringLiteral("f/g/file_0.cpp")).parent(), true);
		settle();
		expect(column_geometry(*view) == closed, "opening the largest folder moves no column");
		const auto file = model->index_for_path(QStringLiteral("f/g/file_0.cpp"));
		expect(view->visualRect(file).left() == view->visualRect(file.parent().siblingAtColumn(qt::overview_file_model::file_column)).left(),
			   "a file's name starts where its folder's name starts");
		if (const auto target = qEnvironmentVariable("BPR_QT_COUNTS_RENDER_PATH"); !target.isEmpty()) {
			expect(page.grab().save(target, "PNG"), "the counts render is saved for inspection");
		}
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
		expect(!opened && !page.presented() && !page.entry_path(), "historical rows cannot enter the source workbench");
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

	constexpr auto lighting = "Engine/Renderer/HairStrandsLighting.cpp";
	constexpr auto voxel    = "Engine/Renderer/HairStrandsVoxelization.cpp";
	constexpr auto deep     = "Engine/Shaders/HairStrandsDeepShadow.usf";
	auto           all_files() -> const std::vector<std::string>&
	{
		static const auto paths = std::vector<std::string>{ lighting, voxel, deep };
		return paths;
	}

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
		for (auto row = 0; row * 0.5 < count.height(); ++row) {
			const auto y = count.top() + row * 0.5;
			for (auto column = 0; column * 0.5 < count.width(); ++column) {
				const auto x = count.left() + column * 0.5;
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

			// [[BPR-1585]]. An unselected row's tile mixes with the row's own
			// ground, the band on a banded row.
			auto rowPalette = view->palette();
			if (qt::overview_row_delegate::banded(lightRow, view)) {
				rowPalette.setColor(QPalette::Active, QPalette::Base, qt::overview_row_band(view->palette()));
			}
			const auto onRow = qt::overview_operation_mark_colours(application::overview_file_operation::modified, rowPalette, page.colours(), false);
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
							tile += distance(colour, onRow.Fill) < 12 ? 1 : 0;
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
		page.present_feedback(feedback_for(review, asked.back(), all_files()));
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
		page.present_feedback(feedback_for(review, asked.back(), all_files()));
		settle();
		chips = chips_of(page, cell(page, lighting));
		expect(chips.size() == 3U && chips[0].Label == core::feedback_label::issue && chips[0].Count == 1U && chips[1].Label == core::feedback_label::question
				   && chips[1].Count == 1U && chips[2].Kind == qt::overview_comment_chip::kind::other && chips[2].Count == 1U,
			   "issue 1, question 1 and Other 1 (the plain thread): disjoint, the resolved issue in none");
		expect(cell(page, lighting).data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 1, question 1, Other 1 open threads"),
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
		expect(cell(page, voxel).data(Qt::AccessibleTextRole).toString() == QStringLiteral("Other 1 open threads; 1 whose state is unknown"),
			   "the unknown resolution is in the accessible text, in plain words");
		const auto uncertainTip = page.uncertain_button()->toolTip();
		expect(page.uncertain_button()->isVisible() && page.uncertain_button()->text() == QStringLiteral("2")
				   && uncertainTip.startsWith(QStringLiteral("Not counted: 2 threads\n")) && uncertainTip.contains(QStringLiteral("• 1 whose state is unknown"))
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
										   asked.back(), all_files()));
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
		page.present_feedback(feedback_for(population(many), asked.back(), all_files()));
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
		page.present_feedback(feedback_for(review, asked.back(), all_files()));
		settle();
		chips = chips_of(page, cell(page, lighting));
		expect(chips[0].Label == core::feedback_label::issue && chips[0].Count == 2U && chips[0].States == core::feedback_state_counts{ 1U, 1U, 0U, 0U },
			   "in All the issue chip counts two, one open and one resolved");
		expect(cell(page, lighting).data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 2 (1 open, 1 resolved), Other 2 (2 open) threads"),
			   "and each count says its breakdown");

		// A stale answer, for a request no longer current, is dropped.
		page.present_feedback(feedback_for(review, request_for({ }, core::feedback_resolution_mode::unresolved), all_files()));
		expect(model->feedback()->Request.Mode == core::feedback_resolution_mode::all, "an answer to an earlier request is dropped");
		expect(resets == 0, "no count, chip or mode change resets the model");
		expect(page.body_view()->geometry() == bodyBefore, "the description keeps its geometry");

		// The Label key lists all twelve marks by name; Escape closes it. Since
		// [[BPR-1565]] it shows no `:xx:` code.
		page.show_label_key();
		settle();
		auto* const key  = page.label_key();
		auto* const list = key != nullptr ? key->findChild<QListWidget*>(QStringLiteral("overviewLabelKeyList")) : nullptr;
		expect(key != nullptr && key->isVisible() && list != nullptr && list->count() == 1 + 12 + 1 + 2 + 4 + 1 + 1 + 4 && list->item(4)->text() == QStringLiteral("issue")
				   && list->item(4)->data(Qt::AccessibleTextRole).toString().startsWith(QStringLiteral("issue: ")),
			   "the Label key names every mark, the state marks and the keys, with no code");
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
		page.present_feedback(feedback_for(review, asked.back(), all_files()));
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
		expect(tip != nullptr && tip->isVisible() && tip->text().startsWith(QStringLiteral("Other labels: 1 open thread here.\nLabels not ticked in Settings' Overview chips"))
				   && !tip->text().contains(QStringLiteral("chip of their own")) && tip->text().contains(QStringLiteral("press Enter")),
			   "the chip's tooltip comes up beside it, in plain words, naming Enter");
		expect(ringBefore == 0 && ring_pixels(page, *focused) > 12, "the chip the keyboard is on wears a ring in the selection's text colour; none before Tab");
		key(*view, Qt::Key_Tab);
		focused = page.focused_chip();
		tip     = qobject_cast<QLabel*>(qt::anchored_tooltip());
		expect(focused && focused->Kind == qt::overview_comment_chip::kind::uncertain && tip != nullptr
				   && tip->text().startsWith(QStringLiteral("Not counted: 1 thread\n• 1 whose state is unknown")),
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
		page.present_feedback(feedback_for(refreshed, asked.back(), all_files()));
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
		auto sorted  = feedback_for(refreshed, asked.back(), all_files());
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

	/// \brief [[BPR-1577]]. \p aWidget drawn over \p aGround at twice its
	/// size, whatever the screen's scale, so a word's stems cover whole
	/// pixels and its ink can be read from them.
	[[nodiscard]] auto snapshot(QWidget& aWidget, const QColor& aGround) -> QImage
	{
		auto image = QImage{ aWidget.size() * 2, QImage::Format_ARGB32_Premultiplied };
		image.setDevicePixelRatio(2.0);
		image.fill(aGround);
		aWidget.render(&image, QPoint{ }, QRegion{ }, QWidget::DrawChildren);
		return image;
	}

	/// \brief [[BPR-1577]]. What one look of the Open and All switch fails,
	/// read from its pixels over the page's ground: empty when both halves
	/// are the row's height with their words at 4.5:1 or more, the chosen half
	/// is a quiet chip (a border of 3:1 or more round a fill that stays a
	/// tint, never a block) and the other half has neither fill nor border.
	[[nodiscard]] auto switch_failures(QToolButton& aChosen, QToolButton& aOther, const QWidget& aRowButton, const QColor& aGround) -> std::vector<std::string>
	{
		auto       failures = std::vector<std::string>{ };
		const auto ground   = qt::to_appearance_color(aGround);
		const auto contrast = [&](const QColor& aColour) { return qt::contrast_ratio(qt::to_appearance_color(aColour), ground); };
		const auto near     = [&](const QColor& aColour) {
			return std::abs(aColour.red() - aGround.red()) <= 3 && std::abs(aColour.green() - aGround.green()) <= 3 && std::abs(aColour.blue() - aGround.blue()) <= 3;
		};
		for (auto* const half : { &aChosen, &aOther }) {
			const auto name  = half->objectName().toStdString();
			const auto image = snapshot(*half, aGround);
			const auto scale = image.devicePixelRatio();
			const auto at    = [&](const int aX, const int aY) { return image.pixelColor(static_cast<int>(aX * scale), static_cast<int>(aY * scale)); };
			const auto high  = half->height();
			const auto wide  = half->width();
			if (high != aRowButton.height()) {
				failures.push_back(name + " is " + std::to_string(high) + " px high beside the row's " + std::to_string(aRowButton.height()));
			}
			// The word: its strongest pixel right of any mark, inside the edges.
			auto strongest = 1.0;
			for (auto y = static_cast<int>(3 * scale); y < image.height() - static_cast<int>(3 * scale); ++y) {
				for (auto x = image.width() / 2; x < image.width() - static_cast<int>(3 * scale); ++x) {
					strongest = std::max(strongest, contrast(image.pixelColor(x, y)));
				}
			}
			if (strongest < 4.5) {
				failures.push_back(name + "'s word reads at " + std::to_string(strongest) + ":1");
			}
			// The edge midway along three sides, and the fill just inside it.
			const auto edges = std::array{ at(0, high / 2), at(wide - 1, high / 2), at(wide / 2, 0) };
			const auto fills = std::array{ at(3, high / 2), at(wide - 4, high / 2), at(wide / 2, 3) };
			if (half == &aChosen) {
				for (const auto& edge : edges) {
					if (contrast(edge) < 3.0) {
						failures.push_back(name + " is chosen with a border of " + std::to_string(contrast(edge)) + ":1, under 3:1");
						break;
					}
				}
				for (const auto& fill : fills) {
					if (near(fill) || contrast(fill) > 1.35) {
						failures.push_back(name + " is chosen with a fill of " + std::to_string(contrast(fill)) + ":1, not a quiet tint");
						break;
					}
				}
			} else if (!std::ranges::all_of(edges, near) || !std::ranges::all_of(fills, near)) {
				failures.push_back(name + " is not chosen but has a fill or a border");
			}
		}
		return failures;
	}

	/// \brief [[BPR-1577]]. The Open and All switch is as quiet as its row in
	/// every built-in theme: the row's height, its words at
	/// 4.5:1, the chosen half a quiet chip with a 3:1 border and the other
	/// bare; choosing
	/// moves nothing, and names, tooltips, Tab, Space and the dispatched mode
	/// are as before. The control is the switch as the style drew it, which
	/// the same reading refuses. With BPR_OVERVIEW_SWITCH_RENDER_ROOT set,
	/// each theme's row is saved there in both states.
	void test_counts_switch_is_quiet_in_every_theme()
	{
		const auto root = qEnvironmentVariable("BPR_OVERVIEW_SWITCH_RENDER_ROOT");
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto                     theme = std::string{ core::stable_name(profile.variant()) };
			qt::pull_request_overview_page page;
			profile.apply_to(page);
			page.adopt_appearance(profile);
			auto asked = std::vector<application::overview_feedback_request>{ };
			page.set_feedback_dispatch([&](const application::overview_feedback_request& aRequest) { asked.push_back(aRequest); });
			page.resize(1'000, 560);
			page.present(projection());
			page.show();
			settle();
			page.present_feedback(feedback_for(mixed_review(), asked.back(), all_files()));
			settle();
			auto* const open   = page.open_counts_button();
			auto* const all    = page.all_counts_button();
			const auto  ground = page.palette().color(QPalette::Active, QPalette::Window);
			expect(open->isVisible() && open->isChecked() && !all->isChecked(), theme + ": the switch shows with Open chosen");
			const auto openFailures = switch_failures(*open, *all, *page.label_key_button(), ground);
			for (const auto& failure : openFailures) {
				std::cerr << theme << ": " << failure << '\n';
			}
			expect(openFailures.empty(), theme + ": with Open chosen, Open alone is a quiet chip");

			// Names and tooltips as BPR-1089 accepted them.
			expect(open->objectName() == QStringLiteral("overviewModeOpen") && open->text() == QStringLiteral("Open") && open->accessibleName() == QStringLiteral("Open threads")
					   && open->toolTip() == QStringLiteral("Unresolved threads (default)") && !open->icon().isNull(),
				   theme + ": Open keeps its name, accessible name, tooltip and mark");
			expect(all->objectName() == QStringLiteral("overviewModeAll") && all->text() == QStringLiteral("All") && all->accessibleName() == QStringLiteral("All threads")
					   && all->toolTip() == QStringLiteral("Every thread: open, resolved, unknown"),
				   theme + ": All keeps its name, accessible name and tooltip");
			expect(open->isCheckable() && open->autoExclusive() && open->focusPolicy() == Qt::StrongFocus && all->isCheckable() && all->autoExclusive() && all->focusPolicy() == Qt::StrongFocus,
				   theme + ": both halves stay one exclusive, focusable choice");

			// The keyboard route is Qt's for an exclusive choice, as before: Tab
			// reaches the chosen half, Right moves to the other and Space
			// chooses it, Tab leaves the switch. The focused half shows its ring.
			const auto openWidth = open->width();
			const auto allWidth  = all->width();
			const auto before    = asked.size();
			const auto ringed    = [&](QToolButton& aHalf) {
				// Two pixels in: inside a chosen chip's border, on the ring.
				const auto image  = snapshot(aHalf, ground);
				const int  middle = aHalf.height() / 2;
				const auto ring   = image.pixelColor(static_cast<int>(2.0 * image.devicePixelRatio()), static_cast<int>(middle * image.devicePixelRatio()));
				return qt::contrast_ratio(qt::to_appearance_color(ring), qt::to_appearance_color(ground)) >= 3.0;
			};
			page.activateWindow();
			settle();
			open->setFocus(Qt::TabFocusReason);
			settle();
			expect(open->hasFocus() && ringed(*open) && !ringed(*all), theme + ": Tab onto Open shows its ring");
			auto right = QKeyEvent{ QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier };
			QApplication::sendEvent(open, &right);
			settle();
			expect(all->hasFocus() && ringed(*all), theme + ": Right moves to All, with its ring");
			auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
			auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
			QApplication::sendEvent(all, &press);
			QApplication::sendEvent(all, &release);
			settle();
			expect(asked.size() == before + 1U && asked.back().Mode == core::feedback_resolution_mode::all && all->isChecked() && !open->isChecked(),
				   theme + ": All asks once for every conversation, as before");
			auto tab = QKeyEvent{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
			QApplication::sendEvent(all, &tab);
			settle();
			expect(!open->hasFocus() && !all->hasFocus(), theme + ": Tab leaves the switch");
			page.present_feedback(feedback_for(mixed_review(), asked.back(), all_files()));
			if (auto* const focused = QApplication::focusWidget(); focused != nullptr) {
				focused->clearFocus();
			}
			settle();
			expect(open->width() == openWidth && all->width() == allWidth, theme + ": choosing moves neither half");
			const auto allFailures = switch_failures(*all, *open, *page.label_key_button(), ground);
			for (const auto& failure : allFailures) {
				std::cerr << theme << ": " << failure << '\n';
			}
			expect(allFailures.empty(), theme + ": with All chosen, All alone is a quiet chip");
			if (!root.isEmpty()) {
				const auto key = page.label_key_button()->mapTo(&page, QPoint{ page.label_key_button()->width(), 0 });
				const auto row = QRect{ QPoint{ open->mapTo(&page, QPoint{ 0, 0 }).x() - 12, key.y() - 6 }, QPoint{ key.x() + 12, key.y() + open->height() + 6 } };
				static_cast<void>(page.grab(row).save(QStringLiteral("%1/switch-%2-all.png").arg(root, QString::fromStdString(theme)), "PNG"));
				open->click();
				settle();
				static_cast<void>(page.grab(row).save(QStringLiteral("%1/switch-%2-open.png").arg(root, QString::fromStdString(theme)), "PNG"));
				page.present_feedback(feedback_for(mixed_review(), asked.back(), all_files()));
				settle();
				expect(page.grab().save(QStringLiteral("%1/overview-%2.png").arg(root, QString::fromStdString(theme)), "PNG"), "the full Overview page is saved beside the switch states");
			}

			// The control: the switch as the style drew it, a checked
			// auto-raise Open beside its mark and a text-only All.
			auto host = QWidget{ };
			profile.apply_to(host);
			auto* const layout   = new QHBoxLayout{ &host };
			auto* const oldOpen  = new QToolButton{ &host };
			auto* const oldAll   = new QToolButton{ &host };
			auto* const oldKey   = new QToolButton{ &host };
			auto        colours  = qt::default_feedback_state_colours(host.palette());
			const auto  markIcon = QIcon{ qt::feedback_state_mark_pixmap(core::feedback_resolution::unresolved, colours, host.devicePixelRatioF()) };
			for (auto* const button : { oldOpen, oldAll, oldKey }) {
				button->setAutoRaise(true);
				button->setIconSize(QSize{ 16, 16 });
				layout->addWidget(button);
			}
			for (auto* const button : { oldOpen, oldAll }) {
				button->setCheckable(true);
				button->setAutoExclusive(true);
			}
			oldOpen->setObjectName(QStringLiteral("controlOpen"));
			oldOpen->setText(QStringLiteral("Open"));
			oldOpen->setIcon(markIcon);
			oldOpen->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			oldAll->setObjectName(QStringLiteral("controlAll"));
			oldAll->setText(QStringLiteral("All"));
			oldAll->setToolButtonStyle(Qt::ToolButtonTextOnly);
			oldKey->setIcon(markIcon);
			oldOpen->setChecked(true);
			host.show();
			settle();
			expect(!switch_failures(*oldOpen, *oldAll, *oldKey, host.palette().color(QPalette::Active, QPalette::Window)).empty(),
				   theme + ": the control fails: the style-drawn switch is not this chip");
		}
	}

	/// \brief [[BPR-1577]]. Saves the Files row's switch, Open chosen and All
	/// chosen, in each built-in theme under \p aRoot, for the owner's review.
	/// \details Run on the platform the owner uses (cocoa on macOS, with
	/// QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1), the style is the
	/// native one the application gets, while the page is never put on screen
	/// (Qt::WA_DontShowOnScreen) and nothing of the desktop is read. The colour
	/// scheme follows the theme, as the system's appearance would.
	void render_counts_switch(const QString& aRoot)
	{
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto dark = mode == core::appearance_mode::dark || mode == core::appearance_mode::high_contrast_dark;
			QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
			settle();
			const auto                     theme = QString::fromStdString(std::string{ core::stable_name(profile.variant()) });
			qt::pull_request_overview_page page;
			page.setAttribute(Qt::WA_DontShowOnScreen, true);
			profile.apply_to(page);
			page.adopt_appearance(profile);
			auto asked = std::vector<application::overview_feedback_request>{ };
			page.set_feedback_dispatch([&](const application::overview_feedback_request& aRequest) { asked.push_back(aRequest); });
			page.resize(1'000, 560);
			page.present(projection());
			page.show();
			settle();
			page.present_feedback(feedback_for(mixed_review(), asked.back(), all_files()));
			settle();
			auto* const open = page.open_counts_button();
			auto* const all  = page.all_counts_button();
			const auto  key  = page.label_key_button()->mapTo(&page, QPoint{ page.label_key_button()->width(), 0 });
			const auto  row  = QRect{ QPoint{ open->mapTo(&page, QPoint{ 0, 0 }).x() - 12, key.y() - 6 }, QPoint{ key.x() + 12, key.y() + open->height() + 6 } };
			static_cast<void>(page.grab(row).save(QStringLiteral("%1/%2-open.png").arg(aRoot, theme), "PNG"));
			all->click();
			page.present_feedback(feedback_for(mixed_review(), asked.back(), all_files()));
			settle();
			static_cast<void>(page.grab(row).save(QStringLiteral("%1/%2-all.png").arg(aRoot, theme), "PNG"));
			std::cout << "switch-render " << theme.toStdString() << " style=" << page.style()->name().toStdString() << '\n';
		}
	}

	/// \brief [[BPR-1094]]. The chips at the scale this row runs at (1.0,
	/// 1.25 or 2.0), in every built-in theme, with Icons
	/// and with Letters (RFC-BPR-0050 R31, R33):
	/// The palettes are the four built-in appearance profiles (light, dark and
	/// both high-contrast themes), applied as the application applies them.
	/// - identity: each label's chip draws its own mark through the page's
	///   `feedback_mark_theme`, Letters draws differently from Icons, and the
	///   cell's accessible text names every label;
	/// - state and coverage: Open and All say which they are, a conversation
	///   of unknown state shows ?, and All gives each count's breakdown;
	/// - full content: one-, two- and three-digit counts sit whole inside
	///   their chips and the cell, and the chips folded into +n are named in
	///   the cell's accessible text and its menu;
	/// - keyboard and scroll: Tab reaches a chip with a ring, Right moves on,
	///   Enter lists it, a refresh keeps the chip and the scroll position, and
	///   End scrolls the last row into view, where Tab still reaches its chip.
	/// With BPR_OVERVIEW_CHIPS_RENDER_ROOT set, each look is saved there.
	void test_chips_read_at_this_scale_in_every_theme_and_style()
	{
		using core::feedback_label;
		using core::feedback_resolution;
		const auto scaleName = QString::number(qRound(qApp->devicePixelRatio() * 100.0));
		auto       projected = projection();
		projected.Files.clear();
		auto paths = std::vector<std::string>{ };
		for (auto index = 0; index < 60; ++index) {
			const auto path = "Engine/Part" + std::to_string(index % 3) + "/File" + (index < 10 ? "0" : "") + std::to_string(index) + ".cpp";
			paths.push_back(path);
			projected.Files.push_back(row(path, application::overview_file_operation::modified, application::overview_change_since_review::unchanged, core::file_read_state::seen));
		}
		std::ranges::sort(projected.Files, { }, &application::overview_file_row::Path);
		std::ranges::sort(paths);
		const auto& busy    = paths[0];
		const auto& unknown = paths[3];
		const auto& last    = paths.back();
		// busy: 128 open and 64 resolved issues, 12 questions, a nitpick and
		// three plain conversations; unknown: one issue of unknown state; the
		// last file: an issue and a question; and a praise on no file.
		auto       units = std::vector<unit_spec>{ };
		const auto add   = [&](const std::string& aPrefix, const std::size_t aCount, const std::optional<feedback_label> aLabel, const feedback_resolution aState, const std::string& aPath) {
			for (std::size_t index = 0U; index < aCount; ++index) {
				units.push_back({ .Id = "review-thread:" + aPrefix + std::to_string(index), .Label = aLabel, .State = aState, .Path = aPath });
			}
		};
		add("io", 128U, feedback_label::issue, feedback_resolution::unresolved, busy);
		add("ir", 64U, feedback_label::issue, feedback_resolution::resolved, busy);
		add("q", 12U, feedback_label::question, feedback_resolution::unresolved, busy);
		add("n", 1U, feedback_label::nitpick, feedback_resolution::unresolved, busy);
		add("p", 3U, std::nullopt, feedback_resolution::unresolved, busy);
		add("u", 1U, feedback_label::issue, feedback_resolution::unknown, unknown);
		add("li", 1U, feedback_label::issue, feedback_resolution::unresolved, last);
		add("lq", 1U, feedback_label::question, feedback_resolution::unresolved, last);
		add("pr", 1U, feedback_label::praise, feedback_resolution::unresolved, { });
		const auto review = population(units);
		// Every label once on the busy file, for all twelve chips.
		auto every = std::vector<unit_spec>{ };
		auto all12 = std::vector<feedback_label>{ };
		for (std::size_t label = 0U; label < core::feedback_label_count; ++label) {
			const auto which = core::feedback_label_catalog()[label].Label;
			all12.push_back(which);
			for (std::size_t copy = 0U; copy <= label; ++copy) {
				every.push_back({ .Id = "review-thread:e" + std::to_string(label) + "-" + std::to_string(copy), .Label = which, .State = feedback_resolution::unresolved, .Path = busy });
			}
		}
		every.push_back({ .Id = "review-thread:eplain", .Label = std::nullopt, .State = feedback_resolution::unresolved, .Path = busy });
		const auto twelve = population(every);

		const auto crop = [](const QImage& aImage, const QRectF& aBox) {
			const auto scale = aImage.devicePixelRatio();
			return aImage.copy(QRect{ static_cast<int>(aBox.left() * scale), static_cast<int>(aBox.top() * scale), static_cast<int>(16.0 * scale), static_cast<int>(16.0 * scale) });
		};
		const auto root = qEnvironmentVariable("BPR_OVERVIEW_CHIPS_RENDER_ROOT");
		// The four built-in appearance profiles, as the application applies them.
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto themeName  = std::string{ core::stable_name(profile.variant()) };
			auto       issueMarks = std::vector<QImage>{ };
			for (const auto style : { qt::feedback_mark_style::icons, qt::feedback_mark_style::letters }) {
				const auto                     name = themeName + (style == qt::feedback_mark_style::icons ? " icons" : " letters") + " at " + scaleName.toStdString() + "%";
				qt::pull_request_overview_page page;
				profile.apply_to(page);
				page.adopt_appearance(profile);
				auto marks  = qt::feedback_mark_theme{ };
				marks.Tints = qt::feedback_label_tints_of(profile.palette());
				marks.Style = style;
				page.set_label_marks(marks);
				auto asked   = std::vector<application::overview_feedback_request>{ };
				auto drilled = std::vector<application::overview_drill_down>{ };
				page.set_feedback_dispatch([&](const application::overview_feedback_request& aRequest) { asked.push_back(aRequest); });
				page.set_drill_down_dispatch([&](const application::overview_drill_down& aDown) { drilled.push_back(aDown); });
				page.resize(1'000, 560);
				page.present(projected);
				page.set_label_columns(*core::make_feedback_label_columns(std::vector{ feedback_label::issue, feedback_label::question, feedback_label::nitpick }));
				page.show();
				settle();
				page.present_feedback(feedback_for(review, asked.back(), paths));
				settle();
				auto* const view = page.files_view();

				// [[BPR-1350]]. The Files heading shortens with an ellipsis and
				// never runs under its row's controls or off the page; the
				// whole sentence stays its text and accessible name, and is its
				// tooltip whenever it is shortened.
				{
					auto* const heading   = page.files_heading();
					const auto  whole     = heading->text();
					auto        shortened = false;
					for (const auto width : { 1'000, 800, 600, 420 }) {
						page.resize(width, 560);
						settle();
						const auto box   = QRect{ heading->mapTo(&page, QPoint{ 0, 0 }), heading->size() };
						const auto label = name + " at " + std::to_string(width) + " px";
						expect(heading->text() == whole && whole.startsWith(QStringLiteral("Files: ")) && heading->accessibleName() == whole,
							   label + ": the heading keeps the whole sentence as its text and accessible name");
						expect(box.left() >= 0 && box.right() < page.width(), label + ": the heading stays on the page");
						for (auto* const control : page.findChildren<QAbstractButton*>()) {
							if (!control->isVisible() || view->isAncestorOf(control)) {
								continue;
							}
							const auto other = QRect{ control->mapTo(&page, QPoint{ 0, 0 }), control->size() };
							if (other.center().y() >= box.top() && other.center().y() <= box.bottom()) {
								expect(!box.intersects(other), label + ": the heading runs under no control in its row (" + control->objectName().toStdString() + ")");
							}
						}
						const auto fits = heading->fontMetrics().horizontalAdvance(whole) <= heading->contentsRect().width();
						expect(fits ? !heading->toolTip().contains(whole) : heading->toolTip().startsWith(whole),
							   label + (fits ? ": a whole heading has no tooltip of itself" : ": a shortened heading's tooltip is the whole sentence"));
						shortened = shortened || !fits;
						if (!root.isEmpty() && style == qt::feedback_mark_style::icons) {
							static_cast<void>(page.grab().save(QStringLiteral("%1/files-heading-%2-%3-%4.png").arg(root, QString::fromStdString(themeName), scaleName, QString::number(width)), "PNG"));
						}
					}
					expect(shortened, name + ": the narrowest page shortens the heading");
					page.resize(1'000, 560);
					settle();
				}

				// Identity: the label chips, their marks and their names.
				const auto chips = chips_of(page, cell(page, busy));
				expect(chips.size() == 4U && chips[0].Label == feedback_label::issue && chips[0].Count == 128U && chips[1].Label == feedback_label::question && chips[1].Count == 12U
						   && chips[2].Label == feedback_label::nitpick && chips[2].Count == 1U && chips[3].Kind == qt::overview_comment_chip::kind::other && chips[3].Count == 3U,
					   name + ": issue 128, question 12, nitpick 1 and Other 3 on the busy file");
				expect(cell(page, busy).data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 128, question 12, nitpick 1, Other 3 open threads"),
					   name + ": the cell's accessible text names each label and its count");
				view->clearSelection();
				view->setCurrentIndex(QModelIndex{ });
				settle();
				const auto image = view->viewport()->grab().toImage();
				const auto issue = crop(image, chips[0].Box);
				expect(issue != crop(image, chips[1].Box) && issue != crop(image, chips[2].Box) && crop(image, chips[1].Box) != crop(image, chips[2].Box),
					   name + ": issue, question and nitpick each draw a mark of their own");
				issueMarks.push_back(issue);
				for (std::size_t one = 0U; one < all12.size(); ++one) {
					const auto first = page.count_mark({ .Kind = core::feedback_count_kind::dedicated, .Label = all12[one] }, true).toImage();
					expect(first == qt::feedback_mark_pixmap(all12[one], { }, qt::feedback_mark_colours_for(marks, all12[one], page.palette()), page.devicePixelRatioF()).toImage(),
						   name + ": a chip's mark is the label's mark in the page's mark theme");
					for (auto two = one + 1U; two < all12.size(); ++two) {
						expect(first != page.count_mark({ .Kind = core::feedback_count_kind::dedicated, .Label = all12[two] }, true).toImage(),
							   name + ": the twelve marks are told apart: " + std::string{ core::stable_name(all12[one]) } + " and " + std::string{ core::stable_name(all12[two]) });
					}
				}

				// Full content: each count's digits fit inside its chip, and every
				// chip inside the cell, without overlapping its neighbour.
				const auto cellRect = view->visualRect(cell(page, busy));
				// The chips' digits are the files' font at 85%, demibold.
				auto chipFont = view->font();
				chipFont.setPointSizeF(chipFont.pointSizeF() * 0.85);
				chipFont.setWeight(QFont::DemiBold);
				const auto digits = QFontMetricsF{ chipFont };
				for (std::size_t index = 0U; index < chips.size(); ++index) {
					const auto& chip = chips[index];
					expect(QRectF{ cellRect }.contains(chip.Box), name + ": every chip stays inside its cell");
					expect(chip.Box.width() >= 16.0 + 3.0 + digits.horizontalAdvance(QString::number(static_cast<qulonglong>(chip.Count))),
						   name + ": a chip is wide enough for all its digits");
					if (index != 0U) {
						expect(chips[index - 1U].Box.right() <= chip.Box.left(), name + ": chips do not overlap");
					}
				}

				// State and coverage: Open and All say which they are; unknown is ?.
				expect(page.open_counts_button()->isVisible() && page.open_counts_button()->isChecked() && !page.open_counts_button()->accessibleName().isEmpty()
						   && !page.open_counts_button()->icon().isNull() && page.all_counts_button()->isVisible() && !page.all_counts_button()->isChecked()
						   && !page.all_counts_button()->accessibleName().isEmpty(),
					   name + ": Open is on and says so; All is offered");
				const auto unknownChips = chips_of(page, cell(page, unknown));
				expect(unknownChips.size() == 1U && unknownChips[0].Kind == qt::overview_comment_chip::kind::uncertain && cell(page, unknown).data().toString() == QStringLiteral("? 1")
						   && cell(page, unknown).data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("whose state is unknown")),
					   name + ": an issue of unknown state shows ?, with its reason in words, never a clean zero");
				expect(page.uncertain_button()->isVisible() && page.uncertain_button()->text() == QStringLiteral("1"), name + ": the ? chip beside the switch counts it");
				if (!root.isEmpty()) {
					expect(page.grab().save(root + QStringLiteral("/chips-%1-%2-%3-open.png").arg(QString::fromStdString(themeName), style == qt::feedback_mark_style::icons ? QStringLiteral("icons") : QStringLiteral("letters"), scaleName)),
						   name + ": the render is saved");
				}

				// Keyboard: Tab onto the busy file's issue chip, Right to the
				// question, Enter lists it; a refresh keeps the chip.
				page.activateWindow();
				view->setFocus();
				view->setCurrentIndex(cell(page, busy));
				settle();
				key(*view, Qt::Key_Tab);
				expect(page.focused_chip() && page.focused_chip()->Label == feedback_label::issue && ring_pixels(page, *page.focused_chip()) > 12,
					   name + ": Tab steps onto the issue chip, which wears a ring");
				key(*view, Qt::Key_Right);
				key(*view, Qt::Key_Return);
				expect(!drilled.empty() && drilled.back().Path == busy && drilled.back().Target == core::feedback_count_target{ .Kind = core::feedback_count_kind::dedicated, .Label = feedback_label::question },
					   name + ": Right then Enter lists exactly the busy file's questions");
				page.present_feedback(feedback_for(review, asked.back(), paths));
				settle();
				expect(page.focused_chip() && page.focused_chip()->Label == feedback_label::question && view->currentIndex() == cell(page, busy),
					   name + ": a refresh keeps the cell and the chip the keyboard is on");

				// Scroll: End brings the last row into view, Tab still reaches its
				// chip, and a refresh keeps the position.
				key(*view, Qt::Key_Escape);
				key(*view, Qt::Key_End);
				const auto bottom = view->currentIndex();
				expect(view->verticalScrollBar()->value() > 0 && view->viewport()->rect().contains(view->visualRect(bottom)),
					   name + ": End scrolls the last row fully into view");
				key(*view, Qt::Key_Tab);
				expect(page.focused_chip() && view->viewport()->rect().contains(page.focused_chip()->Box.toAlignedRect()), name + ": Tab reaches the last row's chip, in view");
				const auto position = view->verticalScrollBar()->value();
				const auto maximum  = view->verticalScrollBar()->maximum();
				const auto requests = asked.size();
				expect(position == maximum, name + ": End leaves the files at the bottom");
				page.present_feedback(feedback_for(review, asked.back(), paths));
				settle();
				// [[BPR-1094]]. A refresh keeps the reader's place: at the bottom
				// it stays at the bottom, and the chip and its row stay in view.
				// macOS lays the first present out 1 px taller than the refresh
				// (maximum 47, then 46, at 20fb3889e) and clamps the value with
				// it, so the bottom is asserted, not the pixel.
				std::cout << "bpr-1094 refresh " << name << " value=" << position << "->" << view->verticalScrollBar()->value() << " maximum=" << maximum << "->"
						  << view->verticalScrollBar()->maximum() << " chip=" << (page.focused_chip() ? "kept" : "null") << '\n';
				expect(view->verticalScrollBar()->value() == view->verticalScrollBar()->maximum() && std::abs(view->verticalScrollBar()->maximum() - maximum) <= 1
						   && page.focused_chip() && view->viewport()->rect().contains(page.focused_chip()->Box.toAlignedRect()),
					   name + ": a refresh at the bottom stays at the bottom, with the chip in view");
				// Away from the bottom, a refresh keeps the exact position.
				const auto middle = view->verticalScrollBar()->maximum() / 2;
				view->verticalScrollBar()->setValue(middle);
				settle();
				page.present_feedback(feedback_for(review, asked.back(), paths));
				settle();
				expect(view->verticalScrollBar()->value() == middle, name + ": a refresh in the middle keeps the scroll position");
				view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
				settle();
				const auto  lowest = view->verticalScrollBar()->value();
				QWheelEvent wheel{ QPointF{ 40, 40 }, view->viewport()->mapToGlobal(QPointF{ 40, 40 }), QPoint{ }, QPoint{ 0, 360 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
				QApplication::sendEvent(view->viewport(), &wheel);
				settle();
				expect(view->verticalScrollBar()->value() < lowest && asked.size() == requests, name + ": the wheel scrolls the files and asks the host for nothing");

				// All: each count gives its breakdown, the three-digit total whole.
				page.all_counts_button()->click();
				page.present_feedback(feedback_for(review, asked.back(), paths));
				settle();
				expect(page.all_counts_button()->isChecked() && cell(page, busy).data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 192 (128 open, 64 resolved), question 12 (12 open), nitpick 1 (1 open), Other 3 (3 open) threads"),
					   name + ": All says each count's breakdown");
				const auto allChips = chips_of(page, cell(page, busy));
				expect(!allChips.empty() && allChips[0].Count == 192U && QRectF{ view->visualRect(cell(page, busy)) }.contains(allChips[0].Box), name + ": 192 sits whole in its chip");

				// All twelve: the chips that fit, then +n; the cell's accessible
				// text and its menu name every label in full.
				page.choose_mode(core::feedback_resolution_mode::unresolved);
				page.set_label_columns(*core::make_feedback_label_columns(all12));
				page.present_feedback(feedback_for(twelve, asked.back(), paths));
				settle();
				const auto many = chips_of(page, cell(page, busy));
				const auto more = std::ranges::find(many, qt::overview_comment_chip::kind::more, &qt::overview_comment_chip::Kind);
				expect(more != many.end() && static_cast<std::size_t>(more - many.begin()) + more->Folded.size() == 13U, name + ": twelve chips and Other fold into +n, none lost");
				const auto spoken = cell(page, busy).data(Qt::AccessibleTextRole).toString();
				auto*      menu   = page.make_comments_menu(cell(page, busy));
				expect(menu != nullptr, name + ": the cell has a menu");
				for (const auto label : all12) {
					const auto word = QString::fromUtf8(std::string{ core::stable_name(label) }.c_str());
					expect(spoken.contains(word + QLatin1Char(' ')), name + ": the accessible text names " + word.toStdString());
					expect(std::ranges::any_of(menu->actions(), [&](QAction* aAction) { return aAction->text().startsWith(word + QLatin1Char(':')); }),
						   name + ": the menu names " + word.toStdString() + " in full");
				}
				delete menu;
				if (!root.isEmpty()) {
					view->scrollToTop();
					settle();
					expect(page.grab().save(root + QStringLiteral("/chips-%1-%2-%3-twelve.png").arg(QString::fromStdString(themeName), style == qt::feedback_mark_style::icons ? QStringLiteral("icons") : QStringLiteral("letters"), scaleName)),
						   name + ": the twelve-chip render is saved");
				}
				page.hide();
			}
			expect(issueMarks.size() == 2U && issueMarks[0] != issueMarks[1], themeName + ": Letters draws the issue chip differently from Icons");
		}
	}

	/// \brief [[BPR-1339]]. The read column shows the row's mark: an accepted
	/// file says and draws accepted whatever its read state, in a mark of its
	/// own, and a receipt update that only accepts moves it. The owner,
	/// 2026-10-08: "Accepted is stronger than read or unread".
	void test_the_read_column_shows_the_mark()
	{
		using mark       = core::file_progress_mark;
		auto projected   = projection();
		projected.Files  = { };
		const auto marks = std::vector<std::pair<std::string, std::optional<mark>>>{
			{ "none", std::nullopt }, { "seen", mark::seen }, { "read", mark::read }, { "accepted", mark::accepted }, { "excluded", mark::excluded }
		};
		for (const auto& [name, value] : marks) {
			const auto read = value == std::optional{ mark::read } ? std::optional{ core::file_read_state::read }
				: value.has_value()                                ? std::optional{ core::file_read_state::seen }
																   : std::nullopt;
			auto       file = row("Marks/" + name + ".cpp", application::overview_file_operation::modified, application::overview_change_since_review::unchanged, read);
			file.Mark       = value;
			projected.Files.push_back(file);
		}
		qt::pull_request_overview_page page;
		page.resize(1'100, 700);
		page.present(projected);
		page.show();
		settle();
		auto* const model = page.file_model();
		auto* const view  = page.files_view();
		const auto  cell  = [&](const std::string& aName) {
			return model->index_for_path(QString::fromStdString("Marks/" + aName + ".cpp")).siblingAtColumn(qt::overview_file_model::read_column);
		};
		for (const auto& [name, value] : marks) {
			const auto word = name == "none" ? QStringLiteral("unread") : QString::fromStdString(name);
			expect(cell(name).data(Qt::AccessibleTextRole).toString() == word && cell(name).data(Qt::ToolTipRole).toString() == word,
				   "the read column says the row's mark: " + name);
			expect(cell(name).siblingAtColumn(0).data(qt::overview_file_model::progress_role).toInt() == (value.has_value() ? static_cast<int>(*value) : -1),
				   "the row carries its mark: " + name);
		}
		expect(cell("accepted").siblingAtColumn(0).data(qt::overview_file_model::read_role).toInt() == static_cast<int>(core::file_read_state::seen),
			   "the accepted row keeps its read state underneath");

		const auto ratio = view->viewport()->grab().toImage().devicePixelRatio();
		const auto crop  = [&](const QImage& aImage, const std::string& aName) {
			const auto rect = view->visualRect(cell(aName));
			return aImage.copy(QRectF{ rect.left() * ratio, rect.top() * ratio, 26.0 * ratio, rect.height() * ratio }.toRect());
		};
		// [[BPR-1331]] v003. Whether a cell paints a colour: a pixel within a
		// small tolerance of it, since a ring ten pixels across is all
		// antialiased edge at 1x.
		const auto near = [](const QColor& aOne, const QColor& aTwo, const int aTolerance) {
			return qAbs(aOne.red() - aTwo.red()) <= aTolerance && qAbs(aOne.green() - aTwo.green()) <= aTolerance && qAbs(aOne.blue() - aTwo.blue()) <= aTolerance;
		};
		const auto paints = [&near](const QImage& aCell, const QColor& aColour) {
			for (auto y = 0; y < aCell.height(); ++y) {
				for (auto x = 0; x < aCell.width(); ++x) {
					if (near(aCell.pixelColor(x, y), aColour, 12)) {
						return true;
					}
				}
			}
			return false;
		};
		// The accepted mark: a green disc round a tick cut out of it, so
		// something far from green shows within three pixels of its centre.
		const auto tickCut = [&](const QImage& aCell) {
			const auto green  = page.colours().Progress.Accepted;
			const auto middle = QPointF{ 13.0, std::floor(((static_cast<qreal>(aCell.height()) / ratio) - 1.0) / 2.0) };
			auto       cut    = false;
			for (auto row = -6; row <= 6; ++row) {
				const auto y = middle.y() + row * 0.5;
				for (auto column = -6; column <= 6; ++column) {
					const auto x = middle.x() + column * 0.5;
					cut          = cut || !near(aCell.pixelColor(QPointF{ x * ratio, y * ratio }.toPoint()), green, 80);
				}
			}
			return paints(aCell, green) && cut;
		};
		const auto image = view->viewport()->grab().toImage();
		const auto root  = qEnvironmentVariable("BPR_OVERVIEW_MARK_RENDER_ROOT");
		if (!root.isEmpty()) {
			const auto rows = view->visualRect(cell("none")).united(view->visualRect(cell("excluded")));
			expect(view->viewport()->grab(QRect{ 0, rows.top() - 2, 560, rows.height() + 4 }).save(root + QStringLiteral("/overview-marks.png")), "the marks render is saved");
		}
		for (std::size_t one = 0U; one < marks.size(); ++one) {
			for (auto two = one + 1U; two < marks.size(); ++two) {
				expect(crop(image, marks[one].first) != crop(image, marks[two].first), "each mark draws differently: " + marks[one].first + " and " + marks[two].first);
			}
		}
		expect(tickCut(crop(image, "accepted")) && !tickCut(crop(image, "read")), "the accepted mark is a green disc with a tick cut out of it");
		// [[BPR-1331]] v003. The owner on v002: "it should get some color. All
		// green aint the best either." The column paints the review bar's
		// family, each state in its own colour.
		const auto& progress = page.colours().Progress;
		expect(progress == qt::builtin_progress_mark_colours(core::appearance_variant::light), "before a theme arrives the marks take the built-in light colours");
		expect(paints(crop(image, "none"), progress.Neutral) && paints(crop(image, "seen"), progress.Seen) && paints(crop(image, "read"), progress.Read)
				   && paints(crop(image, "accepted"), progress.Accepted) && paints(crop(image, "excluded"), progress.Neutral)
				   && paints(crop(image, "excluded"), progress.Excluded),
			   "each mark is painted in its state's colour: neutral unread, amber seen, blue read, green accepted, a red strike excluded");
		expect(!paints(crop(image, "read"), progress.Accepted) && !paints(crop(image, "seen"), progress.Accepted) && !paints(crop(image, "none"), progress.Accepted),
			   "only the accepted mark is green");

		if (!root.isEmpty()) {
			const auto rows = view->visualRect(cell("none")).united(view->visualRect(cell("excluded")));
			// [[BPR-1331]] v003. The built-in light and dark themes, as the
			// composition hands them over.
			for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
				auto settings = qt::default_appearance_settings();
				settings.Mode = mode;
				auto themed   = qt::appearance_profile{ settings };
				themed.apply_to(page);
				page.adopt_appearance(themed);
				settle();
				expect(page.colours().Progress == qt::progress_mark_colours_for(themed), "the page adopts the theme's mark colours");
				expect(view->viewport()->grab(QRect{ 0, rows.top() - 2, 560, rows.height() + 4 }).save(root + QStringLiteral("/overview-marks-%1.png").arg(name)), "the themed marks render is saved");
			}
		}

		// An update that only accepts moves the row and its mark.
		auto update     = application::overview_receipt_update{ };
		auto changed    = projected.Files[2];
		changed.Mark    = mark::accepted;
		update.Rows     = { changed };
		update.Progress = projected.Progress;
		page.apply_receipts(update);
		settle();
		expect(cell("read").data(Qt::AccessibleTextRole).toString() == QStringLiteral("accepted"), "a receipt update that accepts a read file shows accepted");
		expect(tickCut(crop(view->viewport()->grab().toImage(), "read")), "and draws the accepted mark");
	}

	/// \brief [[BPR-1585]]. The nested-folders pull request with
	/// src/engine/shaders holding six files, so turns run past two, and one
	/// conversation on no file, so the Not on a file row shows.
	[[nodiscard]] auto banded_projection() -> application::pull_request_overview_projection
	{
		auto       projected = nested_folders_projection();
		const auto at        = std::ranges::find(projected.Files, std::string{ "src/engine/shaders/basic.vert" }, &application::overview_file_row::Path);
		auto       extra     = std::vector<application::overview_file_row>{ };
		for (const auto* name : { "shadow.frag", "sky.frag", "terrain.vert", "water.frag" }) {
			extra.push_back(row(std::string{ "src/engine/shaders/" } + name, application::overview_file_operation::modified, application::overview_change_since_review::unchanged,
								core::file_read_state::unread));
		}
		projected.Files.insert(std::next(at), extra.begin(), extra.end());
		projected.Progress.FilesTotal = projected.Files.size();
		return projected;
	}

	[[nodiscard]] auto paths_of(const application::pull_request_overview_projection& aProjection) -> std::vector<std::string>
	{
		auto paths = std::vector<std::string>{ };
		for (const auto& file : aProjection.Files) {
			paths.push_back(file.Path);
		}
		return paths;
	}

	/// \brief [[BPR-1585]]. The commonest colour of a shown row's whole strip:
	/// the ground it is painted on.
	[[nodiscard]] auto row_ground(const QImage& aImage, const QTreeView& aView, const QModelIndex& aRow) -> QColor
	{
		const auto rect   = aView.visualRect(aRow.siblingAtColumn(0));
		const auto ratio  = aImage.devicePixelRatio();
		auto       counts = std::map<QRgb, int>{ };
		for (auto y = static_cast<int>(std::ceil(rect.top() * ratio)); y < static_cast<int>(std::floor((rect.bottom() + 1) * ratio)); ++y) {
			for (auto x = 0; x < aImage.width(); ++x) {
				++counts[aImage.pixel(x, y)];
			}
		}
		return QColor{ std::ranges::max_element(counts, { }, [](const auto& aEntry) { return aEntry.second; })->first };
	}

	/// \brief [[BPR-1585]]. The rows the view shows, top to bottom.
	[[nodiscard]] auto shown_rows(const QTreeView& aView) -> std::vector<QModelIndex>
	{
		auto rows = std::vector<QModelIndex>{ };
		for (auto at = aView.model()->index(0, 0); at.isValid(); at = aView.indexBelow(at)) {
			rows.push_back(at);
		}
		return rows;
	}

	[[nodiscard]] auto heading_row(const QModelIndex& aRow) -> bool
	{
		return aRow.data(qt::overview_file_model::folder_role).toBool() || aRow.data(qt::overview_file_model::remainder_role).toBool();
	}

	/// \brief [[BPR-1585]]. The rule, over the shown rows alone: a heading
	/// is on the ground and starts a group; the files under it take turns,
	/// the first on the band.
	[[nodiscard]] auto band_rule(const std::vector<QModelIndex>& aRows) -> std::vector<bool>
	{
		auto banded = std::vector<bool>{ };
		auto turn   = 0;
		for (const auto& at : aRows) {
			if (heading_row(at)) {
				turn = 0;
				banded.push_back(false);
				continue;
			}
			banded.push_back(turn % 2 == 0);
			++turn;
		}
		return banded;
	}

	/// \brief [[BPR-1585]]. Qt's own alternation: every other shown row of
	/// the whole tree, whatever it is.
	[[nodiscard]] auto whole_tree_alternation(const std::vector<QModelIndex>& aRows) -> std::vector<bool>
	{
		auto banded = std::vector<bool>{ };
		for (std::size_t at = 0U; at < aRows.size(); ++at) {
			banded.push_back(at % 2U == 1U);
		}
		return banded;
	}

	/// \brief [[BPR-1585]]. The owner, 2026-10-17: "Would the Overview files
	/// view benefit from a subtle alternating background for rows?" File rows
	/// alternate between the ground and a quiet band, counted over the rows
	/// shown within each folder, the first file on the band; headers and Not
	/// on a file stay on the ground. Opening and closing folders, a sort, a
	/// hidden row and a refresh keep the rule; a selected row is the
	/// selection's. A control with Qt's whole-tree alternation fails the rule.
	void test_file_rows_band_within_each_folder()
	{
		using core::feedback_label;
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		qt::pull_request_overview_page page;
		profile.apply_to(page);
		page.adopt_appearance(profile);
		auto asked = std::vector<application::overview_feedback_request>{ };
		page.set_feedback_dispatch([&](const application::overview_feedback_request& aRequest) { asked.push_back(aRequest); });
		const auto projected = banded_projection();
		const auto paths     = paths_of(projected);
		const auto review    = population({ { .Id = "review-thread:a", .Label = feedback_label::issue, .Path = "src/engine/renderer.cpp" },
											{ .Id = "review-thread:d", .Label = feedback_label::praise, .Path = { } } });
		page.resize(1'100, 1'060);
		page.present(projected);
		page.show();
		settle();
		expect(!asked.empty(), "the page asks for its counts");
		page.present_feedback(feedback_for(review, asked.back(), paths));
		page.expand_folders_button()->click();
		auto* const view  = page.files_view();
		auto* const model = page.file_model();
		view->clearSelection();
		view->setCurrentIndex({ });
		settle();
		expect(view->verticalScrollBar()->maximum() == 0, "every row of the pull request is on screen");
		const auto ground = view->palette().color(QPalette::Active, QPalette::Base);
		const auto band   = qt::overview_row_band(view->palette());
		const auto near   = [](const QColor& aOne, const QColor& aTwo) {
			return std::abs(aOne.red() - aTwo.red()) <= 1 && std::abs(aOne.green() - aTwo.green()) <= 1 && std::abs(aOne.blue() - aTwo.blue()) <= 1;
		};
		expect(!near(band, ground), "the band is not the ground");
		const auto follows = [&](const QImage& aImage, const std::vector<QModelIndex>& aRows, const std::vector<bool>& aBanded) {
			for (std::size_t at = 0U; at < aRows.size(); ++at) {
				if (!near(row_ground(aImage, *view, aRows[at]), aBanded[at] ? band : ground)) {
					return false;
				}
			}
			return true;
		};
		const auto keeps = [&](const std::string& aStep) {
			settle();
			const auto rows  = shown_rows(*view);
			const auto image = view->viewport()->grab().toImage();
			expect(follows(image, rows, band_rule(rows)), aStep + ": each folder's files take turns on the band, the first on it, and headers stay on the ground");
			return image;
		};
		const auto shader    = [&](const char* aName) { return model->index_for_path(QStringLiteral("src/engine/shaders/%1").arg(QLatin1StringView{ aName })); };
		auto       image     = keeps("every folder open");
		const auto remainder = shown_rows(*view).back();
		expect(remainder.data(qt::overview_file_model::remainder_role).toBool(), "Not on a file is the last row");
		expect(near(row_ground(image, *view, shader("basic.frag")), band) && near(row_ground(image, *view, shader("basic.vert")), ground)
				   && near(row_ground(image, *view, shader("shadow.frag")), band) && near(row_ground(image, *view, shader("water.frag")), ground),
			   "src/engine/shaders' six files alternate from the band");
		for (const auto& at : shown_rows(*view)) {
			if (heading_row(at)) {
				expect(near(row_ground(image, *view, at), ground), "a folder header or Not on a file is on the ground");
			}
		}

		// [[BPR-1585]] AC3. Not on a file draws its name and chips, no marks.
		for (const auto column : { qt::overview_file_model::operation_column, qt::overview_file_model::changed_column, qt::overview_file_model::read_column }) {
			expect(!ink_span(image, view->visualRect(remainder.siblingAtColumn(column))), "Not on a file draws no operation, change or read mark");
		}
		expect(ink_span(image, view->visualRect(remainder.siblingAtColumn(qt::overview_file_model::file_column))).has_value()
				   && ink_span(image, view->visualRect(remainder.siblingAtColumn(qt::overview_file_model::comments_column))).has_value(),
			   "Not on a file still draws its name and its chips");

		// Closing a folder moves no other group's turns.
		const auto later = [&](const QImage& aImage) {
			auto grounds = std::vector<QRgb>{ };
			for (const auto* name : { "basic.frag", "basic.vert", "shadow.frag", "sky.frag", "terrain.vert", "water.frag" }) {
				grounds.push_back(row_ground(aImage, *view, shader(name)).rgb());
			}
			return grounds;
		};
		const auto open   = later(image);
		const auto docApi = QPersistentModelIndex{ model->index_for_path(QStringLiteral("docs/api/reference.md")).parent() };
		view->setExpanded(docApi, false);
		image = keeps("docs/api closed");
		expect(later(image) == open, "closing docs/api changes no later file's ground");
		view->setExpanded(model->index_for_path(QStringLiteral("README.md")).parent(), false);
		image = keeps("the root folder closed too");
		expect(later(image) == open, "closing the first folder changes no later file's ground");
		page.collapse_folders_button()->click();
		keeps("every folder closed");
		page.expand_folders_button()->click();
		keeps("every folder open again");

		// A row the view hides takes no turn.
		const auto shaders = QPersistentModelIndex{ shader("basic.frag").parent() };
		view->setRowHidden(shader("basic.frag").row(), shaders, true);
		image = keeps("basic.frag hidden");
		expect(near(row_ground(image, *view, shader("basic.vert")), band), "with basic.frag hidden, basic.vert is its folder's first shown file and on the band");
		view->setRowHidden(shader("basic.frag").row(), shaders, false);
		keeps("basic.frag shown again");

		// A sort: the host's new order arrives as moves, and turns follow it.
		auto sorted = projected;
		std::ranges::reverse(sorted.Files);
		page.present(sorted);
		image = keeps("the files in reverse order");
		expect(near(row_ground(image, *view, shader("water.frag")), band) && near(row_ground(image, *view, shader("basic.frag")), ground),
			   "after the sort water.frag is src/engine/shaders' first file, on the band, and basic.frag its sixth, on the ground");
		page.present(projected);
		image = keeps("the file order again");
		page.present(projected);
		image = keeps("an incremental refresh of the same pull request");
		expect(later(image) == open, "the refresh keeps every turn");

		// A selected row is the selection's, band or not.
		for (const auto* name : { "basic.frag", "basic.vert" }) {
			view->setCurrentIndex(shader(name));
			settle();
			const auto chosen = row_ground(view->viewport()->grab().toImage(), *view, shader(name));
			expect(near(chosen, view->palette().color(QPalette::Active, QPalette::Highlight)) || near(chosen, view->palette().color(QPalette::Inactive, QPalette::Highlight)),
				   std::string{ "the selected " } + name + " is filled with the selection, not the band");
		}
		view->clearSelection();
		view->setCurrentIndex({ });
		const auto rows = shown_rows(*view);
		image           = keeps("the selection cleared");
		expect(!follows(image, rows, whole_tree_alternation(rows)), "the bands are not Qt's whole-tree alternation");

		// Negative control: Qt's own alternation, in the same band colour,
		// follows the whole tree and fails the rule.
		auto* const ours        = view->itemDelegate();
		auto        plain       = QStyledItemDelegate{ };
		auto        alternating = view->palette();
		alternating.setColor(QPalette::AlternateBase, band);
		const auto kept = view->palette();
		view->setPalette(alternating);
		view->setItemDelegate(&plain);
		view->setAlternatingRowColors(true);
		settle();
		const auto control = view->viewport()->grab().toImage();
		expect(follows(control, rows, whole_tree_alternation(rows)), "control: Qt's alternation draws every other shown row in the band");
		expect(!follows(control, rows, band_rule(rows)), "control: the check fails on Qt's whole-tree alternation");
		view->setAlternatingRowColors(false);
		view->setItemDelegate(ours);
		view->setPalette(kept);
		keeps("the page's own delegate again");

		if (const auto target = qEnvironmentVariable("BPR_OVERVIEW_BAND_RENDER_PATH"); !target.isEmpty()) {
			expect(page.grab().save(target, "PNG"), "the band render is saved for inspection");
		}
	}

	/// \brief [[BPR-1585]]. In the four built-in themes the band is visible
	/// and quiet, and every ink a file row draws reads on it as on the
	/// ground: text 4.5:1, marks 3:1. Pairs already short on the ground are
	/// printed with both values and may lose no more than the band's step.
	void test_band_keeps_every_ink_readable()
	{
		const auto contrast = [](const QColor& aOne, const QColor& aTwo) { return qt::contrast_ratio(qt::to_appearance_color(aOne), qt::to_appearance_color(aTwo)); };
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto                     theme = std::string{ core::stable_name(profile.variant()) };
			qt::pull_request_overview_page page;
			profile.apply_to(page);
			page.adopt_appearance(profile);
			// The label tints of the theme, as the composition hands them over.
			auto marks  = qt::feedback_mark_theme{ };
			marks.Tints = qt::feedback_label_tints_of(profile.palette());
			page.set_label_marks(marks);
			const auto  palette = page.files_view()->palette();
			const auto& colours = page.colours();
			const auto  ground  = palette.color(QPalette::Active, QPalette::Base);
			const auto  band    = qt::overview_row_band(palette);
			const auto  step    = contrast(band, ground);
			std::cout << "inks " << theme << ": quiet " << palette.color(QPalette::PlaceholderText).name().toStdString()
					  << " base " << contrast(palette.color(QPalette::PlaceholderText), ground) << " band " << contrast(palette.color(QPalette::PlaceholderText), band)
					  << " window " << contrast(palette.color(QPalette::PlaceholderText), palette.color(QPalette::Window))
					  << " amber " << colours.Marker.name().toStdString() << " base " << contrast(colours.Marker, ground) << " band " << contrast(colours.Marker, band) << '\n';
			expect(contrast(palette.color(QPalette::PlaceholderText), palette.color(QPalette::Window)) >= 4.5, theme + ": quiet labels read on Window too");
			std::cout << "band " << theme << ": ground " << ground.name().toStdString() << " band " << band.name().toStdString() << " step " << step << '\n';
			expect(step >= 1.03 && step <= 1.25, theme + ": the band is visible and quiet against the ground");
			expect(std::abs(step - qt::overview_row_band_contrast) < 0.02, theme + ": the band stands its designed step from the ground");
			struct pair
			{
				std::string                                                              Name;
				double                                                                   Floor;
				std::function<std::pair<QColor, QColor>(const QPalette&, const QColor&)> Colours;
			};
			const auto on    = [](const QColor& aInk) { return [aInk](const QPalette&, const QColor& aGround) { return std::pair{ aInk, aGround }; }; };
			const auto cut   = [](const QColor& aFill) { return [aFill](const QPalette&, const QColor& aGround) { return std::pair{ aGround, aFill }; }; };
			const auto badge = [&colours](const application::overview_file_operation aOperation, const bool aLetter) {
				return [&colours, aOperation, aLetter](const QPalette& aPalette, const QColor& aGround) {
					const auto mark = qt::overview_operation_mark_colours(aOperation, aPalette, colours, false);
					return aLetter ? std::pair{ mark.Letter, mark.Fill } : std::pair{ mark.Fill, aGround };
				};
			};
			using operation = application::overview_file_operation;
			auto pairs      = std::vector<pair>{
				{ "file name", 4.5, on(palette.color(QPalette::Text)) },
				{ "added lines", 4.5, on(colours.Accepted) },
				{ "removed lines", 4.5, on(colours.Defect) },
				{ "open chip digits", 4.5, on(colours.States.Open) },
				{ "note", 4.5, on(palette.color(QPalette::PlaceholderText)) },
				{ "folder path", 4.5, on(palette.color(QPalette::PlaceholderText)) },
				{ "unknown change ?", 4.5, on(palette.color(QPalette::PlaceholderText)) },
				{ "changed chip word", 4.5, cut(colours.Unread) },
				{ "new chip word", 4.5, cut(colours.Accepted) },
				{ "A badge letter", 4.5, badge(operation::added, true) },
				{ "D badge letter", 4.5, badge(operation::removed, true) },
				{ "R badge letter", 4.5, badge(operation::renamed, true) },
				{ "M badge letter", 4.5, badge(operation::modified, true) },
				{ "M badge letter on the row", 4.5, [&colours](const QPalette& aPalette, const QColor& aGround) {
					 return std::pair{ qt::overview_operation_mark_colours(operation::modified, aPalette, colours, false).Letter, aGround };
				 } },
				{ "A badge", 3.0, badge(operation::added, false) },
				{ "D badge", 3.0, badge(operation::removed, false) },
				{ "R badge", 3.0, badge(operation::renamed, false) },
				{ "changed chip", 3.0, on(colours.Unread) },
				{ "unresolved bubble", 3.0, on(colours.Marker) },
				{ "unread ring", 3.0, on(colours.Progress.Neutral) },
				{ "seen mark", 3.0, on(colours.Progress.Seen) },
				{ "read mark", 3.0, on(colours.Progress.Read) },
				{ "accepted mark", 3.0, on(colours.Progress.Accepted) },
				{ "excluded strike", 3.0, on(colours.Progress.Excluded) },
				{ "unknown state mark", 3.0, [](const QPalette& aPalette, const QColor& aGround) { return std::pair{ qt::feedback_state_faint(aPalette), aGround }; } },
			};
			for (std::size_t label = 0U; label < core::feedback_label_count; ++label) {
				pairs.push_back({ "label mark " + std::to_string(label), 3.0, on(colours.Marks.Tints[label]) });
			}
			auto atBand = palette;
			atBand.setColor(QPalette::Active, QPalette::Base, band);
			atBand.setColor(QPalette::Inactive, QPalette::Base, qt::overview_row_band(palette, QPalette::Inactive));
			for (const auto& checked : pairs) {
				const auto [inkG, backG] = checked.Colours(palette, ground);
				const auto [inkB, backB] = checked.Colours(atBand, band);
				const auto onGround      = contrast(inkG, backG);
				const auto onBand        = contrast(inkB, backB);
				const auto name          = theme + ": " + checked.Name;
				expect(onGround >= checked.Floor, name + " reads on the ground (" + std::to_string(onGround) + ")");
				expect(onBand >= checked.Floor, name + " reads on the band (" + std::to_string(onBand) + ")");
			}
		}
	}

	/// \brief [[BPR-1585]]. The owner's look: the banded nested-folders pull
	/// request with every folder open, a few changed files, a note, comments on two
	/// files and one not on a file, one row selected, in the built-in Light
	/// and Dark themes. Saved under BPR_OVERVIEW_BAND_RENDER_ROOT.
	void render_band_look(const QString& aRoot)
	{
		using core::feedback_label;
		auto projected = banded_projection();
		auto paths     = std::vector<std::string>{ };
		for (auto& file : projected.Files) {
			paths.push_back(file.Path);
			if (file.Path == "src/engine/renderer.cpp" || file.Path == "src/ui/widgets/slider.cpp") {
				file.Changed = application::overview_change_since_review::changed;
			}
			if (file.Path == "src/engine/shaders/basic.frag" || file.Path == "src/engine/shaders/basic.vert") {
				file.Operation = application::overview_file_operation::renamed;
				file.Note      = std::string{ "Check the precision qualifiers" };
				file.Changed   = application::overview_change_since_review::unknown;
			}
			if (file.Path == "docs/guide.md" || file.Path == "src/engine/renderer.h") {
				file.Read = core::file_read_state::read;
				file.Mark = core::file_progress_mark::read;
			}
		}
		const auto review = population({
			{ .Id = "review-thread:a", .Label = feedback_label::issue, .Path = "src/engine/renderer.cpp" },
			{ .Id = "review-thread:b", .Label = feedback_label::question, .Path = "src/engine/renderer.cpp" },
			{ .Id = "review-thread:c", .Label = std::nullopt, .Path = "src/ui/widgets/button.cpp" },
			{ .Id = "review-thread:plain", .Label = feedback_label::issue, .Path = "src/engine/shaders/basic.vert" },
			{ .Id = "review-thread:band", .Label = feedback_label::issue, .Path = "src/engine/shaders/basic.frag" },
			{ .Id = "review-thread:d", .Label = feedback_label::praise, .Path = { } },
		});
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") },
										  std::pair{ core::appearance_mode::high_contrast_light, QStringLiteral("high-contrast-light") },
										  std::pair{ core::appearance_mode::high_contrast_dark, QStringLiteral("high-contrast-dark") } }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			qt::pull_request_overview_page page;
			profile.apply_to(page);
			page.adopt_appearance(profile);
			auto marks  = qt::feedback_mark_theme{ };
			marks.Tints = qt::feedback_label_tints_of(profile.palette());
			page.set_label_marks(marks);
			auto asked = std::vector<application::overview_feedback_request>{ };
			page.set_feedback_dispatch([&](const application::overview_feedback_request& aRequest) { asked.push_back(aRequest); });
			page.resize(1'240, 960);
			page.present(projected);
			page.set_label_columns(*core::make_feedback_label_columns(std::vector{ feedback_label::issue, feedback_label::question }));
			page.show();
			settle();
			if (!asked.empty()) {
				page.present_feedback(feedback_for(review, asked.back(), paths));
			}
			page.expand_folders_button()->click();
			settle();
			auto* const view = page.files_view();
			view->setCurrentIndex(page.file_model()->index_for_path(QStringLiteral("src/engine/shaders/post/tonemap.frag")));
			view->setFocus(Qt::OtherFocusReason);
			settle();
			expect(page.grab().save(QStringLiteral("%1/files-band-%2.png").arg(aRoot, name), "PNG"), "the band look is saved");
		}
	}
	/// \brief [[BPR-1589]]. A busy pull request: several roles of mine,
	/// conflicts, two failed checks, a code owner waited on and replies to me.
	[[nodiscard]] auto status_projection() -> application::pull_request_overview_projection
	{
		auto projected            = projection();
		projected.MergeState      = "Conflicts";
		projected.MergeTone       = application::overview_merge_tone::blocked;
		projected.MergeConditions = { "Review required", "build-linux failed", "lint failed" };
		projected.ReviewRequests  = { { .Name = "EpicGames/render", .DisplayName = "Render", .Team = true, .CodeOwner = true }, { .Name = "tbutler" } };
		projected.Checks          = { .Passed = 33U, .Failed = 2U, .Skipped = 2U };
		for (const auto& [name, failed] : { std::pair{ "build-linux", true }, std::pair{ "lint", true }, std::pair{ "test-mac", false } }) {
			projected.CheckRows.push_back({ .Identity = std::string{ "check:" } + name, .Name = name, .Kind = "Check run", .Outcome = failed ? "Failed" : "Passed", .Failed = failed });
		}
		auto& status          = projected.Status;
		status.Viewer         = "mike";
		status.Roles          = { .ViewerKnown = true,
								  .Roles       = { { .Kind = core::review_role_kind::team_requested, .CodeOwner = true, .Teams = { "EpicGames/render" }, .Reason = core::team_membership_reason::needs_read_org },
												   { .Kind = core::review_role_kind::reviewed, .Review = core::my_review_state::changes_requested },
												   { .Kind = core::review_role_kind::mentioned, .Mention = core::mention_place::comment } } };
		status.Attention      = { { .Kind = core::status_attention_kind::conflicts, .Count = 1U },
								  { .Kind = core::status_attention_kind::failed_checks, .Count = 2U, .Names = { "build-linux", "lint" } },
								  { .Kind = core::status_attention_kind::waiting_on, .Count = 2U, .Names = { "EpicGames/render", "tbutler" } },
								  { .Kind = core::status_attention_kind::replies_to_me, .Count = 1U } };
		status.OpenConditions = { "Review required" };
		return projected;
	}

	[[nodiscard]] auto mentions(const QStringList& aRows, const QString& aWords) -> qsizetype
	{
		return std::ranges::count_if(aRows, [&aWords](const QString& aRow) { return aRow.contains(aWords); });
	}

	void test_conversation_count_is_cleared_with_the_pull_request()
	{
		auto page = qt::pull_request_overview_page{ };
		page.present(status_projection());
		page.set_new_conversation_comments(2U);
		expect(!page.new_conversation_button()->parentWidget()->isHidden(), "the current PR has new comments");
		page.clear();
		page.present(status_projection());
		expect(page.new_conversation_button()->parentWidget()->isHidden(), "a reopened PR waits for its own read state instead of retaining the old count");
		page.set_new_conversation_comments(1U);
		expect(!page.new_conversation_button()->parentWidget()->isHidden(), "the next ledger answer shows its own count");
	}

	void test_status_leads_with_my_role_and_what_needs_attention()
	{
		using section = qt::overview_status_section;
		qt::pull_request_overview_page page;
		page.resize(1'100, 900);
		auto opened = std::vector<std::string>{ };
		page.set_new_threads_dispatch([&opened] { opened.emplace_back("threads"); });
		page.set_new_conversation_dispatch([&opened] { opened.emplace_back("discuss"); });
		page.present(status_projection());
		page.show();
		settle();
		auto* const drawer = page.status_drawer();

		auto* const role = page.role_line();
		expect(role->isVisible() && role->primary() == QStringLiteral("Team review requested")
				   && role->chips() == QStringList{ QStringLiteral("@EpicGames/render"), QStringLiteral("Code owner"), QStringLiteral("Reviewer · changes requested"), QStringLiteral("Mentioned") }
				   && role->toolTip().contains(QStringLiteral("lacks the read:org scope")) && !role->toolTip().contains(QStringLiteral("may be a member")),
			   "[[BPR-1589]] v002: one short label, the other roles as chips, and why GitHub would not say in the tooltip, with no hedge on the line");
		const auto rows = page.attention_rows();
		expect(rows.size() == 4 && rows[0].startsWith(QStringLiteral("Conflicts with release")) && rows[1] == QStringLiteral("2 checks failed\nbuild-linux, lint")
				   && rows[2] == QStringLiteral("Waiting on EpicGames/render and tbutler\nCode-owner review requested") && rows[3] == QStringLiteral("1 reply to you"),
			   "Needs attention: conflicts, each failed check named, who is waited on, replies to me");
		auto* const attention = page.status_section_header(section::attention);
		expect(attention->text() == QStringLiteral("Needs attention") && attention->count() == QStringLiteral("4"), "the section counts what needs me");

		// Each failed check is named in one place at a time.
		auto* const conditions = page.findChild<QLabel*>(QStringLiteral("overviewMergeConditions"));
		attention->click();
		settle();
		expect(conditions->isVisible() && conditions->text() == QStringLiteral("Review required"), "Needs attention opens to Merge's other conditions, not the failures again");
		page.status_section_header(section::checks)->click();
		settle();
		expect(page.findChild<QWidget*>(QStringLiteral("overviewCheckList"))->isVisible() && mentions(page.attention_rows(), QStringLiteral("build-linux")) == 0
				   && page.attention_rows()[1] == QStringLiteral("2 checks failed"),
			   "with Checks open, the list names the failures and Needs attention only counts them");

		// My review: the threads with new posts, and Go to them.
		expect(page.findChild<QWidget*>(QStringLiteral("overviewNewThreads"))->isHidden(), "no threads with new posts, no row");
		page.set_new_threads(3U);
		settle();
		auto* const go = page.new_threads_button();
		expect(go->isVisible() && page.findChild<QLabel*>(QStringLiteral("overviewNewThreadsText"))->text() == QStringLiteral("3 threads with new posts"), "the threads with new posts are counted");
		go->click();
		expect(opened == std::vector<std::string>{ "threads" }, "Go to them opens Threads");
		expect(page.new_conversation_button()->parentWidget()->isHidden(), "the conversation's line waits for its read state");
		page.set_new_conversation_comments(2U);
		settle();
		expect(page.new_conversation_button()->isVisible() && page.findChild<QLabel*>(QStringLiteral("overviewNewConversationText"))->text() == QStringLiteral("2 new comments in the conversation"),
			   "once it has a count, the conversation's line shows it");
		page.new_conversation_button()->click();
		expect(opened.back() == "discuss", "its Go to them opens Discuss");
		page.set_new_conversation_comments(std::nullopt);

		// People: faces and one line.
		expect(page.people_faces()->faces().size() == 5U && page.people_summary()->text() == QStringLiteral("1 approved, 1 requested changes, 2 requested"),
			   "People is faces with one summary line");

		// No empty rows, open or closed.
		for (const auto open : { true, false }) {
			for (std::size_t index = 0U; index < qt::overview_status_section_count; ++index) {
				page.status_section_header(static_cast<section>(index))->setChecked(open);
			}
			settle();
			const auto violations = qt::drawer_principle_violations(*drawer);
			expect(violations.isEmpty(), std::string{ "the Status drawer keeps RFC-BPR-0085's principles: " } + violations.join(QStringLiteral("; ")).toStdString());
		}
		auto quiet               = status_projection();
		quiet.SinceLastReview    = { .Known = true, .Uploads = 0U, .FilesChanged = 0U };
		quiet.Progress           = { .FilesRead = 0U, .FilesTotal = 3U, .DraftedComments = 0U };
		quiet.Status.Attention   = { };
		quiet.Status.Roles.Roles = { };
		quiet.MergeState         = "Mergeable";
		page.present(quiet);
		page.status_section_header(section::my_review)->setChecked(true);
		settle();
		expect(page.status_section_header(section::attention)->text() == QStringLiteral("Merge") && page.status_section_header(section::attention)->count() == QStringLiteral("Mergeable")
				   && role->primary() == QStringLiteral("No role") && role->chips().isEmpty(),
			   "nothing needs me: the section is Merge with its state, and no role says so");
		expect(qt::drawer_principle_violations(*drawer).isEmpty(), "a quiet pull request draws no zero rows");
		{
			auto* const zero = new QLabel{ QStringLiteral("0"), page.status_section(section::my_review) };
			zero->show();
			expect(!qt::drawer_principle_violations(*drawer).isEmpty(), "negative control: a zero row is caught");
			delete zero;
		}

		// A disclosure remembers its state for the pull request in the window.
		auto other   = status_projection();
		other.Number = core::pull_request_number{ 7U };
		page.present(other);
		settle();
		expect(!page.status_section_header(section::my_review)->isChecked(), "another pull request opens with its own disclosures");
		page.present(quiet);
		settle();
		expect(page.status_section_header(section::my_review)->isChecked(), "coming back finds My review open as it was left");

		auto unknown                     = status_projection();
		unknown.Status.Roles.ViewerKnown = false;
		page.present(unknown);
		settle();
		expect(!role->isVisible(), "an unknown viewer is told nothing about a role");
	}

	/// \brief [[BPR-1589]] v002. The five roles the owner judges, each as the
	/// drawer draws it.
	[[nodiscard]] auto role_variants() -> std::vector<std::pair<QString, core::review_roles>>
	{
		using kind = core::review_role_kind;
		return {
			{ QStringLiteral("author"), { .ViewerKnown = true, .Roles = { { .Kind = kind::author } } } },
			{ QStringLiteral("requested"), { .ViewerKnown = true, .Roles = { { .Kind = kind::requested, .CodeOwner = true }, { .Kind = kind::reviewed, .Review = core::my_review_state::commented } } } },
			{ QStringLiteral("team-member"), { .ViewerKnown = true, .Roles = { { .Kind = kind::team_requested, .Teams = { "EpicGames/render" }, .Membership = core::team_membership::member } } } },
			{ QStringLiteral("team-unknown"), { .ViewerKnown = true, .Roles = { { .Kind = kind::team_requested, .Teams = { "EpicGames/render" }, .Reason = core::team_membership_reason::needs_read_org } } } },
			{ QStringLiteral("mentioned"), { .ViewerKnown = true, .Roles = { { .Kind = kind::mentioned, .Mention = core::mention_place::description } } } },
		};
	}

	void test_role_line_is_one_short_label()
	{
		const auto expected = std::map<QString, std::pair<QString, QStringList>>{
			{ QStringLiteral("author"), { QStringLiteral("Author"), { } } },
			{ QStringLiteral("requested"), { QStringLiteral("Review requested"), { QStringLiteral("Code owner"), QStringLiteral("Reviewer · commented") } } },
			{ QStringLiteral("team-member"), { QStringLiteral("Review requested"), { QStringLiteral("Your team: @EpicGames/render") } } },
			{ QStringLiteral("team-unknown"), { QStringLiteral("Team review requested"), { QStringLiteral("@EpicGames/render") } } },
			{ QStringLiteral("mentioned"), { QStringLiteral("Mentioned"), { } } },
		};
		qt::pull_request_overview_page page;
		page.resize(1'100, 700);
		page.show();
		for (const auto& [name, roles] : role_variants()) {
			auto projected         = projection();
			projected.Status.Roles = roles;
			page.present(projected);
			settle();
			auto* const line = page.role_line();
			const auto& want = expected.at(name);
			expect(line->primary() == want.first && line->chips() == want.second, "the " + name.toStdString() + " role is one short label: " + line->primary().toStdString());
			expect(line->height() >= line->heightForWidth(line->width()) && line->heightForWidth(line->width()) < (3 * line->fontMetrics().height()) + 24 && line->toolTip().size() > 0,
				   "the " + name.toStdString() + " role is whole at the drawer's width in at most two lines, its reason in the tooltip");
		}
		// Negative control: the v001 sentence would not fit one line.
		auto projected         = projection();
		projected.Status.Roles = role_variants()[3].second;
		page.present(projected);
		settle();
		expect(page.role_line()->toolTip().contains(QStringLiteral("read:org")) && !page.role_line()->primary().contains(QStringLiteral("may be")), "the reason is in the tooltip, never the line");
	}

	/// \brief [[BPR-1589]] v002. The role section before (v001's wording and
	/// layout, rebuilt here: a face, a sentence and an "Also:" line) and after,
	/// for the five roles, Light and Dark.
	void render_role_look(const QString& aRoot)
	{
		const auto before = std::map<QString, std::pair<QString, QString>>{
			{ QStringLiteral("author"), { QStringLiteral("You are the author"), { } } },
			{ QStringLiteral("requested"), { QStringLiteral("Review requested from you as a code owner"), QStringLiteral("Also: you reviewed with comments") } },
			{ QStringLiteral("team-member"), { QStringLiteral("Review requested from EpicGames/render; you may be a member"), { } } },
			{ QStringLiteral("team-unknown"), { QStringLiteral("Review requested from EpicGames/render; you may be a member"), { } } },
			{ QStringLiteral("mentioned"), { QStringLiteral("You are mentioned in the description"), { } } },
		};
		for (const auto& [mode, theme] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			for (const auto& [name, roles] : role_variants()) {
				qt::pull_request_overview_page page;
				profile.apply_to(page);
				page.adopt_appearance(profile);
				page.resize(1'240, 700);
				auto projected         = projection();
				projected.Status.Roles = roles;
				page.present(projected);
				page.show();
				settle();
				auto* const line = page.role_line();
				expect(line->grab().save(QStringLiteral("%1/role-after-%2-%3.png").arg(aRoot, name, theme), "PNG"), "the role after is saved");
				// Before: v001's anatomy at the same width.
				QWidget old;
				profile.apply_to(old);
				old.setAutoFillBackground(true);
				old.setBackgroundRole(QPalette::AlternateBase);
				auto* const row = new QHBoxLayout{ &old };
				row->setContentsMargins(4, 4, 4, 6);
				row->setSpacing(10);
				auto* const face = new qt::identity_avatar_view{ 24, &old };
				face->set_identity(QStringLiteral("mike"));
				row->addWidget(face, 0, Qt::AlignTop);
				auto* const words = new QVBoxLayout;
				auto* const lead  = new QLabel{ before.at(name).first, &old };
				lead->setWordWrap(true);
				auto bold = lead->font();
				bold.setWeight(QFont::DemiBold);
				lead->setFont(bold);
				words->addWidget(lead);
				if (!before.at(name).second.isEmpty()) {
					auto* const also = new QLabel{ before.at(name).second, &old };
					also->setWordWrap(true);
					also->setForegroundRole(QPalette::PlaceholderText);
					words->addWidget(also);
				}
				row->addLayout(words, 1);
				old.resize(line->width(), old.heightForWidth(line->width()) > 0 ? old.heightForWidth(line->width()) : old.sizeHint().height());
				old.show();
				settle();
				expect(old.grab().save(QStringLiteral("%1/role-before-%2-%3.png").arg(aRoot, name, theme), "PNG"), "the role before is saved");
			}
		}
	}

	/// \brief [[BPR-1589]]. The Status drawer for the owner's look, Light and
	/// Dark, closed and with every section open.
	void render_status_look(const QString& aRoot)
	{
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			for (const auto open : { false, true }) {
				auto profile = qt::appearance_profile{ };
				static_cast<void>(profile.select_mode(mode));
				qt::pull_request_overview_page page;
				profile.apply_to(page);
				page.adopt_appearance(profile);
				page.resize(1'240, open ? 1'500 : 900);
				page.present(status_projection());
				page.set_new_threads(3U);
				page.show();
				for (std::size_t index = 0U; index < qt::overview_status_section_count; ++index) {
					page.status_section_header(static_cast<qt::overview_status_section>(index))->setChecked(open);
				}
				settle();
				expect(page.status_drawer()->grab().save(QStringLiteral("%1/status-%2-%3.png").arg(aRoot, name, open ? QStringLiteral("open") : QStringLiteral("closed")), "PNG"), "the Status look is saved");
			}
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   scale = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();
	if (scale > 0) {
		expect(qFuzzyCompare(application.devicePixelRatio(), scale), "the requested display scale is effective");
	}
	// [[BPR-1094]]. The 125% and 200% rows run the chips at their scale only.
	if (argc > 1 && std::string_view{ argv[1] } == "--chips") {
		test_chips_read_at_this_scale_in_every_theme_and_style();
		std::cout << "pull-request-overview-chips:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " dpr=" << application.devicePixelRatio() << '\n';
		return EXIT_SUCCESS;
	}
	// [[BPR-1577]]. The Open and All switch alone, for its renders.
	if (argc > 1 && std::string_view{ argv[1] } == "--switch") {
		test_counts_switch_is_quiet_in_every_theme();
		std::cout << "pull-request-overview-switch:pass dpr=" << application.devicePixelRatio() << '\n';
		return EXIT_SUCCESS;
	}
	// [[BPR-1577]]. The switch's renders for review, in whatever style runs.
	if (argc > 2 && std::string_view{ argv[1] } == "--switch-render") {
		render_counts_switch(QString::fromLocal8Bit(argv[2]));
		std::cout << "pull-request-overview-switch-render:done\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1363]]. The 125% and 200% rows run the folder rows at their scale.
	if (argc > 1 && std::string_view{ argv[1] } == "--folder-rows") {
		test_folder_disclosure_never_moves_a_column();
		test_folder_disclosure_is_the_leftmost_mark();
		test_folder_counts_share_one_column();
		test_file_rows_band_within_each_folder();
		std::cout << "pull-request-overview-folder-rows:pass dpr=" << application.devicePixelRatio() << '\n';
		return EXIT_SUCCESS;
	}
	// [[BPR-1585]]. The banded look in the four themes, for the owner's review.
	if (argc > 2 && std::string_view{ argv[1] } == "--band-look") {
		render_band_look(QString::fromLocal8Bit(argv[2]));
		std::cout << "pull-request-overview-band-look:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1589]] v002. The role section before and after.
	if (argc > 2 && std::string_view{ argv[1] } == "--role-look") {
		render_role_look(QString::fromLocal8Bit(argv[2]));
		std::cout << "pull-request-overview-role-look:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1589]]. The Status drawer's look, for the owner's review.
	if (argc > 2 && std::string_view{ argv[1] } == "--status-look") {
		render_status_look(QString::fromLocal8Bit(argv[2]));
		std::cout << "pull-request-overview-status-look:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1331]] v003. The read column's marks alone.
	if (argc > 1 && std::string_view{ argv[1] } == "--read-marks") {
		test_the_read_column_shows_the_mark();
		std::cout << "pull-request-overview-read-marks:pass\n";
		return EXIT_SUCCESS;
	}
	test_history_publication_does_not_touch_file_rows();
	test_cached_catalogue_availability_and_navigation();
	test_column_sizing_measures_visible_files();
	test_folder_disclosure_never_moves_a_column();
	test_folder_disclosure_is_the_leftmost_mark();
	test_folder_counts_share_one_column();
	test_page_presents_the_projection_and_reports_its_gestures();
	test_status_leads_with_my_role_and_what_needs_attention();
	test_conversation_count_is_cleared_with_the_pull_request();
	test_role_line_is_one_short_label();
	test_folder_controls_preserve_identity_and_refresh_state();
	test_collapse_all_and_expand_all_folders();
	test_document_keys_go_to_the_top_and_bottom_of_the_files();
	test_compact_descriptions();
	test_the_description_is_edited_in_place();
	test_reviewers_are_requested_from_people();
	test_resizable_summary();
	test_summary_seam_holds_its_cursor();
	test_retained_description_reader();
	test_body_edits_clamp_retained_selection();
	test_many_participants_scroll_without_compressing_or_overpainting();
	test_label_chips_show_the_selected_breakdown();
	test_chip_activation_names_exactly_its_count();
	test_modified_mark_reads_on_every_row();
	test_the_read_column_shows_the_mark();
	test_counts_switch_is_quiet_in_every_theme();
	test_chips_read_at_this_scale_in_every_theme_and_style();
	test_file_rows_band_within_each_folder();
	test_band_keeps_every_ink_readable();
	std::cout << "pull-request-overview-page-tests:pass qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
