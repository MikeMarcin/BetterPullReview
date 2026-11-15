#include "bpr/qt/report_page.hpp"

#include "bpr/qt/agent_reference_links.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		using core::dressed_block;
		using core::dressed_block_kind;
		using core::dressed_item;
		using core::dressed_report;
		using core::dressed_row;
		using core::report_tone;

		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const double aAmount) -> QColor
		{
			const auto blend = [aAmount](const int aA, const int aB) { return static_cast<int>(std::lround(aA + (aB - aA) * aAmount)); };
			return QColor{ blend(aFrom.red(), aTo.red()), blend(aFrom.green(), aTo.green()), blend(aFrom.blue(), aTo.blue()) };
		}

		/// The colours the page derives from the six roles: quieter ink, the
		/// rules, the link and the diff's washes.
		struct palette
		{
			report_colours Roles;
			QString        Ink;
			QString        Dim;
			QString        Faint;
			QString        Line;
			QString        Strong;
			QString        Link;
			QString        Wash;
			QString        Removed;
			QString        Added;
		};

		[[nodiscard]] auto palette_of(const report_colours& aColours) -> palette
		{
			const auto dark = aColours.Paper.lightness() < 128;
			auto       link = dark ? QColor::fromHsl(205, 130, 185) : QColor::fromHsl(205, 135, 60);
			return palette{
				.Roles   = aColours,
				.Ink     = aColours.Ink.name(),
				.Dim     = mixed(aColours.Ink, aColours.Paper, 0.25).name(),
				.Faint   = mixed(aColours.Ink, aColours.Paper, 0.42).name(),
				.Line    = mixed(aColours.Ink, aColours.Paper, 0.82).name(),
				.Strong  = mixed(aColours.Ink, aColours.Paper, 0.55).name(),
				.Link    = link.name(),
				.Wash    = aColours.CodeWash.name(),
				.Removed = mixed(aColours.Open, aColours.CodeWash, dark ? 0.80 : 0.86).name(),
				.Added   = mixed(aColours.Done, aColours.CodeWash, dark ? 0.80 : 0.86).name(),
			};
		}

		[[nodiscard]] auto sans() -> QString { return QStringLiteral("'%1'").arg(QGuiApplication::font().family()); }

		[[nodiscard]] auto mono() -> QString
		{
			return QStringLiteral("'SF Mono', Menlo, '%1', Consolas, 'DejaVu Sans Mono', monospace").arg(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
		}

		[[nodiscard]] auto escaped(const std::string& aText) -> QString { return QString::fromStdString(aText).toHtmlEscaped(); }

		/// Where a place goes: the answer's own link, or one made from it.
		[[nodiscard]] auto place_link(const std::string& aPlace, const std::string& aLink) -> QString
		{
			if (!aLink.empty()) {
				return QString::fromStdString(aLink);
			}
			if (const auto place = core::read_report_place(aPlace)) {
				return agent_reference_link_text({ .Paths = { QString::fromStdString(place->Path) }, .First = place->First, .Last = place->Last });
			}
			return QString::fromStdString(aPlace);
		}

		[[nodiscard]] auto place_html(const std::string& aPlace, const std::string& aLink, const palette& aPalette) -> QString
		{
			return QStringLiteral("<a href=\"%1\" style=\"font-family:%2;font-size:13px;color:%3\">%4</a>")
				.arg(place_link(aPlace, aLink).toHtmlEscaped(), mono(), aPalette.Link, escaped(aPlace));
		}

		/// Emphasis, `**strong**` and `*emphasis*` or `_emphasis_`, outside
		/// code, already escaped.
		[[nodiscard]] auto emphasised(QString aText) -> QString
		{
			static const auto strong   = QRegularExpression{ QStringLiteral(R"(\*\*(\S(?:.*?\S)?)\*\*)") };
			static const auto starred  = QRegularExpression{ QStringLiteral(R"((?<![\w*])\*(\S(?:.*?\S)?)\*(?![\w*]))") };
			static const auto lowlined = QRegularExpression{ QStringLiteral(R"((?<!\w)_(\S(?:.*?\S)?)_(?!\w))") };
			aText.replace(strong, QStringLiteral("<b>\\1</b>"));
			aText.replace(starred, QStringLiteral("<i>\\1</i>"));
			aText.replace(lowlined, QStringLiteral("<i>\\1</i>"));
			return aText;
		}

		/// One line of the report's Markdown as rich text: code spans (a
		/// place is a link), links, strong and emphasis.
		[[nodiscard]] auto inline_html(const std::string& aMarkdown, const palette& aPalette) -> QString
		{
			const auto source = QString::fromStdString(aMarkdown);
			auto       html   = QString{ };
			auto       plain  = QString{ };
			const auto flush  = [&] {
				html += emphasised(plain.toHtmlEscaped());
				plain.clear();
			};
			for (auto at = qsizetype{ 0 }; at < source.size();) {
				// A link: `[words](where)`.
				if (source[at] == QLatin1Char{ '[' }) {
					const auto close = source.indexOf(QStringLiteral("]("), at);
					const auto end   = close < 0 ? -1 : source.indexOf(QLatin1Char{ ')' }, close + 2);
					if (close > at && end > close) {
						flush();
						const auto words = source.mid(at + 1, close - at - 1);
						const auto where = source.mid(close + 2, end - close - 2);
						const auto code  = words.size() > 2 && words.startsWith(QLatin1Char{ '`' }) && words.endsWith(QLatin1Char{ '`' });
						html += QStringLiteral("<a href=\"%1\" style=\"%2color:%3\">%4</a>")
									.arg(where.toHtmlEscaped(), code ? QStringLiteral("font-family:%1;font-size:13px;").arg(mono()) : QString{ }, aPalette.Link,
										 (code ? words.mid(1, words.size() - 2) : words).toHtmlEscaped());
						at = end + 1;
						continue;
					}
				}
				if (source[at] == QLatin1Char{ '`' }) {
					const auto close = source.indexOf(QLatin1Char{ '`' }, at + 1);
					if (close > at + 1) {
						flush();
						const auto code = source.mid(at + 1, close - at - 1);
						if (core::read_report_place(code.toStdString())) {
							html += place_html(code.toStdString(), { }, aPalette);
						} else {
							html += QStringLiteral("<span style=\"font-family:%1;font-size:14px;background-color:%2\">%3</span>").arg(mono(), aPalette.Wash, code.toHtmlEscaped());
						}
						at = close + 1;
						continue;
					}
				}
				plain += source[at] == QLatin1Char{ '\n' } ? QLatin1Char{ ' ' } : source[at];
				++at;
			}
			flush();
			return html;
		}

		[[nodiscard]] auto anchor(const std::uint32_t aOutline) -> QString
		{
			return QStringLiteral("<a name=\"%1%2\"></a>").arg(QString::fromLatin1(report_heading_anchor)).arg(aOutline);
		}

		[[nodiscard]] auto code_html(const dressed_block& aBlock, const palette& aPalette) -> QString
		{
			auto lines = QStringList{ };
			for (auto index = std::size_t{ 0U }; index < aBlock.Lines.size(); ++index) {
				auto line = QString::fromStdString(aBlock.Lines[index]).toHtmlEscaped();
				if (aBlock.FirstLine) {
					line = QStringLiteral("<span style=\"color:%1\">%2</span>  %3").arg(aPalette.Faint).arg(*aBlock.FirstLine + index, 4).arg(line);
				}
				lines << line;
			}
			return QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"8\" style=\"margin-top:10px;border:1px solid %1;border-collapse:collapse\" bgcolor=\"%2\">"
								  "<tr><td><pre style=\"margin:0;font-family:%3;font-size:13px;color:%4\">%5</pre></td></tr></table>")
				.arg(aPalette.Line, aPalette.Wash, mono(), aPalette.Ink, lines.join(QLatin1Char{ '\n' }));
		}

		[[nodiscard]] auto suggestion_html(const dressed_block& aBlock, const dressed_item& aItem, const palette& aPalette, const std::map<std::uint32_t, QString>& aNotes) -> QString
		{
			auto       rows  = QString{ };
			const auto first = aBlock.FirstLine.value_or(0U);
			const auto row   = [&](const QString& aWash, const std::uint32_t aNumber, const QChar aSign, const std::string& aLine) {
				rows += QStringLiteral("<tr><td bgcolor=\"%1\"><pre style=\"margin:0;font-family:%2;font-size:13px;color:%3\"><span style=\"color:%4\">%5 %6</span>  %7</pre></td></tr>")
							.arg(aWash, mono(), aPalette.Ink, aPalette.Faint, first == 0U ? QStringLiteral("    ") : QStringLiteral("%1").arg(aNumber, 4), QString{ aSign },
								 QString::fromStdString(aLine).toHtmlEscaped());
			};
			// Only what changes: the lines both sides share at either end are
			// left out, as a diff shows it.
			const auto& was    = aBlock.Replaced;
			const auto& now    = aBlock.Lines;
			auto        before = std::size_t{ 0U };
			while (!was.empty() && before < was.size() && before < now.size() && was[before] == now[before]) {
				++before;
			}
			auto after = std::size_t{ 0U };
			while (!was.empty() && after < was.size() - before && after < now.size() - before && was[was.size() - 1U - after] == now[now.size() - 1U - after]) {
				++after;
			}
			for (auto index = before; index + after < was.size(); ++index) {
				row(aPalette.Removed, first + static_cast<std::uint32_t>(index), QChar{ 0x2212 }, was[index]);
			}
			for (auto index = before; index + after < now.size(); ++index) {
				row(aPalette.Added, first + static_cast<std::uint32_t>(index), QLatin1Char{ '+' }, now[index]);
			}
			const auto note = aNotes.find(aItem.Outline);
			const auto foot = note != aNotes.end() ? note->second.toHtmlEscaped()
												   : QStringLiteral("<a href=\"%1%2\" style=\"color:%3\">%4</a>&nbsp;&nbsp;&nbsp;%5")
														 .arg(QString::fromLatin1(report_draft_scheme))
														 .arg(aItem.Outline)
														 .arg(aPalette.Link, QObject::tr("Add as draft comment"), QObject::tr("Nothing is published"));
			return QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"3\" style=\"margin-top:10px;border:1px solid %1;border-collapse:collapse\" bgcolor=\"%2\">%3"
								  "<tr><td style=\"border-top:1px solid %1;padding:8px 10px;font-family:%4;font-size:14px;color:%5\">%6</td></tr></table>")
				.arg(aPalette.Line, aPalette.Wash, rows, sans(), aPalette.Faint, foot);
		}

		[[nodiscard]] auto rows_html(const std::vector<dressed_row>& aRows, const palette& aPalette) -> QString
		{
			const auto labelled = std::ranges::any_of(aRows, [](const dressed_row& aRow) { return !aRow.Label.empty(); });
			const auto placed   = std::ranges::any_of(aRows, [](const dressed_row& aRow) { return !aRow.Place.empty(); });
			auto       html     = QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin:4px 0 6px 0;border-collapse:collapse\">");
			for (const auto& row : aRows) {
				const auto cell = QStringLiteral("<td valign=\"top\" style=\"border-top:1px solid %1;border-bottom:1px solid %1;padding:6px 12px 6px 0;%2\">%3</td>");
				html += QStringLiteral("<tr>");
				if (labelled) {
					html += cell.arg(aPalette.Line,
									 QStringLiteral("font-family:%1;font-size:13px;font-weight:600;color:%2;white-space:nowrap")
										 .arg(sans(), row.Tone == report_tone::neutral ? aPalette.Dim : report_tone_colour(row.Tone, aPalette.Roles).name()),
									 escaped(row.Label));
				}
				if (placed) {
					html += cell.arg(aPalette.Line, QString{ }, row.Place.empty() ? QString{ } : place_html(row.Place, row.PlaceLink, aPalette));
				}
				html += cell.arg(aPalette.Line, QStringLiteral("font-size:15px;color:%1").arg(aPalette.Dim), inline_html(row.Text, aPalette));
				html += QStringLiteral("</tr>");
			}
			return html + QStringLiteral("</table>");
		}

		[[nodiscard]] auto blocks_html(const std::vector<dressed_block>& aBlocks, const dressed_item* aItem, const palette& aPalette, const std::map<std::uint32_t, QString>& aNotes) -> QString
		{
			auto html = QString{ };
			for (const auto& block : aBlocks) {
				switch (block.Kind) {
				case dressed_block_kind::paragraph: html += QStringLiteral("<p style=\"margin:8px 0 0 0\">%1</p>").arg(inline_html(block.Text, aPalette)); break;
				case dressed_block_kind::quote:     html += QStringLiteral("<p style=\"margin:0 0 8px 0;font-style:italic;color:%1\">%2</p>").arg(aPalette.Dim, inline_html(block.Text, aPalette)); break;
				case dressed_block_kind::check:
					html += QStringLiteral("<p style=\"margin:10px 0 0 0;font-size:15px;color:%1\"><span style=\"font-family:%2;font-size:13px;font-weight:600;color:%3\">%4</span>&nbsp;&nbsp;%5</p>")
								.arg(aPalette.Dim, sans(), aPalette.Ink, QObject::tr("Check"), inline_html(block.Text, aPalette));
					break;
				case dressed_block_kind::rows: html += rows_html(block.Rows, aPalette); break;
				case dressed_block_kind::code: html += code_html(block, aPalette); break;
				case dressed_block_kind::suggestion:
					html += aItem != nullptr && !aItem->Place.empty() ? suggestion_html(block, *aItem, aPalette, aNotes) : code_html(block, aPalette);
					break;
				}
			}
			return html;
		}

		/// An item: a rule at the left in its verdict's colour, its label and
		/// place above its heading. An area (no label, no place) is a heading
		/// over its rows.
		[[nodiscard]] auto item_html(const dressed_item& aItem, const palette& aPalette, const std::map<std::uint32_t, QString>& aNotes) -> QString
		{
			if (aItem.Label.empty() && aItem.Place.empty()) {
				return QStringLiteral("<p style=\"margin:16px 0 6px 0;font-size:17px;font-weight:600\">%1%2</p>%3")
					.arg(anchor(aItem.Outline), inline_html(aItem.Heading, aPalette), blocks_html(aItem.Body, &aItem, aPalette, aNotes));
			}
			const auto tone = aItem.Tone == report_tone::neutral ? aPalette.Strong : report_tone_colour(aItem.Tone, aPalette.Roles).name();
			const auto word = aItem.Tone == report_tone::neutral ? aPalette.Dim : tone;
			return QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:14px\"><tr><td width=\"3\" bgcolor=\"%1\"></td><td style=\"padding-left:16px\">"
								  "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td style=\"font-family:%2;font-size:13px;font-weight:600;color:%3\">%4</td><td align=\"right\">%5</td></tr></table>"
								  "<p style=\"margin:2px 0 6px 0;font-size:19px;font-weight:600\">%6%7</p>%8</td></tr></table>")
				.arg(tone, sans(), word, escaped(aItem.Label), aItem.Place.empty() ? QString{ } : place_html(aItem.Place, aItem.PlaceLink, aPalette), anchor(aItem.Outline),
					 inline_html(aItem.Heading, aPalette), blocks_html(aItem.Body, &aItem, aPalette, aNotes));
		}

		[[nodiscard]] auto tally_html(const dressed_report& aReport, const palette& aPalette) -> QString
		{
			if (aReport.Tally.empty()) {
				return { };
			}
			auto total = std::uint32_t{ 0U };
			for (const auto& count : aReport.Tally) {
				total += count.Count;
			}
			auto meter = QString{ };
			auto words = QStringList{ };
			for (const auto& count : aReport.Tally) {
				const auto colour = count.Tone == report_tone::neutral ? aPalette.Strong : report_tone_colour(count.Tone, aPalette.Roles).name();
				meter += QStringLiteral("<td width=\"%1%\" bgcolor=\"%2\" style=\"font-size:3px\">&nbsp;</td>").arg(std::max(1U, count.Count * 100U / std::max(1U, total))).arg(colour);
				words << QStringLiteral("<span style=\"font-weight:600;color:%1\">%2</span> %3").arg(count.Tone == report_tone::neutral ? aPalette.Ink : colour).arg(count.Count).arg(escaped(count.Word));
			}
			return QStringLiteral("<table width=\"100%\" cellspacing=\"2\" cellpadding=\"0\" style=\"margin-top:18px\"><tr>%1</tr></table>"
								  "<p style=\"margin:6px 0 0 0;font-family:%2;font-size:14px;color:%3\">%4</p>")
				.arg(meter, sans(), aPalette.Dim, words.join(QStringLiteral(", ")));
		}
	} // namespace

	auto report_tone_colour(const core::report_tone aTone, const report_colours& aColours) -> QColor
	{
		switch (aTone) {
		case report_tone::open:    return aColours.Open;
		case report_tone::partial: return aColours.Partial;
		case report_tone::done:    return aColours.Done;
		case report_tone::neutral: break;
		}
		return mixed(aColours.Ink, aColours.Paper, 0.55);
	}

	auto report_serif_families() -> QString { return QStringLiteral("Charter, 'Iowan Old Style', 'Bitstream Charter', Cambria, Georgia, serif"); }

	auto report_page_html(const core::dressed_report& aReport, const report_colours& aColours, const std::map<std::uint32_t, QString>& aDraftNotes) -> QString
	{
		const auto colours = palette_of(aColours);
		auto       html    = QStringLiteral("<html><body style=\"font-family:%1;font-size:17px;color:%2\">").arg(report_serif_families(), colours.Ink);
		if (!aReport.Summary.empty()) {
			auto paragraphs = QStringList{ };
			for (const auto& part : QString::fromStdString(aReport.Summary).split(QStringLiteral("\n\n"))) {
				paragraphs << QStringLiteral("<p style=\"margin:0;font-size:19px\">%1</p>").arg(inline_html(part.toStdString(), colours));
			}
			html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td width=\"3\" bgcolor=\"%1\"></td><td style=\"padding-left:16px\">%2</td></tr></table>")
						.arg(colours.Strong, paragraphs.join(QString{ }));
		}
		html += tally_html(aReport, colours);
		for (const auto& section : aReport.Sections) {
			if (!section.Name.empty()) {
				html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:30px\"><tr>"
									   "<td style=\"border-top:1px solid %1;padding-top:14px;font-family:%2;font-size:15px;font-weight:600;color:%3\">%4%5</td>"
									   "<td align=\"right\" style=\"border-top:1px solid %1;padding-top:14px;font-family:%2;font-size:15px;color:%6\">%7</td></tr></table>")
							.arg(colours.Line, sans(), colours.Dim, anchor(section.Outline), escaped(section.Name), colours.Faint,
								 section.Items.empty() || std::ranges::all_of(section.Items, [](const dressed_item& aItem) { return aItem.Label.empty() && aItem.Place.empty(); })
									 ? QString{ }
									 : QString::number(section.Items.size()));
			}
			html += blocks_html(section.Intro, nullptr, colours, aDraftNotes);
			for (const auto& item : section.Items) {
				html += item_html(item, colours, aDraftNotes);
			}
		}
		return html + QStringLiteral("</body></html>");
	}
} // namespace bpr::qt
