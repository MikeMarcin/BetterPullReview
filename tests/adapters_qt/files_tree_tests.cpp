#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/resident_filter.hpp"

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QStyleOptionViewItem>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
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
	std::cout << "files-tree-tests:pass\n";
	return EXIT_SUCCESS;
}
