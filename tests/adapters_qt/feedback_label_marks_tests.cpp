/// \file
/// \brief [[BPR-1083]]. Proves the one resource vocabulary of the twelve label
/// marks: the compiled glyphs equal the owner-accepted path data element by
/// element, and the marks, tints, shortcodes and names follow the delivery
/// contract (`docs/ux/conventional-comments-feedback.md`).

#include "bpr/core/appearance_model.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
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

	[[nodiscard]] auto read_text(const char* const aPath) -> QString
	{
		auto file = QFile{ QString::fromUtf8(aPath) };
		expect(file.open(QIODevice::ReadOnly), std::string{ "the fixture opens: " } + aPath);
		return QString::fromUtf8(file.readAll());
	}

	struct transcribed_element
	{
		QString Kind;
		QString Width;
		QString Data;
	};

	/// \brief The checked-in transcription, label by label, in file order.
	[[nodiscard]] auto transcription() -> std::map<QString, std::vector<transcribed_element>>
	{
		auto result = std::map<QString, std::vector<transcribed_element>>{ };
		for (const auto& line : read_text(BPR_FEEDBACK_GLYPHS).split(QLatin1Char{ '\n' }, Qt::SkipEmptyParts)) {
			if (line.startsWith(QLatin1Char{ '#' })) {
				continue;
			}
			const auto fields = line.split(QLatin1Char{ ' ' });
			expect(fields.size() >= 3, "a transcription line has a label, a kind and data");
			if (fields[1] == QStringLiteral("disc")) {
				result[fields[0]].push_back({ fields[1], { }, fields.mid(2).join(QLatin1Char{ ' ' }) });
				continue;
			}
			result[fields[0]].push_back({ fields[1], fields[2], fields.mid(3).join(QLatin1Char{ ' ' }) });
		}
		return result;
	}

	/// \brief The same elements read straight from the selected design page's
	/// 48 px glyphs, so the transcription itself is checked against the page.
	[[nodiscard]] auto page_glyphs() -> std::map<QString, std::vector<transcribed_element>>
	{
		const auto page   = read_text(BPR_FEEDBACK_SELECTED_PAGE);
		auto       result = std::map<QString, std::vector<transcribed_element>>{ };
		const auto row    = QRegularExpression{ QStringLiteral(R"re(<tr id="(\w+)">.*?<td class="big">(<svg.*?</svg>)</td>)re"),
												QRegularExpression::DotMatchesEverythingOption };
		const auto part   = QRegularExpression{ QStringLiteral(R"re(<(path|circle)([^>]*)/>)re") };
		const auto attr   = QRegularExpression{ QStringLiteral(R"re(([\w-]+)="([^"]*)")re") };
		for (auto rows = row.globalMatch(page); rows.hasNext();) {
			const auto match = rows.next();
			for (auto parts = part.globalMatch(match.captured(2)); parts.hasNext();) {
				const auto element    = parts.next();
				auto       attributes = std::map<QString, QString>{ };
				for (auto found = attr.globalMatch(element.captured(2)); found.hasNext();) {
					const auto pair              = found.next();
					attributes[pair.captured(1)] = pair.captured(2);
				}
				if (element.captured(1) == QStringLiteral("circle")) {
					result[match.captured(1)].push_back({ QStringLiteral("disc"), { }, attributes["cx"] + QLatin1Char{ ' ' } + attributes["cy"] + QLatin1Char{ ' ' } + attributes["r"] });
					continue;
				}
				const auto filled = attributes["fill"] == QStringLiteral("currentColor");
				result[match.captured(1)].push_back({ filled ? QStringLiteral("fill") : QStringLiteral("stroke"),
													  filled ? QStringLiteral("-") : attributes.contains(QStringLiteral("stroke-width")) ? attributes["stroke-width"]
																																		 : QStringLiteral("2.6"),
													  attributes["d"] });
			}
		}
		return result;
	}

	/// \brief Builds a path from SVG data the way the drawing rules map it.
	[[nodiscard]] auto path_of(const QString& aData) -> QPainterPath
	{
		auto       tokens = std::vector<QString>{ };
		const auto token  = QRegularExpression{ QStringLiteral(R"re([MLCZ]|-?[0-9.]+)re") };
		for (auto found = token.globalMatch(aData); found.hasNext();) {
			tokens.push_back(found.next().captured(0));
		}
		auto       path   = QPainterPath{ };
		const auto number = [&tokens](const std::size_t aIndex) { return tokens.at(aIndex).toDouble(); };
		for (std::size_t index = 0U; index < tokens.size();) {
			if (tokens[index] == QStringLiteral("M")) {
				path.moveTo(number(index + 1U), number(index + 2U));
				index += 3U;
			} else if (tokens[index] == QStringLiteral("L")) {
				path.lineTo(number(index + 1U), number(index + 2U));
				index += 3U;
			} else if (tokens[index] == QStringLiteral("C")) {
				path.cubicTo(number(index + 1U), number(index + 2U), number(index + 3U), number(index + 4U), number(index + 5U), number(index + 6U));
				index += 7U;
			} else if (tokens[index] == QStringLiteral("Z")) {
				path.closeSubpath();
				index += 1U;
			} else {
				expect(false, "the path data uses only M, L, C and Z");
			}
		}
		return path;
	}

	[[nodiscard]] auto same_path(const QPainterPath& aLeft, const QPainterPath& aRight) -> bool
	{
		if (aLeft.elementCount() != aRight.elementCount()) {
			return false;
		}
		for (int index = 0; index < aLeft.elementCount(); ++index) {
			const auto left  = aLeft.elementAt(index);
			const auto right = aRight.elementAt(index);
			if (left.type != right.type || std::abs(left.x - right.x) > 1e-9 || std::abs(left.y - right.y) > 1e-9) {
				return false;
			}
		}
		return true;
	}

	constexpr auto labels = std::array{
		core::feedback_label::praise,
		core::feedback_label::nitpick,
		core::feedback_label::suggestion,
		core::feedback_label::issue,
		core::feedback_label::todo,
		core::feedback_label::question,
		core::feedback_label::thought,
		core::feedback_label::chore,
		core::feedback_label::note,
		core::feedback_label::typo,
		core::feedback_label::polish,
		core::feedback_label::quibble,
	};

	[[nodiscard]] auto name_of(const core::feedback_label aLabel) -> QString
	{
		const auto name = core::stable_name(aLabel);
		return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
	}

	void test_the_glyphs_are_the_accepted_path_data()
	{
		const auto written = transcription();
		const auto page    = page_glyphs();
		expect(page.size() == 12U && written.size() == 12U, "the page and the transcription each hold twelve glyphs");
		expect(page.size() == written.size(), "the same labels");
		auto elements = std::size_t{ 0U };
		for (const auto label : labels) {
			const auto  name     = name_of(label);
			const auto& expected = written.at(name);
			const auto& onPage   = page.at(name);
			expect(expected.size() == onPage.size(), "the transcription has the page's element count");
			for (std::size_t index = 0U; index < expected.size(); ++index) {
				expect(expected[index].Kind == onPage[index].Kind && expected[index].Width == onPage[index].Width && expected[index].Data == onPage[index].Data,
					   "each transcribed element equals the selected page's, byte for byte");
			}
			const auto compiled = qt::feedback_label_glyph(label);
			expect(compiled.size() == expected.size(), "the compiled glyph has the accepted element count");
			for (std::size_t index = 0U; index < expected.size(); ++index) {
				const auto& want = expected[index];
				const auto& have = compiled[index];
				++elements;
				if (want.Kind == QStringLiteral("disc")) {
					const auto values = want.Data.split(QLatin1Char{ ' ' });
					expect(have.Kind == qt::feedback_glyph_element_kind::disc && values.size() == 3
							   && have.Centre == QPointF{ values[0].toDouble(), values[1].toDouble() } && have.Radius == values[2].toDouble(),
						   "a disc is the accepted circle");
					continue;
				}
				const auto stroked = want.Kind == QStringLiteral("stroke");
				expect(have.Kind == (stroked ? qt::feedback_glyph_element_kind::stroke : qt::feedback_glyph_element_kind::fill),
					   "each element keeps its kind");
				expect(!stroked || have.PenWidth == want.Width.toDouble(), "a stroke keeps its pen width (2.6, typo's 2.0, chore's 2.2)");
				expect(same_path(have.Path, path_of(want.Data)), "each compiled path equals the accepted data, element by element");
			}
		}
		std::cout << "glyph elements compared: " << elements << '\n';
		expect(elements == 29U, "all 29 elements are compared");
	}

	void test_shortcodes_are_the_contract_table()
	{
		const auto codes = std::array<std::string_view, 12U>{ "pr", "ni", "su", "is", "td", "qu", "th", "ch", "no", "ty", "po", "qb" };
		auto       seen  = std::set<std::string_view>{ };
		for (std::size_t index = 0U; index < labels.size(); ++index) {
			expect(qt::feedback_shortcode(labels[index]) == codes[index], "each label has the contract's code");
			expect(qt::find_feedback_shortcode(codes[index]) == labels[index], "and the code names it");
			seen.insert(codes[index]);
		}
		expect(seen.size() == 12U, "the twelve codes are distinct");
		expect(!qt::find_feedback_shortcode("QU").has_value() && !qt::find_feedback_shortcode("q").has_value() && !qt::find_feedback_shortcode("zz").has_value(),
			   "only an exact lowercase code names a label");
		using L = core::feedback_label;
		expect(qt::feedback_shortcode_matches("q") == std::vector{ L::question, L::quibble }, ":q offers question and quibble");
		expect(qt::feedback_shortcode_matches("t") == std::vector{ L::todo, L::thought, L::typo }, ":t offers the three t codes in catalog order");
		expect(qt::feedback_shortcode_matches("is") == std::vector{ L::issue }, ":is offers issue");
		expect(qt::feedback_shortcode_matches("su") == std::vector{ L::suggestion }, ":su offers suggestion");
		expect(qt::feedback_shortcode_matches("nit") == std::vector{ L::nitpick }, "a name prefix that is not a code is offered after the codes");
		expect(qt::feedback_shortcode_matches("x").empty() && qt::feedback_shortcode_matches("").empty(), "nothing matches x, and an empty fragment offers nothing");
	}

	void test_the_tints_come_from_the_label_roles()
	{
		const auto light = qt::feedback_label_tints_of(qt::builtin_appearance_palette(core::appearance_variant::light));
		const auto dark  = qt::feedback_label_tints_of(qt::builtin_appearance_palette(core::appearance_variant::dark));
		expect(light[3] == QColor{ 0xD1, 0x24, 0x2F } && dark[3] == QColor{ 0xFF, 0x7B, 0x72 }, "issue is #D1242F in light and #FF7B72 in dark");
		expect(light[0] == QColor{ 0xBF, 0x39, 0x89 } && light[11] == QColor{ 0x9A, 0x5B, 0x7A }, "praise and quibble take their own roles");
		for (const auto label : labels) {
			expect(qt::feedback_label_role(label) == core::find_appearance_role(std::string{ "feedback_label_" } + std::string{ core::stable_name(label) }),
				   "each label's role is feedback_label_<id>");
		}
		const auto open     = QColor{ 0xD2, 0x99, 0x22 };
		const auto resolved = QColor{ 0x1A, 0x7F, 0x37 };
		for (const auto variant : { core::appearance_variant::light, core::appearance_variant::dark, core::appearance_variant::high_contrast_light, core::appearance_variant::high_contrast_dark }) {
			for (const auto& tint : qt::feedback_label_tints_of(qt::builtin_appearance_palette(variant))) {
				expect(tint != open && tint != resolved, "no built-in tint is the amber of open or the green of resolved");
			}
		}
	}

	[[nodiscard]] auto render(const core::feedback_label aLabel, const bool aBlocking, const qt::feedback_mark_colours& aColours, const int aScale) -> QImage
	{
		auto image = QImage{ 16 * aScale, 16 * aScale, QImage::Format_ARGB32_Premultiplied };
		image.fill(aColours.Panel);
		auto painter = QPainter{ &image };
		painter.scale(aScale, aScale);
		qt::paint_feedback_mark(painter, QRectF{ 0.0, 0.0, 16.0, 16.0 }, aLabel, aBlocking, aColours);
		painter.end();
		return image;
	}

	[[nodiscard]] auto ratio(const QColor& aFirst, const QColor& aSecond) -> double
	{
		const auto core = [](const QColor& aColour) {
			return core::appearance_color{ .Red = static_cast<std::uint8_t>(aColour.red()), .Green = static_cast<std::uint8_t>(aColour.green()), .Blue = static_cast<std::uint8_t>(aColour.blue()), .Alpha = 255U };
		};
		return qt::contrast_ratio(core(aFirst), core(aSecond));
	}

	void test_marks_are_soft_or_filled_and_legible()
	{
		struct theme
		{
			core::appearance_variant Variant;
			QColor                   Panel;
			QColor                   Ink;
		};
		const auto themes = std::array{
			theme{ core::appearance_variant::light, QColor{ 0xFF, 0xFF, 0xFF }, QColor{ 0x1F, 0x23, 0x28 } },
			theme{ core::appearance_variant::dark, QColor{ 0x24, 0x29, 0x30 }, QColor{ 0xE6, 0xED, 0xF3 } },
			theme{ core::appearance_variant::high_contrast_light, QColor{ 0xFF, 0xFF, 0xFF }, QColor{ 0x00, 0x00, 0x00 } },
			theme{ core::appearance_variant::high_contrast_dark, QColor{ 0x00, 0x00, 0x00 }, QColor{ 0xFF, 0xFF, 0xFF } },
		};
		auto soft    = 99.0;
		auto filled  = 99.0;
		auto renders = std::vector<QImage>{ };
		for (const auto& entry : themes) {
			const auto tints = qt::feedback_label_tints_of(qt::builtin_appearance_palette(entry.Variant));
			for (std::size_t index = 0U; index < labels.size(); ++index) {
				const auto colours = qt::feedback_mark_colours{ .Tint = tints[index], .Panel = entry.Panel, .Ink = entry.Ink };
				const auto fill    = qt::feedback_mark_soft_fill(tints[index], entry.Panel);
				soft               = std::min(soft, ratio(entry.Ink, fill));
				filled             = std::min(filled, ratio(qt::feedback_mark_blocking_ink(tints[index]), tints[index]));
				const auto plain   = render(labels[index], false, colours, 4);
				const auto heavy   = render(labels[index], true, colours, 4);
				expect(QColor{ plain.pixel(32, 1) } == tints[index], "a soft mark's border is its tint");
				expect(QColor{ plain.pixel(7, 32) } == fill, "inside the border a soft mark is the soft fill");
				expect(QColor{ heavy.pixel(7, 32) } == tints[index], "a blocking mark is filled with its tint");
				expect(plain != heavy, "blocking changes the mark");
				if (entry.Variant == core::appearance_variant::light) {
					renders.push_back(render(labels[index], false, { .Tint = QColor{ 0x80, 0x80, 0x80 }, .Panel = entry.Panel, .Ink = entry.Ink }, 4));
				}
			}
			const auto lightPanel = entry.Panel.lightnessF() >= 0.5;
			const auto mixed      = qt::feedback_mark_soft_fill(QColor{ 0xD1, 0x24, 0x2F }, entry.Panel);
			const auto share      = lightPanel ? 0.22 : 0.30;
			expect(std::abs(mixed.red() - (0xD1 * share + entry.Panel.red() * (1.0 - share))) <= 1.0,
				   "the soft fill is 22% of the tint over a light panel and 30% over a dark one");
		}
		std::cout << "minimum glyph contrast: soft " << soft << ", blocking " << filled << '\n';
		expect(soft >= 6.0 && filled >= 4.5, "glyphs keep at least 6:1 on the soft fill and 4.5:1 on the blocking fill in all four variants");
		for (std::size_t first = 0U; first < renders.size(); ++first) {
			for (std::size_t second = first + 1U; second < renders.size(); ++second) {
				expect(renders[first] != renders[second], "in one tint and greyscale the twelve shapes still differ");
			}
		}
		expect(qt::feedback_mark_blocking_ink(QColor{ 0xFF, 0x7B, 0x72 }) == QColor{ 0x11, 0x14, 0x18 } && qt::feedback_mark_blocking_ink(QColor{ 0xA5, 0x00, 0x00 }) == QColor{ 0xFF, 0xFF, 0xFF },
			   "the blocking glyph is white or #111418, whichever contrasts more");
	}

	void test_names_and_subjects_follow_the_annotation()
	{
		const auto blocking = qt::feedback_body_mark_of("issue (blocking): LightScale ignores the view.");
		expect(blocking.Label == core::feedback_label::issue && blocking.Blocking && blocking.Name == QStringLiteral("issue (blocking)"),
			   "an issue (blocking) body has a filled issue mark named issue (blocking)");
		expect(blocking.SubjectOffset == QStringLiteral("issue (blocking): ").size(), "the subject starts after the prefix");
		expect(qt::feedback_post_name(QStringLiteral("tbutler"), blocking) == QStringLiteral("tbutler, issue (blocking)"), "a post is named with its author first");
		const auto several = qt::feedback_body_mark_of("nitpick (if-minor, non-blocking, security): spacing");
		expect(several.Name == QStringLiteral("nitpick (if-minor, non-blocking)") && !several.Blocking,
			   "standard decorations are named in written order, custom ones are left in the text, and only blocking fills");
		const auto strong = qt::feedback_body_mark_of("**Question:** é then more");
		expect(strong.Label == core::feedback_label::question && strong.Name == QStringLiteral("question") && strong.SubjectOffset == QStringLiteral("**Question:** ").size(),
			   "a strong header names the canonical label");
		const auto bom     = std::string_view{ "\xEF\xBB\xBFpraise: \xE2\x9C\x93 nice" };
		const auto unicode = qt::feedback_body_mark_of(bom);
		expect(unicode.Name == QStringLiteral("praise")
				   && QString::fromUtf8(bom.data(), static_cast<qsizetype>(bom.size())).mid(unicode.SubjectOffset).startsWith(QChar{ 0x2713 }),
			   "a UTF-8 subject offset becomes the QString index of the same text");
		for (const auto* const plain : { "Plain text", "custom: not a catalog label", "issue (blocking: broken", "issue: ", "> issue: quoted", "" }) {
			const auto none = qt::feedback_body_mark_of(plain);
			expect(!none.Label.has_value() && none.Name.isEmpty() && none.SubjectOffset == 0, "plain, unknown, malformed and empty-subject bodies get no mark and read as written");
		}
		expect(qt::feedback_post_name(QStringLiteral("aleks"), { }) == QStringLiteral("aleks"), "a plain post is named by its author alone");
		const auto description = core::definition_of(core::feedback_label::issue).Description;
		expect(qt::feedback_label_description(core::feedback_label::issue) == QString::fromUtf8(description.data(), static_cast<qsizetype>(description.size())),
			   "the tooltip text is the catalog description");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_glyphs_are_the_accepted_path_data();
	test_shortcodes_are_the_contract_table();
	test_the_tints_come_from_the_label_roles();
	test_marks_are_soft_or_filled_and_legible();
	test_names_and_subjects_follow_the_annotation();
	std::cout << "feedback-label-marks-tests:pass\n";
	return EXIT_SUCCESS;
}
