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
#include <tuple>
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
		expect(qt::feedback_shortcode_matches("x").empty(), "nothing matches x");
		expect(qt::feedback_shortcode_matches("") == std::vector<L>(labels.begin(), labels.end()), "a colon alone offers all twelve in catalog order");
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

	[[nodiscard]] auto render(const core::feedback_label aLabel, const qt::feedback_mark_emphasis aEmphasis, const qt::feedback_mark_colours& aColours, const int aScale) -> QImage
	{
		auto image = QImage{ 16 * aScale, 16 * aScale, QImage::Format_ARGB32_Premultiplied };
		image.fill(aColours.Panel);
		auto painter = QPainter{ &image };
		painter.scale(aScale, aScale);
		qt::paint_feedback_mark(painter, QRectF{ 0.0, 0.0, 16.0, 16.0 }, aLabel, aEmphasis, aColours);
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
				const auto plain   = render(labels[index], qt::feedback_mark_emphasis{ }, colours, 4);
				const auto heavy   = render(labels[index], { .Weight = qt::feedback_mark_weight::blocking }, colours, 4);
				expect(QColor{ plain.pixel(32, 1) } == tints[index], "a soft mark's border is its tint");
				expect(QColor{ plain.pixel(7, 32) } == fill, "inside the border a soft mark is the soft fill");
				expect(QColor{ heavy.pixel(7, 32) } == tints[index], "a blocking mark is filled with its tint");
				expect(plain != heavy, "blocking changes the mark");
				if (entry.Variant == core::appearance_variant::light) {
					renders.push_back(render(labels[index], qt::feedback_mark_emphasis{ }, { .Tint = QColor{ 0x80, 0x80, 0x80 }, .Panel = entry.Panel, .Ink = entry.Ink }, 4));
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
		// [[BPR-1083]]. The owner's second style: the two-letter codes in the
		// same mark, with the same tint, border and blocking fill.
		auto lettered = std::vector<QImage>{ };
		for (const auto label : labels) {
			const auto colours = qt::feedback_mark_colours{ .Tint = QColor{ 0x80, 0x80, 0x80 }, .Panel = QColor{ 0xFF, 0xFF, 0xFF }, .Ink = QColor{ 0x1F, 0x23, 0x28 }, .Style = qt::feedback_mark_style::letters };
			lettered.push_back(render(label, qt::feedback_mark_emphasis{ }, colours, 4));
			const auto blocking = render(label, { .Weight = qt::feedback_mark_weight::blocking }, { .Tint = QColor{ 0xD1, 0x24, 0x2F }, .Panel = QColor{ 0xFF, 0xFF, 0xFF }, .Ink = QColor{ 0x1F, 0x23, 0x28 }, .Style = qt::feedback_mark_style::letters }, 4);
			expect(QColor{ blocking.pixel(7, 32) } == QColor{ 0xD1, 0x24, 0x2F } && QColor{ lettered.back().pixel(32, 1) } == QColor{ 0x80, 0x80, 0x80 },
				   "a letters mark keeps the tinted border and the blocking fill");
		}
		for (std::size_t first = 0U; first < lettered.size(); ++first) {
			expect(lettered[first] != renders[first], "letters differ from the icon");
			for (std::size_t second = first + 1U; second < lettered.size(); ++second) {
				expect(lettered[first] != lettered[second], "and each label's letters differ");
			}
		}
		expect(qt::feedback_mark_blocking_ink(QColor{ 0xFF, 0x7B, 0x72 }) == QColor{ 0x11, 0x14, 0x18 } && qt::feedback_mark_blocking_ink(QColor{ 0xA5, 0x00, 0x00 }) == QColor{ 0xFF, 0xFF, 0xFF },
			   "the blocking glyph is white or #111418, whichever contrasts more");
	}

	/// \brief BPR-1083 v004 (owner, decorations-v003: "I think we keey normal,
	/// blocking, non-blocking sytles for easy scanning. We can also support
	/// if-minor style as just filling in the top left corner."). Three weights,
	/// and the if-minor corner over any of them. Non-blocking is an open mark
	/// with a dashed border. Neither touches a glyph or the letters, and both
	/// read without colour.
	void test_weights_and_the_if_minor_corner()
	{
		using W                 = qt::feedback_mark_weight;
		using E                 = qt::feedback_mark_emphasis;
		constexpr auto normal   = E{ };
		constexpr auto heavy    = E{ .Weight = W::blocking };
		constexpr auto light    = E{ .Weight = W::non_blocking };
		const auto     cornered = [](E aEmphasis) {
			aEmphasis.IfMinor = true;
			return aEmphasis;
		};
		const auto panels = std::array{ std::pair{ core::appearance_variant::light, QColor{ 0xFF, 0xFF, 0xFF } }, std::pair{ core::appearance_variant::dark, QColor{ 0x24, 0x29, 0x30 } },
										std::pair{ core::appearance_variant::high_contrast_light, QColor{ 0xFF, 0xFF, 0xFF } }, std::pair{ core::appearance_variant::high_contrast_dark, QColor{ 0x00, 0x00, 0x00 } } };
		expect(qt::feedback_mark_emphasis_of(true, false, false) == heavy && qt::feedback_mark_emphasis_of(false, true, false) == light && qt::feedback_mark_emphasis_of(false, false, false) == normal
				   && qt::feedback_mark_emphasis_of(true, true, false) == normal && qt::feedback_mark_emphasis_of(false, true, true) == cornered(light) && qt::feedback_mark_emphasis_of(true, true, true) == cornered(normal),
			   "blocking or non-blocking alone is its weight, both or neither normal; if-minor is the corner whatever the weight");
		for (auto code = 0; code < 6; ++code) {
			expect(qt::feedback_mark_emphasis_code(qt::feedback_mark_emphasis_from_code(code)) == code, "an emphasis round-trips through its code");
		}
		for (const auto& [variant, panel] : panels) {
			const auto tints = qt::feedback_label_tints_of(qt::builtin_appearance_palette(variant));
			const auto ink   = panel.lightnessF() < 0.5 ? QColor{ 0xE6, 0xED, 0xF3 } : QColor{ 0x1F, 0x23, 0x28 };
			for (const auto style : { qt::feedback_mark_style::icons, qt::feedback_mark_style::letters }) {
				for (std::size_t index = 0U; index < labels.size(); ++index) {
					const auto colours = qt::feedback_mark_colours{ .Tint = tints[index], .Panel = panel, .Ink = ink, .Style = style };
					const auto soft    = render(labels[index], normal, colours, 4);
					const auto open    = render(labels[index], light, colours, 4);
					// Non-blocking: no fill, so the panel shows inside the border.
					expect(QColor{ open.pixel(7, 32) } == panel && QColor{ soft.pixel(7, 32) } != panel, "a non-blocking mark is open: the panel shows inside its border");
					// Its top edge alternates tint and panel: a dashed border.
					auto runs = 0;
					auto last = QColor{ open.pixel(16, 1) } == tints[index];
					for (auto x = 16; x < 48; ++x) {
						const auto tinted = QColor{ open.pixel(x, 1) } == tints[index];
						runs += tinted != last ? 1 : 0;
						last = tinted;
					}
					expect(runs >= 3 && QColor{ soft.pixel(32, 1) } == tints[index], "its border is broken into dashes along each side");
					for (const auto weight : { normal, heavy, light }) {
						const auto plain = render(labels[index], weight, colours, 4);
						const auto minor = render(labels[index], cornered(weight), colours, 4);
						// The top-left corner squared off in the tint, so the
						// outline differs in greyscale, and filled: in the tint
						// on a soft or open mark, in the glyph's ink on a filled one.
						expect(QColor{ minor.pixel(1, 1) } == tints[index] && QColor{ plain.pixel(1, 1) } == panel, "if-minor squares the top-left corner off in the tint");
						const auto filled = weight == heavy ? qt::feedback_mark_blocking_ink(tints[index]) : tints[index];
						expect(QColor{ minor.pixel(9, 9) } == filled && QColor{ plain.pixel(9, 9) } != filled, "and fills the corner, on every weight");
						expect(QColor{ minor.pixel(7, 32) } == QColor{ plain.pixel(7, 32) }, "and keeps the weight's fill elsewhere");
						// Outside the corner, the glyph or the letters are drawn
						// exactly as without it.
						for (auto y = 8; y < 56; ++y) {
							for (auto x = 8; x < 56; ++x) {
								if (x + y > 6 * 4 + 4) {
									expect(minor.pixel(x, y) == plain.pixel(x, y), "the if-minor corner never touches a glyph or the letters");
								}
							}
						}
					}
					expect(open != soft && render(labels[index], heavy, colours, 4) != soft, "the three weights differ");
				}
			}
		}
		// A sheet of the six treatments in the four variants, at 1x and 3x,
		// when BPR_FEEDBACK_RENDER_DIR names a folder: layout evidence only.
		if (const auto folder = qEnvironmentVariable("BPR_FEEDBACK_RENDER_DIR"); !folder.isEmpty()) {
			for (const auto scale : { 1, 3 }) {
				auto sheet = QImage{ (6 * 3 * 30 + 10) * scale, (4 * 40 + 10) * scale, QImage::Format_ARGB32_Premultiplied };
				sheet.fill(Qt::gray);
				auto painter = QPainter{ &sheet };
				painter.scale(scale, scale);
				for (std::size_t row = 0U; row < panels.size(); ++row) {
					const auto& [variant, panel] = panels[row];
					const auto tints             = qt::feedback_label_tints_of(qt::builtin_appearance_palette(variant));
					const auto ink               = panel.lightnessF() < 0.5 ? QColor{ 0xE6, 0xED, 0xF3 } : QColor{ 0x1F, 0x23, 0x28 };
					const auto top               = 5.0 + (static_cast<double>(row) * 40.0);
					painter.fillRect(QRectF{ 5.0, top, 6.0 * 3.0 * 30.0, 36.0 }, panel);
					auto column = 0;
					for (const auto emphasis : { normal, cornered(normal), heavy, cornered(heavy), light, cornered(light) }) {
						for (const auto label : { core::feedback_label::issue, core::feedback_label::suggestion, core::feedback_label::chore }) {
							const auto tint = tints[static_cast<std::size_t>(label)];
							const auto left = 12.0 + (column * 30.0);
							qt::paint_feedback_rail(painter, left + 2.0, top + 4.0, top + 32.0, tint, emphasis);
							qt::paint_feedback_mark(painter, QRectF{ left + 6.0, top + 10.0, 16.0, 16.0 }, label, emphasis, { .Tint = tint, .Panel = panel, .Ink = ink });
							++column;
						}
					}
				}
				painter.end();
				static_cast<void>(sheet.save(folder + QStringLiteral("/decorations-%1x.png").arg(scale)));
			}
		}
		// Rails: 4 px solid when blocking, 2 px solid, 2 px dashed; if-minor
		// leaves a rail as its weight draws it.
		const auto paint_rail = [](const E aEmphasis) {
			auto image = QImage{ 8, 60, QImage::Format_ARGB32_Premultiplied };
			image.fill(Qt::white);
			auto painter = QPainter{ &image };
			qt::paint_feedback_rail(painter, 6.0, 0.0, 60.0, QColor{ 0xD1, 0x24, 0x2F }, aEmphasis);
			painter.end();
			return image;
		};
		const auto rail = [&paint_rail](const E aEmphasis) {
			const auto image  = paint_rail(aEmphasis);
			auto       inked  = 0;
			auto       gaps   = 0;
			auto       wasInk = true;
			// The rows between the rounded ends.
			for (auto y = 3; y < 57; ++y) {
				const auto ink = QColor{ image.pixel(5, y) }.red() - QColor{ image.pixel(5, y) }.green() > 0x30;
				inked += ink ? 1 : 0;
				gaps += !ink && wasInk ? 1 : 0;
				wasInk = ink;
			}
			const auto wide = QColor{ image.pixel(3, 30) }.red() - QColor{ image.pixel(3, 30) }.green() > 0x30;
			return std::tuple{ inked, gaps, wide };
		};
		const auto [blockingInk, blockingGaps, blockingWide] = rail(heavy);
		const auto [softInk, softGaps, softWide]             = rail(normal);
		const auto [dashInk, dashGaps, dashWide]             = rail(light);
		expect(blockingWide && !softWide && !dashWide && qt::feedback_rail_width(heavy) == 4.0 && qt::feedback_rail_width(cornered(normal)) == 2.0, "only blocking widens the rail to 4 px");
		expect(blockingGaps == 0 && softGaps == 0 && softInk == 54, "blocking and normal rails are solid");
		expect(dashGaps >= 5 && dashInk < softInk, "non-blocking's rail is dashed, lighter than solid");
		for (const auto weight : { normal, heavy, light }) {
			expect(paint_rail(cornered(weight)) == paint_rail(weight), "if-minor does not change a rail");
		}
	}

	void test_names_follow_the_written_decorations()
	{
		using W             = qt::feedback_mark_weight;
		const auto blocking = qt::feedback_body_mark_of("issue (blocking): LightScale ignores the view.");
		expect(blocking.Label == core::feedback_label::issue && blocking.Emphasis == qt::feedback_mark_emphasis{ .Weight = W::blocking } && blocking.Name == QStringLiteral("issue (blocking)"),
			   "an issue (blocking) body has a filled issue mark named issue (blocking)");
		expect(qt::feedback_post_name(QStringLiteral("tbutler"), blocking) == QStringLiteral("tbutler, issue (blocking)"), "a post is named with its author first");
		// [[BPR-1083]] v004: several decorations ride one label and custom
		// ones are an organisation's own; every one is named as written.
		const auto several = qt::feedback_body_mark_of("nitpick (if-minor, non-blocking, security): spacing");
		expect(several.Name == QStringLiteral("nitpick (if-minor, non-blocking, security)") && several.Emphasis == qt::feedback_mark_emphasis{ .Weight = W::non_blocking, .IfMinor = true },
			   "every decoration is named in written order, and non-blocking with if-minor is the open mark with the corner");
		const auto custom = qt::feedback_body_mark_of("note (Security, ux, SECURITY, Blocking): fine");
		expect(custom.Name == QStringLiteral("note (Security, ux, blocking)") && custom.Emphasis == qt::feedback_mark_emphasis{ .Weight = W::blocking },
			   "custom decorations keep their spelling, a repeat is named once, a standard one in its canonical spelling");
		expect(qt::feedback_body_mark_of("nitpick (non-blocking): spacing").Emphasis == qt::feedback_mark_emphasis{ .Weight = W::non_blocking }
				   && qt::feedback_body_mark_of("suggestion (if-minor): rename").Emphasis == qt::feedback_mark_emphasis{ .IfMinor = true }
				   && qt::feedback_body_mark_of("issue (blocking, non-blocking): both").Emphasis == qt::feedback_mark_emphasis{ }
				   && qt::feedback_body_mark_of("todo: add a test").Emphasis == qt::feedback_mark_emphasis{ }
				   && qt::feedback_body_mark_of("note (security): fine").Emphasis == qt::feedback_mark_emphasis{ },
			   "each weight and the corner follow the written kinds; guidance and custom decorations have no style");
		const auto strong = qt::feedback_body_mark_of("**Question:** é then more");
		expect(strong.Label == core::feedback_label::question && strong.Name == QStringLiteral("question"), "a strong header names the canonical label");
		expect(qt::feedback_body_mark_of("\xEF\xBB\xBFpraise: \xE2\x9C\x93 nice").Name == QStringLiteral("praise"), "a BOM before the label is read as the recognizer reads it");
		for (const auto* const plain : { "Plain text", "custom: not a catalog label", "issue (blocking: broken", "issue: ", "> issue: quoted", "" }) {
			const auto none = qt::feedback_body_mark_of(plain);
			expect(!none.Label.has_value() && none.Name.isEmpty(), "plain, unknown, malformed and empty-subject bodies get no mark");
		}
		expect(qt::feedback_post_name(QStringLiteral("aleks"), { }) == QStringLiteral("aleks"), "a plain post is named by its author alone");
		expect(qt::feedback_mark_emphasis_words({ .Weight = W::non_blocking, .IfMinor = true }) == QStringLiteral(" (non-blocking, if-minor)") && qt::feedback_mark_emphasis_words({ }).isEmpty(),
			   "the preview's words for an emphasis");
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
	test_weights_and_the_if_minor_corner();
	test_names_follow_the_written_decorations();
	std::cout << "feedback-label-marks-tests:pass\n";
	return EXIT_SUCCESS;
}
