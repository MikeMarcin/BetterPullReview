/// \file
/// \brief [[BPR-1504]]. Find in Files' result view: side filter, file and match
/// rows, the painted match wash, the details toggle and collapse-all.
/// [[BPR-PENDING-T]]: multi-select, Copy and the result menu.

#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/source_search_panel.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFontDatabase>
#include <QImage>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QToolButton>
#include <QTreeView>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto side(std::string aPath) -> application::git_cache_changed_side { return { .Path = std::move(aPath), .Object = { }, .Mode = 0100644U }; }

	/// One occurrence of `aNeedle` in `aSource`, with the core's own preview and span.
	auto occurrence(core::file_identity aFile, std::string_view aSource, std::string_view aNeedle, bool aLeft, bool aRight, std::uint32_t aLine) -> application::source_search_match
	{
		const auto begin  = aSource.find(aNeedle);
		const auto span   = core::byte_span{ begin, begin + aNeedle.size() };
		auto       record = core::source_match_record{ };
		if (aLeft) {
			record.Locations[0] = core::source_match_location{ .Side = core::diff_side::left, .Blob = { }, .Span = span, .Line = aLine, .Column = 2U };
		}
		if (aRight) {
			record.Locations[1] = core::source_match_location{ .Side = core::diff_side::right, .Blob = { }, .Span = span, .Line = aLine, .Column = 2U };
		}
		record.Preview      = core::source_match_preview(aSource, span);
		record.PreviewMatch = core::source_match_preview_span(aSource, span);
		return { aFile, std::move(record) };
	}

	auto fixture() -> application::source_search_projection
	{
		const auto modified = core::file_identity_of("src/search/find_controller.cpp");
		const auto added    = core::file_identity_of("src/search/added_view.cpp");
		const auto renamed  = core::file_identity_of("src/renamed.cpp");
		auto       corpus   = std::make_shared<application::source_corpus>();
		corpus->Entries.push_back({ modified, side("src/search/find_controller.cpp"), side("src/search/find_controller.cpp"), 'M' });
		corpus->Entries.push_back({ added, std::nullopt, side("src/search/added_view.cpp"), 'A' });
		corpus->Entries.push_back({ renamed, side("old/name.cpp"), side("src/renamed.cpp"), 'R' });
		auto matches = std::make_shared<std::vector<application::source_search_match>>();
		matches->push_back(occurrence(modified, "int a;\n\tconst auto needle = locate(scope);\n", "needle", true, false, 11U));
		matches->push_back(occurrence(modified, "int a;\n    return needle_for(scope, options);\n", "needle", false, true, 13U));
		matches->push_back(occurrence(modified, "x\n  // unchanged: the needle is shared\n", "needle", true, true, 40U));
		matches->back().Match.Locations[1]->Line = 1203U;
		matches->push_back(occurrence(added, "auto view = make_needle_view(model);\n", "needle", false, true, 0U));
		matches->push_back(occurrence(renamed, "static int needle = 1;\n", "needle", true, true, 0U));
		auto projection                  = application::source_search_projection{ };
		projection.Query                 = "needle";
		projection.Status                = core::source_search_status::complete;
		projection.Summary.Matches       = static_cast<std::uint32_t>(matches->size());
		projection.Summary.EligibleFiles = 3U;
		projection.Summary.SearchedFiles = 3U;
		projection.Corpus                = corpus;
		projection.Matches               = matches;
		projection.CurrentMatch          = 0U;
		projection.Open                  = true;
		return projection;
	}

	auto leaves(const QAbstractItemModel& aModel) -> std::vector<QModelIndex>
	{
		auto result = std::vector<QModelIndex>{ };
		for (int row = 0; row < aModel.rowCount(); ++row) {
			const auto group = aModel.index(row, 0);
			for (int child = 0; child < aModel.rowCount(group); ++child) {
				result.push_back(aModel.index(child, 0, group));
			}
		}
		return result;
	}

	void render(QWidget& aPanel, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_FIND_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "owned render root");
		QApplication::processEvents();
		const auto scale = QString::number(aPanel.devicePixelRatioF(), 'g', 3);
		expect(aPanel.grab().save(root + QStringLiteral("/") + aName + QStringLiteral("-") + scale + QStringLiteral("x.png")), "panel render saved");
	}

	void click(QWidget& aWidget, const QPoint& aPoint)
	{
		const auto  local  = QPointF{ aPoint };
		const auto  global = QPointF{ aWidget.mapToGlobal(aPoint) };
		QMouseEvent press{ QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QMouseEvent release{ QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}

	/// [[BPR-PENDING-T]]. A click as the window system reports it, with the
	/// button and modifiers a hand holds, so QtGui and the widget window
	/// decide the selection and the context menu as they do for a hand. Each
	/// click is two seconds after the last, so none makes a double click.
	void hand_click(QWidget& aWidget, const QPoint& aPoint, Qt::KeyboardModifiers aModifiers = Qt::NoModifier, Qt::MouseButton aButton = Qt::LeftButton)
	{
		static auto clock  = 500000;
		auto* const window = aWidget.window()->windowHandle();
		const auto  local  = QPointF{ aWidget.mapTo(aWidget.window(), aPoint) };
		clock += 2000;
		qt_handleMouseEvent(window, local, window->mapToGlobal(local), aButton, aButton, QEvent::MouseButtonPress, aModifiers, clock);
		clock += 40;
		qt_handleMouseEvent(window, local, window->mapToGlobal(local), Qt::NoButton, aButton, QEvent::MouseButtonRelease, aModifiers, clock);
		QApplication::processEvents();
	}

	/// The platform's key for `aKey` (Command-C, Ctrl+C), through the window.
	void hand_key(QWidget& aWidget, QKeySequence::StandardKey aKey)
	{
		const auto combination = QKeySequence{ aKey }[0];
		tests::window_system_key(aWidget, static_cast<int>(combination.key()), combination.keyboardModifiers());
	}

	/// The result menu showing over `aTree`, if one is.
	auto shown_menu(QTreeView& aTree) -> QMenu*
	{
		for (auto* menu : aTree.findChildren<QMenu*>(QStringLiteral("sourceSearchResultMenu"))) {
			if (menu->isVisible()) {
				return menu;
			}
		}
		return nullptr;
	}

	/// The result menu a right-click on `aIndex` opens.
	auto right_click_menu(QTreeView& aTree, const QModelIndex& aIndex) -> QMenu*
	{
		hand_click(*aTree.viewport(), aTree.visualRect(aIndex).center(), Qt::NoModifier, Qt::RightButton);
		return shown_menu(aTree);
	}

	auto menu_action(QMenu& aMenu, const char* aName) -> QAction*
	{
		for (auto* action : aMenu.actions()) {
			if (action->objectName() == QString::fromUtf8(aName)) {
				return action;
			}
		}
		return nullptr;
	}

	/// Chooses the item named `aName` with a click on it; the menu closes.
	void choose(QMenu* aMenu, const char* aName)
	{
		auto* action = aMenu ? menu_action(*aMenu, aName) : nullptr;
		expect(action != nullptr && action->isEnabled(), std::string{ "the menu offers " } + aName);
		// The menu deletes itself once closed, so it is watched rather than read.
		const auto watched = QPointer<QMenu>{ aMenu };
		hand_click(*aMenu, aMenu->actionGeometry(action).center());
		expect(!watched || !watched->isVisible(), std::string{ "choosing closes the menu: " } + aName);
	}

	/// Closes the menu with Escape, choosing nothing.
	void dismiss(QMenu* aMenu)
	{
		expect(aMenu != nullptr, "a menu to dismiss");
		const auto watched = QPointer<QMenu>{ aMenu };
		tests::window_system_key(*aMenu, Qt::Key_Escape);
		expect(!watched || !watched->isVisible(), "Escape closes the menu");
	}

	auto painted_left_edge(QTreeView& aTree, const QModelIndex& aIndex, const QColor& aFill) -> int
	{
		const auto image = aTree.viewport()->grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto area  = aTree.visualRect(aIndex);
		for (int x = 0; x < image.width(); ++x) {
			for (int y = static_cast<int>(area.top() * ratio); y < static_cast<int>((area.bottom() + 1) * ratio); ++y) {
				if (image.pixelColor(x, y) == aFill) {
					return static_cast<int>(x / ratio);
				}
			}
		}
		return -1;
	}

	/// The run of pixels painted in the wash along one scan line of the row.
	auto painted_wash(QTreeView& aTree, const QModelIndex& aIndex, const QColor& aWash) -> std::pair<int, int>
	{
		const auto rect  = aTree.visualRect(aIndex);
		const auto image = aTree.viewport()->grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto y     = static_cast<int>((rect.top() + 3) * ratio);
		auto       first = -1;
		auto       last  = -1;
		for (int x = static_cast<int>(rect.left() * ratio); x < static_cast<int>(rect.right() * ratio); ++x) {
			const auto pixel = image.pixelColor(x, y);
			if (std::abs(pixel.red() - aWash.red()) <= 3 && std::abs(pixel.green() - aWash.green()) <= 3 && std::abs(pixel.blue() - aWash.blue()) <= 3) {
				first = first < 0 ? x : first;
				last  = x;
			}
		}
		return { static_cast<int>(first / ratio), static_cast<int>(last / ratio) };
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	auto         panel = qt::source_search_panel{ nullptr };
	auto         light = qt::appearance_profile{ application::appearance_settings{ .Mode = core::appearance_mode::light } };
	light.apply_to(panel);
	panel.adopt_appearance(light);
	panel.set_snippet_font(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	panel.set_scope_labels(QStringLiteral("PR #77  0123abcd → 4567ef01"), QStringLiteral("Left 0123abcd\nRight 4567ef01"));
	panel.resize(300, 560);
	panel.show();
	auto filters            = std::vector<core::source_side_filter>{ };
	panel.SideFilterChanged = [&](auto aFilter) { filters.push_back(aFilter); };
	auto* tree              = panel.tree();
	auto* model             = tree->model();

	const auto idle = panel.summary_text();
	expect(idle.startsWith(QStringLiteral("Type to search")), "idle summary prompts");
	panel.present(fixture());
	expect(model->rowCount() == 3, "three file rows");
	expect(panel.summary_text() == QStringLiteral("5 matches in 3 files"), "plain summary counts");

	// AC3: file rows.
	const auto modified = model->index(0, 0);
	const auto added    = model->index(1, 0);
	const auto renamed  = model->index(2, 0);
	expect(modified.data(qt::source_search_name_role).toString() == QStringLiteral("find_controller.cpp") && modified.data(qt::source_search_folder_role).toString() == QStringLiteral("src/search"), "file name and folder");
	expect(modified.data(qt::source_search_count_role).toUInt() == 3U && modified.data(qt::source_search_change_role).toString().isEmpty(), "modified file count, no tag");
	expect(added.data(qt::source_search_change_role).toString() == QStringLiteral("added"), "added tag");
	expect(renamed.data(qt::source_search_change_role).toString() == QStringLiteral("renamed") && renamed.data(qt::source_search_name_role).toString() == QStringLiteral("name.cpp → renamed.cpp") && renamed.data(qt::source_search_folder_role).toString() == QStringLiteral("old → src"), "rename shows old → new");
	expect(renamed.data().toString().contains(QStringLiteral("old/name.cpp → src/renamed.cpp")), "rename accessible text names both paths");

	// AC2: match rows carry side tags, line numbers and the exact match range.
	for (const auto& leaf : leaves(*model)) {
		const auto preview = leaf.data(qt::source_search_preview_role).toString();
		const auto start   = leaf.data(qt::source_search_highlight_start_role).toInt();
		const auto length  = leaf.data(qt::source_search_highlight_length_role).toInt();
		expect(preview.mid(start, length) == QStringLiteral("needle"), "declared highlight is exactly the match");
		expect(!preview.startsWith(QLatin1Char(' ')), "leading blanks are not drawn");
	}
	const auto first = model->index(0, 0, modified);
	expect(first.data(qt::source_search_sides_role).toInt() == 1 && first.data(qt::source_search_line_role).toString() == QStringLiteral("12"), "left tag and one-based line");
	expect(model->index(1, 0, modified).data(qt::source_search_sides_role).toInt() == 2, "right tag");
	expect(model->index(2, 0, modified).data(qt::source_search_sides_role).toInt() == 3, "deduplicated match carries both tags");
	expect(model->index(2, 0, modified).data(qt::source_search_line_role).toString() == QStringLiteral("1204") && model->index(2, 0, modified).data(Qt::ToolTipRole).toString().startsWith(QStringLiteral("Left 41, Right 1204: ")), "both-sides row shows the Right line and names both");
	expect(first.data().toString().startsWith(QStringLiteral("Left line 12: ")), "match accessible text names side and line");

	// AC2: the wash is painted over exactly the match's advance.
	tree->clearSelection();
	tree->setCurrentIndex(model->index(0, 0));
	QApplication::processEvents();
	// The pane's composite: the find wash at the selection's opacity over the code background.
	const auto wash          = qt::to_presentation_color(core::composited(light.palette()[core::index_of(core::appearance_role::find_match_background)], light.palette()[core::index_of(core::appearance_role::code_background)], core::selection_wash_alpha));
	const auto [left, right] = painted_wash(*tree, first, wash);
	const auto advance       = QFontMetrics{ QFontDatabase::systemFont(QFontDatabase::FixedFont) }.horizontalAdvance(QStringLiteral("needle"));
	std::cout << "PAINTED_WASH left=" << left << " right=" << right << " advance=" << advance << '\n';
	expect(left >= 0 && std::abs((right - left + 1) - (advance + 2)) <= 3, "painted wash spans the match text");
	// [[BPR-1504]]. On the selected row the match keeps the same amber rather than greying over the accent.
	tree->setCurrentIndex(first);
	tree->selectionModel()->select(first, QItemSelectionModel::ClearAndSelect);
	QApplication::processEvents();
	const auto [selectedLeft, selectedRight] = painted_wash(*tree, first, wash);
	std::cout << "SELECTED_WASH left=" << selectedLeft << " right=" << selectedRight << '\n';
	expect(selectedLeft >= 0 && std::abs(selectedLeft - left) <= 2 && std::abs(selectedRight - right) <= 2, "a selected row paints the match in the same find colour");

	// Owner, 2026-10-14: line numbers and snippets start at one x on every row.
	const auto aligned = [&](const char* aWhere) {
		const auto rows = leaves(*model);
		const auto one  = panel.match_columns(rows.front());
		expect(one.Tags.left() == 4, "side markers start at the viewport's left content inset");
		expect(one.Number.width() > 0 && one.Snippet.left() > one.Number.right(), "match columns are laid out");
		for (const auto& leaf : rows) {
			const auto columns = panel.match_columns(leaf);
			expect(tree->visualRect(leaf).left() == 0, "match item geometry includes the side marker hit area");
			std::cout << "COLUMNS " << aWhere << " sides=" << leaf.data(qt::source_search_sides_role).toInt() << " tags=" << columns.Tags.left() << "+" << columns.Tags.width() << " number=" << columns.Number.left() << "+" << columns.Number.width() << " snippet=" << columns.Snippet.left() << '\n';
			expect(columns.Tags.left() == one.Tags.left() && columns.Tags.width() == one.Tags.width(), "side-tag column is fixed");
			expect(columns.Number.left() == one.Number.left() && columns.Number.right() == one.Number.right(), "line-number column is fixed");
			expect(columns.Snippet.left() == one.Snippet.left(), "snippet starts at one x");
		}
		expect(panel.match_columns(model->index(0, 0)).Number.isNull(), "file rows have no match columns");
	};
	aligned("narrow");
	const auto second = model->index(1, 0, modified);
	const auto tags   = panel.match_columns(first).Tags;
	expect(std::abs(painted_left_edge(*tree, first, light.color(core::appearance_role::diff_intraline_removed_background)) - tags.left()) <= 1, "left marker is painted at the viewport edge");
	expect(std::abs(painted_left_edge(*tree, second, light.color(core::appearance_role::diff_intraline_added_background)) - (tags.left() + (tags.width() - 2) / 2 + 2)) <= 1, "right marker retains its fixed slot at the viewport edge");
	auto selected  = std::vector<std::uint32_t>{ };
	auto activated = std::vector<std::uint32_t>{ };
	panel.Select   = [&](auto aIndex) { selected.push_back(aIndex); };
	panel.Activate = [&](auto aIndex) { activated.push_back(aIndex); };
	click(*tree->viewport(), QPoint{ tags.left() + 2, tree->visualRect(first).center().y() });
	expect(tree->currentIndex() == first && !activated.empty() && activated.back() == 0U, "clicking the left marker activates its result");
	click(*tree->viewport(), QPoint{ tags.right() - 2, tree->visualRect(second).center().y() });
	expect(tree->currentIndex() == second && !selected.empty() && selected.back() == 1U && activated.back() == 1U, "clicking the right marker selects and activates its result");
	QKeyEvent previous{ QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier };
	QApplication::sendEvent(tree, &previous);
	expect(tree->currentIndex() == first && selected.back() == 0U, "keyboard navigation preserves result order");
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(tree, &enter);
	expect(activated.back() == 0U, "Enter activates the keyboard-selected result");
	click(*tree->viewport(), QPoint{ tree->indentation() / 2, tree->visualRect(modified).center().y() });
	expect(!tree->isExpanded(modified), "native file disclosure still collapses its results");
	click(*tree->viewport(), QPoint{ tree->indentation() / 2, tree->visualRect(modified).center().y() });
	expect(tree->isExpanded(modified), "native file disclosure still expands its results");
	expect(model->index(0, 0, renamed).data(qt::source_search_line_role).toString() == QStringLiteral("1"), "rename row line");
	render(panel, QStringLiteral("panel-light-narrow"));
	panel.resize(520, 560);
	render(panel, QStringLiteral("panel-light-wide"));
	QApplication::processEvents();
	aligned("wide");
	panel.resize(300, 560);

	// AC1: two independent side chips, both on; a match shows when any of
	// its sides is on, and the last side on stays on.
	constexpr auto leftOnly  = core::source_side_filter{ .Left = true, .Right = false };
	constexpr auto rightOnly = core::source_side_filter{ .Left = false, .Right = true };
	auto*          leftChip  = panel.findChild<QAbstractButton*>(QStringLiteral("sourceSearchSideLeft"));
	auto*          rightChip = panel.findChild<QAbstractButton*>(QStringLiteral("sourceSearchSideRight"));
	expect(leftChip && rightChip && leftChip->isChecked() && rightChip->isChecked() && panel.side_filter() == core::source_side_filter{ }, "both sides start on");
	expect(leftChip->accessibleName() == QStringLiteral("Left side") && rightChip->accessibleName() == QStringLiteral("Right side") && leftChip->focusPolicy() == Qt::StrongFocus && leftChip->toolTip().contains(QStringLiteral("at least one side stays on")), "chips are named, reachable and explain the rule");
	leftChip->click();
	expect(panel.side_filter() == rightOnly && !leftChip->isChecked() && filters.size() == 1U && filters.back() == rightOnly, "Left off announced to the controller");
	{
		auto* spoken = QAccessible::queryAccessibleInterface(leftChip);
		expect(spoken && spoken->state().checkable && !spoken->state().checked, "screen readers hear the chip's unchecked state");
	}
	expect(leaves(*model).size() == 4U && model->rowCount() == 3 && panel.summary_text() == QStringLiteral("4 matches in 3 files"), "Right only shows records with a right location");
	for (const auto& leaf : leaves(*model)) {
		expect((leaf.data(qt::source_search_sides_role).toInt() & 2) != 0, "every Right row has a right location");
	}
	rightChip->click();
	expect(rightChip->isChecked() && panel.side_filter() == rightOnly && filters.size() == 1U, "the last side on stays on");
	render(panel, QStringLiteral("panel-light-right-only"));
	leftChip->click();
	rightChip->click();
	expect(panel.side_filter() == leftOnly && leaves(*model).size() == 3U && model->rowCount() == 2 && panel.summary_text() == QStringLiteral("3 matches in 2 files"), "Left only hides the added file and right-only rows");
	expect(model->index(0, 0).data(qt::source_search_count_role).toUInt() == 2U, "file pill counts follow the filter");
	expect(model->index(1, 0, model->index(0, 0)).data(qt::source_search_sides_role).toInt() == 3, "a both-sides match shows under Left only");
	expect(tree->currentIndex().data(qt::source_search_match_role).toUInt() == 1U, "current match stays selected when shown");
	aligned("left-only");
	render(panel, QStringLiteral("panel-light-left-only"));
	rightChip->click();
	expect(panel.side_filter() == core::source_side_filter{ } && leaves(*model).size() == 5U && panel.summary_text() == QStringLiteral("5 matches in 3 files"), "both on restores every record");

	// [[BPR-PENDING-T]]. Shift and Command (Ctrl) select more results; Copy
	// writes them one per line in the list's order; the row menu opens one
	// result in the active tab or a new one and copies. Clicks and keys come
	// through the window system, as a hand's do.
	{
		const auto rows   = leaves(*model);
		const auto lines  = QStringList{ QStringLiteral("src/search/find_controller.cpp:12: const auto needle = locate(scope);"), QStringLiteral("src/search/find_controller.cpp:14: return needle_for(scope, options);"), QStringLiteral("src/search/find_controller.cpp:1204: // unchanged: the needle is shared"), QStringLiteral("src/search/added_view.cpp:1: auto view = make_needle_view(model);"), QStringLiteral("src/renamed.cpp:1: static int needle = 1;") };
		const auto copied = [&](std::initializer_list<int> aLines) {
			auto words = QStringList{ };
			for (const auto line : aLines) {
				words << lines[line];
			}
			return words.join(QLatin1Char('\n'));
		};
		const auto centre    = [&](const QModelIndex& aIndex) { return tree->visualRect(aIndex).center(); };
		auto*      clipboard = QApplication::clipboard();
		// What Copy (Command-C, Ctrl+C) puts on the clipboard now.
		const auto copyText = [&] {
			clipboard->setText(QStringLiteral("before"));
			hand_key(*tree, QKeySequence::Copy);
			return clipboard->text();
		};
		panel.activateWindow();
		tree->setFocus();
		QApplication::processEvents();
		expect(panel.isActiveWindow() && tree->hasFocus(), "the list holds the keyboard");
		expect(rows.size() == 5U && tree->selectionMode() == QAbstractItemView::ExtendedSelection && tree->selectionBehavior() == QAbstractItemView::SelectRows, "the results take an extended selection of rows");
		expect(tree->accessibleDescription().contains(QStringLiteral("Shift+F10")) && tree->accessibleDescription().contains(qt::spoken_key(qt::shortcut_key(qt::shortcut_command::copy_results))) && tree->accessibleDescription().contains(qt::spoken_key(qt::shortcut_key(qt::shortcut_command::select_results))), "the list says how to select, copy and reach the menu");

		// Without a new-tab handler the menu offers the item disabled.
		panel.ActivateInNewTab = { };
		auto* menu             = right_click_menu(*tree, rows[0]);
		expect(menu && menu->isVisible() && menu_action(*menu, "sourceSearchOpenInNewTab") && !menu_action(*menu, "sourceSearchOpenInNewTab")->isEnabled(), "Open in New Tab waits for a handler");
		dismiss(menu);
		auto newTab            = std::vector<std::uint32_t>{ };
		panel.ActivateInNewTab = [&](auto aIndex) { newTab.push_back(aIndex); };

		// A plain click selects and opens one result.
		hand_click(*tree->viewport(), centre(rows[3]));
		const auto opened = activated.size();
		expect(activated.back() == 3U && tree->selectionModel()->selectedRows().size() == 1, "a plain click selects one result and opens it");
		// Shift-click selects the range up to the first result and opens nothing.
		hand_click(*tree->viewport(), centre(rows[0]), Qt::ShiftModifier);
		expect(activated.size() == opened && tree->currentIndex() == rows[0], "Shift-click moves the cursor and opens nothing");
		expect(copyText() == copied({ 0, 1, 2, 3 }), "a range over a file row copies its results once, in the list's order");
		// Command (Ctrl) toggles single rows.
		hand_click(*tree->viewport(), centre(rows[4]), Qt::ControlModifier);
		hand_click(*tree->viewport(), centre(rows[1]), Qt::ControlModifier);
		expect(activated.size() == opened, "Command-click selects and opens nothing");
		expect(copyText() == copied({ 0, 2, 3, 4 }), "Command-click adds and removes single results");
		{
			QKeyEvent override{ QEvent::ShortcutOverride, static_cast<int>(QKeySequence{ QKeySequence::Copy }[0].key()), QKeySequence{ QKeySequence::Copy }[0].keyboardModifiers() };
			override.ignore();
			QApplication::sendEvent(tree, &override);
			expect(override.isAccepted(), "the list claims Copy ahead of any window shortcut");
		}
		expect(copyText() == copied({ 0, 2, 3, 4 }), "Copy puts the selected results on the clipboard, one per line, in the list's order");
		// Return still opens the current result alone.
		tests::window_system_key(*tree, Qt::Key_Return);
		expect(activated.size() == opened + 1U && activated.back() == 1U, "Return opens the current result, not the selection");

		// Select All takes every row; a selected file stands for its results.
		hand_key(*tree, QKeySequence::SelectAll);
		expect(tree->selectionModel()->selectedRows().size() == 8, "Select All selects every file and result row");
		hand_key(*tree, QKeySequence::Copy);
		expect(clipboard->text() == copied({ 0, 1, 2, 3, 4 }), "Select All then Copy writes every result once");
		// A plain arrow goes back to one selected result.
		tests::window_system_key(*tree, Qt::Key_Down);
		expect(tree->selectionModel()->selectedRows().size() == 1 && tree->currentIndex() == rows[2] && selected.back() == 2U, "a plain arrow moves the cursor and selects one result");
		tests::window_system_key(*tree, Qt::Key_Up, Qt::ShiftModifier);
		expect(copyText() == copied({ 1, 2 }) && activated.size() == opened + 1U, "Shift+Up extends the selection and opens nothing");
		{
			auto* spoken = QAccessible::queryAccessibleInterface(tree);
			auto* table  = spoken ? spoken->tableInterface() : nullptr;
			expect(table && table->selectedCellCount() == 2, "screen readers hear both selected results");
		}

		// The row menu: Open, Open in New Tab, Copy, Copy Path.
		menu = right_click_menu(*tree, rows[2]);
		expect(menu && menu->isVisible(), "right-click opens the result menu");
		auto names = QStringList{ };
		for (const auto* action : menu->actions()) {
			names << (action->isSeparator() ? QStringLiteral("|") : action->text());
		}
		std::cout << "RESULT_DESCRIPTION " << tree->accessibleDescription().toStdString() << '\n';
		std::cout << "RESULT_MENU " << names.join(QLatin1Char(',')).toStdString() << '\n';
		expect(names == QStringList{ QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("|"), QStringLiteral("Copy"), QStringLiteral("Copy Path") }, "the menu's items, in order");
		expect(menu_action(*menu, "sourceSearchCopy")->shortcut() == QKeySequence{ QKeySequence::Copy } && qt::shortcut_key(qt::shortcut_command::copy_results) == QKeySequence{ QKeySequence::Copy }, "Copy shows the catalogue's key, the platform's Copy");
		expect(tree->selectionModel()->selectedRows() == QModelIndexList{ rows[1], rows[2] }, "right-click inside the selection keeps it");
		choose(menu, "sourceSearchOpenInNewTab");
		expect(newTab.size() == 1U && newTab.back() == 2U && activated.size() == opened + 1U, "Open in New Tab asks for the clicked result in a new tab");
		menu = right_click_menu(*tree, rows[2]);
		choose(menu, "sourceSearchOpen");
		expect(activated.back() == 2U && newTab.size() == 1U, "Open opens the clicked result in the active tab");
		menu = right_click_menu(*tree, rows[2]);
		clipboard->setText(QStringLiteral("before"));
		choose(menu, "sourceSearchCopy");
		expect(clipboard->text() == copied({ 1, 2 }), "the menu's Copy writes the selected results");
		menu = right_click_menu(*tree, rows[2]);
		choose(menu, "sourceSearchCopyPath");
		expect(clipboard->text() == QStringLiteral("src/search/find_controller.cpp"), "Copy Path writes the file's path once");
		// Right-click outside the selection selects that row alone.
		menu = right_click_menu(*tree, rows[4]);
		expect(menu_action(*menu, "sourceSearchCopyPath")->text() == QStringLiteral("Copy Path"), "one file's path");
		dismiss(menu);
		expect(copyText() == copied({ 4 }), "right-click outside the selection chooses that result");
		// A file row's menu opens the file at its first result.
		menu = right_click_menu(*tree, added);
		choose(menu, "sourceSearchOpenInNewTab");
		expect(newTab.back() == 3U, "a file row opens at its first result");
		tree->selectionModel()->select(QItemSelection{ rows[0], rows[0] }, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		tree->selectionModel()->select(QItemSelection{ rows[3], rows[3] }, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		menu = right_click_menu(*tree, rows[3]);
		expect(menu_action(*menu, "sourceSearchCopyPath")->text() == QStringLiteral("Copy Paths"), "results in several files copy their paths");
		choose(menu, "sourceSearchCopyPath");
		expect(clipboard->text() == QStringLiteral("src/search/find_controller.cpp\nsrc/search/added_view.cpp"), "Copy Paths writes each file once, in the list's order");

		// The menu key and Shift+F10 open the current result's menu; F10 alone does not.
		tree->setFocus();
		tree->setCurrentIndex(rows[4]);
		for (const auto& [key, modifiers] : { std::pair{ Qt::Key_Menu, Qt::KeyboardModifiers{ Qt::NoModifier } }, std::pair{ Qt::Key_F10, Qt::KeyboardModifiers{ Qt::ShiftModifier } } }) {
			tests::window_system_key(*tree, key, modifiers);
			menu = shown_menu(*tree);
			expect(menu != nullptr, "the menu key and Shift+F10 open the current result's menu");
			expect(menu->geometry().top() >= tree->viewport()->mapToGlobal(tree->visualRect(rows[4]).topLeft()).y(), "the keyboard's menu opens at the current row");
			choose(menu, "sourceSearchOpenInNewTab");
			expect(newTab.back() == 4U, "the keyboard's menu acts on the current result");
		}
		tests::window_system_key(*tree, Qt::Key_F10);
		expect(shown_menu(*tree) == nullptr, "F10 alone opens no result menu");

		// A move from outside the list (F4) selects the one current result.
		tree->selectionModel()->select(QItemSelection{ rows[0], rows[1] }, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		auto moved         = fixture();
		moved.CurrentMatch = 2U;
		panel.present(moved);
		expect(tree->currentIndex() == rows[2] && tree->selectionModel()->selectedRows().size() == 1 && copyText() == copied({ 2 }), "F4's current result replaces the selection");
		tree->clearSelection();
	}

	// AC3: the details toggle and collapse-all.
	auto* include = panel.findChild<QLineEdit*>(QStringLiteral("sourceFilesInclude"));
	auto* details = panel.findChild<QToolButton*>(QStringLiteral("sourceSearchDetailsToggle"));
	expect(include && details && !include->isVisible() && !details->accessibleName().isEmpty(), "include and exclude start behind the … toggle");
	details->click();
	expect(include->isVisible() && panel.findChild<QLineEdit*>(QStringLiteral("sourceFilesExclude"))->isVisible(), "… shows include and exclude");
	render(panel, QStringLiteral("panel-light-details"));
	details->click();
	expect(!include->isVisible(), "… hides them again");
	expect(tree->isExpanded(modified) && tree->isExpanded(renamed), "file groups start expanded");
	panel.findChild<QToolButton*>(QStringLiteral("sourceSearchCollapseAll"))->click();
	expect(!tree->isExpanded(modified) && !tree->isExpanded(added) && !tree->isExpanded(renamed), "collapse-all collapses every file");
	tree->expandAll();

	// Partial coverage is one short line that expands to the reasons.
	auto partial                 = fixture();
	partial.Status               = core::source_search_status::partial;
	partial.Summary.SkippedSides = 2U;
	partial.RetryAvailable       = true;
	auto skipped                 = std::make_shared<std::vector<application::source_search_skipped_side>>();
	skipped->push_back({ core::file_identity_of("src/search/added_view.cpp"), core::diff_side::right, application::diff_blob_status::binary });
	skipped->push_back({ core::file_identity_of("src/renamed.cpp"), core::diff_side::left, application::diff_blob_status::missing });
	partial.Skipped = skipped;
	panel.present(partial);
	auto* toggle = panel.findChild<QToolButton*>(QStringLiteral("sourceSearchSkippedToggle"));
	expect(toggle->isVisible() && toggle->text() == QStringLiteral("2 sides skipped") && panel.findChild<QToolButton*>(QStringLiteral("sourceSearchRetry"))->isVisible(), "skipped sides show as one line with Retry");
	toggle->click();
	render(panel, QStringLiteral("panel-light-partial"));
	toggle->click();

	// Empty and filtered-empty states read as plain words with a hint.
	auto empty                  = application::source_search_projection{ };
	empty.Query                 = "absent";
	empty.Status                = core::source_search_status::complete;
	empty.Summary.EligibleFiles = 3U;
	empty.Summary.SearchedFiles = 3U;
	empty.NoResults             = true;
	empty.Open                  = true;
	panel.present(empty);
	expect(panel.summary_text() == QStringLiteral("No matches in 3 files.") && panel.findChild<QLabel*>(QStringLiteral("sourceSearchHint"))->isVisible(), "empty state and hint");
	render(panel, QStringLiteral("panel-light-empty"));
	auto searching   = fixture();
	searching.Status = core::source_search_status::searching;
	searching.Query  = "needle ";
	panel.present(searching);
	expect(panel.summary_text() == QStringLiteral("Searching… 5 matches in 3 files so far"), "searching reads plainly");
	auto invalid   = application::source_search_projection{ };
	invalid.Status = core::source_search_status::invalid_pattern;
	invalid.Query  = "(";
	panel.present(invalid);
	expect(panel.summary_text() == QStringLiteral("Invalid regular expression"), "invalid regex named in the summary");

	// [[BPR-1688]]. Under a commit or file filter Find in files says what it searches.
	auto filteredIdle                 = application::source_search_projection{ };
	filteredIdle.Scope                = core::source_search_scope{ };
	filteredIdle.Scope->Target        = core::source_search_target::files;
	filteredIdle.Scope->FilteredFiles = 12U;
	panel.present(filteredIdle);
	expect(panel.summary_text() == QStringLiteral("Type to search the filtered set's 12 files on both sides."), "idle under a filter names the filtered set");
	filteredIdle.Scope->FilteredFiles = 1U;
	panel.present(filteredIdle);
	expect(panel.summary_text() == QStringLiteral("Type to search the filtered set's 1 file on both sides."), "one filtered file reads in the singular");
	filteredIdle.Scope->FilteredFiles = 0U;
	panel.present(filteredIdle);
	expect(panel.summary_text() == QStringLiteral("The filters leave no file to search. Clear a filter to search more."), "a filter that leaves nothing says how to search more");
	auto filteredDone                 = fixture();
	filteredDone.Scope                = filteredIdle.Scope;
	filteredDone.Scope->FilteredFiles = 3U;
	panel.present(filteredDone);
	expect(panel.summary_text() == QStringLiteral("5 matches in 3 files; searched the filtered set"), "a finished search says it searched the filtered set");
	filteredDone.Scope->FilteredFiles.reset();
	panel.present(filteredDone);
	expect(panel.summary_text() == QStringLiteral("5 matches in 3 files"), "an unfiltered search says nothing more");

	// Dark theme renders.
	auto dark = qt::appearance_profile{ application::appearance_settings{ .Mode = core::appearance_mode::dark } };
	dark.apply_to(panel);
	panel.adopt_appearance(dark);
	panel.present(fixture());
	tree->setCurrentIndex(model->index(1, 0, model->index(0, 0)));
	render(panel, QStringLiteral("panel-dark-narrow"));
	panel.resize(520, 560);
	render(panel, QStringLiteral("panel-dark-wide"));

	leftChip->click();
	panel.reset();
	expect(panel.side_filter() == core::source_side_filter{ } && leftChip->isChecked(), "reset restores both sides");
	std::cout << "source-search-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
