/// \file
/// \brief Proves RFC-BPR-0034's labelled Type preview is the panes' own painter.
///
/// \details Criterion C4. The six code lines show an italic comment, bold
/// keywords, a type, a function, the ligature pairs, two tab depths with faint
/// markers, a removed line, an added line carrying a selection and a macro, and
/// the current line; the caption states the values in effect in points; and every
/// accepted change repaints it.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/type_preview.hpp"

#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QList>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextLayout>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

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

	void settle()
	{
		QApplication::processEvents();
		QApplication::sendPostedEvents();
		QApplication::processEvents();
	}

	void show(qt::type_preview& aPreview)
	{
		aPreview.resize(420, 320);
		aPreview.show();
		settle();
	}

	[[nodiscard]] auto block_formats(const qt::type_preview& aPreview, const int aRow)
		-> QList<QTextLayout::FormatRange>
	{
		const auto block = aPreview.code_surface()->document()->findBlockByNumber(
			aRow);
		return !block.isValid() || block.layout() == nullptr
			? QList<QTextLayout::FormatRange>{ }
			: block.layout()->formats();
	}

	/// \brief The format range painted in one colour, if any.
	[[nodiscard]] auto range_in(
		const QList<QTextLayout::FormatRange>& aFormats,
		const QColor&                          aColor)
		-> std::optional<QTextLayout::FormatRange>
	{
		for (const auto& range : aFormats) {
			if (range.format.foreground().color() == aColor) {
				return range;
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] auto band_on(
		const qt::type_preview& aPreview,
		const int               aRow,
		const QColor&           aColor) -> bool
	{
		for (const auto& selection : aPreview.code_surface()->extraSelections()) {
			if (
				selection.cursor.blockNumber() == aRow
				&& selection.format.boolProperty(QTextFormat::FullWidthSelection)
				&& selection.format.background().color() == aColor) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] auto legal_typography() -> core::typography_settings
	{
		return qt::default_typography_settings();
	}

	void test_points_are_never_hundredths()
	{
		expect(
			qt::point_size_text(1'300U) == QStringLiteral("13"),
			"a whole size reads as a whole number of points, not as 1300 and not "
			"as 13.00");
		expect(
			qt::point_size_text(1'250U) == QStringLiteral("12.50")
				&& qt::point_size_text(1'205U) == QStringLiteral("12.05"),
			"a fractional size keeps its two decimals");
	}

	void test_the_sample_shows_everything_the_criterion_names()
	{
		auto preview = qt::type_preview{ };
		show(preview);
		auto* const code = preview.code_surface();
		expect(code != nullptr, "the preview holds a code surface");
		expect(
			code->isReadOnly()
				&& code->lineWrapMode() == QPlainTextEdit::NoWrap,
			"the code surface is a read-only unwrapped code pane, which is what "
			"the review window's panes are");
		expect(
			preview.heading() != nullptr
				&& preview.heading()->text() == QStringLiteral("Preview"),
			"the preview is labelled");
		expect(
			preview.caption() != nullptr && preview.caption()->wordWrap(),
			"the caption wraps rather than widening the card");

		expect(
			code->document()->blockCount() == 6,
			"six code lines");
		const auto text = code->toPlainText();
		expect(
			text.contains(QStringLiteral("!="))
				&& text.contains(QStringLiteral("&&"))
				&& text.contains(QStringLiteral(">=")),
			"the three pairs a ligature would join are on screen, so turning "
			"ligatures off has something to show");
		expect(
			text.contains(QStringLiteral("MAX_SAMPLES")),
			"the added line carries a macro");
		expect(
			code->document()->findBlockByNumber(2).text().startsWith(
				QLatin1Char{ '\t' })
				&& code->document()
					   ->findBlockByNumber(3)
					   .text()
					   .startsWith(QStringLiteral("\t\t")),
			"two tab depths, as tabs, so the reviewer's tab width decides them");
	}

	void test_the_panes_own_painter_colours_and_bands_the_sample()
	{
		auto preview = qt::type_preview{ };
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		show(preview);
		preview.adopt_appearance(profile);
		settle();

		expect(
			preview.statistics().Adoptions == 1U
				&& preview.statistics().FormattedRows == 6U,
			"adopting a theme writes the formats of every row");

		const auto comment = range_in(
			block_formats(preview, 0), profile.color(role::syntax_comment));
		expect(
			comment.has_value() && comment->format.fontItalic(),
			"the comment is painted in the comment role and slanted, because the "
			"accepted typography asks for italic emphasis");
		const auto keyword = range_in(
			block_formats(preview, 1), profile.color(role::syntax_keyword));
		expect(
			keyword.has_value() && keyword->format.fontWeight() == QFont::Bold,
			"a keyword is painted in the keyword role and bold");
		expect(
			range_in(block_formats(preview, 1), profile.color(role::syntax_type))
					.has_value()
				&& range_in(
					   block_formats(preview, 1),
					   profile.color(role::syntax_function))
					   .has_value(),
			"the type name and the function name each carry their own role");
		const auto macro = range_in(
			block_formats(preview, 4), profile.color(role::syntax_macro));
		expect(
			macro.has_value() && !macro->format.fontItalic()
				&& macro->format.fontWeight() != QFont::Bold,
			"the macro carries the macro role and takes no emphasis it did not ask "
			"for");

		expect(
			band_on(preview, 3, profile.color(role::diff_removed_background)),
			"the removed line wears the removed fill");
		expect(
			band_on(preview, 4, profile.color(role::diff_added_background)),
			"the added line wears the added fill");
		expect(
			band_on(
				preview, 5, profile.color(role::code_current_line_background)),
			"the current line is highlighted — the role nothing in the review "
			"window painted before this preview");

		auto wash = profile.color(role::code_selection_background);
		wash.setAlpha(core::selection_wash_alpha);
		expect(
			band_on(preview, 4, wash),
			"the added line also carries the selection, in this theme's selection "
			"colour at the core's own opacity");
		const auto& selections = preview.code_surface()->extraSelections();
		const auto  added      = std::ranges::find_if(
			selections,
			[&profile](const QTextEdit::ExtraSelection& aSelection) {
				return aSelection.cursor.blockNumber() == 4
					&& aSelection.format.background().color()
					== profile.color(role::diff_added_background);
			});
		const auto washed = std::ranges::find_if(
			selections,
			[&wash](const QTextEdit::ExtraSelection& aSelection) {
				return aSelection.cursor.blockNumber() == 4
					&& aSelection.format.background().color() == wash;
			});
		expect(
			added != selections.end() && washed != selections.end()
				&& std::distance(selections.begin(), added)
					< std::distance(selections.begin(), washed),
			"the wash is laid over the added band rather than under it, which is "
			"the order the comparison panes lay them in");

		expect(
			preview.statistics().TabMarkersPainted == 5U,
			"five tab markers: one on the first indented row and two on each of "
			"the next two");
		expect(
			preview.statistics().GutterNumbersPainted == 6U,
			"the gutter numbers all six rows");
	}

	void test_every_accepted_change_repaints_the_preview()
	{
		auto preview = qt::type_preview{ };
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		show(preview);
		preview.adopt_appearance(profile);
		settle();
		const auto lightComment = profile.color(role::syntax_comment);
		expect(
			range_in(block_formats(preview, 0), lightComment).has_value(),
			"the light theme's comment colour is on screen");

		preview.adopt_appearance(profile);
		expect(
			preview.statistics().FormattedRows == 0U
				&& preview.statistics().Adoptions == 2U,
			"a repeated adoption writes no format, so nothing is relaid out to "
			"restate colours the sample already carried");

		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		preview.adopt_appearance(profile);
		settle();
		const auto darkComment = profile.color(role::syntax_comment);
		expect(
			darkComment != lightComment
				&& range_in(block_formats(preview, 0), darkComment).has_value()
				&& !range_in(block_formats(preview, 0), lightComment).has_value(),
			"changing the theme repaints the sample in the new palette");

		// A colour customisation is an accepted change too, and it must reach the
		// preview without the mode moving.
		auto customised = profile.palette();
		customised[core::index_of(role::syntax_comment)]
			= core::appearance_color{ 1U, 2U, 3U, 255U };
		expect(profile.preview_palette(customised), "the preview palette applies");
		preview.adopt_appearance(profile);
		settle();
		expect(
			range_in(block_formats(preview, 0), QColor{ 1, 2, 3 }).has_value(),
			"an edited colour shows in the preview immediately");
	}

	void test_typography_reaches_the_sample_and_the_caption()
	{
		auto preview = qt::type_preview{ };
		auto profile = qt::appearance_profile{ };
		show(preview);
		preview.adopt_appearance(profile);
		settle();

		auto record          = legal_typography();
		record.Code.TabWidth = 4U;
		expect(
			profile.select_typography(record).accepted(),
			"the fixture typography is one the core accepts");
		preview.adopt_appearance(profile);
		settle();
		const auto narrow = preview.code_surface()->tabStopDistance();
		const auto height = preview.code_surface()->height();

		record.Code.TabWidth = 8U;
		expect(
			profile.select_typography(record).accepted(),
			"eight is a legal tab width");
		preview.adopt_appearance(profile);
		settle();
		expect(
			preview.code_surface()->tabStopDistance() > narrow,
			"the tab stop follows the chosen width, through the same profile call "
			"every code surface takes");
		expect(
			preview.caption_text().contains(QStringLiteral("tabs of 8")),
			"and the caption says so");

		record.Code.Font.PointSizeCentipoints += 400U;
		expect(
			profile.select_typography(record).accepted(),
			"a larger code size is legal");
		preview.adopt_appearance(profile);
		settle();
		expect(
			preview.code_surface()->height() > height,
			"a larger code size makes the sample taller, so the size is applied "
			"rather than described");
		expect(
			preview.caption_text().contains(
				qt::point_size_text(record.Code.Font.PointSizeCentipoints)
				+ QStringLiteral(" pt")),
			"the caption states the code size in points");
		expect(
			!preview.caption_text().contains(
				QString::number(record.Code.Font.PointSizeCentipoints)),
			"and never in hundredths");

		record.Code.SyntaxBold   = false;
		record.Code.SyntaxItalic = false;
		expect(
			profile.select_typography(record).accepted(),
			"turning both emphases off is legal");
		preview.adopt_appearance(profile);
		settle();
		expect(
			!range_in(
				 block_formats(preview, 0), profile.color(role::syntax_comment))
				 ->format.fontItalic(),
			"with italic emphasis off the comment is upright");
		expect(
			range_in(block_formats(preview, 1), profile.color(role::syntax_keyword))
					->format.fontWeight()
				!= QFont::Bold,
			"with bold emphasis off the keyword is regular");
		expect(
			preview.caption_text().contains(
				QStringLiteral("bold and italic emphasis off")),
			"the caption reports the switches as they are");

		const auto shortLine        = preview.interface_sample()->height();
		record.Ui.LineHeightPercent = 200U;
		expect(
			profile.select_typography(record).accepted(),
			"a doubled line height is legal");
		preview.adopt_appearance(profile);
		settle();
		expect(
			preview.interface_sample()->height() > shortLine,
			"the interface sample grows with the line height, so that control has "
			"something to show as well");
	}

	void test_the_caption_names_the_theme_the_composition_supplied()
	{
		auto preview = qt::type_preview{ };
		expect(
			preview.caption_text().contains(
				QStringLiteral("the theme on screen")),
			"until the composition names the theme the caption says what is true "
			"of any of them rather than guessing one");
		preview.set_theme_name(QStringLiteral("Dark"));
		expect(
			preview.caption_text().contains(
				QStringLiteral("colours from Dark.")),
			"once named, the caption says which theme the colours come from");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewTypePreviewTests"));
	test_points_are_never_hundredths();
	test_the_sample_shows_everything_the_criterion_names();
	test_the_panes_own_painter_colours_and_bands_the_sample();
	test_every_accepted_change_repaints_the_preview();
	test_typography_reaches_the_sample_and_the_caption();
	test_the_caption_names_the_theme_the_composition_supplied();
	std::cout << "type-preview-tests:pass\n";
	return EXIT_SUCCESS;
}
