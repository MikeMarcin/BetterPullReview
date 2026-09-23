#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/resident_filter.hpp"

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QStyleOptionViewItem>

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
		view.set_filter(QStringLiteral("b.cpp"));
		expect(!oldFolder.isValid() && chosen.empty(), "model rebuild invalidates old folder indexes without dispatch");
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
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_tree_compacts_nesting_only_folders_and_counts_files();
	test_orders_filter_and_the_overview_join();
	test_selection_reports_files_and_only_the_reviewers_own();
	test_folders_are_nonselectable_across_gestures_and_rebuilds();
	test_operation_colours_and_accessible_names();
	std::cout << "files-tree-tests:pass\n";
	return EXIT_SUCCESS;
}
