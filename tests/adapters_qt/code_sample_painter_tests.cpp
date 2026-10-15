/// \file
/// \brief Proves the shared code sample painter is one painter with two consumers.
///
/// \details RFC-BPR-0034 forbids the Appearance preview from being "a preview
/// painter of its own". What makes that true is this translation unit: the role
/// colour table, the byte-to-character conversion, the diff bands, the selection
/// wash and the syntax format ranges all live here, and the comparison panes and
/// the preview both go through them. These courses hold that boundary: each one
/// fails if the shared answer stops agreeing with the answer the panes relied on.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRectF>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using role = core::appearance_role;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto bytes_of(const QByteArray& aBytes)
		-> std::span<const std::uint8_t>
	{
		return std::span<const std::uint8_t>{
			reinterpret_cast<const std::uint8_t*>(aBytes.constData()),
			static_cast<std::size_t>(aBytes.size())
		};
	}

	/// \brief The shared table must be the profile's own answer, role for role.
	///
	/// \details This is what makes replacing `diff_workbench::adopt_appearance`'s
	/// inline loop an extraction rather than a rewrite. If either side ever starts
	/// converting a colour differently, the panes and the preview would disagree
	/// about the same theme and this comparison is what says so.
	void test_the_role_table_is_the_profiles_own_selection()
	{
		for (const auto mode : { core::appearance_mode::light,
								 core::appearance_mode::dark,
								 core::appearance_mode::high_contrast_light,
								 core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto shared = qt::code_role_colors_of(profile.palette());
			for (auto index = std::size_t{ 0U };
				 index < core::appearance_role_count;
				 ++index) {
				expect(
					shared[index]
						== profile.color(static_cast<role>(index)),
					"the shared role table equals the profile's own colour for "
					"every role of every built-in theme");
			}
		}

		// And it follows a customisation rather than the built-in profile, which is
		// what a tile painted "plus that theme's overrides" depends on.
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		auto customised = profile.palette();
		customised[core::index_of(role::code_background)]
			= core::appearance_color{ 7U, 9U, 11U, 255U };
		expect(
			profile.preview_palette(customised),
			"previewing a changed palette advances the profile");
		expect(
			qt::code_role_colors_of(profile.palette())[core::index_of(
				role::code_background)]
				== QColor{ 7, 9, 11 },
			"the shared table reads the palette in effect, overrides included");
	}

	void test_byte_offsets_become_character_offsets()
	{
		// "naïve" is six characters and seven bytes: the accent carries a
		// continuation byte, so a span behind it must not keep its byte offset.
		const auto line = QByteArray{ "const auto na\xc3\xafve = 1;" };
		expect(
			qt::character_offset(bytes_of(line), 13U) == 13,
			"a byte offset ahead of every multi-byte character is unchanged");
		expect(
			qt::character_offset(bytes_of(line), 16U) == 15,
			"a byte offset behind a two-byte character loses that byte");
		expect(
			qt::character_offset(bytes_of(line), 9'000U)
				== QString::fromUtf8(line).size(),
			"an offset past the end is bounded by the line rather than read past");
	}

	void test_format_ranges_colour_only_what_was_classified()
	{
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		const auto colors = qt::code_role_colors_of(profile.palette());
		const auto line   = QByteArray{ "if (ready) return;" };
		const auto spans  = std::vector<application::token_span>{
			{ .Line = 0U, .Column = 0U, .Length = 2U, .Role = role::syntax_control_flow },
			{ .Line = 0U, .Column = 4U, .Length = 5U, .Role = role::syntax_normal },
			{ .Line = 0U, .Column = 11U, .Length = 0U, .Role = role::syntax_keyword },
			{ .Line = 0U, .Column = 11U, .Length = 6U, .Role = role::syntax_comment },
		};
		const auto plain = qt::code_syntax_format_ranges(
			qt::code_syntax_format_request{
				.LineBytes = bytes_of(line),
				.Spans     = spans,
				.Colors    = &colors,
			});
		expect(
			plain.size() == 2,
			"a normal span and a zero-length span produce no range, so an "
			"unclassified word costs nothing");
		expect(
			plain.at(0).start == 0 && plain.at(0).length == 2
				&& plain.at(0).format.foreground().color()
					== profile.color(role::syntax_control_flow),
			"a classified span is painted in its own role's colour");
		expect(
			plain.at(0).format.fontWeight() != QFont::Bold
				&& !plain.at(0).format.fontItalic(),
			"with emphasis off — which is what the comparison panes pass — no "
			"range carries a weight or a slant, so extracting this function "
			"changed nothing about what they paint");

		const auto emphasised = qt::code_syntax_format_ranges(
			qt::code_syntax_format_request{
				.LineBytes = bytes_of(line),
				.Spans     = spans,
				.Colors    = &colors,
				.Emphasis  = qt::code_syntax_emphasis{ .Bold = true, .Italic = true },
			});
		expect(
			emphasised.size() == 2
				&& emphasised.at(0).format.fontWeight() == QFont::Bold
				&& !emphasised.at(0).format.fontItalic(),
			"control flow takes the bold switch and not the italic one");
		expect(
			emphasised.at(1).format.fontItalic()
				&& emphasised.at(1).format.fontWeight() != QFont::Bold,
			"a comment takes the italic switch and not the bold one");

		expect(
			qt::code_syntax_emphasis_of(
				role::syntax_variable,
				qt::code_syntax_emphasis{ .Bold = true, .Italic = true })
				== qt::code_syntax_emphasis{ },
			"a category that asks for no emphasis gets none however the switches "
			"are set");
		expect(
			qt::code_syntax_emphasis_of(
				role::syntax_keyword, qt::code_syntax_emphasis{ })
				== qt::code_syntax_emphasis{ },
			"a category that asks for bold gets it only when the switch is on");

		const auto struck = qt::code_syntax_format_ranges(
			qt::code_syntax_format_request{
				.LineBytes       = bytes_of(line),
				.Spans           = spans,
				.Colors          = &colors,
				.StrikeOut       = true,
				.StrikeOutLength = 18,
			});
		expect(
			struck.size() == 3 && struck.at(0).length == 18
				&& struck.at(0).format.fontStrikeOut()
				&& struck.at(1).format.fontStrikeOut(),
			"a struck line strikes the whole row and every coloured span in it");
	}

	void test_formats_are_written_only_where_they_differ()
	{
		auto pane = QPlainTextEdit{ };
		pane.setPlainText(QStringLiteral("if (ready) return;"));
		const auto block = pane.document()->findBlockByNumber(0);
		expect(block.isValid() && block.layout() != nullptr, "the block lays out");

		auto formats = QList<QTextLayout::FormatRange>{ };
		auto range   = QTextLayout::FormatRange{ };
		range.start  = 0;
		range.length = 2;
		range.format.setForeground(QColor{ 10, 20, 30 });
		formats.push_back(range);

		expect(
			qt::apply_syntax_format_ranges(*pane.document(), block, formats),
			"the first application writes the formats");
		expect(
			!qt::apply_syntax_format_ranges(*pane.document(), block, formats),
			"repeating the same formats writes nothing, so a scroll cannot "
			"relayout every visible block to restate colours it already carried");

		formats[0].format.setForeground(QColor{ 30, 20, 10 });
		expect(
			qt::apply_syntax_format_ranges(*pane.document(), block, formats),
			"a changed colour is written");
	}

	void test_bands_and_the_selection_wash()
	{
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		const auto colors = qt::code_role_colors_of(profile.palette());

		expect(
			!qt::code_sample_row_fill(qt::code_sample_row_kind::context, colors)
				 .isValid(),
			"an unchanged row carries no fill, because a background on every line "
			"marks nothing");
		expect(
			qt::code_sample_row_fill(qt::code_sample_row_kind::removed, colors)
					== profile.color(role::diff_removed_background)
				&& qt::code_sample_row_fill(
					   qt::code_sample_row_kind::added, colors)
					== profile.color(role::diff_added_background)
				&& qt::code_sample_row_fill(
					   qt::code_sample_row_kind::current_line, colors)
					== profile.color(role::code_current_line_background),
			"each row kind wears its own role's fill");

		auto pane = QPlainTextEdit{ };
		pane.setPlainText(QStringLiteral("one\ntwo\nthree"));
		auto selections = QList<QTextEdit::ExtraSelection>{ };
		qt::append_line_band(
			selections, pane, core::line_span{ 0U, 2U }, QColor{ 1, 2, 3 });
		expect(
			selections.size() == 2
				&& selections.at(0).format.boolProperty(
					QTextFormat::FullWidthSelection)
				&& selections.at(0).format.background().color() == QColor{ 1, 2, 3 },
			"a band is one full-width selection per line of the span");
		qt::append_line_band(
			selections, pane, core::line_span{ 0U, 2U }, QColor{ });
		qt::append_line_band(
			selections, pane, core::line_span{ 2U, 2U }, QColor{ 1, 2, 3 });
		expect(
			selections.size() == 2,
			"an invalid colour and an empty span append nothing");

		// The wash reads the pane's own highlight. That is not a gap: applying the
		// profile is what makes the highlight this theme's selection colour, so the
		// wash a preview shows is `code_selection_background` without this function
		// naming the role.
		profile.apply_to(pane);
		auto expected = profile.color(role::code_selection_background);
		expected.setAlpha(core::selection_wash_alpha);
		selections.clear();
		qt::append_selection_band(
			selections,
			pane,
			core::line_span{ 0U, 3U },
			core::line_span{ 1U, 2U });
		expect(
			selections.size() == 1 && selections.at(0).cursor.blockNumber() == 1
				&& selections.at(0).format.background().color() == expected,
			"the wash covers the visible part of the selection only, in the theme's "
			"selection colour at the core's own opacity");
	}

	void test_the_accepted_samples_exercise_what_the_criteria_name()
	{
		const auto tile = qt::theme_tile_code_sample();
		expect(tile.size() == 4U, "the tile snippet is four lines of code");
		auto tileRoles = std::vector<role>{ };
		auto tileKinds = std::vector<qt::code_sample_row_kind>{ };
		for (const auto& row : tile) {
			tileKinds.push_back(row.Kind);
			for (const auto& token : row.Tokens) {
				tileRoles.push_back(token.Role);
			}
		}
		const auto hasRole = [](const std::vector<role>& aRoles, const role aRole) {
			return std::ranges::find(aRoles, aRole) != aRoles.end();
		};
		expect(
			hasRole(tileRoles, role::syntax_keyword)
				&& hasRole(tileRoles, role::syntax_type)
				&& hasRole(tileRoles, role::syntax_function)
				&& hasRole(tileRoles, role::syntax_comment)
				&& std::ranges::find(tileKinds, qt::code_sample_row_kind::removed)
					!= tileKinds.end()
				&& std::ranges::find(tileKinds, qt::code_sample_row_kind::added)
					!= tileKinds.end()
				&& tile.front().Number == 41U,
			"the tile snippet shows a keyword, a type, a function, a removed line, "
			"an added line and a comment, numbered in a gutter");

		const auto preview = qt::type_preview_code_sample();
		expect(preview.size() == 6U, "the preview snippet is six lines of code");
		auto previewRoles = std::vector<role>{ };
		auto text         = QString{ };
		auto indents      = std::vector<std::uint32_t>{ };
		auto selected     = 0U;
		auto current      = 0U;
		for (const auto& row : preview) {
			indents.push_back(row.Indent);
			selected += row.Selected ? 1U : 0U;
			current += row.Kind == qt::code_sample_row_kind::current_line ? 1U : 0U;
			for (const auto& token : row.Tokens) {
				previewRoles.push_back(token.Role);
				text += token.Text;
			}
		}
		expect(
			hasRole(previewRoles, role::syntax_comment)
				&& hasRole(previewRoles, role::syntax_keyword)
				&& hasRole(previewRoles, role::syntax_control_flow)
				&& hasRole(previewRoles, role::syntax_type)
				&& hasRole(previewRoles, role::syntax_function)
				&& hasRole(previewRoles, role::syntax_macro),
			"the preview snippet names a comment, keywords, a type, a function and "
			"a macro, which is what the criterion asks it to show");
		expect(
			text.contains(QStringLiteral("!="))
				&& text.contains(QStringLiteral("&&"))
				&& text.contains(QStringLiteral(">=")),
			"the three pairs a ligature would join are all present");
		expect(
			std::ranges::max(indents) == 2U
				&& std::ranges::count(indents, 1U) >= 1
				&& std::ranges::count(indents, 2U) >= 1,
			"two tab depths are exercised, not one");
		expect(
			selected == 1U && current == 1U,
			"exactly one row carries the selection and one is the current line");
	}

	void test_a_sample_becomes_a_document_and_a_classification()
	{
		const auto sample = qt::type_preview_code_sample();
		const auto built  = qt::build_code_sample_document(sample);
		expect(
			built.Text.count(QLatin1Char{ '\n' })
					== static_cast<qsizetype>(sample.size()) - 1
				&& built.RowBytes.size() == sample.size(),
			"one document line and one byte row per sample row");

		// The spans must land on the text they claim, counted from the first byte
		// of the row including its tabs. An indent the columns did not account for
		// is exactly how every indented line would come out mis-coloured.
		auto checked = 0U;
		for (const auto& span : built.Spans) {
			expect(
				span.Role != role::syntax_normal,
				"normal text produces no span, as a real classification would not");
			const auto& bytes = built.RowBytes[span.Line];
			expect(
				static_cast<qsizetype>(span.Column) + span.Length <= bytes.size(),
				"every span lies inside its row");
			const auto covered = QString::fromUtf8(
				bytes.mid(
					static_cast<qsizetype>(span.Column),
					static_cast<qsizetype>(span.Length)));
			auto found = false;
			for (const auto& token : sample[span.Line].Tokens) {
				found = found || (token.Role == span.Role && token.Text == covered);
			}
			expect(
				found,
				"each span covers exactly the run of text whose role it names");
			++checked;
		}
		expect(checked > 10U, "there are spans to check");

		expect(
			built.RowBytes[2].startsWith('\t')
				&& !built.RowBytes[2].startsWith("\t\t")
				&& built.RowBytes[3].startsWith("\t\t"),
			"the indents reach the document as tabs, so the reviewer's tab width "
			"is what decides how far the sample is indented");

		expect(
			std::ranges::is_sorted(
				built.Spans,
				{ },
				[](const application::token_span& aSpan) { return aSpan.Line; }),
			"the spans are non-decreasing in line, which is the contract the panes "
			"search them by");
		const auto second = qt::spans_of_line(built.Spans, 2U);
		expect(
			!second.empty()
				&& std::ranges::all_of(
					second,
					[](const application::token_span& aSpan) {
						return aSpan.Line == 2U;
					}),
			"one line's spans are found without scanning the rest");
		expect(
			qt::spans_of_line(built.Spans, 99U).empty(),
			"a line with no spans answers nothing rather than the nearest run");
	}

	void test_painting_a_sample_directly_uses_the_supplied_palette()
	{
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		const auto colors = qt::code_role_colors_of(profile.palette());
		const auto sample = qt::theme_tile_code_sample();

		auto image = QImage{ 232, 132, QImage::Format_ARGB32 };
		image.fill(Qt::transparent);
		auto font   = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		auto result = qt::code_sample_paint_result{ };
		// The row height and the gutter width are given rather than derived, so the
		// probes below name exact pixels on any platform. `BandProbeX` sits in the
		// gap between the gutter and the row's first glyph: the one column of a row
		// that is always the row's own fill and never a letter of its code.
		const auto rowHeight   = 26.0;
		const auto gutterWidth = 40.0;
		const auto bandProbeX  = 44;
		{
			auto painter = QPainter{ &image };
			result       = qt::paint_code_sample(
				painter,
				QRectF{ 0.0, 0.0, 232.0, 132.0 },
				sample,
				colors,
				font,
				qt::code_sample_paint_options{ .RowHeight   = rowHeight,
											   .GutterWidth = gutterWidth });
		}
		expect(
			result.RowsPainted == 4U && result.NumbersPainted == 4U
				&& result.BandsPainted == 2U,
			"four rows, four numbers and exactly the two changed rows' bands");
		expect(
			image.pixelColor(200, 120) == profile.color(role::code_background),
			"beneath the rows the tile is the theme's own code background");
		expect(
			image.pixelColor(bandProbeX, static_cast<int>(rowHeight * 1.5))
				== profile.color(role::diff_removed_background),
			"the removed row is filled with the removed role, not the added one");
		expect(
			image.pixelColor(bandProbeX, static_cast<int>(rowHeight * 2.5))
				== profile.color(role::diff_added_background),
			"the added row is filled with the added role");
		expect(
			image.pixelColor(bandProbeX, static_cast<int>(rowHeight * 0.5))
				== profile.color(role::code_background),
			"an unchanged row keeps the code background, so a band really does mark "
			"only the rows that changed");
		expect(
			image.pixelColor(2, static_cast<int>(rowHeight * 0.5))
				== profile.color(role::code_gutter_background),
			"the gutter carries the gutter role behind its numbers");

		auto light = qt::appearance_profile{ };
		static_cast<void>(light.select_mode(core::appearance_mode::light));
		auto other = QImage{ 232, 132, QImage::Format_ARGB32 };
		other.fill(Qt::transparent);
		{
			auto painter = QPainter{ &other };
			static_cast<void>(qt::paint_code_sample(
				painter,
				QRectF{ 0.0, 0.0, 232.0, 132.0 },
				sample,
				qt::code_role_colors_of(light.palette()),
				font,
				qt::code_sample_paint_options{ .RowHeight   = rowHeight,
											   .GutterWidth = gutterWidth }));
		}
		expect(
			other.pixelColor(200, 120) == light.color(role::code_background)
				&& other.pixelColor(200, 120) != image.pixelColor(200, 120),
			"the same snippet in another theme is another picture, so the painter "
			"decides no colour of its own");
	}

	/// \brief [[BPR-1107]]. A space is a dot at the middle of its cell, and a
	/// tab a line from where it starts to its stop.
	void test_white_space_is_dots_and_lines_to_the_stop()
	{
		auto image = QImage{ 200, 40, QImage::Format_ARGB32 };
		image.fill(Qt::black);
		// A tab from 0 to its stop at 40, a space from 40 to 48, and a tab that
		// begins at 48 and so is shorter: it ends at the stop at 80.
		const auto marks = std::vector<qt::white_space_mark>{
			{ .Left = 0.0, .Right = 40.0, .Tab = true },
			{ .Left = 40.0, .Right = 48.0, .Tab = false },
			{ .Left = 48.0, .Right = 80.0, .Tab = true },
		};
		auto painted = std::uint32_t{ 0U };
		{
			auto painter = QPainter{ &image };
			painted      = qt::paint_white_space(painter, QRectF{ 0.0, 0.0, 200.0, 40.0 }, marks, QColor{ 255, 255, 255 });
		}
		expect(painted == 3U, "one mark per white space character");
		const auto lit = [&image](const int aFrom, const int aTo, const int aY) {
			for (auto x = aFrom; x < aTo; ++x) {
				if (image.pixelColor(x, aY) != QColor{ 0, 0, 0 }) {
					return true;
				}
			}
			return false;
		};
		expect(lit(4, 36, 20) && !lit(4, 36, 14) && !lit(4, 36, 26), "a tab is a thin line across its cell at the row's middle");
		expect(lit(43, 46, 20) && !lit(41, 42, 20) && !lit(47, 48, 20), "a space is a dot at the middle of its cell, not a line across it");
		expect(lit(52, 76, 20) && !lit(82, 200, 20), "a tab that begins partway to a stop is a shorter line that ends at the stop");

		auto empty = QImage{ 200, 40, QImage::Format_ARGB32 };
		empty.fill(Qt::black);
		{
			auto painter = QPainter{ &empty };
			expect(qt::paint_white_space(painter, QRectF{ 0.0, 0.0, 200.0, 40.0 }, { }, QColor{ 255, 255, 255 }) == 0U, "no white space draws nothing");
			expect(qt::paint_white_space(painter, QRectF{ 0.0, 0.0, 200.0, 40.0 }, marks, QColor{ }) == 0U, "a surface with no colour yet draws nothing");
		}
	}

	/// \brief [[BPR-1107]]. The owner's own example from his note of
	/// 2026-09-25, byte for byte, drawn the way the review panes draw it: each
	/// tab is a line and nothing else, each space a dot and nothing else, and
	/// nothing lands on a letter. v1's design captures had copied a stray space
	/// after the reference's tabs, so each tab run ended in a dot, and he asked
	/// to be sure a tab is never a line and a dot.
	void test_the_owners_example_draws_a_tab_as_a_line_alone()
	{
		// Large enough that a line (one pixel thick) and a dot (a radius of
		// 1.6) differ in height by whole pixels. The letter spacing widens every
		// cell whatever face the host resolves: offscreen macOS answers the
		// fixed font with its proportional system face, whose space is 6 px.
		constexpr auto spacing = 12.0;
		auto           font    = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		font.setPixelSize(28);
		font.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
		auto editor = QPlainTextEdit{ };
		editor.setFont(font);
		editor.setLineWrapMode(QPlainTextEdit::NoWrap);
		editor.setTabStopDistance((QFontMetricsF{ font }.horizontalAdvance(QLatin1Char{ ' ' }) + spacing) * 4.0);
		editor.setPlainText(QStringLiteral("     4 spaces.          8 more spaces\n\t\t2 tabs.\n\t1 tab.\tAnother tab, smaller because of spacing"));
		editor.resize(1'600, 240);
		editor.show();
		QApplication::processEvents();

		auto image = QImage{ editor.viewport()->size(), QImage::Format_ARGB32 };
		image.fill(Qt::transparent);
		auto painted = std::uint32_t{ 0U };
		{
			auto painter = QPainter{ &image };
			for (auto block = editor.document()->firstBlock(); block.isValid(); block = block.next()) {
				painted += qt::paint_white_space_of_block(painter, editor, block, core::white_space_drawing::all, false, QColor{ 255, 255, 255 });
			}
		}
		expect(painted == 18U + 3U + 8U, "one mark per white space character: 18 spaces, then 2 tabs and a space, then 2 tabs and 6 spaces");

		const auto lit = [&image](const int aX, const int aY) {
			return aX >= 0 && aY >= 0 && aX < image.width() && aY < image.height() && image.pixelColor(aX, aY).alpha() > 0;
		};
		// The rows lit in one column around the row's middle: a line is one
		// or two (its antialiased edge), a dot of radius 1.6 at least three.
		const auto height = [&lit](const int aX, const int aMiddle) {
			auto rows = 0;
			for (auto y = aMiddle - 6; y <= aMiddle + 6; ++y) {
				rows += lit(aX, y) ? 1 : 0;
			}
			return rows;
		};
		auto tabs = 0;
		auto dots = 0;
		for (auto block = editor.document()->firstBlock(); block.isValid(); block = block.next()) {
			const auto text = block.text();
			auto       at   = QTextCursor{ block };
			for (auto index = 0; index < text.size(); ++index) {
				at.setPosition(block.position() + index);
				const auto start = editor.cursorRect(at);
				at.setPosition(block.position() + index + 1);
				const auto end    = editor.cursorRect(at);
				const auto left   = start.left();
				const auto right  = end.left();
				const auto middle = static_cast<int>(QRectF{ start }.center().y());
				if (right - left < 12) {
					continue; // Too narrow to tell a line from a dot.
				}
				auto tallest = 0;
				auto across  = 0;
				for (auto x = left + 2; x < right - 2; ++x) {
					tallest = std::max(tallest, height(x, middle));
					across += height(x, middle) > 0 ? 1 : 0;
				}
				const auto character = text.at(index);
				if (character == QLatin1Char{ '\t' }) {
					++tabs;
					expect(across == right - left - 4, "a tab is a line all the way across its cell, to its stop");
					expect(tallest <= 2, "a tab is a line alone: no dot anywhere on it");
				} else if (character == QLatin1Char{ ' ' }) {
					++dots;
					const auto centre = (left + right) / 2;
					expect(lit(centre, middle) && tallest >= 3, "a space is a dot at the middle of its cell");
					expect(!lit(left + 2, middle) && !lit(right - 3, middle), "and not a line across it");
				} else {
					expect(across == 0, "nothing is drawn on a letter, so no dot follows a tab where the owner's text has none");
				}
			}
		}
		expect(tabs >= 3 && dots == 25, "every space of the owner's text was read, and his tabs (the partial one when it has room)");
		editor.hide();
	}

	/// \brief [[BPR-1107]]. A sample painted directly draws its white space
	/// for each choice, and a text editor's line through the same helper.
	void test_each_choice_draws_the_white_space_it_names()
	{
		const auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		auto       dark = qt::appearance_profile{ };
		static_cast<void>(dark.select_mode(core::appearance_mode::dark));
		// One row: a tab, then "a b", then two trailing spaces; selected or not.
		const auto sampleOf = [](const bool aSelected) {
			return qt::code_sample{
				qt::code_sample_row{
					.Number   = 1U,
					.Kind     = qt::code_sample_row_kind::context,
					.Indent   = 1U,
					.Selected = aSelected,
					.Tokens   = { { QStringLiteral("a b  "), role::syntax_normal } },
				},
			};
		};
		const auto painted = [&](const core::white_space_drawing aDrawing, const bool aSelected) {
			auto image = QImage{ 240, 40, QImage::Format_ARGB32 };
			image.fill(Qt::transparent);
			auto painter = QPainter{ &image };
			return qt::paint_code_sample(painter, QRectF{ 0.0, 0.0, 240.0, 40.0 }, sampleOf(aSelected), qt::code_role_colors_of(dark.palette()), font,
										 qt::code_sample_paint_options{ .WhiteSpace = aDrawing })
				.WhiteSpacePainted;
		};
		expect(painted(core::white_space_drawing::none, true) == 0U, "none draws nothing");
		expect(painted(core::white_space_drawing::selection, false) == 0U && painted(core::white_space_drawing::selection, true) == 4U,
			   "selection draws every white space character of a selected row and nothing on another");
		expect(painted(core::white_space_drawing::leading, false) == 1U, "leading draws the indenting tab");
		expect(painted(core::white_space_drawing::trailing, false) == 2U, "trailing draws the two trailing spaces");
		expect(painted(core::white_space_drawing::boundary, false) == 3U, "leading and trailing draws both");
		expect(painted(core::white_space_drawing::all, false) == 4U, "all draws the enclosed space too");

		auto editor = QPlainTextEdit{ };
		editor.setFont(font);
		editor.setTabStopDistance(QFontMetricsF{ font }.horizontalAdvance(QLatin1Char{ ' ' }) * 4.0);
		editor.setPlainText(QStringLiteral("\ta b  "));
		editor.resize(320, 80);
		editor.show();
		QApplication::processEvents();
		auto image = QImage{ editor.viewport()->size(), QImage::Format_ARGB32 };
		image.fill(Qt::transparent);
		auto       painter = QPainter{ &image };
		const auto block   = editor.document()->firstBlock();
		expect(qt::paint_white_space_of_block(painter, editor, block, core::white_space_drawing::all, false, QColor{ 255, 255, 255 }) == 4U,
			   "an editor's line draws through the same helper, at the editor's own positions");
		expect(qt::paint_white_space_of_block(painter, editor, block, core::white_space_drawing::none, false, QColor{ 255, 255, 255 }) == 0U,
			   "and draws nothing when none is chosen");
		editor.hide();
	}

	void test_long_editor_white_space_matches_full_native_projection()
	{
		for (const auto unicode : { false, true }) {
			auto editor = QPlainTextEdit{ };
			editor.setLineWrapMode(QPlainTextEdit::NoWrap);
			editor.resize(320, 80);
			auto text = QString{ };
			for (auto word = 0; word < 200; ++word) {
				text += word % 3 == 0 ? QStringLiteral("token\t") : QStringLiteral("token ");
			}
			if (unicode) {
				text.prepend(QString::fromUtf8("na\xc3\xafve "));
			}
			editor.setPlainText(text);
			editor.show();
			QApplication::processEvents();
			const auto block = editor.document()->firstBlock();
			for (const auto position : { 0, editor.horizontalScrollBar()->maximum() / 2, editor.horizontalScrollBar()->maximum() }) {
				editor.horizontalScrollBar()->setValue(position);
				QApplication::processEvents();
				auto expected = QImage{ editor.viewport()->size(), QImage::Format_ARGB32 };
				expected.fill(Qt::transparent);
				auto actual = expected;
				auto marks  = std::vector<qt::white_space_mark>{ };
				auto row    = QRectF{ };
				auto cursor = QTextCursor{ block };
				for (auto index = 0; index < text.size(); ++index) {
					if (text.at(index) != QLatin1Char{ ' ' } && text.at(index) != QLatin1Char{ '\t' }) {
						continue;
					}
					cursor.setPosition(index);
					const auto begin = editor.cursorRect(cursor);
					cursor.setPosition(index + 1);
					if (row.isNull()) {
						row = begin;
					}
					marks.push_back({ .Left = static_cast<qreal>(begin.left()), .Right = static_cast<qreal>(editor.cursorRect(cursor).left()), .Tab = text.at(index) == QLatin1Char{ '\t' } });
				}
				const auto ink = QColor{ 255, 255, 255 };
				{
					auto painter = QPainter{ &expected };
					static_cast<void>(qt::paint_white_space(painter, row, marks, ink));
				}
				{
					auto       painter = QPainter{ &actual };
					const auto count   = qt::paint_white_space_of_block(painter, editor, block, core::white_space_drawing::all, false, ink);
					expect(unicode ? count == marks.size() : count > 0U && count < marks.size() / 2U,
						   "ASCII projection is viewport bounded; Unicode keeps the full native fallback");
				}
				expect(actual == expected, "visible whitespace pixels match projecting every native endpoint at both ends and the middle");
			}
		}
	}

	void test_the_gutter_paints_the_numbers_it_was_given()
	{
		auto image = QImage{ 40, 90, QImage::Format_ARGB32 };
		image.fill(Qt::transparent);
		const auto rows = std::vector<qt::code_gutter_row>{
			{ .Top = 0.0, .Height = 30.0, .Number = 40U },
			{ .Top = 30.0, .Height = 30.0, .Number = 41U },
			{ .Top = 60.0, .Height = 30.0, .Number = 42U },
		};
		auto painted = std::uint32_t{ 0U };
		{
			auto painter = QPainter{ &image };
			painted      = qt::paint_code_gutter(
				painter,
				QRectF{ 0.0, 0.0, 40.0, 90.0 },
				rows,
				QColor{ 20, 30, 40 },
				QColor{ 200, 210, 220 });
		}
		expect(painted == 3U, "one number per row");
		expect(
			image.pixelColor(1, 1) == QColor{ 20, 30, 40 },
			"the column carries its own background");
		{
			auto blind = QImage{ 40, 90, QImage::Format_ARGB32 };
			blind.fill(Qt::transparent);
			auto painter = QPainter{ &blind };
			expect(
				qt::paint_code_gutter(
					painter,
					QRectF{ 0.0, 0.0, 40.0, 90.0 },
					rows,
					QColor{ 20, 30, 40 },
					QColor{ })
					== 0U,
				"a column with no text colour paints no number rather than an "
				"invisible one");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewCodeSamplePainterTests"));
	test_the_role_table_is_the_profiles_own_selection();
	test_byte_offsets_become_character_offsets();
	test_format_ranges_colour_only_what_was_classified();
	test_formats_are_written_only_where_they_differ();
	test_bands_and_the_selection_wash();
	test_the_accepted_samples_exercise_what_the_criteria_name();
	test_a_sample_becomes_a_document_and_a_classification();
	test_painting_a_sample_directly_uses_the_supplied_palette();
	test_white_space_is_dots_and_lines_to_the_stop();
	test_the_owners_example_draws_a_tab_as_a_line_alone();
	test_each_choice_draws_the_white_space_it_names();
	test_long_editor_white_space_matches_full_native_projection();
	test_the_gutter_paints_the_numbers_it_was_given();
	std::cout << "code-sample-painter-tests:pass\n";
	return EXIT_SUCCESS;
}
