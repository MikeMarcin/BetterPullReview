#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/bundled_code_font.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "review_cycle_desktop.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QFontInfo>
#include <QGuiApplication>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QToolBar>
#include <QToolButton>

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief The real workbench window with the real local-cycle composition
/// (BPR-1157): Open Project with no account to a first note and a suggestion
/// in the RFC-BPR-0052 scaffold, Home's Review Cycles section, resume from
/// recents, and a switch to a pull request and back. The source pane is in
/// the review panes' syntax colours, and a profile whose cycles another
/// version saved leaves Home in place and says so when a project is opened.
/// BPR-1161: Compose feedback and Share feedback through the real composition:
/// the platform clipboard, a chosen file and the store, across a restart.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 4; ++round) {
			aLanes.drain();
			QApplication::processEvents();
		}
	}

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_CYCLE_DESKTOP_RENDER_DIR");
		auto       image     = aWidget.grab().toImage();
		expect(!image.isNull(), "rendered window");
		// A sheet or dialog over the window, drawn where it stands, as the reviewer sees it.
		auto       painter = QPainter{ &image };
		const auto over    = [&](const QWidget* aTop) {
			for (auto* parent = aTop->parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
				if (parent->window() == aWidget.window()) {
					return true;
				}
			}
			return false;
		};
		// The sheet first, then the larger view over it.
		for (const auto depth : { 1, 2 }) {
			for (auto* const top : QApplication::topLevelWidgets()) {
				const auto nested = top->parentWidget() != nullptr && top->parentWidget()->window() != aWidget.window();
				if (top != aWidget.window() && top->isVisible() && qobject_cast<QDialog*>(top) != nullptr && over(top) && (nested ? 2 : 1) == depth) {
					painter.drawImage(top->mapToGlobal(QPoint{ }) - aWidget.mapToGlobal(QPoint{ }), top->grab().toImage());
				}
			}
		}
		painter.end();
		if (!directory.isEmpty()) {
			QDir{ }.mkpath(directory);
			expect(image.save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "render saved");
		}
		std::cout << "RENDER " << aName.toStdString() << ' ' << image.width() << 'x' << image.height() << '\n';
	}

	auto snapshot(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto files = std::map<std::string, std::string>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			if (entry.is_regular_file()) {
				files.emplace(entry.path().lexically_relative(aRoot).generic_string(), read_file(entry.path()));
			}
		}
		return files;
	}

	/// Freezes F1 over the cycle's first note through the production runtime,
	/// beside the desktop, and returns the round.
	auto freeze_first_note(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject) -> core::cycle_decoded_round
	{
		auto lanes   = application::manual_executor{ };
		auto runtime = application::review_cycle_runtime{ adapters::sqlite::make_review_cycle_store(aProfileRoot, core::bpr_profile_id{ 1 }), lanes, lanes };
		runtime.select(aSubject);
		auto operation = [](const std::uint8_t aIndex) {
			auto bytes = std::array<std::uint8_t, 32>{ };
			bytes[0]   = 0xD5;
			bytes[1]   = aIndex;
			return application::cycle_operation_id{ bytes };
		};
		auto state = std::optional<application::cycle_store_result<std::optional<core::review_cycle_state>>>{ };
		runtime.load_selected([&](auto aValue) { state = std::move(aValue); });
		lanes.drain();
		expect(state && *state && **state, "cycle state");
		auto begun = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
		runtime.begin_round_selected(operation(1), (**state)->Revision, [&](auto aValue) { begun = std::move(aValue); });
		lanes.drain();
		expect(begun && *begun, "draft");
		const auto draft = core::cycle_round_draft_ref{ .Cycle = aSubject.Cycle, .Id = core::cycle_round_draft_id{ (*begun)->Allocated } };
		auto       saved = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
		runtime.save_round_selected(operation(2), draft, 1, { .Source = core::cycle_source_revision_id{ 1 }, .Goal = "Rename check.", .Items = { { .Item = core::cycle_feedback_item_id{ 1 }, .Wording = core::private_review_turn_revision_id{ 1 } } } },
									[&](auto aValue) { saved = std::move(aValue); });
		lanes.drain();
		expect(saved && *saved, "draft saved");
		auto frozen = std::optional<application::cycle_store_result<application::cycle_round_freeze_result>>{ };
		runtime.freeze_round_selected({ .Operation = operation(3), .Draft = draft, .ExpectedDraftRevision = (*saved)->Revision, .Editor = { .ShownRevision = (*saved)->Revision } },
									  [&](auto aValue) { frozen = std::move(aValue); });
		lanes.drain();
		expect(frozen && *frozen && (*frozen)->Frozen, "F1 frozen");
		auto content = (*frozen)->Frozen->Content;
		runtime.shutdown();
		lanes.drain();
		return content;
	}

	// Not `near`: the Windows SDK defines `near` as an empty macro.
	auto colour_near(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 24 && std::abs(aPixel.green() - aColour.green()) <= 24 && std::abs(aPixel.blue() - aColour.blue()) <= 24;
	}

	/// \brief Whether the first `aWord` in `aEdit` is painted in `aColour`:
	/// inside the word's own rectangle at least one pixel is near `aColour`
	/// and none is near the plain text colour. Relative to the word's own
	/// ink, so it holds at any scale and face.
	auto word_painted(QPlainTextEdit& aEdit, const QString& aWord, const QColor& aColour) -> bool
	{
		const auto found = aEdit.document()->find(aWord);
		if (found.isNull()) {
			return false;
		}
		auto start = found;
		start.setPosition(found.selectionStart());
		auto end = found;
		end.setPosition(found.selectionEnd());
		const auto left   = aEdit.cursorRect(start);
		const auto right  = aEdit.cursorRect(end);
		const auto pixmap = aEdit.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aEdit.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(left.top() * ratio); y < static_cast<int>((left.bottom() + 1) * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(left.left() * ratio); x < static_cast<int>(right.left() * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	/// \brief The same rule in the code view, over the first `aWord`'s cells.
	auto word_painted(qt::suggestion_editor& aView, const QString& aWord, const QColor& aColour) -> bool
	{
		const auto word  = aWord.toUtf8();
		const auto found = aView.text().find(std::string_view{ word.constData(), static_cast<std::size_t>(word.size()) });
		if (found == std::string::npos) {
			return false;
		}
		const auto left   = aView.cell_rectangle(aView.model().point_of(found));
		const auto right  = aView.cell_rectangle(aView.model().point_of(found + static_cast<std::size_t>(word.size())));
		const auto pixmap = aView.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aView.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(std::ceil(left.top() * ratio)); y < static_cast<int>(left.bottom() * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(std::ceil(left.left() * ratio)); x < static_cast<int>(right.left() * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	/// \brief The same rule in the suggestion editor, between the middles of
	/// columns `aFirst` and `aLast` of the caret's line.
	auto span_painted(qt::suggestion_editor& aEditor, const std::uint64_t aFirst, const std::uint64_t aLast, const QColor& aColour) -> bool
	{
		const auto line  = aEditor.caret_rectangle();
		auto       begin = -1;
		auto       end   = -1;
		for (auto x = 0; x < aEditor.viewport()->width() && end < 0; ++x) {
			const auto column = aEditor.point_at(QPoint{ x, line.center().y() }).Column;
			if (begin < 0 && column > aFirst) {
				begin = x;
			}
			if (column > aLast) {
				end = x;
			}
		}
		if (begin < 0 || end <= begin) {
			return false;
		}
		const auto pixmap = aEditor.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aEditor.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(line.top() * ratio); y < static_cast<int>((line.bottom() + 1) * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(begin * ratio); x < static_cast<int>(end * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	/// `aTop` laid over `aBase`, as a band with alpha is painted.
	auto over(const QColor& aTop, const QColor& aBase) -> QColor
	{
		const auto alpha = aTop.alphaF();
		return QColor::fromRgbF(static_cast<float>(aTop.redF() * alpha + aBase.redF() * (1.0 - alpha)), static_cast<float>(aTop.greenF() * alpha + aBase.greenF() * (1.0 - alpha)),
								static_cast<float>(aTop.blueF() * alpha + aBase.blueF() * (1.0 - alpha)));
	}

	/// Whether the preview row `aRow` is filled with `aColour` past the end
	/// of its text, where no glyph is. A flat fill rounds by a few units at most.
	auto row_filled(QPlainTextEdit& aPreview, const QString& aRow, const QColor& aColour) -> bool
	{
		for (auto block = aPreview.document()->firstBlock(); block.isValid(); block = block.next()) {
			if (block.text() != aRow) {
				continue;
			}
			const auto row    = aPreview.cursorRect(QTextCursor{ block });
			const auto pixmap = aPreview.viewport()->grab();
			const auto ratio  = pixmap.devicePixelRatio();
			const auto pixel  = pixmap.toImage().pixelColor(static_cast<int>((aPreview.viewport()->width() - 4) * ratio), static_cast<int>(row.center().y() * ratio));
			return std::abs(pixel.red() - aColour.red()) <= 3 && std::abs(pixel.green() - aColour.green()) <= 3 && std::abs(pixel.blue() - aColour.blue()) <= 3;
		}
		return false;
	}

	/// Marks a profile's cycle store as saved by another schema version.
	void mark_store_newer(const std::filesystem::path& aProfileRoot)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open((aProfileRoot / "state" / "v1" / "review-cycles.sqlite3").string().c_str(), &database) == SQLITE_OK, "open the owned store");
		expect(sqlite3_exec(database, "PRAGMA user_version=99", nullptr, nullptr, nullptr) == SQLITE_OK, "store marked newer");
		sqlite3_close(database);
	}

	/// Whether a preview row carries `aColour` on the characters [aStart, aStart + aLength).
	auto row_coloured(const QPlainTextEdit& aPreview, const QString& aRow, const int aStart, const int aLength, const QColor& aColour) -> bool
	{
		for (auto block = aPreview.document()->firstBlock(); block.isValid(); block = block.next()) {
			if (block.text() == aRow && block.layout() != nullptr) {
				return std::ranges::any_of(block.layout()->formats(), [&](const QTextLayout::FormatRange& aRange) {
					return aRange.start == aStart && aRange.length == aLength && aRange.format.foreground().color() == aColour;
				});
			}
		}
		return false;
	}

	/// The middle of the cell of byte `aByte` of the code view, in its viewport.
	auto byte_point(const qt::suggestion_editor& aView, const std::uint64_t aByte) -> QPointF
	{
		return aView.cell_rectangle(aView.model().point_of(aByte)).center();
	}

	/// The pixel at the middle of one cell of the code view.
	auto cell_colour(qt::suggestion_editor& aView, const core::text_point& aPoint) -> QColor
	{
		const auto pixmap = aView.viewport()->grab();
		const auto middle = aView.cell_rectangle(aPoint).center() * pixmap.devicePixelRatio();
		return pixmap.toImage().pixelColor(static_cast<int>(middle.x()), static_cast<int>(middle.y()));
	}

	/// \brief Whether the cells of bytes [aFrom, aTo) of the code view are
	/// painted in `aInk`, not in `aRole` and not in the plain text colour:
	/// at least one pixel is closer to the ink than to the role colour, the
	/// fill and the text colour, and none is within 12 of the role colour
	/// or of the text colour.
	///
	/// \details [[BPR-1156]]. A thin stroke at 1x need not reach its ink
	/// exactly: in 9-point DejaVu Sans Mono the closest pixel was 6 off, so
	/// counting pixels within 4 of the ink found none. Anti-aliasing blends a
	/// stroke toward the fill, so the nearest colour still names the one a
	/// stroke's core was painted in. A plain-text stroke's blend toward the
	/// fill can also come nearest the ink, so a plain-text core near the
	/// text colour refuses it.
	auto ink_painted(qt::suggestion_editor& aView, const std::uint64_t aFrom, const std::uint64_t aTo, const QColor& aInk, const QColor& aRole, const QColor& aFill) -> bool
	{
		const auto text     = aView.palette().color(QPalette::Text);
		const auto left     = aView.cell_rectangle(aView.model().point_of(aFrom));
		const auto right    = aView.cell_rectangle(aView.model().point_of(aTo));
		const auto pixmap   = aView.viewport()->grab();
		const auto image    = pixmap.toImage();
		const auto ratio    = pixmap.devicePixelRatio();
		const auto distance = [](const QColor& aPixel, const QColor& aColour) {
			const auto red   = aPixel.red() - aColour.red();
			const auto green = aPixel.green() - aColour.green();
			const auto blue  = aPixel.blue() - aColour.blue();
			return (red * red) + (green * green) + (blue * blue);
		};
		// Not `near`: the Windows SDK defines `near` as an empty macro.
		const auto closeColour = [](const QColor& aPixel, const QColor& aColour) {
			return std::abs(aPixel.red() - aColour.red()) <= 12 && std::abs(aPixel.green() - aColour.green()) <= 12 && std::abs(aPixel.blue() - aColour.blue()) <= 12;
		};
		auto inked = 0;
		auto other = 0;
		// One logical pixel in from each side: a neighbouring glyph's edge
		// reaches across the cell boundary at 2x.
		const auto first = static_cast<int>(std::ceil((left.left() + 1.0) * ratio));
		const auto last  = static_cast<int>(std::floor((right.left() - 1.0) * ratio));
		for (auto y = static_cast<int>(std::ceil(left.top() * ratio)); y < static_cast<int>(left.bottom() * ratio) && y < image.height(); ++y) {
			for (auto x = first; x < last && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				const auto ink   = distance(pixel, aInk);
				inked += ink < distance(pixel, aRole) && ink < distance(pixel, aFill) && ink < distance(pixel, text) ? 1 : 0;
				other += closeColour(pixel, aRole) || closeColour(pixel, text) ? 1 : 0;
			}
		}
		return inked > 0 && other == 0;
	}

	auto fill_is(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 3 && std::abs(aPixel.green() - aColour.green()) <= 3 && std::abs(aPixel.blue() - aColour.blue()) <= 3;
	}

	/// \brief [[BPR-1157]]. Ring pixels inside the left and right edges of a
	/// control's render, over the middle third of its height: within 24 of
	/// the ring's colour on every channel, as `colour_near` reads a role.
	auto ring_pixels(const QImage& aImage, const QColor& aRing) -> int
	{
		const auto band  = static_cast<int>(std::ceil(qt::focus_ring_width * aImage.devicePixelRatio()));
		auto       count = 0;
		for (auto y = aImage.height() / 3; y < (2 * aImage.height()) / 3; ++y) {
			for (auto x = 0; x < band; ++x) {
				count += colour_near(aImage.pixelColor(x, y), aRing) ? 1 : 0;
				count += colour_near(aImage.pixelColor(aImage.width() - 1 - x, y), aRing) ? 1 : 0;
			}
		}
		return count;
	}

	/// Whether `aOn` shows a ring `aOff`, the same control unfocused, does not.
	auto ring_appears(const QImage& aOff, const QImage& aOn, const QColor& aRing) -> bool
	{
		const auto rows = (2 * aOn.height()) / 3 - aOn.height() / 3;
		return aOff.size() == aOn.size() && ring_pixels(aOn, aRing) - ring_pixels(aOff, aRing) >= rows;
	}

	/// The ring's colour on a control: over the bar for a toolbar's button,
	/// over the control's own background otherwise.
	auto ring_on(const QWidget& aControl) -> QColor
	{
		auto barred = false;
		for (const auto* parent = aControl.parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
			barred = barred || parent->inherits("QToolBar");
		}
		return qt::focus_ring_colour(aControl.palette(), barred ? QPalette::Window : aControl.backgroundRole());
	}

	auto identity_text(qt::workbench_window& aWindow) -> QString
	{
		auto* const label = aWindow.findChild<QLabel*>(QStringLiteral("workbenchStripIdentity"));
		return label != nullptr && label->isVisible() ? label->text() : QString{ };
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication                         app{ aArgc, aArgv };
	tests::sqlite_contract::scratch_root profile{ "cycle-desktop-profile" };
	tests::sqlite_contract::scratch_root scratch{ "cycle-desktop-project" };
	const auto                           project = scratch.path() / "parser-project";
	write_file(project / "README.md", "# Parser\n");
	write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\r\n{\r\n\treturn check(aText);\r\n}\r\n");
	const auto before = snapshot(project);
	// [[BPR-1270]]. The bundled code font, registered as the application does at
	// startup, before the appearance below is built from it.
	expect(qt::register_bundled_code_font(), "the bundled code font registers");

	auto lanes  = application::manual_executor{ };
	auto window = qt::workbench_window{ };
	window.resize(1280, 800);
	window.show();
	// BPR-1156: the packaged definitions the review panes classify with.
	const auto definitions = adapters::syntax::packaged_definition_catalog{ };
	// Captures on the same manual lane, so each step settles deterministically ([[BPR-1184]]).
	auto desktop = app::review_cycle_desktop{ window, profile.path(), core::bpr_profile_id{ 1 }, lanes, lanes, { .Definitions = &definitions, .Effects = &lanes }, &lanes };
	// The platform's own scheme, as the application observes it, so a native
	// render paints dark-scheme role colours on a dark window.
	const auto scheme     = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? core::os_color_scheme::dark : core::os_color_scheme::light;
	auto       appearance = qt::appearance_profile{ qt::default_appearance_settings(), { .Scheme = scheme } };
	// The window's palette as the application applies it, so the selection is
	// the profile's own code_selection_background wash (BPR-1156, v005).
	appearance.apply_to(window);
	desktop.adopt_appearance(appearance);
	settle(lanes);

	// Home offers the local way in with no account: File > Open Project… and the section.
	expect(window.showing_home() && window.open_project_action() != nullptr && window.open_project_action()->isEnabled(), "Open Project offered signed out");
	expect(window.findChild<QToolButton*>(QStringLiteral("homeOpenProjectButton")) != nullptr && !window.home_cycles_list()->isVisible(), "Home's Review Cycles section, no recents yet");
	capture(window, QStringLiteral("desktop-home-signed-out"));

	desktop.open_project(project.string());
	settle(lanes);
	auto* const page = window.local_cycle();
	expect(window.showing_local_cycle() && page != nullptr && page->isVisible(), "the cycle takes the review surface's place");
	expect(identity_text(window).contains(QStringLiteral("parser-project")) && identity_text(window).contains(QStringLiteral("Local cycle")), "title row names the cycle and badges it local");
	expect(page->projection().Revisions.size() == 1U && page->source()->text().starts_with("# Parser") && page->source()->read_only(), "R0 full source in one pane");
	auto* const modality = window.findChild<QWidget*>(QStringLiteral("workbenchModalityAction"));
	static_cast<void>(modality);
	capture(window, QStringLiteral("desktop-cycle-r0"));
	auto* const status = window.findChild<QLabel*>(QStringLiteral("workbenchStatus"));
	expect(status != nullptr && !status->text().contains(QStringLiteral("pull request")), "the status line does not ask for a pull request");

	// The first note: open the parser, select a range, comment.
	emit page->files()->itemActivated(page->files()->item(1));
	settle(lanes);
	expect(page->projection().Source && page->projection().Source->Path == "src/parser.cpp", "file opened from the drawer");
	// BPR-1157: the owner rejected a plain source pane. The real classifier
	// colours the file the pane shows, as the diff panes would.
	const auto function = appearance.color(core::appearance_role::syntax_function);
	const auto flow     = appearance.color(core::appearance_role::syntax_control_flow);
	expect(word_painted(*page->source(), QStringLiteral("return"), flow) && word_painted(*page->source(), QStringLiteral("check"), function), "the source pane in the panes' syntax colours");
	capture(window, QStringLiteral("desktop-cycle-source-syntax"));

	// [[BPR-1157]]. The owner rejected native-keyboard v001 (event 794): "Tab
	// does cycle between things but it's not obvious at all what has focus."
	// In the composed window, with window-system keys, every stop Tab reaches
	// shows the window's focus ring as focus arrives and loses it as focus
	// leaves, the window's own title row included.
	using byte_range = std::pair<std::uint64_t, std::uint64_t>;
	auto* const code = page->source();
	window.raise();
	window.activateWindow();
	for (auto wait = 0; wait < 40 && !window.isActiveWindow(); ++wait) {
		QApplication::processEvents(QEventLoop::AllEvents, 50);
	}
	// Keys reach only an active window: natively, another application in
	// front can keep this one from activating, and the course says so.
	expect(window.isActiveWindow(), "the window is active, so keys reach it");
	page->files()->setFocus(Qt::TabFocusReason);
	QApplication::processEvents();
	expect(QApplication::focusWidget() == page->files(), "focus starts on the files");
	auto ringed = 0;
	for (auto step = 0; step < 80; ++step) {
		auto* const from     = QApplication::focusWidget();
		auto        unringed = std::vector<std::pair<QWidget*, QImage>>{ };
		for (auto* const control : window.findChildren<QWidget*>()) {
			if ((control->focusPolicy() & Qt::TabFocus) != 0 && control->isVisible() && control != from) {
				unringed.emplace_back(control, control->grab().toImage());
			}
		}
		const auto held = from->grab().toImage();
		tests::window_system_key(*from, Qt::Key_Tab);
		auto* const to = QApplication::focusWidget();
		expect(to != nullptr, "Tab leaves focus on a control");
		const auto unfocused = std::ranges::find(unringed, to, &std::pair<QWidget*, QImage>::first);
		if (unfocused == unringed.end() || !ring_appears(unfocused->second, to->grab().toImage(), ring_on(*to))) {
			std::cerr << "no ring on " << (to != nullptr ? (to->objectName() + QStringLiteral(" ") + to->accessibleName()).toStdString() : std::string{ "nothing" }) << '\n';
		}
		expect(unfocused != unringed.end() && ring_appears(unfocused->second, to->grab().toImage(), ring_on(*to)), "every stop Tab reaches shows the focus ring");
		expect(ring_appears(from->grab().toImage(), held, ring_on(*from)), "and the stop it left loses it");
		++ringed;
		if (to == page->review_bar()->widgetForAction(page->comment_action())) {
			capture(window, QStringLiteral("desktop-cycle-focus-review-bar"));
		}
		if (to == page->files()) {
			break;
		}
	}
	std::cout << "focus stops ringed in the window: " << ringed << '\n';
	expect(QApplication::focusWidget() == page->files() && ringed > 10, "Tab comes round, through the title row, the review bar, the tabs, the code and the notes");
	tests::window_system_key(*page->files(), Qt::Key_Tab);
	expect(QApplication::focusWidget() == code, "the code view follows the files");
	capture(window, QStringLiteral("desktop-cycle-focus-code"));

	// The owner's suggestion, recorded as the decision: reading the code,
	// Return makes a note where the caret is and Shift+Return suggests a
	// change there. No window shortcut claims either key, so the window's own
	// keys never shadow them.
	// A key bound to one widget ([[BPR-1342]]: Home's row list reads its own
	// Shift+Return) is not a window shortcut. It is set aside and checked
	// below: it must never fire from a text field.
	const auto claimsReturn = [](const QAction& aAction) {
		return std::ranges::any_of(aAction.shortcuts(), [](const QKeySequence& aKey) {
			const auto bare = aKey[0].key();
			return aKey.count() == 1 && (bare == Qt::Key_Return || bare == Qt::Key_Enter) && (aKey[0].keyboardModifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) == Qt::NoModifier;
		});
	};
	auto scopedReturn = std::vector<QAction*>{ };
	for (auto* const action : window.findChildren<QAction*>()) {
		if (!claimsReturn(*action)) {
			continue;
		}
		if (action->shortcutContext() == Qt::WidgetShortcut || action->shortcutContext() == Qt::WidgetWithChildrenShortcut) {
			scopedReturn.push_back(action);
			continue;
		}
		std::cerr << "action " << action->objectName().toStdString() << " claims Return or Shift+Return for the window\n";
		expect(false, "no window shortcut claims Return or Shift+Return");
	}
	auto scopedFired = 0;
	for (auto* const action : scopedReturn) {
		for (auto* const owner : action->associatedObjects()) {
			const auto* const widget = qobject_cast<const QWidget*>(owner);
			expect(widget == nullptr
					   || (qobject_cast<const QLineEdit*>(widget) == nullptr && qobject_cast<const QTextEdit*>(widget) == nullptr && qobject_cast<const QPlainTextEdit*>(widget) == nullptr
						   && widget != code && !widget->isAncestorOf(code)),
				   "a widget-scoped Return key belongs to no text field and does not hold the code");
		}
		QObject::connect(action, &QAction::triggered, &window, [&scopedFired] { ++scopedFired; });
	}
	// A text field of the window, or one of the test's own on the page when
	// none is shown.
	auto* field = static_cast<QLineEdit*>(nullptr);
	for (auto* const candidate : window.findChildren<QLineEdit*>()) {
		if (candidate->isVisible()) {
			field = candidate;
			break;
		}
	}
	auto scratchField = std::unique_ptr<QLineEdit>{ };
	if (field == nullptr) {
		scratchField = std::make_unique<QLineEdit>(page);
		scratchField->show();
		field = scratchField.get();
	}
	expect(!scopedReturn.empty(), "Home's row list keeps its own Shift+Return");
	field->setFocus();
	QApplication::processEvents();
	expect(QApplication::focusWidget() == field, "a text field has focus");
	tests::window_system_key(*field, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
	tests::window_system_key(*field, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
	expect(scopedFired == 0, "Return and Shift+Return in a text field fire no widget-scoped Return key");
	scratchField.reset();
	code->setFocus();
	QApplication::processEvents();
	expect(QApplication::focusWidget() == code, "focus returns to the code");
	code->select_bytes(36U, 36U);
	tests::window_system_key(*code, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
	expect(page->comment_editor()->isVisible() && QApplication::focusWidget() == page->comment_editor() && page->findChild<QLabel*>(QStringLiteral("cycleCommentTarget"))->text() == QStringLiteral("src/parser.cpp · line 3"),
		   "Return in the reading pane opens a note on the caret's line");
	expect(scopedFired == 0, "Return in the reading pane fires no widget-scoped Return key");
	capture(window, QStringLiteral("desktop-cycle-return-note"));
	// [[BPR-1157]]. The owner's request with native-keyboard v002 (event 806):
	// Escape, and Control-C on macOS, cancel the open note or suggestion. Qt
	// names the Mac's Control key Meta; its Control (Command-C on macOS,
	// Ctrl+C elsewhere) is Copy and stays so. Nothing else in the window
	// claims a cancel key where the page has focus, and no standard key of
	// this platform's theme is Meta+C.
	const auto controlC = QKeySequence{ Qt::META | Qt::Key_C };
	const auto escape   = QKeySequence{ Qt::Key_Escape };
	auto*      ours     = page->findChild<QShortcut*>(QStringLiteral("cycleComposerCancelKeys"));
	// A key bound to a widget is live where that widget, or its child, has
	// focus: it competes on the page only when that widget is the page, holds
	// it, or lies inside it.
	// A key of another window, such as the share sheet's, is live only there.
	const auto overPage = [&](const Qt::ShortcutContext aContext, const QWidget* aOwner) {
		if (aOwner != nullptr && aContext != Qt::ApplicationShortcut && aOwner->window() != page->window()) {
			return false;
		}
		return aContext == Qt::WindowShortcut || aContext == Qt::ApplicationShortcut || aOwner == nullptr || aOwner == page || aOwner->isAncestorOf(page) || page->isAncestorOf(aOwner);
	};
	for (auto* const action : window.findChildren<QAction*>()) {
		const auto claims = action->shortcuts().contains(controlC) || action->shortcuts().contains(escape);
		const auto owners = action->associatedObjects();
		const auto live   = owners.isEmpty() ? overPage(action->shortcutContext(), nullptr) : std::ranges::any_of(owners, [&](QObject* aOwner) { return overPage(action->shortcutContext(), qobject_cast<QWidget*>(aOwner)); });
		if (claims && live) {
			std::cerr << "action " << action->objectName().toStdString() << " claims a cancel key over the page\n";
		}
		expect(!claims || !live, "no window action claims Escape or Control-C where the page has focus");
	}
	for (auto* const shortcut : window.findChildren<QShortcut*>()) {
		const auto claims = shortcut != ours && (shortcut->keys().contains(controlC) || shortcut->keys().contains(escape));
		if (claims && overPage(shortcut->context(), qobject_cast<QWidget*>(shortcut->parent()))) {
			std::cerr << "shortcut " << shortcut->objectName().toStdString() << " claims a cancel key over the page\n";
		}
		expect(!claims || !overPage(shortcut->context(), qobject_cast<QWidget*>(shortcut->parent())), "no other shortcut claims Escape or Control-C where the page has focus");
	}
	for (auto standard = int{ QKeySequence::HelpContents }; standard <= int{ QKeySequence::Cancel }; ++standard) {
		const auto bindings = QKeySequence::keyBindings(static_cast<QKeySequence::StandardKey>(standard));
		expect(!bindings.contains(controlC), "no standard key is Meta+C, the Mac's Control-C");
		expect(standard == int{ QKeySequence::Cancel } || !bindings.contains(escape), "Escape is only the standard Cancel");
	}
#if defined(Q_OS_MACOS)
	constexpr auto onMac = true;
#else
	constexpr auto onMac = false;
#endif
	expect(ours != nullptr && ours->isEnabled() && ours->keys().contains(escape) && ours->keys().contains(controlC) == onMac, "the page claims Escape, and Control-C on macOS only, while the note is open");
	page->comment_editor()->setPlainText(QStringLiteral("Not this one."));
	// Copy, with nothing selected, copies nothing and leaves the note open.
	tests::window_system_key(*page->comment_editor(), Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"));
	expect(page->comment_editor()->isVisible() && page->comment_editor()->toPlainText() == QStringLiteral("Not this one."), "Copy is not cancel");
	tests::window_system_key(*page->comment_editor(), Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
	expect(!page->comment_editor()->isVisible() && QApplication::focusWidget() == code && page->notes()->count() == 0 && !ours->isEnabled(), "Escape cancels the note in the window; the code has focus again");
	QApplication::processEvents();
	// BPR-1156, the owner's notes on composer-layout v005 (event 776), in the
	// composed window through the window system. A double click selects the
	// word even with the pointer drifting while the button is down; a click
	// past a line's end puts the caret at the end, not in virtual space; and
	// a selection is the profile's solid selection colour, ending at the text.
	tests::window_system_clicks(*code->viewport(), byte_point(*code, 6U), 2, 1.0);
	expect(code->selected_bytes() == byte_range{ 4U, 9U }, "reading: a double click in the window selects exactly the word");
	tests::window_system_clicks(*code->viewport(), byte_point(*code, 6U), 3, 1.0);
	expect(code->selected_bytes() == byte_range{ 0U, 30U }, "reading: a triple click selects the line and its break");
	tests::window_system_clicks(*code->viewport(), code->cell_rectangle({ 0U, 40U }).center(), 1);
	expect(!code->has_selection() && code->model().state().Carets.front().Head == core::text_point{ 0U, 28U }, "a click past the end puts the caret at the line's end");
	tests::window_system_mouse(*code->viewport(), code->cell_rectangle({ 0U, 0U }).topLeft() + QPointF{ 1.0, 4.0 }, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
	tests::window_system_mouse(*code->viewport(), code->cell_rectangle({ 0U, 40U }).center(), QEvent::MouseMove, Qt::LeftButton);
	tests::window_system_mouse(*code->viewport(), code->cell_rectangle({ 0U, 40U }).center(), QEvent::MouseButtonRelease, Qt::NoButton);
	const auto selectionFill = appearance.color(core::appearance_role::code_selection_background);
	expect(code->selected_bytes() == byte_range{ 0U, 28U } && fill_is(cell_colour(*code, { 0U, 3U }), selectionFill), "a drag selects the line's text in the profile's solid selection colour");
	expect(fill_is(cell_colour(*code, { 0U, 29U }), code->palette().color(QPalette::Base)), "and paints nothing past the text");
	// The selected call keeps its role, in the ink that reads on the selection.
	const auto functionInk = [&] {
		const auto channel = [](const QColor& aSource) {
			return core::appearance_color{ static_cast<std::uint8_t>(aSource.red()), static_cast<std::uint8_t>(aSource.green()), static_cast<std::uint8_t>(aSource.blue()), 255U };
		};
		const auto ink = core::readable_selection_ink(channel(function), channel(selectionFill), appearance.variant());
		return QColor{ ink.Red, ink.Green, ink.Blue };
	}();
	code->select_bytes(41U, 46U);
	QApplication::processEvents();
	expect(functionInk != function && ink_painted(*code, 41U, 46U, functionInk, function, selectionFill), "a selected call keeps its role, in the ink that reads on the selection");
	capture(window, QStringLiteral("desktop-cycle-source-word-selection"));
	page->source()->select_bytes(41U, 53U);
	QApplication::processEvents();
	const auto anchor = page->selection_anchor();
	expect(anchor && anchor->Original == "check(aText)" && anchor->Start == 41U, "exact CRLF byte anchor from the native selection");
	page->comment_action()->trigger();
	page->comment_editor()->setPlainText(QStringLiteral("Why check here?"));
	page->findChild<QPushButton*>(QStringLiteral("cycleCommentSave"))->click();
	settle(lanes);
	expect(page->notes()->count() == 1 && page->notes()->item(0)->text().contains(QStringLiteral("Why check here?")), "first note saved and listed");

	// Suggest change from the source pane's context menu, on the same
	// selection: the whole file is edited in the main pane and each separate
	// change is one exact suggestion (owner amendment to BPR-1156,
	// 2026-10-01), coloured by the panes' classifier.
	page->source()->select_bytes(41U, 53U);
	QApplication::processEvents();
	capture(window, QStringLiteral("desktop-cycle-source-selection"));
	auto* const sourceMenu  = page->source_menu(page->source()->cell_rectangle(page->source()->model().point_of(45U)).center().toPoint());
	auto* const menuSuggest = sourceMenu != nullptr ? sourceMenu->findChild<QAction*>(QStringLiteral("cycleSourceSuggest")) : nullptr;
	expect(menuSuggest != nullptr && sourceMenu->actions().contains(menuSuggest) && menuSuggest->isEnabled() && page->source()->selected_bytes() == std::pair<std::uint64_t, std::uint64_t>{ 41U, 53U },
		   "Suggest change in the source pane's context menu; a click inside the selection keeps it");
	expect(menuSuggest->shortcut()[0].keyboardModifiers() == Qt::ShiftModifier && menuSuggest->isShortcutVisibleInContextMenu(), "shown with Shift+Return");
	menuSuggest->trigger();
	delete sourceMenu;
	QApplication::processEvents();
	capture(window, QStringLiteral("desktop-cycle-suggest-begun"));
	auto* const suggestion = page->composer()->editor();
	auto* const preview    = page->composer()->preview();
	const auto  original   = read_file(project / "src" / "parser.cpp");
	expect(page->suggesting() && suggestion == page->source() && suggestion->isVisible() && !suggestion->read_only() && suggestion->text() == original, "the whole file in the main pane, in the same code view");
	// The source pane already classified these bytes, so the retained
	// classification colours the editor without waiting.
	expect(span_painted(*suggestion, 4U, 9U, flow), "the editor paints return in the control-flow colour the panes use");
	// Replace the selection, then add a line at the top: two separate changes.
	suggestion->apply(core::insert_text{ "validate(aText)" });
	suggestion->apply(core::move_carets{ .Motion = core::text_motion::document_start });
	suggestion->apply(core::insert_text{ "// Parses one value.\r\n" });
	settle(lanes);
	expect(suggestion->text() == "// Parses one value.\r\nint parse(const char* aText)\r\n{\r\n\treturn validate(aText);\r\n}\r\n", "edited in place, CRLF kept");
	expect(page->composer()->edits().size() == 2U && page->composer()->summary_text() == QStringLiteral("2 changes · lines 1, 3"), "two separate changes");
	expect(row_coloured(*preview, QStringLiteral("- \treturn check(aText);"), 3, 6, flow) && word_painted(*preview, QStringLiteral("return"), flow), "the preview's removed row colours return as control flow");
	expect(row_coloured(*preview, QStringLiteral("+ \treturn validate(aText);"), 3, 6, flow), "the added row too, once the edited file is classified");
	const auto base = preview->palette().color(QPalette::Base);
	expect(row_filled(*preview, QStringLiteral("- \treturn check(aText);"), over(appearance.color(core::appearance_role::diff_removed_background), base))
			   && row_filled(*preview, QStringLiteral("+ \treturn validate(aText);"), over(appearance.color(core::appearance_role::diff_added_background), base))
			   && row_filled(*preview, QStringLiteral("+ // Parses one value."), over(appearance.color(core::appearance_role::diff_added_background), base)),
		   "removed and added rows in the diff panes' fills");
	suggestion->apply(core::place_caret{ .At = { 3U, 0U } });
	QApplication::processEvents();
	expect(span_painted(*suggestion, 4U, 9U, flow), "the edited file is classified again and coloured");
	capture(window, QStringLiteral("desktop-cycle-suggest"));
	capture(*page->composer(), QStringLiteral("desktop-cycle-suggest-composer"));
	page->composer()->save_button()->click();
	settle(lanes);
	expect(page->notes()->count() == 2 && page->notes()->item(1)->text().contains(QStringLiteral("Suggested change · 2")) && page->projection().Notes[1].Suggestions == 2U, "one note with both suggestions saved");
	expect(!page->suggesting() && page->source()->isVisible() && page->source()->read_only() && page->source()->text() == original && read_file(project / "src" / "parser.cpp") == original,
		   "the source pane back on the captured bytes; the file on disk untouched");
	// Shift+Return in the reading pane begins Suggest change at the caret
	// ([[BPR-1157]]); then Enter indents as a code editor does: after the
	// opening brace, one tab more, chosen for this C++ file (owner note of
	// 2026-10-02).
	code->setFocus(Qt::TabFocusReason);
	code->select_bytes(36U, 36U);
	tests::window_system_key(*code, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
	expect(page->suggesting() && !code->read_only() && QApplication::focusWidget() == code && code->selected_bytes() == byte_range{ 36U, 36U }, "Shift+Return in the reading pane begins Suggest change at the caret");
	capture(window, QStringLiteral("desktop-cycle-shift-return-suggest"));
	tests::window_system_clicks(*code->viewport(), byte_point(*code, 43U), 2, 1.0);
	expect(!code->read_only() && code->selected_bytes() == byte_range{ 41U, 46U }, "editing: a double click in the window selects exactly the word");
	page->source()->apply(core::place_caret{ .At = { 1U, 1U } });
	tests::window_system_key(*code, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
	expect(page->source()->text() == "int parse(const char* aText)\r\n{\r\n\t\r\n\treturn check(aText);\r\n}\r\n" && !page->comment_editor()->isVisible(), "Enter after a brace indents one tab, CRLF kept, and opens no note");
	// Escape with only a caret cancels the suggestion ([[BPR-1157]]).
	tests::window_system_key(*code, Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
	settle(lanes);
	expect(!page->suggesting() && page->source()->text() == original && page->notes()->count() == 2, "Escape cancels the suggestion back to the captured bytes; nothing saved");
#if defined(Q_OS_MACOS)
	// And Control-C on macOS, on a note and on a suggestion.
	code->setFocus(Qt::TabFocusReason);
	code->select_bytes(36U, 36U);
	tests::window_system_key(*code, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
	page->comment_editor()->setPlainText(QStringLiteral("Not this one either."));
	tests::window_system_key(*page->comment_editor(), Qt::Key_C, Qt::MetaModifier);
	expect(!page->comment_editor()->isVisible() && page->notes()->count() == 2, "Control-C cancels the note on macOS");
	code->select_bytes(36U, 36U);
	tests::window_system_key(*code, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
	page->source()->apply(core::insert_text{ "x" });
	tests::window_system_key(*code, Qt::Key_C, Qt::MetaModifier);
	settle(lanes);
	expect(!page->suggesting() && page->source()->text() == original && page->notes()->count() == 2, "and the suggestion");
#endif
	capture(window, QStringLiteral("desktop-cycle-notes"));

	// One discussion across two files: link a README range to the first note.
	emit page->files()->itemActivated(page->files()->item(0));
	settle(lanes);
	page->source()->select_bytes(2U, 8U);
	page->notes()->setCurrentRow(0);
	QApplication::processEvents();
	expect(page->relate_button()->isEnabled(), "a selection can join a note");
	page->relate_button()->click();
	settle(lanes);
	expect(page->projection().Notes[0].Related.size() == 1U && std::get<core::cycle_source_anchor>(page->projection().Notes[0].Related[0].Target).Path == "README.md", "related location in another file");
	expect(std::holds_alternative<core::cycle_source_anchor>(page->projection().Notes[0].Primary) && std::get<core::cycle_source_anchor>(page->projection().Notes[0].Primary).Path == "src/parser.cpp", "primary kept");
	expect(page->notes()->item(0)->text().contains(QStringLiteral("Related locations · 1")), "shown on the note");

	// No extra permanent row: the review bar and the tabs, the Updates line folded.
	const auto top = page->findChild<QWidget*>(QStringLiteral("cycleBody"))->mapTo(page, QPoint{ 0, 0 }).y();
	expect(top <= qt::workbench_strip::review_bar_height_pixels + qt::document_tab_strip::height_pixels + 4, "two rows above the panes");

	// Home, recents and back.
	window.choose_home(true);
	settle(lanes);
	expect(window.showing_home() && window.home_cycles_list()->isVisible() && window.home_cycles_list()->count() == 1, "recent cycle on Home");
	capture(window, QStringLiteral("desktop-home-recents"));
	emit window.home_cycles_list()->itemDoubleClicked(window.home_cycles_list()->item(0));
	settle(lanes);
	expect(window.showing_local_cycle() && page->notes()->count() == 2, "resumed from Home");

	// A pull request arriving takes the review surface back; the cycle is not lost.
	window.publish(application::github_presentation_update{
		.PullRequest = application::github_pull_request_projection{ .Owner = "octo", .RepositoryName = "parser", .Number = core::pull_request_number{ 7U }, .Title = "Tighten parsing" },
	});
	settle(lanes);
	expect(!window.showing_local_cycle() && !window.showing_home() && identity_text(window).contains(QStringLiteral("#7")) && !identity_text(window).contains(QStringLiteral("Local cycle")), "switched to the pull request");
	capture(window, QStringLiteral("desktop-switch-to-pull-request"));
	emit window.home_cycles_list()->itemDoubleClicked(window.home_cycles_list()->item(0));
	settle(lanes);
	expect(window.showing_local_cycle() && identity_text(window).contains(QStringLiteral("Local cycle")) && page->notes()->count() == 2, "and back to the cycle");

	expect(snapshot(project) == before, "the chosen folder was only read");

	// BPR-1163: someone edits the folder with ordinary tools and sends nothing
	// back; Capture revision is the reviewer's way back, with no callback.
	emit page->files()->itemActivated(page->files()->item(1));
	settle(lanes);
	write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\r\n{\r\n\treturn aText ? check(aText) : 0;\r\n}\r\n");
	expect(page->capture_action()->isEnabled() && page->projection().Revisions.size() == 1U, "Capture revision offered; nothing captured by itself");
	page->capture_action()->trigger();
	settle(lanes);
	expect(page->projection().Revisions.size() == 2U && page->projection().Source && page->projection().Source->Revision == core::cycle_source_revision_id{ 1 }, "R1 captured; the open tab keeps R0");
	expect(page->refresh_action()->isVisible(), "newer material is offered, not shown by itself");
	auto* const menu = page->view_button()->menu();
	emit        menu->aboutToShow();
	const auto  actions = menu->actions();
	const auto  all     = std::ranges::find_if(actions, [](const QAction* aAction) { return aAction->text() == QStringLiteral("All cycle changes"); });
	expect(all != actions.end() && (*all)->isEnabled(), "All cycle changes in the view chooser");
	(*all)->trigger();
	settle(lanes);
	expect(page->projection().Comparison && page->comparison()->isVisible() && page->comparison()->toPlainText().contains(QStringLiteral("+ \treturn aText ? check(aText) : 0;")),
		   "the comparison pane shows the shared engine's rows");
	expect(page->view_button()->text() == QStringLiteral("All cycle changes · R1 → R2"), "the review bar names the view and its pair");
	// [[BPR-1281]]. The comparison in the panes' syntax colours, through the
	// same classifier and painter: the removed row's words painted, and the
	// added row's code carrying the same role colours after its line numbers.
	expect(word_painted(*page->comparison(), QStringLiteral("return"), flow) && word_painted(*page->comparison(), QStringLiteral("check"), function),
		   "the comparison's removed row in the panes' syntax colours");
	const auto addedRow = [&] {
		for (auto block = page->comparison()->document()->firstBlock(); block.isValid(); block = block.next()) {
			if (block.text().endsWith(QStringLiteral("+ \treturn aText ? check(aText) : 0;"))) {
				return block;
			}
		}
		return QTextBlock{ };
	}();
	const auto rowCoded = [&](const QColor& aColour, const QString& aWord) {
		const auto at      = addedRow.text().indexOf(aWord);
		const auto formats = addedRow.isValid() && addedRow.layout() != nullptr ? addedRow.layout()->formats() : QList<QTextLayout::FormatRange>{ };
		return std::ranges::any_of(formats, [&](const QTextLayout::FormatRange& aRange) { return aRange.start == at && aRange.length == aWord.size() && aRange.format.foreground().color() == aColour; });
	};
	expect(rowCoded(flow, QStringLiteral("return")) && rowCoded(function, QStringLiteral("check")), "the added row's code in the same role colours, after its line numbers");
	capture(window, QStringLiteral("desktop-cycle-compare-syntax"));
	page->mark_reviewed_action()->trigger();
	settle(lanes);
	expect(page->projection().Facts.LastReview && page->projection().Facts.LastReview->Revision == core::cycle_source_revision_id{ 2 }, "reviewed through R1");
	page->capture_action()->trigger();
	settle(lanes);
	expect(page->projection().Revisions.size() == 2U && page->updates_line()->text().contains(QStringLiteral("No change since R2")), "an unchanged capture says so");
	capture(window, QStringLiteral("desktop-cycle-compare"));

	// BPR-1164: a round frozen in this profile; what comes back is recorded
	// as claims, by hand or from a response file, and changes nothing else.
	const auto round   = freeze_first_note(profile.path(), page->projection().Cycle->Subject);
	const auto written = snapshot(project);
	desktop.open_project(project.string());
	settle(lanes);
	expect(page->projection().Facts.Rounds.size() == 1U && page->projection().Revisions.size() == 2U, "the frozen round is known; nothing captured on reopening");
	page->notes()->setCurrentRow(0);
	auto* const replies = page->reply_button()->menu();
	emit        replies->aboutToShow();
	replies->actions()[0]->trigger();
	page->reply_from()->setText(QStringLiteral("Dana (chat)"));
	page->reply_editor()->setPlainText(QStringLiteral("Kept check(); it validates too."));
	page->findChild<QPushButton*>(QStringLiteral("cycleReplySave"))->click();
	settle(lanes);
	expect(page->notes()->item(0)->text().contains(QStringLiteral("F1 reply · Dana (chat), entered by hand replied: Kept check(); it validates too.")), "a pasted reply beside its note");
	const auto responseFile = scratch.path() / "response.json";
	write_file(responseFile, *core::encode_response_document({
								 .ResponseId    = "RESP-1",
								 .CycleId       = round.Payload.CycleId,
								 .RoundId       = round.Payload.RoundId,
								 .RoundDigest   = round.Digest,
								 .ClaimedAuthor = { .DisplayName = "Harness", .Kind = core::cycle_claimed_author_kind::agent, .Model = "Claude Opus" },
								 .Entries       = { { .Item = "I1", .ItemRevision = 1, .Disposition = core::cycle_response_disposition::addressed, .Body = "Renamed to validate.", .Evidence = { { .Kind = core::cycle_evidence_kind::test_report, .Value = "tests passed" } } } },
							 }));
	desktop.host().import_response(core::cycle_round_id{ 1 }, responseFile.string());
	settle(lanes);
	expect(page->projection().Responses.size() == 2U && page->notes()->item(0)->text().contains(QStringLiteral("Harness (claims agent Claude Opus), response file says addressed: Renamed to validate.")),
		   "a response file's statement beside its note, as a claim");
	desktop.host().import_response(core::cycle_round_id{ 1 }, responseFile.string());
	settle(lanes);
	expect(page->projection().Responses.size() == 2U && page->updates_line()->text().contains(QStringLiteral("already added")), "the same file again adds nothing");
	expect(snapshot(project) == written && page->projection().Revisions.size() == 2U && page->projection().Facts.LastReview && page->projection().Facts.LastReview->Revision == core::cycle_source_revision_id{ 2 },
		   "replies wrote no source, captured nothing and moved no review mark");
	capture(window, QStringLiteral("desktop-cycle-replies"));

	// BPR-1165: the claim asks for verification; only the reviewer's decision gives it.
	expect(page->notes()->item(0)->text().contains(QStringLiteral("Recipient says addressed; needs your verification")), "addressed asks for verification");
	auto* const decisions = page->decision_menu(0);
	expect(decisions != nullptr && decisions->actions()[0]->text() == QStringLiteral("Verified in R2"), "verify against the revision shown");
	decisions->actions()[0]->trigger();
	settle(lanes);
	expect(page->notes()->item(0)->text().contains(QStringLiteral("Verified by you in R2")), "verified by the reviewer");
	page->next_round_button()->click();
	settle(lanes);
	expect(page->projection().NextRound && page->projection().NextRound->Resolved == 1U && page->projection().NextRound->Added == 1U && page->projection().NextRound->Carried == 0U,
		   "the next round's draft: the verified note left out, the other note added");
	expect(snapshot(project) == written && page->projection().Revisions.size() == 2U, "deciding and preparing wrote no source");
	capture(window, QStringLiteral("desktop-cycle-verified"));

	// BPR-1161: Compose feedback opens that draft; one click freezes F2 and
	// copies it, Save writes the same bytes to the file chosen, and each
	// observation is listed in its own words, after a restart too.
	const auto handoff = scratch.path() / "handoff";
	std::filesystem::create_directories(handoff);
	auto chosen = std::vector<std::string>{ };
	desktop.set_save_chooser([&](const std::string& aSuggested, const bool aBundle) {
		chosen.push_back(aSuggested);
		const auto path = (handoff / aSuggested).u8string();
		return aBundle ? std::string{ } : std::string{ path.begin(), path.end() };
	});
	auto* const clipboard = QGuiApplication::clipboard();
	const auto  kept      = clipboard->text();
	page->compose_action()->trigger();
	settle(lanes);
	auto* const share = page->share_page();
	expect(share->isVisible() && share->isWindow() && page->projection().Share && page->projection().Share->Round == core::cycle_round_id{ 2 } && page->projection().Share->Items == 1U, "Share notes opens the next round's draft in a sheet");
	expect(share->title()->text() == QStringLiteral("Share round 2") && !share->rows().empty() && share->rows()[0].Include->isChecked(), "round 2, its note included");
	settle(lanes);
	expect(!page->projection().Share->Contexts.empty() && page->projection().Share->Contexts[0].Status == application::review_material_status::available
			   && !share->rows()[0].Where->text().isEmpty(),
		   "the note's code is read by the host and its place shown");
	// The note's code behind Show Change, and its whole file over the sheet, through the composition.
	// The owner rejected v003's arrow beside the include box (event 921): it is words now.
	expect(share->rows()[0].Disclose->text() == QStringLiteral("Show Change") && share->rows()[0].Disclose->arrowType() == Qt::NoArrow && !share->rows()[0].Code->isVisible(), "the note's change closed, behind the words Show Change");
	capture(window, QStringLiteral("desktop-cycle-share-closed"));
	share->rows()[0].Disclose->click();
	settle(lanes);
	expect(share->rows()[0].Code->isVisible() && !share->rows()[0].Code->excerpt().Rows.empty() && share->rows()[0].Disclose->text() == QStringLiteral("Hide Change"), "Show Change shows the note's code, and then says Hide Change");
	// [[BPR-1161]]. The owner rejected compose-share v002 (event 895): the
	// note's code was not aligned, not coloured and not in the code font. It
	// is in the profile's code font, the bundled family, and the real
	// classifier colours it as it colours the source pane.
	expect(share->rows()[0].Code->font().family() == appearance.code_font().family() && QFontInfo{ share->rows()[0].Code->font() }.family() == qt::bundled_code_font_family(),
		   "the note's code in the profile's code font, the bundled family");
	expect(word_painted(*share->rows()[0].Code, QStringLiteral("return"), flow) && word_painted(*share->rows()[0].Code, QStringLiteral("check"), function),
		   "the note's code in the panes' syntax colours");
	capture(window, QStringLiteral("desktop-cycle-share-code"));
	share->rows()[0].Whole->click();
	settle(lanes);
	expect(page->projection().Share->Larger && page->projection().Share->Larger->Item == share->rows()[0].Item && share->viewer()->isVisible() && share->viewer()->foot()->isHidden()
			   && share->viewer()->code()->excerpt().Rows.size() >= share->rows()[0].Code->excerpt().Rows.size(),
		   "Show the Whole File reads the file through the host and shows it over the sheet");
	expect(QFontInfo{ share->viewer()->code()->font() }.family() == qt::bundled_code_font_family() && word_painted(*share->viewer()->code(), QStringLiteral("return"), flow)
			   && word_painted(*share->viewer()->code(), QStringLiteral("check"), function),
		   "the whole file in the same font and colours");
	capture(window, QStringLiteral("desktop-cycle-share-whole-file"));
	tests::window_system_key(*share->viewer()->done_button(), Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
	settle(lanes);
	expect(!share->viewer()->isVisible() && share->isVisible() && !page->projection().Share->Larger, "Escape closes it; the host lets the whole file go");
	auto* const copy = share->route_button(core::cycle_share_route::copy_document);
	expect(copy->isVisible() && copy->isEnabled() && share->route_button(core::cycle_share_route::save_document)->isEnabled() && !share->route_button(core::cycle_share_route::external_agent)->isVisible(),
		   "Copy and Save first-class, and no external interface offered");
#if defined(BPR_AGENT_INTEGRATIONS)
	// [[BPR-1180]]. The installed adapter's route is listed, refused until this
	// source's channel allows it; nothing else stands in for it.
	expect(share->route_button(core::cycle_share_route::send_to_agent)->isVisible() && !share->route_button(core::cycle_share_route::send_to_agent)->isEnabled(),
		   "Send to Agent… listed, refused for source of unknown origin");
#else
	expect(!share->route_button(core::cycle_share_route::send_to_agent)->isVisible(), "no agent route in a build without agent integrations");
#endif
	const auto bytes    = page->projection().Share->Document;
	const auto document = QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size()));
	share->exact_text_button()->click();
	settle(lanes);
	expect(share->viewer()->isVisible() && share->viewer()->text()->toPlainText() == QString{ document }.remove(QLatin1Char('\r')) && document.startsWith(QStringLiteral("#")) && document.contains(QStringLiteral("validate(aText)")),
		   "the exact text is the readable document, the suggestion in it");
	share->viewer()->done_button()->click();
	settle(lanes);
	capture(window, QStringLiteral("desktop-cycle-share-draft"));
	const auto unchanged = snapshot(project);
	copy->click();
	settle(lanes);
	const auto& copied = *page->projection().Share;
	expect(clipboard->text() == document && copied.Frozen && copied.Round == core::cycle_round_id{ 2 } && page->projection().Facts.Rounds.size() == 2U, "one click froze F2 and copied exactly the preview");
	expect(share->status()->text().startsWith(QStringLiteral("Copied F2 to the clipboard")) && share->deliveries()->item(0)->text().startsWith(QStringLiteral("F2 · Copied to the clipboard")), "and says only that");
	share->route_button(core::cycle_share_route::save_document)->click();
	settle(lanes);
	expect(chosen == std::vector<std::string>{ "RC-1-F2.md" } && read_file(handoff / "RC-1-F2.md") == bytes, "Save offers the round's own name and writes the same bytes");
	share->route_button(core::cycle_share_route::save_bundle)->click();
	settle(lanes);
	expect(chosen.size() == 2U && chosen[1] == "RC-1-F2.bpr-review.zip" && !std::filesystem::exists(handoff / "RC-1-F2.bpr-review.zip") && page->projection().Share->Deliveries.size() == 2U,
		   "a save dialog dismissed writes nothing and records nothing");
	expect(share->deliveries()->count() == 2 && share->deliveries()->item(1)->text().startsWith(QStringLiteral("F2 · Saved as RC-1-F2.md")), "copied and saved, each said apart");
	expect(snapshot(project) == unchanged && page->projection().Revisions.size() == 2U, "sharing wrote no source and captured nothing");
	capture(window, QStringLiteral("desktop-cycle-share-shared"));
	clipboard->setText(kept);
	share->goal()->setFocus(Qt::TabFocusReason);
	tests::window_system_key(*share->goal(), Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
	settle(lanes);
	expect(!share->isVisible() && !page->projection().Share && (page->source()->isVisible() || page->comparison()->isVisible()), "Escape closes the sheet; the reading pane again");
	desktop.shutdown();
	settle(lanes);
	expect(desktop.is_quiescent(), "quiescent after shutdown");
	{
		// A restart: the observations are the store's, not the window's.
		auto other = qt::workbench_window{ };
		other.resize(1280, 800);
		other.show();
		auto again = app::review_cycle_desktop{ other, profile.path(), core::bpr_profile_id{ 1 }, lanes, lanes, { }, &lanes };
		again.open_project(project.string());
		settle(lanes);
		auto* const restarted = other.local_cycle();
		restarted->compose_action()->trigger();
		settle(lanes);
		const auto& reopened = *restarted->projection().Share;
		expect(reopened.Frozen && reopened.Round == core::cycle_round_id{ 2 } && reopened.Deliveries.size() == 2U && restarted->share_page()->deliveries()->item(0)->text().startsWith(QStringLiteral("F2 · Copied")),
			   "after a restart F2 is still shared, with the same two observations");
		again.shutdown();
		settle(lanes);
		expect(again.is_quiescent(), "the restarted desktop is quiescent");
	}

	// BPR-1157 v001 was rejected on a window that showed an empty
	// "Open Project · Local cycle" page at start: an older build met a store
	// a newer one had saved. Home stays; the refusal is said on the page.
	{
		tests::sqlite_contract::scratch_root newer{ "cycle-desktop-newer" };
		{
			auto store = adapters::sqlite::make_review_cycle_store(newer.path(), core::bpr_profile_id{ 1 });
			expect(store->catalog({ }).has_value(), "a store to mark");
		}
		mark_store_newer(newer.path());
		auto other = qt::workbench_window{ };
		other.resize(1280, 800);
		other.show();
		auto refused = app::review_cycle_desktop{ other, newer.path(), core::bpr_profile_id{ 1 }, lanes, lanes, { }, &lanes };
		settle(lanes);
		expect(other.showing_home() && !other.showing_local_cycle(), "Home stays in place at start");
		refused.open_project(project.string());
		settle(lanes);
		auto* const notice = other.findChild<QLabel*>(QStringLiteral("cycleSourceNotice"));
		expect(other.showing_local_cycle() && notice != nullptr && notice->isVisible() && notice->text().contains(QStringLiteral("saved by a different version")), "the refusal is said where the source would be");
		capture(other, QStringLiteral("desktop-cycle-newer-store"));
		refused.shutdown();
		settle(lanes);
		expect(refused.is_quiescent(), "the refused desktop is quiescent");
	}
	std::cout << "review-cycle-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
