#include "bpr/qt/report_page.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/markdown_code.hpp"
#include "bpr/qt/agent_reference_links.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QObject>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// The slots the page being written collects its code blocks into.
		std::vector<report_code_slot>* Collected = nullptr;
		/// The page being written's keep actions.
		const report_page_actions* Actions = nullptr;
		/// The page being written's look.
		report_look Look = report_look::cards;

		[[nodiscard]] auto code_marker(report_code_slot aSlot) -> QString
		{
			if (Collected == nullptr) {
				return { };
			}
			Collected->push_back(std::move(aSlot));
			return QStringLiteral("<p style=\"margin:10px 0 0 0\">%1%2</p>").arg(QString::fromLatin1(report_code_marker)).arg(Collected->size() - 1U);
		}

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
			/// [[BPR-2219]], round 2. The ground a band or a card's head
			/// stands on: the paper, a little towards the ink.
			QString Raised;
		};

		[[nodiscard]] auto palette_of(const report_colours& aColours) -> palette
		{
			// [[BPR-2184]] (review N6): the quieter inks, the rules and the link
			// are core's, derived from the paper and ink and held to the
			// contrast floor there.
			const auto to_core = [](const QColor& aColour) {
				return core::appearance_color{ .Red = static_cast<std::uint8_t>(aColour.red()), .Green = static_cast<std::uint8_t>(aColour.green()), .Blue = static_cast<std::uint8_t>(aColour.blue()), .Alpha = 255U };
			};
			const auto to_qt                                              = [](const core::appearance_color aColour) { return QColor{ aColour.Red, aColour.Green, aColour.Blue }; };
			auto       roles                                              = core::appearance_palette{ };
			roles[core::index_of(core::appearance_role::code_background)] = to_core(aColours.Paper);
			roles[core::index_of(core::appearance_role::syntax_normal)]   = to_core(aColours.Ink);
			const auto shades                                             = core::derive_report_shades(roles);
			const auto dark                                               = aColours.Paper.lightness() < 128;
			return palette{
				.Roles   = aColours,
				.Ink     = aColours.Ink.name(),
				.Dim     = to_qt(shades.Dim).name(),
				.Faint   = to_qt(shades.Faint).name(),
				.Line    = to_qt(shades.Line).name(),
				.Strong  = to_qt(shades.Strong).name(),
				.Link    = to_qt(shades.Link).name(),
				.Wash    = aColours.CodeWash.name(),
				.Removed = mixed(aColours.Tones.Open, aColours.CodeWash, dark ? 0.80 : 0.86).name(),
				.Added   = mixed(aColours.Tones.Done, aColours.CodeWash, dark ? 0.80 : 0.86).name(),
				.Raised  = mixed(aColours.Paper, aColours.Ink, dark ? 0.09 : 0.05).name(),
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

		/// [[BPR-2219]]. A button-shaped link of the page's own text: a
		/// bordered cell, so Tab and Return reach it as they reach any link.
		/// [[BPR-2268]] (owner, 2026-11-18: "Draft comment button in a report
		/// doesn't look good when the description text to its right line
		/// wraps"): the bordered cell is a table of its own in a top-aligned
		/// cell, its words never wrapped, so it keeps its one line's height
		/// and width however many lines the words beside it take.
		[[nodiscard]] auto chip(const QString& aLink, const QString& aWords, const QString& aInk, const palette& aPalette) -> QString
		{
			return QStringLiteral("<td valign=\"top\"><table cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse:collapse\"><tr>"
								  "<td style=\"border:1px solid %1;background-color:%2;padding:4px 10px;white-space:nowrap\"><a href=\"%3\" style=\"text-decoration:none;font-weight:600;color:%4\">%5</a></td>"
								  "</tr></table></td>")
				.arg(aPalette.Strong, aPalette.Wash, aLink.toHtmlEscaped(), aInk, aWords.toHtmlEscaped());
		}

		/// [[BPR-2268]]. Words beside a chip: top-aligned, their first line
		/// level with the chip's (its border and padding, 5 px, above it).
		[[nodiscard]] auto beside(const QString& aText, const QString& aColour, const bool aStrong = false) -> QString
		{
			return QStringLiteral("<td valign=\"top\" style=\"padding:5px 12px 4px 0;color:%1;%2\">%3</td>").arg(aColour, aStrong ? QStringLiteral("font-weight:600;") : QString{ }, aText.toHtmlEscaped());
		}

		/// A fenced block as the page's own HTML code box.
		[[nodiscard]] auto code_box_html(const dressed_block& aBlock, const palette& aPalette) -> QString
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

		/// \brief [[BPR-2352]] (owner, 2026-11-21: "The code block didn't
		/// appear in the suggested comment"). A suggested comment as the page
		/// shows it, after `aLabel`: its words as written, and each fenced
		/// block, a fix in the file's language or a suggestion, as the page's
		/// own code box. The boxes stay page HTML, not collected code views:
		/// the plain box the owner accepted, without Copy or Wrap.
		/// \details The finding's Suggested change, drawn above from the
		/// same lines (`aDrawn`), is the comment's `suggestion` block already:
		/// the removed and added lines, numbered. That block is not drawn a
		/// second time as a plain box of its added lines (the owner, of an
		/// updated comment written out under its finding: "It's just
		/// restating the thing that's drawn above it"). The label says the
		/// change goes with the words, so the comment does not read as words
		/// only. A fix in the file's language is not a Suggested change and
		/// keeps its box: nothing above says the comment carries it.
		[[nodiscard]] auto comment_html(const QString& aComment, const std::vector<std::string>* const aDrawn, const palette& aPalette) -> QString
		{
			// Which lines a comment suggests is core's reading.
			const auto withAbove = aDrawn != nullptr && core::comment_suggestion_lines(aComment.toStdString()) == *aDrawn;
			auto       above     = withAbove;
			auto       html      = QString{ };
			auto       lead      = (withAbove ? QObject::tr("Suggested comment, with the suggested change above:") : QObject::tr("Suggested comment:")).toHtmlEscaped() + QLatin1Char{ ' ' };
			auto       words     = QStringList{ };
			auto       fence     = QString{ };
			auto       code      = dressed_block{ .Kind = dressed_block_kind::code };
			// Whether the comment shows anything of its own here.
			auto       shown     = false;
			const auto paragraph = [&] {
				const auto text = words.join(QLatin1Char{ '\n' }).trimmed();
				words.clear();
				if (text.isEmpty() && lead.isEmpty()) {
					return;
				}
				shown = shown || !text.isEmpty();
				html += QStringLiteral("<p style=\"margin-top:6px;font-family:%1;font-size:13px;color:%2;white-space:pre-wrap\">%3<span style=\"color:%4\">%5</span></p>")
							.arg(sans(), aPalette.Faint, std::exchange(lead, QString{ }), aPalette.Ink, text.toHtmlEscaped());
			};
			// A closed block: its box, or nothing for the one drawn above.
			const auto box = [&] {
				if (above && core::markdown_code_language_of(code.Language).Kind == core::markdown_code_kind::suggestion && code.Lines == *aDrawn) {
					above = false;
					return;
				}
				shown = true;
				html += code_box_html(code, aPalette);
			};
			for (const auto& line : aComment.split(QLatin1Char{ '\n' })) {
				const auto bare = line.trimmed();
				if (fence.isEmpty()) {
					const auto mark = bare.isEmpty() ? QChar{ } : bare.front();
					auto       run  = qsizetype{ 0 };
					while (run < bare.size() && (mark == QLatin1Char{ '`' } || mark == QLatin1Char{ '~' }) && bare[run] == mark) {
						++run;
					}
					auto indent = qsizetype{ 0 };
					while (indent < line.size() && line[indent] == QLatin1Char{ ' ' }) {
						++indent;
					}
					if (run >= 3 && indent <= 3) {
						paragraph();
						fence         = bare.left(run);
						code.Language = bare.mid(run).trimmed().toStdString();
						code.Lines.clear();
						continue;
					}
					words << line;
					continue;
				}
				if (bare.startsWith(fence) && bare.count(fence.front()) == bare.size()) {
					box();
					fence.clear();
					continue;
				}
				code.Lines.push_back(line.toStdString());
			}
			if (!fence.isEmpty()) {
				box();
			}
			paragraph();
			if (withAbove && !shown) {
				// A comment that is its suggestion and no words.
				return QStringLiteral("<p style=\"margin-top:6px;font-family:%1;font-size:13px;color:%2\">%3</p>").arg(sans(), aPalette.Faint, QObject::tr("The suggested comment is the suggested change above.").toHtmlEscaped());
			}
			return html;
		}

		/// One keep action's row: the chip and what it does, or what became
		/// of it. `aOutline` 0 is the summary's.
		/// `aDrawn`: the lines of the Suggested change the finding draws
		/// above the row, when it draws one.
		[[nodiscard]] auto keep_row(const std::uint32_t aOutline, const core::report_keep aKeep, const QString& aWhere, const bool aGoes, const palette& aPalette, const std::vector<std::string>* const aDrawn = nullptr)
			-> QString
		{
			if (Actions == nullptr || !Actions->Link || aKeep == core::report_keep::none) {
				return { };
			}
			const auto found  = Actions->States.find(aOutline);
			const auto state  = found == Actions->States.end() ? report_keep_state{ } : found->second;
			const auto draft  = aKeep == core::report_keep::draft_comment;
			const auto spacer = QStringLiteral("<td width=\"12\"></td>");
			const auto unsent = QObject::tr("Nothing is published");
			auto       cells  = QString{ };
			// [[BPR-2286]]. Ask follow-up, beside a finding's keep action.
			const auto ask = Actions->FollowUp && aOutline != 0U ? chip(Actions->Link(aOutline, report_link::ask), QObject::tr("Ask follow-up"), aPalette.Ink, aPalette) + spacer : QString{ };
			// [[BPR-2331]]. Dismiss, beside a finding's keep action, as its
			// Agent tab card had it.
			const auto dismiss  = Actions->Dismiss && aOutline != 0U ? chip(Actions->Link(aOutline, report_link::dismiss), QObject::tr("Dismiss"), aPalette.Ink, aPalette) + spacer : QString{ };
			const auto outdated = draft && state.Phase == report_keep_phase::kept && std::ranges::find(Actions->Outdated, aOutline) != Actions->Outdated.end();
			switch (state.Phase) {
			case report_keep_phase::working: cells = beside(draft ? QObject::tr("Drafting\u2026") : QObject::tr("Keeping\u2026"), aPalette.Faint, false); break;
			case report_keep_phase::kept:
				// [[BPR-2376]]. A draft the reviewer changed before a
				// follow-up updated the comment is kept and offers the update.
				// Its row is its three controls, and what it is and why are
				// said under them, whole at any width: beside them, in a
				// report narrowed by its side chat, "Drafted" and the
				// sentence had a letter's width each.
				if (!outdated) {
					cells = beside(draft ? QObject::tr("Drafted") : QObject::tr("Kept"), aPalette.Roles.Tones.Done.name(), true);
				}
				if (aGoes) {
					cells += chip(Actions->Link(aOutline, report_link::go), draft ? QObject::tr("Go to draft") : QObject::tr("Go to comment"), aPalette.Link, aPalette) + spacer;
				}
				if (outdated) {
					cells += chip(Actions->Link(aOutline, report_link::update_draft), QObject::tr("Update draft"), aPalette.Ink, aPalette) + spacer + ask;
					break;
				}
				cells += ask;
				// [[BPR-2331]]. What the draft says of its lines, when it says
				// something ([[BPR-2298]]), as its card said it.
				cells += beside(!state.Why.isEmpty() ? state.Why
									: aOutline == 0U ? QObject::tr("A local comment on the pull request. %1").arg(unsent)
													 : unsent,
								aPalette.Faint, false);
				break;
			case report_keep_phase::dismissed:
				cells = beside(QObject::tr("Dismissed"), aPalette.Dim, true) + chip(Actions->Link(aOutline, report_link::undo_dismiss), QObject::tr("Undo"), aPalette.Ink, aPalette) + spacer;
				cells += beside(QObject::tr("Nothing was made"), aPalette.Faint, false);
				break;
			case report_keep_phase::refused:
			case report_keep_phase::offered:
				cells = chip(Actions->Link(aOutline, report_link::keep), aOutline == 0U ? QObject::tr("Keep summary") : draft ? QObject::tr("Draft comment")
																															  : QObject::tr("Keep local"),
							 aPalette.Ink, aPalette)
					+ spacer + dismiss + ask;
				cells += state.Phase == report_keep_phase::refused ? beside(state.Why, aPalette.Roles.Tones.Open.name(), false) : beside(QStringLiteral("%1. %2").arg(aWhere, unsent), aPalette.Faint, false);
				break;
			}
			// [[BPR-2331]]. The comment Draft comment drafts, as the agent
			// suggested it and its card showed it.
			// The owner, of a comment a follow-up updated, shown here again
			// under its finding: "This updated comment section at the bottom
			// is more confusing than helpful. It's just restating the thing
			// that's drawn above it." The finding above is drawn from the
			// update already (its Suggested change, as the core revised it),
			// so neither comment is written out again: one line says what
			// Draft comment drafts now, or that the draft made earlier
			// differs from it.
			auto under = QString{ };
			if (outdated || Actions->Revised.contains(aOutline)) {
				const auto open = state.Phase == report_keep_phase::offered || state.Phase == report_keep_phase::refused;
				const auto said = outdated ? QObject::tr("The draft differs from the updated suggested comment. %1").arg(unsent)
					: !open                ? QString{ }
					: draft                ? QObject::tr("Draft comment drafts the comment as the follow-up updated it.")
										   : QObject::tr("Keep local keeps the comment as the follow-up updated it.");
				if (!said.isEmpty()) {
					const auto lead = outdated ? QStringLiteral("<span style=\"font-weight:600;color:%1\">%2</span> ").arg(aPalette.Roles.Tones.Done.name(), QObject::tr("Drafted.").toHtmlEscaped()) : QString{ };
					under           = QStringLiteral("<p style=\"margin-top:6px;font-family:%1;font-size:13px;color:%2\">%3%4</p>").arg(sans(), outdated ? aPalette.Roles.Tones.Open.name() : aPalette.Faint, lead, said.toHtmlEscaped());
				}
			} else if (const auto suggested = Actions->Suggested.find(aOutline); suggested != Actions->Suggested.end()) {
				under = comment_html(suggested->second, aDrawn, aPalette);
			}
			return QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:12px;border-collapse:collapse;font-family:%1;font-size:13px\"><tr>%2</tr></table>").arg(sans(), cells) + under;
		}

		/// [[BPR-2232]]. Draft all from the report: the chip while it has a
		/// finding to draft, and what it is doing or last did.
		[[nodiscard]] auto draft_all_row(const palette& aPalette) -> QString
		{
			if (Actions == nullptr || !Actions->Link || (Actions->DraftAll == 0U && !Actions->DraftAllBusy && Actions->DraftAllSaid.isEmpty() && !Actions->DraftAllUndo)) {
				return { };
			}
			auto cells = QString{ };
			if (Actions->DraftAll > 0U && !Actions->DraftAllBusy) {
				cells += chip(Actions->Link(0U, report_link::draft_all), QObject::tr("Draft all (%1)").arg(Actions->DraftAll), aPalette.Ink, aPalette) + QStringLiteral("<td width=\"12\"></td>");
			}
			cells += Actions->DraftAllSaid.isEmpty() ? beside(QObject::tr("Each finding not yet drafted, at its lines. Nothing is published"), aPalette.Faint)
													 : beside(Actions->DraftAllSaid, Actions->DraftAllBusy ? aPalette.Faint : aPalette.Dim);
			// [[BPR-2331]]. Its Undo, beside what it did, as the Agent tab's.
			if (Actions->DraftAllUndo && !Actions->DraftAllBusy) {
				cells += chip(Actions->Link(0U, report_link::undo_draft_all), QObject::tr("Undo"), aPalette.Ink, aPalette);
			}
			return QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:12px;border-collapse:collapse;font-family:%1;font-size:13px\"><tr>%2</tr></table>").arg(sans(), cells);
		}

		/// The keep action of `aItem`, when it is a finding.
		[[nodiscard]] auto finding_row(const dressed_item& aItem, const palette& aPalette) -> QString
		{
			if (Actions == nullptr) {
				return { };
			}
			const auto aOutline = aItem.Outline;
			const auto found    = std::ranges::find(Actions->Findings, aOutline, &core::report_finding::Outline);
			if (found == Actions->Findings.end()) {
				return { };
			}
			const auto file  = QString::fromStdString(found->Path).section(QLatin1Char{ '/' }, -1);
			const auto where = found->First == 0U ? (found->Path.empty() ? QObject::tr("as a local comment on the pull request") : QObject::tr("as a local comment on %1").arg(file))
				: found->First == found->Last     ? QObject::tr("at %1:%2").arg(file).arg(found->First)
												  : QObject::tr("at %1:%2-%3").arg(file).arg(found->First).arg(found->Last);
			// What `blocks_html` drew as the item's Suggested change: a
			// suggestion under a place.
			const auto drawn = aItem.Place.empty() ? aItem.Body.end() : std::ranges::find(aItem.Body, dressed_block_kind::suggestion, &dressed_block::Kind);
			return keep_row(aOutline, found->Keep, where, !found->Path.empty(), aPalette, drawn != aItem.Body.end() ? &drawn->Lines : nullptr);
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

		[[nodiscard]] auto code_html(const dressed_block& aBlock, const dressed_item* aItem, const palette& aPalette) -> QString
		{
			if (Collected != nullptr) {
				// [[BPR-2351]]. Code under a finding's place is anchored to
				// that file, so it can read with the file's names.
				const auto place = aItem != nullptr ? core::read_report_place(aItem->Place) : std::nullopt;
				return code_marker({ .Language = aBlock.Language, .Lines = aBlock.Lines, .FirstLine = aBlock.FirstLine, .Path = place ? place->Path : std::string{ } });
			}
			return code_box_html(aBlock, aPalette);
		}

		[[nodiscard]] auto suggestion_html(const dressed_block& aBlock, const dressed_item& aItem, const palette& aPalette) -> QString
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
			if (Collected != nullptr) {
				// The suggested change as the Agent tab shows one. Its keep
				// action stands at the finding's foot ([[BPR-2219]]).
				const auto place = core::read_report_place(aItem.Place);
				return code_marker({ .Language   = aBlock.Language,
									 .Lines      = aBlock.Lines,
									 .FirstLine  = aBlock.FirstLine,
									 .Suggestion = true,
									 .Replaced   = aBlock.Replaced,
									 .Path       = place ? place->Path : std::string{ } });
			}
			return QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"3\" style=\"margin-top:10px;border:1px solid %1;border-collapse:collapse\" bgcolor=\"%2\">%3</table>").arg(aPalette.Line, aPalette.Wash, rows);
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
				html += cell.arg(aPalette.Line, QStringLiteral("font-size:13px;color:%1").arg(aPalette.Dim), inline_html(row.Text, aPalette));
				html += QStringLiteral("</tr>");
			}
			return html + QStringLiteral("</table>");
		}

		[[nodiscard]] auto blocks_html(const std::vector<dressed_block>& aBlocks, const dressed_item* aItem, const palette& aPalette) -> QString
		{
			auto html = QString{ };
			for (const auto& block : aBlocks) {
				switch (block.Kind) {
				case dressed_block_kind::paragraph: html += QStringLiteral("<p style=\"margin:8px 0 0 0\">%1</p>").arg(inline_html(block.Text, aPalette)); break;
				case dressed_block_kind::quote:     html += QStringLiteral("<p style=\"margin:0 0 8px 0;font-style:italic;color:%1\">%2</p>").arg(aPalette.Dim, inline_html(block.Text, aPalette)); break;
				case dressed_block_kind::check:
					html += QStringLiteral("<p style=\"margin:10px 0 0 0;font-size:13px;color:%1\"><span style=\"font-family:%2;font-size:13px;font-weight:600;color:%3\">%4</span>&nbsp;&nbsp;%5</p>")
								.arg(aPalette.Dim, sans(), aPalette.Ink, QObject::tr("Check"), inline_html(block.Text, aPalette));
					break;
				case dressed_block_kind::rows: html += rows_html(block.Rows, aPalette); break;
				case dressed_block_kind::code: html += code_html(block, aItem, aPalette); break;
				case dressed_block_kind::suggestion:
					html += aItem != nullptr && !aItem->Place.empty() ? suggestion_html(block, *aItem, aPalette) : code_html(block, aItem, aPalette);
					break;
				}
			}
			return html;
		}

		/// An item: a rule at the left in its verdict's colour, its label and
		/// place above its heading. An area (no label, no place) is a heading
		/// over its rows.
		[[nodiscard]] auto item_html(const dressed_item& aItem, const palette& aPalette) -> QString
		{
			if (aItem.Label.empty() && aItem.Place.empty()) {
				return QStringLiteral("<p style=\"margin:16px 0 6px 0;font-size:14px;font-weight:600\">%1%2</p>%3")
					.arg(anchor(aItem.Outline), inline_html(aItem.Heading, aPalette), blocks_html(aItem.Body, &aItem, aPalette) + finding_row(aItem, aPalette));
			}
			const auto tone = aItem.Tone == report_tone::neutral ? aPalette.Strong : report_tone_colour(aItem.Tone, aPalette.Roles).name();
			const auto word = aItem.Tone == report_tone::neutral ? aPalette.Dim : tone;
			// Round 2 (owner: "feel more native but also still special"): in
			// the cards look a finding is a bordered box, as a conversation
			// card is, its verdict's rule at its left.
			const auto box = Look == report_look::cards ? QStringLiteral("margin-top:12px;border:1px solid %1;border-collapse:collapse").arg(aPalette.Line) : QStringLiteral("margin-top:14px");
			const auto pad = Look == report_look::cards ? QStringLiteral("padding:10px 14px 12px 14px") : QStringLiteral("padding-left:16px");
			// The box is made first: a finding's own words may hold a marker.
			const auto open = QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"%1\"><tr><td width=\"3\" bgcolor=\"%2\"></td><td style=\"%3\">").arg(box, tone, pad);
			return open
				+ QStringLiteral(
					  "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td style=\"font-family:%2;font-size:13px;font-weight:600;color:%3\">%4</td><td align=\"right\">%5</td></tr></table>"
					  "<p style=\"margin:2px 0 6px 0;font-size:15px;font-weight:600\">%6%7</p>%8</td></tr></table>")
					  .arg(sans(), word, escaped(aItem.Label), aItem.Place.empty() ? QString{ } : place_html(aItem.Place, aItem.PlaceLink, aPalette), anchor(aItem.Outline),
						   inline_html(aItem.Heading, aPalette),
						   (aItem.OtherPlaces.empty() ? QString{ }
													  : QStringLiteral("<p style=\"margin:0 0 6px 0;font-family:%1;font-size:13px;color:%2\">%3 %4</p>").arg(sans(), aPalette.Dim, QObject::tr("Also"), inline_html(aItem.OtherPlaces, aPalette)))
							   + blocks_html(aItem.Body, &aItem, aPalette) + finding_row(aItem, aPalette));
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

	auto tone_colours_of(const appearance_profile& aProfile) -> tone_colours
	{
		using role = core::appearance_role;
		return { .Open = aProfile.color(role::report_verdict_open), .Partial = aProfile.color(role::report_verdict_partial), .Done = aProfile.color(role::report_verdict_done) };
	}

	auto builtin_tone_colours(const bool aDark) -> tone_colours
	{
		const auto& palette = builtin_appearance_palette(aDark ? core::appearance_variant::dark : core::appearance_variant::light);
		const auto  of      = [&palette](const core::appearance_role aRole) {
			const auto colour = palette[core::index_of(aRole)];
			return QColor{ colour.Red, colour.Green, colour.Blue };
		};
		return { .Open = of(core::appearance_role::report_verdict_open), .Partial = of(core::appearance_role::report_verdict_partial), .Done = of(core::appearance_role::report_verdict_done) };
	}

	auto tone_colour(const core::report_tone aTone, const tone_colours& aColours) -> std::optional<QColor>
	{
		switch (aTone) {
		case report_tone::open:    return aColours.Open;
		case report_tone::partial: return aColours.Partial;
		case report_tone::done:    return aColours.Done;
		case report_tone::neutral: break;
		}
		return std::nullopt;
	}

	auto report_tone_colour(const core::report_tone aTone, const report_colours& aColours) -> QColor
	{
		return tone_colour(aTone, aColours.Tones).value_or(QColor{ palette_of(aColours).Strong });
	}

	auto report_link_colour(const report_colours& aColours) -> QColor { return QColor{ palette_of(aColours).Link }; }

	auto mark_report_inline_code(QTextDocument& aDocument) -> std::size_t
	{
		// Found first, then marked: a merge rewrites the fragments.
		auto runs = std::vector<std::pair<int, int>>{ };
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				const auto format   = fragment.charFormat();
				if (!format.isAnchor() && !format.fontFixedPitch() && format.fontFamilies().toStringList().contains(QStringLiteral("monospace"))) {
					runs.emplace_back(fragment.position(), fragment.position() + fragment.length());
				}
			}
		}
		auto fixed = QTextCharFormat{ };
		fixed.setFontFixedPitch(true);
		auto cursor = QTextCursor{ &aDocument };
		for (const auto& [from, to] : runs) {
			cursor.setPosition(from);
			cursor.setPosition(to, QTextCursor::KeepAnchor);
			cursor.mergeCharFormat(fixed);
		}
		return runs.size();
	}

	auto report_page_html(const core::dressed_report& aReport, const report_colours& aColours, const report_page_actions& aActions, std::vector<report_code_slot>* aSlots, const report_look aLook) -> QString
	{
		Collected          = aSlots;
		Actions            = &aActions;
		Look               = aLook;
		const auto reset   = qScopeGuard([] {
			Collected = nullptr;
			Actions   = nullptr;
		});
		const auto colours = palette_of(aColours);
		auto       html    = QStringLiteral("<html><body style=\"font-family:%1;font-size:14px;color:%2\">").arg(sans(), colours.Ink);
		// [[BPR-2219]]. The verdict over the summary, whose rule wears its
		// colour.
		const auto verdict = aReport.Verdict.empty() || aReport.VerdictTone == report_tone::neutral ? colours.Strong : report_tone_colour(aReport.VerdictTone, colours.Roles).name();
		const auto banded  = Look != report_look::plain;
		auto       opening = QString{ };
		if (!aReport.Verdict.empty()) {
			// Plain: the verdict as a coloured word. Band and cards: a filled
			// mark, the app's own way of saying a state.
			opening += banded ? QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\" style=\"margin:0 0 8px 0\"><tr><td style=\"font-family:%1;font-size:12px;color:%2;padding:3px 10px 3px 0\">%3</td>"
											   "<td style=\"background-color:%4;color:%5;font-family:%1;font-size:12px;font-weight:600;padding:3px 9px\">%6</td></tr></table>")
									.arg(sans(), colours.Dim, QObject::tr("Verdict"), aReport.VerdictTone == report_tone::neutral ? colours.Strong : verdict, colours.Roles.Paper.name(), escaped(aReport.Verdict))
							  : QStringLiteral("<p style=\"margin:0 0 8px 0;font-family:%1;font-size:13px;font-weight:600;color:%2\">%3&nbsp;&nbsp;<span style=\"font-size:14px;color:%4\">%5</span></p>")
									.arg(sans(), colours.Dim, QObject::tr("Verdict"), aReport.VerdictTone == report_tone::neutral ? colours.Ink : verdict, escaped(aReport.Verdict));
		}
		if (!aReport.Summary.empty()) {
			auto paragraphs = QStringList{ };
			for (const auto& part : QString::fromStdString(aReport.Summary).split(QStringLiteral("\n\n"))) {
				paragraphs << QStringLiteral("<p style=\"margin:0;font-size:15px\">%1</p>").arg(inline_html(part.toStdString(), colours));
			}
			opening += paragraphs.join(QString{ });
			opening += aActions.Summary ? keep_row(0U, core::report_keep::keep_local, QObject::tr("as a local comment on the pull request"), false, colours) : QString{ };
		}
		opening += draft_all_row(colours);
		if (!opening.isEmpty()) {
			// The opening: on a raised band with the verdict's rule (band,
			// cards), or beside a rule alone (plain).
			html += banded ? QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"%1\" style=\"border:1px solid %2;border-collapse:collapse\"><tr><td width=\"3\" bgcolor=\"%3\"></td>"
											"<td style=\"padding:12px 14px 12px 14px\">%4</td></tr></table>")
								 .arg(colours.Raised, colours.Line, verdict, opening)
						   : QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td width=\"3\" bgcolor=\"%1\"></td><td style=\"padding-left:16px\">%2</td></tr></table>").arg(verdict, opening);
		}
		html += tally_html(aReport, colours);
		// [[BPR-2376]]. A finding a follow-up updated shows the Suggested
		// change its updated comment makes, as the core revised it.
		auto revisions = std::vector<core::report_revision>{ };
		for (const auto& [outline, comment] : aActions.Revised) {
			revisions.push_back({ .Outline = outline, .Comment = comment.toStdString() });
		}
		const auto  revised = revisions.empty() ? std::optional<core::dressed_report>{ } : std::optional{ core::revise_report(aReport, revisions) };
		const auto& shown   = revised ? *revised : aReport;
		for (const auto& section : shown.Sections) {
			if (!section.Name.empty()) {
				html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:30px\"><tr>"
									   "<td style=\"border-top:1px solid %1;padding-top:14px;font-family:%2;font-size:13px;font-weight:600;color:%3\">%4%5</td>"
									   "<td align=\"right\" style=\"border-top:1px solid %1;padding-top:14px;font-family:%2;font-size:13px;color:%6\">%7</td></tr></table>")
							.arg(colours.Line, sans(), colours.Dim, anchor(section.Outline), escaped(section.Name), colours.Faint,
								 section.Items.empty() || std::ranges::all_of(section.Items, [](const dressed_item& aItem) { return aItem.Label.empty() && aItem.Place.empty(); })
									 ? QString{ }
									 : QString::number(section.Items.size()));
			}
			html += blocks_html(section.Intro, nullptr, colours);
			for (const auto& item : section.Items) {
				html += item_html(item, colours);
			}
		}
		return html + QStringLiteral("</body></html>");
	}
} // namespace bpr::qt
