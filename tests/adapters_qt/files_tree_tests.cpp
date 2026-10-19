#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/resident_filter.hpp"

#include <QApplication>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QStyleOptionViewItem>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

	[[nodiscard]] auto file(const std::string& aPath) -> qt::workbench_drawer_row
	{
		const auto identity = core::file_identity_of(aPath);
		return qt::workbench_drawer_row{
			.Identity = QString::fromStdString(aPath),
			.Title    = QString::fromStdString(aPath),
			.Detail   = QString::fromStdString(aPath),
			.File     = identity,
		};
	}

	[[nodiscard]] auto overview(
		const std::string&                              aPath,
		const std::uint64_t                             aAdditions,
		const std::uint64_t                             aDeletions,
		const application::overview_change_since_review aChanged,
		const std::optional<core::file_read_state>      aRead,
		const std::optional<std::size_t>                aUnread) -> application::overview_file_row
	{
		return application::overview_file_row{
			.Path        = aPath,
			.Name        = aPath.substr(aPath.rfind('/') + 1U),
			.Operation   = application::overview_file_operation::modified,
			.Additions   = aAdditions,
			.Deletions   = aDeletions,
			.Changed     = aChanged,
			.Read        = aRead,
			.UnreadCount = aUnread,
		};
	}

	[[nodiscard]] auto labels(const qt::files_tree_model& aModel, const QModelIndex& aParent = { })
		-> std::vector<QString>
	{
		auto found = std::vector<QString>{ };
		for (int row = 0; row < aModel.rowCount(aParent); ++row) {
			found.push_back(aModel.index(row, 0, aParent).data().toString());
		}
		return found;
	}

	[[nodiscard]] auto pixels(const QImage& aImage, const QRect& aArea, const QColor& aColour) -> int
	{
		auto count = 0;
		for (auto y = aArea.top(); y <= aArea.bottom(); ++y) {
			for (auto x = aArea.left(); x <= aArea.right(); ++x) {
				const auto actual = aImage.pixelColor(x, y);
				count += qAbs(actual.red() - aColour.red()) < 24
						&& qAbs(actual.green() - aColour.green()) < 24
						&& qAbs(actual.blue() - aColour.blue()) < 24
					? 1
					: 0;
			}
		}
		return count;
	}

	// [[BPR-0658]]. The tree order: folders as the paths nest, nesting-only
	// folders compacted into one row, folders before files, counts on folders.
	auto test_the_tree_compacts_nesting_only_folders_and_counts_files() -> void
	{
		qt::files_tree_view view;
		view.resize(320, 500);
		view.show();
		QApplication::processEvents();
		view.apply({
			file("Engine/Source/Runtime/Renderer/Private/HairStrands/HairStrandsLighting.cpp"),
			file("Engine/Source/Runtime/Renderer/Private/HairStrands/HairStrandsVoxelization.cpp"),
			file("Engine/Shaders/Private/HairStrands/HairStrandsLightingCommon.ush"),
			file("README.md"),
		});
		QApplication::processEvents();
		auto* const model = view.tree_model();
		expect(model->file_count() == 4U, "every file is counted once at the root");
		expect(labels(*model) == std::vector<QString>{ QStringLiteral("Engine"), QStringLiteral("README.md") },
			   "the root holds the one top folder before the top-level file");
		const auto engine = model->index(0, 0);
		expect(engine.data(qt::files_tree_model::folder_role).toBool()
				   && engine.data(qt::files_tree_model::file_count_role).toULongLong() == 3U,
			   "a folder is a folder and counts the files beneath it");
		expect(labels(*model, engine)
				   == std::vector<QString>{ QStringLiteral("Shaders/Private/HairStrands"),
											QStringLiteral("Source/Runtime/Renderer/Private/HairStrands") },
			   "nesting-only folders are compacted into one row each, alphabetical");
		const auto source = model->index(1, 0, engine);
		expect(source.data(qt::files_tree_model::file_count_role).toULongLong() == 2U
				   && labels(*model, source)
					   == std::vector<QString>{ QStringLiteral("HairStrandsLighting.cpp"),
												QStringLiteral("HairStrandsVoxelization.cpp") },
			   "a compacted folder carries its count and its files by name");
		const auto lighting = model->index(0, 0, source);
		expect(!lighting.data(qt::files_tree_model::folder_role).toBool()
				   && lighting.data(qt::files_tree_model::path_role).toString()
					   == QStringLiteral("Engine/Source/Runtime/Renderer/Private/HairStrands/HairStrandsLighting.cpp")
				   && model->parent(lighting) == source && model->parent(source) == engine
				   && !model->parent(engine).isValid(),
			   "a file names its full path and its parents lead back to the root");
		const auto id = core::file_identity_of("README.md");
		expect(model->index_for_file(id).isValid()
				   && model->file_at(model->index_for_file(id)) == std::optional{ id }
				   && !model->file_at(engine).has_value(),
			   "a file is found by its identity and a folder is no file");
		view.hide();
	}

	// [[BPR-0658]]. Flat lists full paths; changed-first puts what the
	// overview says changed since my last review before the rest; the filter
	// keeps a matching file's parents; the overview's facts reach the roles.
	auto test_orders_filter_and_the_overview_join() -> void
	{
		qt::files_tree_view view;
		view.resize(320, 500);
		view.show();
		QApplication::processEvents();
		view.apply({ file("src/b.cpp"), file("src/a.cpp"), file("docs/x.md") });
		view.apply_overview(std::vector{
			overview("src/b.cpp", 12U, 3U, application::overview_change_since_review::changed,
					 core::file_read_state::unread, std::optional<std::size_t>{ 2U }),
			overview("src/a.cpp", 1U, 0U, application::overview_change_since_review::unchanged,
					 core::file_read_state::read, std::optional<std::size_t>{ 0U }),
		});
		QApplication::processEvents();
		auto* const model = view.tree_model();
		view.set_order(qt::files_order::flat);
		expect(labels(*model) == std::vector<QString>{ QStringLiteral("docs/x.md"), QStringLiteral("src/a.cpp"), QStringLiteral("src/b.cpp") },
			   "flat lists every file by its full path, alphabetical");
		view.set_order(qt::files_order::changed_first);
		expect(labels(*model) == std::vector<QString>{ QStringLiteral("src/b.cpp"), QStringLiteral("docs/x.md"), QStringLiteral("src/a.cpp") },
			   "changed-first puts the file the overview says changed before the rest, in order");
		const auto b = model->index(0, 0);
		expect(b.data(qt::files_tree_model::additions_role).toULongLong() == 12U
				   && b.data(qt::files_tree_model::deletions_role).toULongLong() == 3U
				   && b.data(qt::files_tree_model::changed_role).toBool()
				   && b.data(qt::files_tree_model::read_role).toInt() == static_cast<int>(core::file_read_state::unread)
				   && b.data(qt::files_tree_model::unread_role).toInt() == 2
				   && b.data(qt::files_tree_model::operation_role).toString() == QStringLiteral("M"),
			   "the overview's counts, change, read state and operation reach the row");
		const auto x = model->index(1, 0);
		expect(x.data(qt::files_tree_model::read_role).toInt() == -1 && x.data(qt::files_tree_model::unread_role).toInt() == -1
				   && x.data(qt::files_tree_model::operation_role).toString().isEmpty(),
			   "a file the overview has not described says so rather than guessing");
		view.set_order(qt::files_order::tree);
		view.set_filter(QStringLiteral("A.CPP"));
		expect(labels(*model) == std::vector<QString>{ QStringLiteral("src") }
				   && labels(*model, model->index(0, 0)) == std::vector<QString>{ QStringLiteral("a.cpp") }
				   && model->file_count() == 1U,
			   "the filter keeps the matching file's folder and drops the rest, case-insensitively");
		// [[BPR-0981]]. RFC-BPR-0025's resident fuzzy match: letters in order,
		// not a substring; each space-separated term must match; the folders
		// of a shown file stay in every order.
		view.set_filter(QStringLiteral("acp"));
		expect(labels(*model) == std::vector<QString>{ QStringLiteral("src") }
				   && labels(*model, model->index(0, 0)) == std::vector<QString>{ QStringLiteral("a.cpp") }
				   && model->file_count() == 1U,
			   "letters in order find a file whose path spells them apart");
		view.set_filter(QStringLiteral("src b"));
		expect(model->file_count() == 1U
				   && labels(*model, model->index(0, 0)) == std::vector<QString>{ QStringLiteral("b.cpp") },
			   "every term of the filter must match, so 'src b' is src/b.cpp alone");
		view.set_order(qt::files_order::flat);
		expect(labels(*model) == std::vector<QString>{ QStringLiteral("src/b.cpp") },
			   "the fuzzy filter narrows the flat order the same way");
		view.set_order(qt::files_order::tree);
		view.set_filter(QStringLiteral("zzz"));
		expect(model->file_count() == 0U && model->rowCount() == 0,
			   "a filter nothing matches leaves the tree honestly empty");
		expect(qt::resident_filter_matches(QStringLiteral("adaptDispatch.cpp"), QStringLiteral("ADISP"))
				   && !qt::resident_filter_matches(QStringLiteral("adaptDispatch.cpp"), QStringLiteral("dispa x"))
				   && qt::resident_filter_matches(QStringLiteral("anything"), QStringLiteral("  ")),
			   "the matcher folds case, needs every term, and an empty needle matches everything");
		view.set_filter(QString{ });
		expect(model->file_count() == 3U, "clearing the filter restores every file");
		view.hide();
	}

	// [[BPR-1331]] v003. The owner on v002, 2026-10-09: "It's very weird to me
	// that we have 2 icon languages for the same concept. Pick one. I think the
	// circles here is probably better overall but it should get some color."
	// Each file shows its mark in the review bar's family, Overview's, in the
	// theme's colours; the unread comments' dot sits beside the counts.
	auto test_file_marks_are_the_review_bar_family() -> void
	{
		using mark = core::file_progress_mark;
		qt::files_tree_view view;
		view.resize(480, 300);
		view.set_order(qt::files_order::flat);
		const auto names = std::array<std::string, 7>{ "unread.cpp", "seen.cpp", "read.cpp", "accepted.cpp", "excluded.cpp", "comments.cpp", "unknown.cpp" };
		auto       rows  = std::vector<qt::workbench_drawer_row>{ };
		for (const auto& name : names) {
			rows.push_back(file(name));
		}
		view.apply(rows);
		const auto marked = [](const std::string& aPath, const std::optional<mark> aMark, const std::optional<std::size_t> aUnread) {
			auto row = overview(aPath, 3U, 1U, application::overview_change_since_review::unchanged, std::nullopt, aUnread);
			row.Mark = aMark;
			return row;
		};
		view.apply_overview(std::vector{ marked("unread.cpp", std::nullopt, 0U), marked("seen.cpp", mark::seen, 0U), marked("read.cpp", mark::read, 0U),
										 marked("accepted.cpp", mark::accepted, 0U), marked("excluded.cpp", mark::excluded, 0U),
										 marked("comments.cpp", mark::read, 2U) });
		view.show();
		QApplication::processEvents();
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(view);
			view.adopt_appearance(profile);
			QApplication::processEvents();
			const auto& colours = view.delegate()->progress_colours();
			expect(colours == qt::progress_mark_colours_for(profile), "the drawer adopts the theme's mark colours");
			auto images = std::vector<QImage>{ };
			for (const auto& path : names) {
				// At 2x, so a ring's stroke has fully covered pixels of its colour.
				auto image = QImage{ 960, 48, QImage::Format_ARGB32 };
				image.setDevicePixelRatio(2.0);
				image.fill(view.palette().color(QPalette::Base));
				auto option = QStyleOptionViewItem{ };
				option.initFrom(&view);
				option.widget = &view;
				option.rect   = QRect{ 0, 0, 480, 24 };
				option.font   = view.font();
				option.state  = QStyle::State_Enabled;
				{
					auto painter = QPainter{ &image };
					view.delegate()->paint(&painter, option, view.tree_model()->index_for_file(core::file_identity_of(path)));
				}
				images.push_back(image);
			}
			if (const auto renderRoot = qEnvironmentVariable("BPR_FILES_TREE_RENDER_ROOT"); !renderRoot.isEmpty()) {
				auto sheet = QImage{ 960, 48 * static_cast<int>(images.size()), QImage::Format_ARGB32 };
				auto paint = QPainter{ &sheet };
				for (std::size_t row = 0U; row < images.size(); ++row) {
					auto plain = images[row];
					plain.setDevicePixelRatio(1.0);
					paint.drawImage(QPoint{ 0, 48 * static_cast<int>(row) }, plain);
				}
				paint.end();
				expect(sheet.save(renderRoot + QStringLiteral("/files-marks-%1.png").arg(name)), "the drawer's marks render is saved");
			}
			// The mark's slot and the counts' end of the row, in device pixels.
			const auto slot    = QRect{ 8, 0, 30, 48 };
			const auto counts  = QRect{ 660, 0, 300, 48 };
			const auto has     = [&](const std::size_t aRow, const QColor& aColour) { return pixels(images[aRow], slot, aColour) > 0; };
			const auto exactly = [&](const std::size_t aRow, const QColor& aColour) {
				for (auto y = slot.top(); y <= slot.bottom(); ++y) {
					for (auto x = slot.left(); x <= slot.right(); ++x) {
						if (images[aRow].pixelColor(x, y).rgb() == aColour.rgb()) {
							return true;
						}
					}
				}
				return false;
			};
			expect(exactly(0U, colours.Neutral) && exactly(1U, colours.Seen) && exactly(2U, colours.Read) && exactly(3U, colours.Accepted)
					   && exactly(4U, colours.Neutral) && exactly(4U, colours.Excluded),
				   "each file's mark is painted in its state's colour, a file with no receipt as unread");
			expect(!has(1U, colours.Accepted) && !has(2U, colours.Accepted) && !has(0U, colours.Accepted), "only an accepted file's mark is green");
			expect(images[1].copy(slot) != images[2].copy(slot) && images[2].copy(slot) != images[3].copy(slot) && images[0].copy(slot) != images[4].copy(slot),
				   "seen, read, accepted, unread and excluded draw differently");
			expect(images[5].copy(slot) == images[2].copy(slot), "unread comments leave the file's mark as it is");
			expect(pixels(images[5], counts, colours.Read) > 0 && pixels(images[2], counts, colours.Read) == 0,
				   "the unread comments' accent dot sits beside the counts");
			expect(!has(6U, colours.Neutral) && !has(6U, colours.Read), "a file the overview has not described shows no mark yet");
		}
	}

	auto test_operation_colours_and_accessible_names() -> void
	{
		qt::files_tree_view view;
		view.resize(480, 200);
		view.set_order(qt::files_order::flat);
		view.apply({ file("added.cpp"), file("removed.cpp"), file("modified.cpp") });
		auto added          = overview("added.cpp", 12U, 0U, application::overview_change_since_review::added, { }, { });
		added.Operation     = application::overview_file_operation::added;
		auto removed        = overview("removed.cpp", 0U, 8U, application::overview_change_since_review::changed, { }, { });
		removed.Operation   = application::overview_file_operation::removed;
		const auto modified = overview("modified.cpp", 2U, 1U, application::overview_change_since_review::changed, { }, { });
		view.apply_overview(std::vector{ added, removed, modified });
		view.show();
		for (const auto mode : { core::appearance_mode::dark, core::appearance_mode::light,
								 core::appearance_mode::high_contrast_dark, core::appearance_mode::high_contrast_light }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(view);
			view.adopt_appearance(profile);
			QApplication::processEvents();
			for (const auto& row : { added, removed, modified }) {
				const auto index     = view.tree_model()->index_for_file(core::file_identity_of(row.Path));
				const auto operation = row.Operation == application::overview_file_operation::added ? QStringLiteral("added")
					: row.Operation == application::overview_file_operation::removed                ? QStringLiteral("removed")
																									: QStringLiteral("modified");
				expect(index.data(Qt::AccessibleTextRole).toString().contains(operation), "operation is named without relying on colour");
				expect(index.data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("%1 added lines, %2 removed lines").arg(row.Additions).arg(row.Deletions)), "line counts have accessible non-colour wording");
				for (const auto selected : { false, true }) {
					auto image = QImage{ 480, 32, QImage::Format_ARGB32 };
					image.fill(view.palette().color(QPalette::Base));
					auto option = QStyleOptionViewItem{ };
					option.initFrom(&view);
					option.widget = &view;
					option.rect   = image.rect();
					option.font   = view.font();
					option.state  = QStyle::State_Enabled | (selected ? QStyle::State_Selected : QStyle::State_None);
					{
						auto painter = QPainter{ &image };
						view.delegate()->paint(&painter, option, index);
					}
					const auto renderRoot = qEnvironmentVariable("BPR_FILES_TREE_RENDER_ROOT");
					if (!renderRoot.isEmpty()) {
						expect(image.save(renderRoot + QStringLiteral("/delegate-%1-%2-%3.png").arg(static_cast<int>(mode)).arg(static_cast<int>(row.Operation)).arg(selected)), "capture row ink evidence");
					}
					const auto normal     = view.palette().color(selected ? QPalette::HighlightedText : QPalette::Text);
					const auto nameArea   = QRect{ 16, 0, 190, image.height() };
					const auto detailArea = QRect{ 300, 0, 180, image.height() };
					expect(pixels(image, nameArea, normal) > 8, "filenames keep ordinary native text instead of semantic operation ink");
					for (const auto& [count, role] : { std::pair{ row.Additions, core::appearance_role::state_accepted }, std::pair{ row.Deletions, core::appearance_role::state_defect } }) {
						const auto colour = selected ? normal : profile.color(role);
						if (count != 0U) {
							expect(pixels(image, detailArea, colour) > 0, "each nonzero line count has its own semantic ink, including modified files");
						}
					}
				}
			}
			const auto root = qEnvironmentVariable("BPR_FILES_TREE_RENDER_ROOT");
			if (!root.isEmpty()) {
				expect(view.grab().save(root + QStringLiteral("/files-tree-%1.png").arg(static_cast<int>(mode))), "capture themed drawer rendering");
			}
		}
	}

	// [[BPR-0658]]. Choosing a file row reports its identity once; a
	// selection the window makes reports nothing; a folder reports nothing.
	auto test_selection_reports_files_and_only_the_reviewers_own() -> void
	{
		qt::files_tree_view view;
		view.resize(320, 500);
		view.show();
		QApplication::processEvents();
		auto chosen = std::vector<core::workbench_file_id>{ };
		view.set_select_dispatch([&chosen](const core::workbench_file_id& aFile) { chosen.push_back(aFile); });
		view.apply({ file("src/a.cpp"), file("src/b.cpp") });
		QApplication::processEvents();
		auto* const model = view.tree_model();
		const auto  src   = model->index(0, 0);
		view.setCurrentIndex(src);
		QApplication::processEvents();
		expect(chosen.empty(), "a folder row reports nothing");
		view.setCurrentIndex(model->index(1, 0, src));
		QApplication::processEvents();
		expect(chosen == std::vector{ core::file_identity_of("src/b.cpp") }, "a file row reports its identity once");
		view.select_file(core::file_identity_of("src/a.cpp"));
		QApplication::processEvents();
		expect(chosen.size() == 1U && model->file_at(view.currentIndex()) == std::optional{ core::file_identity_of("src/a.cpp") },
			   "a selection the window makes is shown and not reported");
		view.select_file(std::nullopt);
		expect(!view.currentIndex().isValid() && chosen.size() == 1U, "clearing the selection reports nothing");
		view.hide();
	}

	void click_row(qt::files_tree_view& aView, const QModelIndex& aIndex, const bool aBranch = false)
	{
		const auto  rect  = aView.visualRect(aIndex);
		const auto  point = QPointF{ QPoint{ aBranch ? rect.left() - aView.indentation() / 2 : rect.center().x(), rect.center().y() } };
		QMouseEvent press{ QEvent::MouseButtonPress, point, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QMouseEvent release{ QEvent::MouseButtonRelease, point, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aView.viewport(), &press);
		QApplication::sendEvent(aView.viewport(), &release);
		QApplication::processEvents();
	}

	void test_folders_are_nonselectable_across_gestures_and_rebuilds()
	{
		qt::files_tree_view view;
		view.resize(420, 300);
		const auto rows = std::vector{ file("src/a.cpp"), file("src/b.cpp"), file("docs/readme.md") };
		view.apply(rows);
		view.show();
		QApplication::processEvents();
		auto chosen = std::vector<core::workbench_file_id>{ };
		view.set_select_dispatch([&](const auto& aFile) { chosen.push_back(aFile); });
		auto* const model    = view.tree_model();
		const auto  selected = core::file_identity_of("src/a.cpp");
		view.select_file(selected);
		const auto folder = model->parent(model->index_for_file(selected));
		expect(folder.isValid() && !(model->flags(folder) & Qt::ItemIsSelectable)
				   && (model->flags(folder) & Qt::ItemIsEnabled),
			   "folders are structurally nonselectable but remain expandable");
		click_row(view, folder);
		expect(!view.selectionModel()->isSelected(folder) && chosen.empty(), "folder mouse selection never becomes a file action or selected-file highlight");
		for (const auto key : { Qt::Key_Left, Qt::Key_Right, Qt::Key_Return, Qt::Key_Space }) {
			view.setCurrentIndex(folder);
			QKeyEvent event{ QEvent::KeyPress, key, Qt::NoModifier };
			QApplication::sendEvent(&view, &event);
			expect(chosen.empty(), "folder keyboard expansion and activation emit no file selection");
		}
		view.expand(folder);
		click_row(view, folder, true);
		expect(!view.isExpanded(folder) && chosen.empty(), "mouse branch collapse does not select a file");
		click_row(view, folder, true);
		expect(view.isExpanded(folder) && chosen.empty(), "mouse branch expansion does not select a file");
		const auto oldFolder = QPersistentModelIndex{ folder };
		const auto docs      = QPersistentModelIndex{ model->parent(model->index_for_file(core::file_identity_of("docs/readme.md"))) };
		view.set_filter(QStringLiteral("b.cpp"));
		// [[BPR-0972]]. The filter is reconciled, not reset: the folder still
		// shown keeps its identity and the one filtered away is retired.
		expect(oldFolder.isValid() && oldFolder.data(qt::files_tree_model::path_role).toString() == QStringLiteral("src")
				   && !docs.isValid() && chosen.empty(),
			   "a filter keeps a still-shown folder, retires a hidden one, and dispatches nothing");
		view.set_filter({ });
		expect(model->file_at(view.currentIndex()) == std::optional{ selected } && chosen.empty(), "clearing a filter restores the actual selected file, not a focused folder");
		view.apply_overview({ });
		view.apply(rows);
		view.set_order(qt::files_order::flat);
		view.set_order(qt::files_order::tree);
		expect(model->file_at(view.currentIndex()) == std::optional{ selected } && chosen.empty(), "overview, row and order rebuilds preserve exact selection without emitting gestures");
		auto foreign = qt::files_tree_model{ };
		foreign.apply({ file("elsewhere/file.cpp") });
		expect(!model->file_at(foreign.index_for_file(core::file_identity_of("elsewhere/file.cpp"))).has_value(), "foreign model indexes cannot alias a file");
		click_row(view, model->index_for_file(core::file_identity_of("src/b.cpp")));
		expect(chosen == std::vector{ core::file_identity_of("src/b.cpp") }, "a real file mouse gesture still dispatches its exact identity once");
	}
	void test_file_lookup_reuses_indexes()
	{
		for (const auto count : { 32U, 4096U }) {
			auto rows = std::vector<qt::workbench_drawer_row>{ };
			for (unsigned index = 0; index < count; ++index) {
				rows.push_back(file("src/" + std::to_string(index) + ".cpp"));
			}
			qt::workbench_drawer_model drawer;
			drawer.apply(rows);
			qt::files_tree_model tree;
			tree.apply(rows);
			const auto before     = drawer.statistics();
			const auto treeBefore = tree.work_statistics();
			for (int gesture = 0; gesture < 16; ++gesture) {
				expect(drawer.index_for_file(*rows.back().File).row() == static_cast<int>(count - 1U), "drawer lookup retains projection order");
				expect(tree.file_at(tree.index_for_file(*rows.back().File)) == rows.back().File, "tree lookup retains exact identity");
			}
			const auto comparisons = drawer.statistics().FileLookupComparisons - before.FileLookupComparisons;
			std::cout << "file-models rows=" << count << " gestures=16 drawer_comparisons=" << comparisons
					  << " tree_comparisons=" << tree.work_statistics().LookupComparisons - treeBefore.LookupComparisons << '\n';
			expect(comparisons <= 16U * 14U, "routine drawer lookup uses retained index");
			expect(drawer.statistics().FileIndexRowsBuilt == before.FileIndexRowsBuilt, "routine lookup never rebuilds index");
			std::swap(rows.front(), rows.back());
			drawer.apply(rows);
			expect(drawer.index_for_file(*rows.front().File).row() == 0, "index follows a moved row");
			rows.erase(rows.begin());
			drawer.apply(rows);
			expect(!drawer.index_for_file(core::file_identity_of("src/" + std::to_string(count - 1U) + ".cpp")).isValid(), "index retires removed file");
		}
	}

	// [[BPR-0972]]. One file leaving or re-entering the actionable order
	// examines each row a bounded number of times and signals only that row.
	void test_drawer_reconciliation_is_linear_in_rows()
	{
		for (const auto count : { 32U, 4096U }) {
			auto rows = std::vector<qt::workbench_drawer_row>{ };
			for (unsigned index = 0; index < count; ++index) {
				rows.push_back(file("src/" + std::to_string(index) + ".cpp"));
			}
			qt::workbench_drawer_model drawer;
			drawer.apply(rows);
			auto resets = 0;
			QObject::connect(&drawer, &QAbstractItemModel::modelReset, &drawer, [&] { ++resets; });
			const auto measure = [&](const char* aStep, const std::vector<qt::workbench_drawer_row>& aRows) {
				const auto before = drawer.statistics();
				drawer.apply(aRows);
				const auto after = drawer.statistics();
				std::cout << "drawer-reconcile rows=" << count << " step=" << aStep
						  << " rows_visited=" << after.ReconcileRowsVisited - before.ReconcileRowsVisited
						  << " inserted=" << after.RowsInserted - before.RowsInserted
						  << " removed=" << after.RowsRemoved - before.RowsRemoved
						  << " moved=" << after.RowsMoved - before.RowsMoved << '\n';
				expect(after.ReconcileRowsVisited - before.ReconcileRowsVisited <= 3U * count,
					   "reconciliation examines each row a bounded number of times");
				return after;
			};
			auto       without = rows;
			const auto leaving = without[count / 2U];
			without.erase(without.begin() + static_cast<std::ptrdiff_t>(count / 2U));
			auto before = drawer.statistics();
			auto after  = measure("leave", without);
			expect(after.RowsRemoved == before.RowsRemoved + 1U && after.RowsInserted == before.RowsInserted,
				   "a file leaving the order removes exactly its row");
			before = after;
			after  = measure("return", rows);
			expect(after.RowsInserted == before.RowsInserted + 1U && after.RowsRemoved == before.RowsRemoved,
				   "a file returning inserts exactly its row");
			expect(drawer.index_for_file(*leaving.File).row() == static_cast<int>(count / 2U), "the returned row takes its order position");
			auto moved = rows;
			std::rotate(moved.begin(), moved.end() - 1, moved.end());
			before = after;
			after  = measure("move", moved);
			expect(after.RowsMoved == before.RowsMoved + 1U, "one moved file is one row move");
			before = after;
			after  = measure("unchanged", moved);
			expect(after.RowsMoved == before.RowsMoved && after.RowsChanged == before.RowsChanged, "an unchanged order signals nothing");
			expect(resets == 0, "no bounded order change resets the drawer");
		}
	}

	// [[BPR-0972]]. A file leaving and re-entering the actionable order is a
	// bounded update to the tree: its row alone is removed and inserted, other
	// rows keep their identity, and a folder the reviewer collapsed stays so.
	void test_file_list_changes_reconcile_without_reset()
	{
		for (const auto count : { 32U, 4096U }) {
			auto rows = std::vector<qt::workbench_drawer_row>{ };
			for (unsigned index = 0; index < count; ++index) {
				rows.push_back(file("src/part" + std::to_string(index % 8U) + "/" + std::to_string(index) + ".cpp"));
			}
			qt::files_tree_view view;
			view.apply(rows);
			auto* const model  = view.tree_model();
			auto        resets = 0;
			QObject::connect(model, &QAbstractItemModel::modelReset, model, [&] { ++resets; });
			const auto kept      = QPersistentModelIndex{ model->index_for_file(*rows.front().File) };
			const auto collapsed = QPersistentModelIndex{ model->parent(model->index_for_file(*rows[1].File)) };
			view.collapse(collapsed);
			auto without = rows;
			without.erase(without.begin() + 2);
			const auto before = model->work_statistics();
			view.apply(without);
			const auto removed = model->work_statistics();
			view.apply(rows);
			const auto after = model->work_statistics();
			std::cout << "files-tree-reconcile rows=" << count << " leave_removed=" << removed.RowsRemoved - before.RowsRemoved
					  << " leave_inserted=" << removed.RowsInserted - before.RowsInserted << " leave_changed=" << removed.ChangedRows - before.ChangedRows
					  << " leave_moved=" << removed.RowsMoved - before.RowsMoved
					  << " return_inserted=" << after.RowsInserted - removed.RowsInserted << " return_changed=" << after.ChangedRows - removed.ChangedRows
					  << " return_moved=" << after.RowsMoved - removed.RowsMoved << '\n';
			expect(resets == 0 && after.ModelResets == before.ModelResets, "a bounded file-list change never resets the tree");
			expect(removed.RowsRemoved == before.RowsRemoved + 1U && removed.RowsInserted == before.RowsInserted
					   && after.RowsInserted == removed.RowsInserted + 1U && after.RowsMoved == before.RowsMoved,
				   "only the leaving file's row is removed and then inserted");
			expect(removed.ChangedRows - before.ChangedRows == 2U && after.ChangedRows - removed.ChangedRows == 2U,
				   "only the two folders whose counts changed are signalled");
			expect(kept.isValid() && model->file_at(kept) == rows.front().File, "other file rows keep their identity");
			expect(collapsed.isValid() && !view.isExpanded(collapsed), "a folder the reviewer collapsed stays collapsed");
			view.apply(std::vector{ rows.front(), file("docs/new/readme.md") });
			const auto added = model->parent(model->index_for_file(core::file_identity_of("docs/new/readme.md")));
			expect(added.isValid() && view.isExpanded(added), "a folder that newly appears is expanded");
		}
	}

	void expect_same_tree(const qt::files_tree_model& aActual, const qt::files_tree_model& aExpected,
						  const QModelIndex& aActualParent = { }, const QModelIndex& aExpectedParent = { })
	{
		expect(aActual.rowCount(aActualParent) == aExpected.rowCount(aExpectedParent), "delta and full tree have identical children");
		for (int row = 0; row < aActual.rowCount(aActualParent); ++row) {
			const auto actual   = aActual.index(row, 0, aActualParent);
			const auto expected = aExpected.index(row, 0, aExpectedParent);
			for (int role = qt::files_tree_model::path_role; role <= qt::files_tree_model::thread_count_role; ++role) {
				expect(actual.data(role) == expected.data(role), "delta and full tree expose identical file facts");
			}
			expect(actual.data() == expected.data() && aActual.file_at(actual) == aExpected.file_at(expected), "delta preserves labels and file identity");
			expect_same_tree(aActual, aExpected, actual, expected);
		}
	}

	void test_file_deltas_equal_complete_projection()
	{
		for (const auto order : { qt::files_order::tree, qt::files_order::flat, qt::files_order::changed_first }) {
			for (const auto& filter : { QString{ }, QStringLiteral("cpp") }) {
				auto                       rows  = std::vector{ file("src/a.cpp"), file("src/b.cpp"), file("src/sub/c.cpp"), file("docs/deep/readme.md"), file("root.cpp") };
				auto                       facts = std::vector{ overview("src/a.cpp", 2U, 1U, application::overview_change_since_review::changed, core::file_read_state::read, 0U) };
				qt::workbench_drawer_model drawer;
				qt::files_tree_model       tree;
				drawer.apply(rows);
				tree.apply(rows);
				tree.apply_overview(facts);
				tree.set_order(order);
				tree.set_filter(filter);
				auto resets = 0;
				QObject::connect(&tree, &QAbstractItemModel::modelReset, &tree, [&] { ++resets; });
				QObject::connect(&drawer, &QAbstractItemModel::modelReset, &drawer, [&] { ++resets; });
				const auto check = [&] {
					qt::files_tree_model fresh;
					fresh.apply(rows);
					fresh.apply_overview(facts);
					fresh.set_order(order);
					fresh.set_filter(filter);
					expect_same_tree(tree, fresh);
					expect(drawer.rowCount() == static_cast<int>(rows.size()), "drawer delta preserves row count");
					for (std::size_t row = 0U; row < rows.size(); ++row) {
						expect(drawer.index_for_file(*rows[row].File).row() == static_cast<int>(row), "drawer delta preserves exact order");
						expect(drawer.index(static_cast<int>(row), 0).data() == rows[row].Title, "drawer delta preserves row text");
					}
					expect(resets == 0, "deltas never reset either file model");
				};
				const auto remove = [&](std::size_t aRow) {
					const auto id = *rows[aRow].File;
					rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(aRow));
					drawer.apply_delta(std::array{ id }, { });
					tree.apply_delta(std::array{ id }, { });
					check();
				};
				remove(0U);
				remove(0U); // Leaves a nesting-only folder to compact.
				remove(1U); // Removes a whole compacted branch.
				const auto insert = [&](const std::size_t aAt, const qt::workbench_drawer_row& aRow) {
					rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(aAt), aRow);
					drawer.apply_delta({ }, std::array{ std::pair{ aAt, aRow } });
					tree.apply_delta({ }, std::array{ aRow });
					check();
				};
				insert(0U, file("src/a.cpp")); // Restores retained overview facts.
				insert(1U, file("src/b.cpp"));
				insert(2U, file("new/branch/file.cpp"));
				insert(rows.size(), file("docs/deep/readme.md"));
			}
		}
	}

	void test_receipt_updates_preserve_tree_indexes()
	{
		for (const auto order : { qt::files_order::tree, qt::files_order::flat, qt::files_order::changed_first }) {
			qt::files_tree_model model;
			model.apply({ file("src/a.cpp"), file("src/b.cpp"), file("docs/nested/c.md") });
			model.set_order(order);
			auto rows = std::vector{
				overview("src/a.cpp", 1U, 0U, application::overview_change_since_review::unchanged, core::file_read_state::unread, 0U),
				overview("src/b.cpp", 1U, 0U, application::overview_change_since_review::changed, core::file_read_state::unread, 0U),
				overview("docs/nested/c.md", 1U, 0U, application::overview_change_since_review::unchanged, core::file_read_state::unread, 0U),
			};
			model.apply_overview(rows);
			const auto fileId  = core::file_identity_of("src/b.cpp");
			const auto kept    = QPersistentModelIndex{ model.index_for_file(fileId) };
			auto       resets  = 0;
			auto       changes = 0;
			QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&] { ++resets; });
			QObject::connect(&model, &QAbstractItemModel::dataChanged, &model, [&](const QModelIndex& aFirst, const QModelIndex& aLast) {
				expect(aFirst == kept && aLast == kept, "receipt changes only its stable file row");
				++changes;
			});
			rows[1].Read = core::file_read_state::read;
			model.apply_overview(rows);
			expect(resets == 0 && changes == 1 && kept.isValid(), "one receipt changes data without resetting the tree");
			expect(kept.data(qt::files_tree_model::read_role).toInt() == static_cast<int>(core::file_read_state::read), "stable index presents committed read state");
			model.apply_overview(rows);
			expect(resets == 0 && changes == 1, "identical overview causes no model notification");
		}
	}

	/// \brief [[BPR-1363]]. The owner's nested-folders pull request in the
	/// Files drawer: opening and closing folders, so that the rows outgrow the
	/// tree and a scroll bar comes and goes, never moves or narrows the
	/// column, so the counts at its right edge stay where they are.
	void test_folder_disclosure_never_moves_the_column()
	{
		qt::files_tree_view view;
		view.resize(320, 300);
		view.show();
		QApplication::processEvents();
		auto rows = std::vector<qt::workbench_drawer_row>{ };
		for (const auto* path : { "README.md", "docs/api/reference.md", "docs/api/v2/endpoints.md", "docs/guide.md", "src/engine/renderer.cpp",
								  "src/engine/renderer.h", "src/engine/shaders/basic.frag", "src/engine/shaders/basic.vert",
								  "src/engine/shaders/post/bloom.frag", "src/engine/shaders/post/tonemap.frag", "src/main.cpp",
								  "src/ui/widgets/button.cpp", "src/ui/widgets/button.h", "src/ui/widgets/slider.cpp",
								  "tests/engine/renderer_tests.cpp", "tests/ui/widgets/button_tests.cpp", "tools/build/scripts/package.py" }) {
			rows.push_back(file(path));
		}
		view.apply(rows);
		QApplication::processEvents();
		view.collapseAll();
		QApplication::processEvents();
		const auto geometry = [&view] { return std::pair{ view.header()->sectionViewportPosition(0), view.header()->sectionSize(0) }; };
		const auto closed   = geometry();
		const auto same     = [&](const std::string_view aStep) {
			QApplication::processEvents();
			const auto now = geometry();
			if (now != closed) {
				std::cerr << "column before: " << closed.first << '+' << closed.second << " after: " << now.first << '+' << now.second << '\n';
			}
			expect(now == closed, aStep);
		};
		expect(view.verticalScrollBar()->maximum() == 0, "with every folder closed the rows fit");
		view.expandAll();
		same("opening every folder moves and narrows nothing");
		expect(view.verticalScrollBar()->maximum() > 0, "with every folder open the rows outgrow the tree");
		view.scrollToBottom();
		same("scrolling moves nothing");
		view.collapseAll();
		same("closing every folder moves and widens nothing");
		const auto* model = view.model();
		for (auto folder = 0; folder < model->rowCount(); ++folder) {
			view.expand(model->index(folder, 0));
			same("opening each folder in turn moves nothing");
		}
		view.apply(rows);
		same("a refresh moves nothing");
		view.resize(360, 300);
		QApplication::processEvents();
		expect(geometry().second > closed.second, "a wider tree still widens its column");
	}

	/// \brief [[BPR-1363]]. The leftmost device pixel of a band that differs
	/// from the band's commonest colour, in logical coordinates.
	[[nodiscard]] auto first_ink(const QImage& aImage, const QRect& aBand) -> std::optional<qreal>
	{
		const auto ratio  = aImage.devicePixelRatio();
		const auto top    = static_cast<int>(std::ceil(aBand.top() * ratio)) + 1;
		const auto bottom = static_cast<int>(std::floor((aBand.bottom() + 1) * ratio)) - 2;
		const auto right  = std::min(aImage.width(), static_cast<int>(std::floor((aBand.right() + 1) * ratio))) - 1;
		auto       counts = std::map<QRgb, int>{ };
		for (auto y = top; y <= bottom; ++y) {
			for (auto x = 0; x <= right; ++x) {
				++counts[aImage.pixel(x, y)];
			}
		}
		const auto ground = std::ranges::max_element(counts, { }, [](const auto& aEntry) { return aEntry.second; })->first;
		for (auto x = 0; x <= right; ++x) {
			for (auto y = top; y <= bottom; ++y) {
				const auto pixel = aImage.pixel(x, y);
				if (std::abs(qRed(pixel) - qRed(ground)) + std::abs(qGreen(pixel) - qGreen(ground)) + std::abs(qBlue(pixel) - qBlue(ground)) > 60) {
					return x / ratio;
				}
			}
		}
		return std::nullopt;
	}

	/// \brief [[BPR-1363]]. In the Files drawer too, a folder row's
	/// disclosure mark is the leftmost thing painted on it: the view draws it
	/// in the row's indentation, before the folder's name.
	void test_folder_disclosure_is_the_leftmost_mark()
	{
		qt::files_tree_view view;
		view.resize(320, 500);
		view.show();
		QApplication::processEvents();
		view.apply({ file("README.md"), file("docs/api/reference.md"), file("docs/guide.md"), file("src/engine/renderer.cpp"), file("src/main.cpp") });
		view.collapseAll();
		view.expand(view.tree_model()->index(0, 0));
		view.clearSelection();
		view.setCurrentIndex({ });
		QApplication::processEvents();
		const auto  image   = view.viewport()->grab().toImage();
		const auto* model   = view.model();
		auto        checked = 0;
		const auto  check   = [&](const QModelIndex& aFolder) {
			const auto rect = view.visualRect(aFolder);
			const auto ink  = first_ink(image, QRect{ 0, rect.top(), view.viewport()->width(), rect.height() });
			expect(ink && *ink >= rect.left() - view.indentation() && *ink < rect.left(), "the disclosure mark is the leftmost thing on a drawer folder row");
			++checked;
		};
		for (auto row = 0; row < model->rowCount(); ++row) {
			const auto top = model->index(row, 0);
			if (top.data(qt::files_tree_model::folder_role).toBool()) {
				check(top);
				for (auto child = 0; child < model->rowCount(top); ++child) {
					if (const auto inner = model->index(child, 0, top); view.isExpanded(top) && inner.data(qt::files_tree_model::folder_role).toBool()) {
						check(inner);
					}
				}
			}
		}
		expect(checked >= 3, "top-level and nested drawer folders were checked");
	}

	/// \brief [[BPR-1529]]. Ink pixels of a logical area of a grab.
	[[nodiscard]] auto ink_in(const QImage& aImage, const QRect& aLogical, const QColor& aColour) -> int
	{
		const auto ratio = aImage.devicePixelRatio();
		const auto area  = QRect{ QPoint{ static_cast<int>(aLogical.left() * ratio), static_cast<int>(aLogical.top() * ratio) },
								  QPoint{ static_cast<int>(((aLogical.right() + 1) * ratio) - 1), static_cast<int>(((aLogical.bottom() + 1) * ratio) - 1) } }
							   .intersected(aImage.rect());
		return pixels(aImage, area, aColour);
	}

	/// \brief [[BPR-1529]]. The owner, 2026-10-15: "For the file tree in
	/// review can we draw folders different to files, maybe the same color as
	/// they are in overview". Under one appearance profile, in every variant,
	/// the drawer's folder names and Overview's folder paths are drawn in the
	/// same ink, the palette's `PlaceholderText`, and neither in the ordinary
	/// `Text` the drawer's file names keep.
	void test_folders_take_overviews_folder_ink()
	{
		const auto          paths = std::vector<std::string>{ "docs/api/reference.md", "docs/guide.md", "src/engine/renderer.cpp", "src/main.cpp", "README.md" };
		qt::files_tree_view view;
		view.resize(360, 300);
		auto rows = std::vector<qt::workbench_drawer_row>{ };
		for (const auto& path : paths) {
			rows.push_back(file(path));
		}
		view.apply(rows);

		auto projected           = application::pull_request_overview_projection{ };
		projected.Owner          = "MikeMarcin";
		projected.RepositoryName = "BPR-Test";
		projected.Number         = core::pull_request_number{ 1U };
		projected.Title          = "Folders";
		for (const auto& path : paths) {
			const auto slash = path.rfind('/');
			projected.Files.push_back(application::overview_file_row{
				.Path      = path,
				.Folder    = slash == std::string::npos ? std::string{ } : path.substr(0U, slash),
				.Name      = slash == std::string::npos ? path : path.substr(slash + 1U),
				.Operation = application::overview_file_operation::modified,
				.Additions = 1U,
			});
		}
		projected.Progress.FilesTotal = projected.Files.size();
		qt::pull_request_overview_page page;
		page.resize(1100, 640);
		page.present(projected);
		view.show();
		page.show();
		QApplication::processEvents();

		auto* const tree     = view.tree_model();
		auto* const overview = page.file_model();
		auto* const files    = page.files_view();
		auto        bold     = view.font();
		bold.setWeight(QFont::DemiBold);
		auto checked = 0;
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark,
								 core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(view);
			view.adopt_appearance(profile);
			profile.apply_to(page);
			page.adopt_appearance(profile);
			view.expandAll();
			view.clearSelection();
			view.setCurrentIndex({ });
			files->clearSelection();
			files->setCurrentIndex({ });
			QApplication::processEvents();
			const auto folderInk = view.palette().color(QPalette::Active, QPalette::PlaceholderText);
			const auto fileInk   = view.palette().color(QPalette::Active, QPalette::Text);
			expect(files->palette().color(QPalette::Active, QPalette::PlaceholderText) == folderInk,
				   "Overview and the drawer read one folder ink under one profile");
			expect(std::abs(folderInk.red() - fileInk.red()) + std::abs(folderInk.green() - fileInk.green()) + std::abs(folderInk.blue() - fileInk.blue()) >= 96,
				   "the folder ink is far enough from the file ink to tell apart");

			const auto drawer = view.viewport()->grab().toImage();
			const auto name   = [&](const QModelIndex& aIndex, const QFont& aFont) {
				const auto rect = view.visualRect(aIndex);
				return QRect{ rect.left() + 4, rect.top(), QFontMetrics{ aFont }.horizontalAdvance(aIndex.data().toString()), rect.height() };
			};
			for (auto row = 0; row < tree->rowCount(); ++row) {
				const auto index = tree->index(row, 0);
				if (index.data(qt::files_tree_model::folder_role).toBool()) {
					const auto area = name(index, bold);
					expect(ink_in(drawer, area, folderInk) > 8, "a drawer folder's name is drawn in Overview's folder ink");
					expect(ink_in(drawer, area, fileInk) == 0, "a drawer folder's name is not drawn in the file ink");
					++checked;
				}
			}
			const auto readme = tree->index_for_file(core::file_identity_of("README.md"));
			auto       nameAt = view.visualRect(readme);
			nameAt.setLeft(nameAt.left() + 4 + qt::files_tree_delegate::name_inset);
			nameAt.setWidth(QFontMetrics{ view.font() }.horizontalAdvance(readme.data().toString()));
			expect(ink_in(drawer, nameAt, fileInk) > 8, "a drawer file's name keeps the ordinary text ink");

			const auto overviewImage = files->viewport()->grab().toImage();
			auto       overviewRows  = 0;
			for (auto row = 0; row < overview->rowCount(); ++row) {
				const auto folder = overview->index(row, qt::overview_file_model::operation_column);
				if (!folder.data(qt::overview_file_model::folder_role).toBool()) {
					continue;
				}
				const auto cell         = files->visualRect(folder.siblingAtColumn(qt::overview_file_model::file_column));
				const auto folderPixels = ink_in(overviewImage, cell, folderInk);
				const auto filePixels   = ink_in(overviewImage, cell, fileInk);
				if (folderPixels <= 8 || filePixels != 0) {
					std::cerr << "folder-ink mode=" << static_cast<int>(mode)
							  << " path=" << folder.siblingAtColumn(qt::overview_file_model::file_column).data().toString().toStdString()
							  << " folder_pixels=" << folderPixels << " file_pixels=" << filePixels
							  << " cell=" << cell.x() << ',' << cell.y() << ',' << cell.width() << ',' << cell.height()
							  << " viewport=" << files->viewport()->width() << ',' << files->viewport()->height()
							  << " dpr=" << overviewImage.devicePixelRatio()
							  << " font=" << files->font().family().toStdString() << " point_size=" << files->font().pointSizeF()
							  << " folder_colour=" << folderInk.name().toStdString() << " file_colour=" << fileInk.name().toStdString() << '\n';
					if (const auto root = qEnvironmentVariable("BPR_FILES_TREE_RENDER_ROOT"); !root.isEmpty()) {
						static_cast<void>(overviewImage.save(root + QStringLiteral("/failure-overview-%1.png").arg(static_cast<int>(mode))));
					}
				}
				expect(folderPixels > 8 && filePixels == 0,
					   "Overview's folder path is drawn in the same folder ink");
				++overviewRows;
			}
			expect(overviewRows == 5, "Overview's five folder rows, the root's \"/\" among them, were checked");
			if (const auto root = qEnvironmentVariable("BPR_FILES_TREE_RENDER_ROOT"); !root.isEmpty()) {
				expect(view.grab().save(root + QStringLiteral("/folders-drawer-%1.png").arg(static_cast<int>(mode))), "capture the drawer's folders");
				expect(files->grab().save(root + QStringLiteral("/folders-overview-%1.png").arg(static_cast<int>(mode))), "capture Overview's folders");
			}
		}
		expect(checked == 4 * 2, "both top-level drawer folders were checked in all four variants");
	}

	/// \brief [[BPR-1530]]. The owner, 2026-10-15: "I think the file count on
	/// folders in the file tree explorer is a bit too noisy. Lets hide that
	/// and only show it when the folder is collapsed." A folder row draws its
	/// count only while collapsed, and follows every way of opening and
	/// closing it; its accessible text keeps the count throughout.
	void test_folder_counts_show_only_while_collapsed()
	{
		qt::files_tree_view view;
		view.resize(360, 300);
		view.apply({ file("docs/api/reference.md"), file("docs/guide.md"), file("src/main.cpp"), file("README.md") });
		view.show();
		QApplication::processEvents();
		auto* const model = view.tree_model();
		const auto  docs  = model->index(0, 0);
		const auto  src   = model->index(1, 0);
		expect(docs.data().toString() == QStringLiteral("docs") && src.data().toString() == QStringLiteral("src"), "the two top folders lead the tree");
		auto small = view.font();
		small.setPointSizeF(small.pointSizeF() * 0.85);
		// Any ink in the slot at the row's right edge where a count is drawn.
		const auto counted = [&](const QModelIndex& aFolder) {
			QApplication::processEvents();
			const auto image  = view.viewport()->grab().toImage();
			const auto rect   = view.visualRect(aFolder);
			const auto width  = QFontMetrics{ small }.horizontalAdvance(aFolder.data(qt::files_tree_model::file_count_role).toString());
			const auto ratio  = image.devicePixelRatio();
			const auto ground = view.palette().color(QPalette::Base);
			for (auto y = static_cast<int>((rect.top() + 1) * ratio); y < static_cast<int>((rect.bottom() - 1) * ratio); ++y) {
				for (auto x = static_cast<int>((rect.right() - 4 - width - 1) * ratio); x < static_cast<int>((rect.right() - 3) * ratio); ++x) {
					const auto pixel = image.pixelColor(x, y);
					if (std::abs(pixel.red() - ground.red()) + std::abs(pixel.green() - ground.green()) + std::abs(pixel.blue() - ground.blue()) > 60) {
						return true;
					}
				}
			}
			return false;
		};
		const auto accessible = [&](const QModelIndex& aFolder) {
			return aFolder.data(Qt::AccessibleTextRole).toString().endsWith(QStringLiteral(", %1 files").arg(aFolder.data(qt::files_tree_model::file_count_role).toULongLong()));
		};
		const auto press = [&](const QModelIndex& aFolder, const Qt::Key aKey) {
			view.setCurrentIndex(aFolder);
			QKeyEvent event{ QEvent::KeyPress, aKey, Qt::NoModifier };
			QApplication::sendEvent(&view, &event);
		};
		expect(view.isExpanded(docs) && view.isExpanded(src), "new folders start open");
		expect(!counted(docs) && !counted(src), "an open folder draws no count");
		expect(accessible(docs) && accessible(src), "an open folder's accessible text keeps its count");

		view.collapseAll();
		expect(counted(docs) && counted(src), "collapse-all brings every folder's count back");
		expect(accessible(docs), "a closed folder's accessible text keeps its count");
		view.expandAll();
		expect(!counted(docs) && !counted(src), "expand-all hides every count");

		click_row(view, docs, true);
		expect(!view.isExpanded(docs) && counted(docs) && !counted(src), "closing a folder by mouse shows its count and no other's");
		click_row(view, docs, true);
		expect(view.isExpanded(docs) && !counted(docs), "opening it by mouse hides the count again");

		press(src, Qt::Key_Left);
		expect(!view.isExpanded(src) && counted(src), "Left closes a folder and shows its count");
		press(src, Qt::Key_Right);
		expect(view.isExpanded(src) && !counted(src), "Right opens a folder and hides its count");
		press(src, Qt::Key_Space);
		expect(!view.isExpanded(src) && counted(src), "Space toggles a folder closed and shows its count");
		press(src, Qt::Key_Space);
		expect(view.isExpanded(src) && !counted(src), "Space toggles it open and hides its count");
		if (const auto root = qEnvironmentVariable("BPR_FILES_TREE_RENDER_ROOT"); !root.isEmpty()) {
			view.clearSelection();
			view.setCurrentIndex({ });
			expect(view.grab().save(root + QStringLiteral("/counts-expanded.png")), "capture open folders");
			view.collapseAll();
			QApplication::processEvents();
			expect(view.grab().save(root + QStringLiteral("/counts-collapsed.png")), "capture closed folders");
		}
	}

	/// \brief [[BPR-1529]] and [[BPR-1530]]. Offscreen renders for the
	/// owner's look: the nested-folders pull request's drawer, open and partly
	/// closed, and its Overview files, in Light and Dark.
	void render_folder_looks(const QString& aRoot)
	{
		const auto paths = std::vector<std::string>{
			"README.md", "docs/api/reference.md", "docs/api/v2/endpoints.md", "docs/guide.md", "src/engine/renderer.cpp",
			"src/engine/renderer.h", "src/engine/shaders/basic.frag", "src/engine/shaders/basic.vert",
			"src/engine/shaders/post/bloom.frag", "src/engine/shaders/post/tonemap.frag", "src/main.cpp",
			"src/ui/widgets/button.cpp", "src/ui/widgets/button.h", "src/ui/widgets/slider.cpp",
			"tests/engine/renderer_tests.cpp", "tests/ui/widgets/button_tests.cpp", "tools/build/scripts/package.py"
		};
		auto rows                = std::vector<qt::workbench_drawer_row>{ };
		auto overviews           = std::vector<application::overview_file_row>{ };
		auto projected           = application::pull_request_overview_projection{ };
		projected.Owner          = "MikeMarcin";
		projected.RepositoryName = "BPR-Test";
		projected.Number         = core::pull_request_number{ 1U };
		projected.Title          = "Nested folders";
		for (const auto& path : paths) {
			rows.push_back(file(path));
			const auto slash = path.rfind('/');
			auto       row   = overview(path, 4U, path.ends_with(".h") ? 0U : 1U, application::overview_change_since_review::unchanged, core::file_read_state::unread, std::nullopt);
			row.Folder       = slash == std::string::npos ? std::string{ } : path.substr(0U, slash);
			row.Mark         = core::file_progress_mark::unread;
			overviews.push_back(row);
			projected.Files.push_back(row);
		}
		projected.Progress.FilesTotal = projected.Files.size();
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			qt::files_tree_view inks;
			profile.apply_to(inks);
			const auto& palette = inks.palette();
			std::cout << "inks mode=" << static_cast<int>(mode) << " text=" << palette.color(QPalette::Text).name().toStdString()
					  << " folder=" << palette.color(QPalette::PlaceholderText).name().toStdString()
					  << " base=" << palette.color(QPalette::Base).name().toStdString() << '\n';
		}
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			qt::files_tree_view view;
			view.resize(300, 470);
			view.apply(rows);
			view.apply_overview(overviews);
			profile.apply_to(view);
			view.adopt_appearance(profile);
			view.show();
			view.expandAll();
			view.select_file(core::file_identity_of("src/engine/renderer.cpp"));
			QApplication::processEvents();
			expect(view.grab().save(aRoot + QStringLiteral("/drawer-open-%1.png").arg(name)), "render the open drawer");
			auto* const model = view.tree_model();
			for (auto row = 0; row < model->rowCount(); ++row) {
				const auto top = model->index(row, 0);
				if (top.data(qt::files_tree_model::folder_role).toBool() && top.data().toString() != QStringLiteral("src")) {
					view.collapse(top);
				}
			}
			view.collapse(model->parent(model->index_for_file(core::file_identity_of("src/ui/widgets/button.cpp"))));
			QApplication::processEvents();
			expect(view.grab().save(aRoot + QStringLiteral("/drawer-mixed-%1.png").arg(name)), "render the partly closed drawer");

			qt::pull_request_overview_page page;
			page.resize(900, 560);
			page.present(projected);
			profile.apply_to(page);
			page.adopt_appearance(profile);
			page.show();
			page.files_view()->clearSelection();
			page.files_view()->setCurrentIndex({ });
			QApplication::processEvents();
			expect(page.files_view()->grab().save(aRoot + QStringLiteral("/overview-files-%1.png").arg(name)), "render Overview's files");
		}
	}

} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	// [[BPR-1529]] and [[BPR-1530]]. Renders for the owner's look; no checks.
	if (argc > 2 && std::string_view{ argv[1] } == "--render-looks") {
		render_folder_looks(QString::fromLocal8Bit(argv[2]));
		std::cout << "files-tree-render-looks:done\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1363]]. The 125% and 200% rows run the folder rows at their scale.
	if (argc > 1 && std::string_view{ argv[1] } == "--folder-rows") {
		test_folder_disclosure_never_moves_the_column();
		test_folder_disclosure_is_the_leftmost_mark();
		std::cout << "files-tree-folder-rows:pass dpr=" << application.devicePixelRatio() << '\n';
		return EXIT_SUCCESS;
	}
	test_folder_disclosure_never_moves_the_column();
	test_folder_disclosure_is_the_leftmost_mark();
	test_file_deltas_equal_complete_projection();
	test_file_lookup_reuses_indexes();
	test_receipt_updates_preserve_tree_indexes();
	test_drawer_reconciliation_is_linear_in_rows();
	test_file_list_changes_reconcile_without_reset();
	test_the_tree_compacts_nesting_only_folders_and_counts_files();
	test_orders_filter_and_the_overview_join();
	test_selection_reports_files_and_only_the_reviewers_own();
	test_folders_are_nonselectable_across_gestures_and_rebuilds();
	test_operation_colours_and_accessible_names();
	test_file_marks_are_the_review_bar_family();
	test_folders_take_overviews_folder_ink();
	test_folder_counts_show_only_while_collapsed();
	std::cout << "files-tree-tests:pass\n";
	return EXIT_SUCCESS;
}
