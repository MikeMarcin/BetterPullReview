/// \file
/// \brief [[BPR-1504]]. Find in Files' result view: side filter, file and match
/// rows, the painted match wash, the details toggle and collapse-all.

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/source_search_panel.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
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
	auto wash = light.color(core::appearance_role::find_match_background);
	{
		const auto base = tree->viewport()->palette().color(QPalette::Active, QPalette::Base);
		const auto a    = core::selection_wash_alpha / 255.0;
		wash            = QColor::fromRgbF(static_cast<float>(wash.redF() * a + base.redF() * (1 - a)), static_cast<float>(wash.greenF() * a + base.greenF() * (1 - a)), static_cast<float>(wash.blueF() * a + base.blueF() * (1 - a)));
	}
	const auto [left, right] = painted_wash(*tree, first, wash);
	const auto advance       = QFontMetrics{ QFontDatabase::systemFont(QFontDatabase::FixedFont) }.horizontalAdvance(QStringLiteral("needle"));
	std::cout << "PAINTED_WASH left=" << left << " right=" << right << " advance=" << advance << '\n';
	expect(left >= 0 && std::abs((right - left + 1) - (advance + 2)) <= 3, "painted wash spans the match text");

	// Owner, 2026-10-14: line numbers and snippets start at one x on every row.
	const auto aligned = [&](const char* aWhere) {
		const auto rows = leaves(*model);
		const auto one  = panel.match_columns(rows.front());
		expect(one.Number.width() > 0 && one.Snippet.left() > one.Number.right(), "match columns are laid out");
		for (const auto& leaf : rows) {
			const auto columns = panel.match_columns(leaf);
			std::cout << "COLUMNS " << aWhere << " sides=" << leaf.data(qt::source_search_sides_role).toInt() << " tags=" << columns.Tags.left() << "+" << columns.Tags.width() << " number=" << columns.Number.left() << "+" << columns.Number.width() << " snippet=" << columns.Snippet.left() << '\n';
			expect(columns.Tags.left() == one.Tags.left() && columns.Tags.width() == one.Tags.width(), "side-tag column is fixed");
			expect(columns.Number.left() == one.Number.left() && columns.Number.right() == one.Number.right(), "line-number column is fixed");
			expect(columns.Snippet.left() == one.Snippet.left(), "snippet starts at one x");
		}
		expect(panel.match_columns(model->index(0, 0)).Number.isNull(), "file rows have no match columns");
	};
	aligned("narrow");
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
