#include "bpr/qt/review_cycle_share_sheet.hpp"

#include "bpr/core/review_cycle_suggestion.hpp"
#include "bpr/core/suggestion_text_editing.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/review_cycle_share_marks.hpp"
#include "bpr/qt/suggestion_editor.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		auto text_of(const std::string& aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		auto round_text(const core::cycle_round_id aRound) -> QString
		{
			return QStringLiteral("F%1").arg(aRound.value());
		}

		/// "1,414 bytes": grouped the same way whatever the system locale.
		auto grouped(const std::uint64_t aValue) -> QString
		{
			auto digits = QString::number(aValue);
			for (auto at = digits.size() - 3; at > 0; at -= 3) {
				digits.insert(at, QLatin1Char(','));
			}
			return digits;
		}

		auto bytes_text(const std::uint64_t aBytes) -> QString
		{
			return grouped(aBytes) + (aBytes == 1U ? QStringLiteral(" byte") : QStringLiteral(" bytes"));
		}

		auto count_words(const std::uint64_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return QStringLiteral("%1 %2").arg(grouped(aCount), aCount == 1U ? aOne : aMany);
		}

		/// "one", "two" … for the small counts a sentence reads best with.
		auto spelled(const std::uint64_t aCount) -> QString
		{
			static const auto words = std::array{ QStringLiteral("no"), QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three"), QStringLiteral("four"), QStringLiteral("five"),
												  QStringLiteral("six"), QStringLiteral("seven"), QStringLiteral("eight"), QStringLiteral("nine") };
			return aCount < words.size() ? words.at(static_cast<std::size_t>(aCount)) : grouped(aCount);
		}

		auto route_label(const core::cycle_share_route aRoute, const core::cycle_round_id aRound) -> QString
		{
			switch (aRoute) {
			case core::cycle_share_route::copy_document:  return QStringLiteral("Copy round %1").arg(aRound.value());
			case core::cycle_share_route::save_document:  return QStringLiteral("Save as File…");
			case core::cycle_share_route::save_bundle:    return QStringLiteral("Save as Bundle…");
			case core::cycle_share_route::external_agent: return QStringLiteral("Use in External Agent…");
			case core::cycle_share_route::send_to_agent:  return QStringLiteral("Send to Agent…");
			}
			return { };
		}

		/// When to use the route: said beside Copy and Save, and as every route's tooltip.
		auto route_use(const core::cycle_share_route aRoute) -> QString
		{
			switch (aRoute) {
			case core::cycle_share_route::copy_document:  return QStringLiteral("Paste it into your agent's chat, an issue or a message. This is the usual way.");
			case core::cycle_share_route::save_document:  return QStringLiteral("Keep a Markdown file to attach, check in or hand over.");
			case core::cycle_share_route::save_bundle:    return QStringLiteral("Save the text, the exact round and its selected source as one bundle, for a tool that reads bundles.");
			case core::cycle_share_route::external_agent: return QStringLiteral("Let your own harness read this round through the local interface. Nothing is sent or started.");
			case core::cycle_share_route::send_to_agent:  return QStringLiteral("Hand this round to the installed agent adapter.");
			}
			return { };
		}

		auto refused_words(const core::cycle_authorization aDecision) -> QString
		{
			switch (aDecision) {
			case core::cycle_authorization::channel_denied:       return QStringLiteral("Not allowed for this project's source by policy; no other way is used instead.");
			case core::cycle_authorization::channel_unconfigured: return QStringLiteral("Not allowed until it is configured for source of unknown origin.");
			default:                                              return QStringLiteral("Not allowed (%1).").arg(QString::fromLatin1(core::name(aDecision).data(), static_cast<qsizetype>(core::name(aDecision).size())));
			}
		}

		/// [[BPR-1180]]. Why the host refuses an agent, as its list says it.
		auto send_refusal_text(const core::agent_invocation_verdict& aVerdict) -> QString
		{
			if (aVerdict.Refusal == core::agent_invocation_refusal::profile_forbidden_on_host || aVerdict.Refusal == core::agent_invocation_refusal::profile_not_required_one) {
				return QStringLiteral("Not allowed by the host this review answers to; no other agent is used instead.");
			}
			const auto name = core::stable_name(aVerdict.Refusal);
			return QStringLiteral("Not allowed (%1).").arg(QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())));
		}

		/// [[BPR-1282]]. An agent as its list names it: by its harness, the
		/// two Codex protocols told apart, a custom harness by its own name.
		auto offer_label(const core::cycle_send_offer& aOffer) -> QString
		{
			switch (aOffer.Protocol) {
			case core::agent_protocol::codex_app_server:   return QStringLiteral("%1 (app server)").arg(QString::fromStdString(aOffer.Harness));
			case core::agent_protocol::codex_jsonl_exec:   return QStringLiteral("%1 (exec)").arg(QString::fromStdString(aOffer.Harness));
			case core::agent_protocol::claude_stream_json:
			case core::agent_protocol::external_terminal:  break;
			}
			return QString::fromStdString(aOffer.Harness);
		}

		/// The folder's own name; the whole path is in the details.
		auto folder_name(const std::string& aFolder) -> QString
		{
			const auto name = QDir::fromNativeSeparators(QString::fromStdString(aFolder)).section(QLatin1Char('/'), -1, -1, QString::SectionSkipEmpty);
			return name.isEmpty() ? QStringLiteral("the cycle's folder") : name;
		}

		/// [[BPR-1180]], shortened by [[BPR-1282]]. What pressing Send does,
		/// in one line, naming the harness; sessions only for a harness that
		/// has them.
		auto send_summary(const core::cycle_send_offer& aOffer, const std::optional<std::string>& aSession, const std::string& aFolder) -> QString
		{
			const auto harness = QString::fromStdString(aOffer.Harness);
			const auto folder  = folder_name(aFolder);
			if (aSession) {
				return QStringLiteral("Continues %1 session %2 in %3.").arg(harness, QString::fromStdString(*aSession), folder);
			}
			return aOffer.Continues ? QStringLiteral("Starts a new %1 session in %2.").arg(harness, folder) : QStringLiteral("Runs %1 in %2.").arg(harness, folder);
		}

		/// The rest, behind Details: the exact text, whose settings, no
		/// credential, no fallback, and the whole folder.
		auto send_detail_words(const core::cycle_send_offer& aOffer, const std::string& aFolder, const core::cycle_round_id aRound) -> QString
		{
			const auto harness = QString::fromStdString(aOffer.Harness);
			const auto folder  = aFolder.empty() ? QStringLiteral("the cycle's folder") : QDir::toNativeSeparators(QString::fromStdString(aFolder));
			return QStringLiteral("Sends round %1's text exactly as Copy gives it. %2 uses its own settings and sign-in; no credential is passed. If the send fails, nothing else is tried.\nFolder: %3")
				.arg(aRound.value())
				.arg(harness, folder);
		}

		constexpr auto all_actions = std::array{ core::cycle_round_action::discuss, core::cycle_round_action::plan, core::cycle_round_action::implement };

		auto action_label(const core::cycle_round_action aAction) -> QString
		{
			switch (aAction) {
			case core::cycle_round_action::discuss:   return QStringLiteral("Discuss");
			case core::cycle_round_action::plan:      return QStringLiteral("Plan");
			case core::cycle_round_action::implement: return QStringLiteral("Implement");
			}
			return { };
		}

		auto action_meaning(const core::cycle_round_action aAction) -> QString
		{
			switch (aAction) {
			case core::cycle_round_action::discuss:   return QStringLiteral("Answer the notes; change nothing.");
			case core::cycle_round_action::plan:      return QStringLiteral("Reply with a plan before changing anything.");
			case core::cycle_round_action::implement: return QStringLiteral("Make the changes the notes describe.");
			}
			return { };
		}

		auto lines_words(const core::cycle_line_span& aSpan) -> QString
		{
			return aSpan.First == aSpan.Last ? QStringLiteral("line %1").arg(aSpan.First) : QStringLiteral("lines %1–%2").arg(aSpan.First).arg(aSpan.Last);
		}

		/// Where a note attaches, with its lines once its code was read.
		auto where_words(const core::cycle_feedback_target& aTarget, const application::review_cycle_note_context* aContext) -> QString
		{
			if (const auto* anchor = std::get_if<core::cycle_source_anchor>(&aTarget)) {
				if (aContext != nullptr && aContext->Excerpt.Lines) {
					return QStringLiteral("%1, %2").arg(text_of(anchor->Path), lines_words(*aContext->Excerpt.Lines));
				}
				return text_of(anchor->Path);
			}
			if (const auto* file = std::get_if<core::cycle_file_target>(&aTarget)) {
				return QStringLiteral("%1, whole file").arg(text_of(file->Path));
			}
			if (const auto* module = std::get_if<core::cycle_module_target>(&aTarget)) {
				return QStringLiteral("Folder %1/").arg(text_of(module->Path));
			}
			return QStringLiteral("The whole project");
		}

		auto about_a_file(const core::cycle_feedback_target& aTarget) -> bool
		{
			return std::holds_alternative<core::cycle_source_anchor>(aTarget) || std::holds_alternative<core::cycle_file_target>(aTarget);
		}

		auto kind_words(const application::review_cycle_note_entry& aNote) -> QString
		{
			if (aNote.Suggestions == 0U) {
				return QStringLiteral("Note");
			}
			return aNote.Suggestions == 1U ? QStringLiteral("Suggested change") : QStringLiteral("%1 suggested changes").arg(aNote.Suggestions);
		}

		auto first_line(const std::string& aBody) -> QString
		{
			return text_of(aBody).section(QLatin1Char('\n'), 0, 0).trimmed();
		}

		/// The rest of a note's text after its first line.
		auto rest_of(const std::string& aBody) -> QString
		{
			const auto text = text_of(aBody);
			const auto at   = text.indexOf(QLatin1Char('\n'));
			return at < 0 ? QString{ } : text.mid(at + 1).trimmed();
		}

		/// What the code says about itself, below it.
		auto code_facts(const application::review_cycle_note_context& aContext) -> QString
		{
			const auto  path     = text_of(aContext.Path);
			const auto  revision = aContext.Revision ? QStringLiteral("R%1").arg(aContext.Revision->value()) : QString{ };
			const auto& excerpt  = aContext.Excerpt;
			switch (aContext.Status) {
			case application::review_material_status::available:       break;
			case application::review_material_status::missing:         return QStringLiteral("%1 is not in %2 as this note saw it, so its code is not shown.").arg(path, revision);
			case application::review_material_status::too_large:       return QStringLiteral("%1 is larger than 16 MiB, so its code is not shown here. The exact text has the note.").arg(path);
			case application::review_material_status::cancelled:       return QStringLiteral("Reading %1 stopped.").arg(path);
			case application::review_material_status::wrong_authority:
			case application::review_material_status::corrupt:
			case application::review_material_status::failure:         return QStringLiteral("%1 could not be read at %2, so its code is not shown.").arg(path, revision);
			}
			auto words = QStringList{ };
			auto first = std::optional<std::uint64_t>{ };
			auto last  = std::optional<std::uint64_t>{ };
			for (const auto& row : excerpt.Rows) {
				if (row.Kind != core::cycle_excerpt_row_kind::added && row.Line) {
					first = first ? std::min(*first, *row.Line) : *row.Line;
					last  = last ? std::max(*last, *row.Line) : *row.Line;
				}
			}
			if (first && last) {
				words.append(QStringLiteral("%1 at %2, lines %3–%4 of %5.").arg(path, revision, QString::number(*first), QString::number(*last), grouped(excerpt.FileLines)));
			}
			if (excerpt.Cut) {
				words.append(QStringLiteral("More of it is in the whole file."));
			}
			if (aContext.Elsewhere > 0U) {
				words.append(QStringLiteral("%1 to another file %2 in the exact text.")
								 .arg(count_words(aContext.Elsewhere, QStringLiteral("suggested change"), QStringLiteral("suggested changes")), aContext.Elsewhere == 1U ? QStringLiteral("is") : QStringLiteral("are")));
			}
			if (excerpt.Unplaced > 0U) {
				words.append(QStringLiteral("%1 no longer %2 this file at %3 and %4 not shown here.")
								 .arg(count_words(excerpt.Unplaced, QStringLiteral("suggested change"), QStringLiteral("suggested changes")), excerpt.Unplaced == 1U ? QStringLiteral("matches") : QStringLiteral("match"), revision,
									  excerpt.Unplaced == 1U ? QStringLiteral("is") : QStringLiteral("are")));
			}
			return words.join(QLatin1Char(' '));
		}

		/// One observation's parts: its round, what was seen, and its size.
		struct delivery_parts
		{
			QString Round;
			QString What;
			QString Size;
		};

		/// One observation in its own words: what was seen, never more (accepted as
		/// BPR-1161-delivery-words-v001, 2026-10-02; kept verbatim).
		auto delivery_words(const core::cycle_delivery_record& aRecord) -> delivery_parts
		{
			const auto& seen   = aRecord.Observation;
			const auto  bundle = seen.Output && seen.Output->Format == core::cycle_round_file_kind::bundle;
			auto        what   = QString{ };
			switch (seen.State) {
			case core::cycle_delivery_state::copied:                 what = QStringLiteral("Copied to the clipboard"); break;
			case core::cycle_delivery_state::saved:                  what = QStringLiteral("Saved %1as %2").arg(bundle ? QStringLiteral("a bundle ") : QString{ }, text_of(seen.Destination)); break;
			case core::cycle_delivery_state::retrieved:              what = seen.Channel == core::cycle_disclosure_channel::terminal_output ? QStringLiteral("Printed in a terminal") : QStringLiteral("Read through the local interface"); break;
			case core::cycle_delivery_state::accepted_by_adapter:    what = QStringLiteral("Accepted by the agent adapter"); break;
			case core::cycle_delivery_state::recipient_acknowledged: what = QStringLiteral("The recipient acknowledged receiving it"); break;
			case core::cycle_delivery_state::failed:                 what = seen.Channel == core::cycle_disclosure_channel::clipboard ? QStringLiteral("Copy failed: there was no clipboard") : QStringLiteral("Delivery failed"); break;
			case core::cycle_delivery_state::uncertain:              what = seen.Channel == core::cycle_disclosure_channel::clipboard ? QStringLiteral("Copy not confirmed by the clipboard") : QStringLiteral("Delivery not confirmed"); break;
			}
			return { .Round = round_text(seen.Round.Id), .What = what, .Size = seen.Output ? bytes_text(seen.Output->Bytes) : QString{ } };
		}

		auto delivery_text(const core::cycle_delivery_record& aRecord) -> QString
		{
			const auto parts = delivery_words(aRecord);
			return QStringLiteral("%1 · %2%3").arg(parts.Round, parts.What, parts.Size.isEmpty() ? QString{ } : QStringLiteral(" · %1").arg(parts.Size));
		}

		/// [[BPR-1285]]. An observation's glyph and the tone that colours it:
		/// bytes that left here, bytes that went on, not confirmed, failed.
		auto delivery_mark(const core::cycle_delivery_record& aRecord) -> std::pair<share_glyph, share_tone>
		{
			const auto& seen = aRecord.Observation;
			switch (seen.State) {
			case core::cycle_delivery_state::copied: return { share_glyph::copied, share_tone::local };
			case core::cycle_delivery_state::saved:
				return { seen.Output && seen.Output->Format == core::cycle_round_file_kind::bundle ? share_glyph::bundle : share_glyph::save, share_tone::local };
			case core::cycle_delivery_state::retrieved:
				return { seen.Channel == core::cycle_disclosure_channel::terminal_output ? share_glyph::terminal : share_glyph::read, share_tone::onward };
			case core::cycle_delivery_state::accepted_by_adapter:    return { share_glyph::handoff, share_tone::onward };
			case core::cycle_delivery_state::recipient_acknowledged: return { share_glyph::acknowledged, share_tone::onward };
			case core::cycle_delivery_state::failed:                 return { share_glyph::failed, share_tone::refused };
			case core::cycle_delivery_state::uncertain:              return { share_glyph::uncertain, share_tone::warning };
			}
			return { share_glyph::uncertain, share_tone::warning };
		}

		/// The parts a delivery line paints; its text stays the whole sentence.
		constexpr int delivery_round_role = Qt::UserRole + 3;
		constexpr int delivery_what_role  = Qt::UserRole + 4;
		constexpr int delivery_size_role  = Qt::UserRole + 5;
		constexpr int delivery_times_role = Qt::UserRole + 6;
		/// The tone, so new role colours can paint the line again.
		constexpr int delivery_tone_role = Qt::UserRole + 7;

		/// \brief [[BPR-1285]]. A delivery line as marks: the round in an
		/// outline, the state's glyph in its role colour, the words, then the
		/// size and any repeat count quietly. The item's text, its accessible
		/// name and its tooltip stay the whole sentence in the accepted words.
		class delivery_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto* const widget = option.widget;
				auto* const       style  = widget != nullptr ? widget->style() : QApplication::style();
				style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, aPainter, widget);
				aPainter->save();
				aPainter->setRenderHint(QPainter::Antialiasing, true);
				aPainter->setFont(option.font);
				const auto metrics = QFontMetrics{ option.font };
				const auto box     = option.rect.adjusted(6, 0, -6, 0);
				const auto middle  = box.center().y();
				const auto quiet   = option.palette.color(QPalette::PlaceholderText);
				const auto ink     = option.palette.color(QPalette::Text);
				auto       x       = box.left();
				// The round, in a quiet outline.
				const auto round = aIndex.data(delivery_round_role).toString();
				const auto pill  = QRectF{ static_cast<qreal>(x), middle - (metrics.height() + 2) / 2.0, static_cast<qreal>(metrics.horizontalAdvance(round) + 12), static_cast<qreal>(metrics.height() + 2) };
				aPainter->setPen(QPen{ quiet, 1.0 });
				aPainter->setBrush(Qt::NoBrush);
				aPainter->drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), pill.height() / 2.0, pill.height() / 2.0);
				aPainter->drawText(pill, Qt::AlignCenter, round);
				x = static_cast<int>(pill.right()) + 8;
				// The state's glyph, in its role colour.
				const auto glyph = metrics.height();
				const auto mark  = static_cast<share_glyph>(aIndex.data(review_cycle_share_sheet::delivery_glyph_role).toInt());
				const auto tone  = aIndex.data(review_cycle_share_sheet::delivery_colour_role).value<QColor>();
				aPainter->drawPixmap(QPointF{ static_cast<qreal>(x), middle - glyph / 2.0 }, share_glyph_pixmap(mark, tone, glyph, aPainter->device()->devicePixelRatioF()));
				x += glyph + 6;
				// The words, then the size and the repeat count, quiet.
				auto tail = aIndex.data(delivery_size_role).toString();
				if (const auto times = aIndex.data(delivery_times_role).toInt(); times > 1) {
					tail += (tail.isEmpty() ? QString{ } : QStringLiteral("   ")) + QStringLiteral("×%1").arg(times);
				}
				const auto tailWidth = tail.isEmpty() ? 0 : metrics.horizontalAdvance(tail) + 12;
				const auto words     = metrics.elidedText(aIndex.data(delivery_what_role).toString(), Qt::ElideMiddle, std::max(0, box.right() - x - tailWidth));
				aPainter->setPen(ink);
				aPainter->drawText(QRect{ x, box.top(), box.right() - x, box.height() }, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, words);
				if (!tail.isEmpty()) {
					aPainter->setPen(quiet);
					aPainter->drawText(QRect{ x + metrics.horizontalAdvance(words) + 12, box.top(), tailWidth, box.height() }, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, tail);
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				const auto metrics = QFontMetrics{ aOption.font };
				return QSize{ QStyledItemDelegate::sizeHint(aOption, aIndex).width(), metrics.height() + 10 };
			}
		};

		auto refusal_text(const core::cycle_round_refusal& aRefusal) -> QString
		{
			switch (aRefusal.Problem) {
			case core::cycle_round_problem::empty:         return QStringLiteral("Include at least one note to share.");
			case core::cycle_round_problem::archived_item: return QStringLiteral("A note in this round is archived; leave it out to share.");
			case core::cycle_round_problem::unportable_path:
				return QStringLiteral("A note in this round is on a file whose name a round cannot carry; leave it out to share.");
			case core::cycle_round_problem::oversized: return QStringLiteral("This round is over its size budget; leave notes out, or share them as rounds you choose.");
			default:                                   return QStringLiteral("This round cannot be shared as it stands (%1).").arg(QString::fromLatin1(core::stable_name(aRefusal.Problem).data(), static_cast<qsizetype>(core::stable_name(aRefusal.Problem).size())));
			}
		}

		/// What the sheet says once its round was shared: the last thing seen
		/// happen to it, never more.
		auto shared_title(const application::review_cycle_share_view& aShare) -> QString
		{
			const auto number = aShare.Round.value();
			for (auto record = aShare.Deliveries.rbegin(); record != aShare.Deliveries.rend(); ++record) {
				const auto& seen = record->Observation;
				if (seen.Round.Id != aShare.Round) {
					continue;
				}
				if (seen.State == core::cycle_delivery_state::copied) {
					return QStringLiteral("Round %1 is on your clipboard").arg(number);
				}
				if (seen.State == core::cycle_delivery_state::saved) {
					return QStringLiteral("Round %1 is saved as %2").arg(number).arg(text_of(seen.Destination));
				}
				break;
			}
			return QStringLiteral("Round %1 is fixed as you shared it").arg(number);
		}

		constexpr auto all_routes = std::array{ core::cycle_share_route::copy_document, core::cycle_share_route::save_document, core::cycle_share_route::save_bundle,
												core::cycle_share_route::external_agent, core::cycle_share_route::send_to_agent };

		auto row_fill(const core::cycle_excerpt_row_kind aKind, const code_role_colors& aColours, const QPalette& aPalette) -> QColor
		{
			switch (aKind) {
			case core::cycle_excerpt_row_kind::removed: return code_sample_row_fill(code_sample_row_kind::removed, aColours);
			case core::cycle_excerpt_row_kind::added:   return code_sample_row_fill(code_sample_row_kind::added, aColours);
			case core::cycle_excerpt_row_kind::noted:   {
				// The review panes' current line; the palette's highlight, faint, before an appearance applies.
				auto colour = code_sample_row_fill(code_sample_row_kind::current_line, aColours);
				if (!colour.isValid()) {
					colour = aPalette.color(QPalette::Highlight);
					colour.setAlpha(48);
				}
				return colour;
			}
			case core::cycle_excerpt_row_kind::context:
			case core::cycle_excerpt_row_kind::omitted: break;
			}
			return { };
		}

		auto mark_of(const core::cycle_excerpt_row_kind aKind) -> QChar
		{
			switch (aKind) {
			case core::cycle_excerpt_row_kind::noted:   return QChar{ 0x2022 };
			case core::cycle_excerpt_row_kind::removed: return QLatin1Char('-');
			case core::cycle_excerpt_row_kind::added:   return QLatin1Char('+');
			case core::cycle_excerpt_row_kind::context:
			case core::cycle_excerpt_row_kind::omitted: break;
			}
			return QLatin1Char(' ');
		}

		/// The width of the line-number column: the widest number shown.
		auto gutter_digits(const core::cycle_note_excerpt& aExcerpt) -> int
		{
			auto widest = std::uint64_t{ 1 };
			for (const auto& row : aExcerpt.Rows) {
				widest = std::max(widest, row.Line.value_or(1U));
			}
			return static_cast<int>(QString::number(widest).size());
		}

		/// Control-C cancels on macOS, as in the code view (BPR-1157); elsewhere Ctrl+C is Copy.
		void cancel_with_control_c([[maybe_unused]] QDialog& aDialog)
		{
#ifdef Q_OS_MACOS
			auto* const keys = new QShortcut{ QKeySequence{ Qt::META | Qt::Key_C }, &aDialog };
			keys->setContext(Qt::WindowShortcut);
			QObject::connect(keys, &QShortcut::activated, &aDialog, [&aDialog] { aDialog.reject(); });
#endif
		}

		constexpr auto inline_rows = static_cast<int>(core::cycle_excerpt_rows) + 1;

		/// Tabs as spaces to the review panes' tab stops, counted from the code's
		/// first column, so the line number before it moves no stop.
		auto expand_tabs(const QString& aText, const std::uint32_t aWidth) -> QString
		{
			if (!aText.contains(QLatin1Char('\t'))) {
				return aText;
			}
			auto       expanded = QString{ };
			const auto width    = static_cast<qsizetype>(aWidth > 0U ? aWidth : core::default_suggestion_tab_width);
			for (const auto character : aText) {
				if (character == QLatin1Char('\t')) {
					expanded.append(QString(width - expanded.size() % width, QLatin1Char(' ')));
				} else {
					expanded.append(character);
				}
			}
			return expanded;
		}

		/// \brief A note's "Show Change" control: words in the link colour, at the
		/// trailing end of the note's second line, and no glyph.
		/// \details The owner read v003's arrow beside the include box as a
		/// second check mark (BPR-1161, event 921). Words say what a click does,
		/// and the place keeps it apart from the box. It is painted from the
		/// palette at paint time, so a palette change needs no refresh. A hover
		/// underlines it; the sheet's focus ring shows keyboard focus.
		class note_code_toggle final : public QToolButton
		{
		public:
			explicit note_code_toggle(QWidget* aParent)
				: QToolButton{ aParent }
			{
				setCheckable(true);
				setAutoRaise(true);
				setToolButtonStyle(Qt::ToolButtonTextOnly);
				setCursor(Qt::PointingHandCursor);
				setAttribute(Qt::WA_Hover);
				connect(this, &QToolButton::toggled, this, [this](const bool aOpen) { setText(aOpen ? Open : Closed); });
			}

			/// Draws the words in the font of `aLine`, the label whose line they end.
			void set_line(QWidget* const aLine)
			{
				Line = aLine;
				remeasure();
			}

			/// What it says while the note is closed ("Show Change") and open ("Hide Change").
			void set_words(const QString& aClosed, const QString& aOpen)
			{
				// [[BPR-1274]]. The same words again change nothing: every row is
				// filled again on each refresh, and a new measure lays out the list.
				if (aClosed == Closed && aOpen == Open) {
					return;
				}
				Closed = aClosed;
				Open   = aOpen;
				setText(isChecked() ? Open : Closed);
				remeasure();
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				// Measured once per font and words: the layout asks for it often.
				if (!Measured) {
					const auto metrics = QFontMetrics{ words_font() };
					// Room for the wider of the two words, so opening moves nothing.
					const auto width = std::max(metrics.horizontalAdvance(Closed), metrics.horizontalAdvance(Open));
					Measured         = QSize{ width + 4, metrics.height() + 2 };
				}
				return *Measured;
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return sizeHint();
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				auto face    = words_font();
				face.setUnderline(underMouse() && isEnabled());
				painter.setFont(face);
				const auto ink = !isEnabled() ? palette().color(QPalette::PlaceholderText) : isDown() ? palette().color(QPalette::Link).darker(130)
																									  : palette().color(QPalette::Link);
				painter.setPen(ink);
				painter.drawText(rect().adjusted(2, 0, -2, 0), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, text());
			}

			auto event(QEvent* aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::HoverEnter || aEvent->type() == QEvent::HoverLeave) {
					update();
				} else if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::ParentChange) {
					remeasure();
					update();
				}
				return QToolButton::event(aEvent);
			}

		private:
			void remeasure()
			{
				Measured.reset();
				updateGeometry();
			}

			/// The font of the line the words end: macOS gives tool buttons, and
			/// plain widgets, a smaller font than its labels.
			[[nodiscard]] auto words_font() const -> QFont
			{
				auto face = Line != nullptr ? Line->font() : font();
				// Every property explicit, or the painter fills them from the button's own font.
				face.setResolveMask(QFont::AllPropertiesResolved);
				return face;
			}

			QPointer<QWidget>            Line;
			QString                      Closed;
			QString                      Open;
			mutable std::optional<QSize> Measured;
		};
	}

	note_code_view::note_code_view(QWidget* aParent)
		: QPlainTextEdit{ aParent }
	{
		setReadOnly(true);
		setLineWrapMode(QPlainTextEdit::NoWrap);
		setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		setUndoRedoEnabled(false);
	}

	void note_code_view::set_code_font(const QFont& aFont, const std::uint32_t aTabWidth)
	{
		// [[BPR-1161]]. The panes' font through the shared code view's helper: on
		// Windows a generic monospace family otherwise resolves to a proportional face.
		const auto font = fixed_pitch_code_font(aFont);
		setFont(font);
		const auto width = aTabWidth > 0U ? aTabWidth : core::default_suggestion_tab_width;
		setTabStopDistance(QFontMetricsF{ font }.horizontalAdvance(QLatin1Char(' ')) * width);
		if (width != TabWidth) {
			TabWidth = width;
			if (!Empty) {
				// The excerpt's tabs are expanded at this width, so show it again.
				Empty = true;
				show_excerpt(core::cycle_note_excerpt{ Shown });
			}
		}
	}

	void note_code_view::set_colours(const code_role_colors& aColours)
	{
		Colours = aColours;
		paint_bands();
	}

	void note_code_view::show_excerpt(const core::cycle_note_excerpt& aExcerpt, const std::string& aPath)
	{
		if (!Empty && aExcerpt == Shown && aPath == Path) {
			return;
		}
		Shown            = aExcerpt;
		Path             = aPath;
		Empty            = false;
		const auto n     = gutter_digits(Shown);
		auto       lines = QStringList{ };
		lines.reserve(static_cast<qsizetype>(Shown.Rows.size()));
		// [[BPR-1161]]. The two documents the rows are classified in: each row's
		// code as shown, so a colour lands on the column it was found at.
		Documents  = { };
		Classified = { };
		Places.assign(Shown.Rows.size(), std::nullopt);
		Requested = false;
		for (std::size_t index = 0; index < Shown.Rows.size(); ++index) {
			const auto& row = Shown.Rows[index];
			if (row.Kind == core::cycle_excerpt_row_kind::omitted) {
				lines.append(QStringLiteral("%1   ⋯ %2").arg(QString{ n, QLatin1Char(' ') }, count_words(row.Omitted, QStringLiteral("line not shown"), QStringLiteral("lines not shown"))));
				for (auto& document : Documents) {
					document.push_back('\n');
				}
				continue;
			}
			const auto code = expand_tabs(text_of(row.Text), TabWidth);
			lines.append(QStringLiteral("%1 %2 %3").arg(row.Line ? QString::number(*row.Line) : QString{ }, n).arg(mark_of(row.Kind)).arg(code));
			const auto bytes = code.toStdString();
			const auto side  = row.Kind == core::cycle_excerpt_row_kind::added ? std::size_t{ 1 } : std::size_t{ 0 };
			Places[index]    = row_syntax{ .Side = side, .Begin = Documents[side].size(), .Length = bytes.size() };
			for (std::size_t other = 0; other < Documents.size(); ++other) {
				// A removed row is only before the change, an added one only after it.
				const auto belongs = other == side || (row.Kind != core::cycle_excerpt_row_kind::removed && row.Kind != core::cycle_excerpt_row_kind::added);
				if (belongs) {
					Documents[other].append(bytes).push_back('\n');
				}
			}
		}
		setPlainText(lines.join(QLatin1Char('\n')));
		auto description = QStringList{ };
		for (const auto& row : Shown.Rows) {
			switch (row.Kind) {
			case core::cycle_excerpt_row_kind::noted:   description.append(QStringLiteral("line %1, noted: %2").arg(*row.Line).arg(text_of(row.Text))); break;
			case core::cycle_excerpt_row_kind::removed: description.append(QStringLiteral("line %1 removed: %2").arg(*row.Line).arg(text_of(row.Text))); break;
			case core::cycle_excerpt_row_kind::added:   description.append(QStringLiteral("added: %1").arg(text_of(row.Text))); break;
			case core::cycle_excerpt_row_kind::context:
			case core::cycle_excerpt_row_kind::omitted: break;
			}
		}
		setAccessibleDescription(description.join(QStringLiteral("; ")));
		paint_bands();
		request_syntax();
	}

	auto note_code_view::adopt_syntax(const std::string_view aDocument, const application::shared_token_spans& aSpans) -> bool
	{
		auto adopted = false;
		for (std::size_t side = 0; side < Documents.size(); ++side) {
			if (!Empty && aSpans && Documents[side] == aDocument) {
				Classified[side] = classify_text_lines(std::span{ reinterpret_cast<const std::uint8_t*>(Documents[side].data()), Documents[side].size() }, aSpans);
				adopted          = true;
			}
		}
		if (adopted) {
			paint_bands();
		}
		return adopted;
	}

	auto note_code_view::syntax_documents() const noexcept -> const std::array<std::string, 2>&
	{
		return Documents;
	}

	void note_code_view::request_syntax()
	{
		// Only what is shown is classified: a closed note asks when it opens.
		if (Requested || Empty || Path.empty() || !SyntaxWanted || !isVisible()) {
			return;
		}
		Requested = true;
		SyntaxWanted({ .Path = Path, .Document = Documents[0] });
		if (Documents[1] != Documents[0]) {
			SyntaxWanted({ .Path = Path, .Document = Documents[1] });
		}
	}

	void note_code_view::showEvent(QShowEvent* aEvent)
	{
		QPlainTextEdit::showEvent(aEvent);
		request_syntax();
	}

	void note_code_view::paint_bands()
	{
		// The gutter and mark in the placeholder colour; each row on its kind's
		// fill; the code in the review panes' role colours once it is classified.
		auto       bands  = QList<QTextEdit::ExtraSelection>{ };
		const auto gutter = gutter_digits(Shown) + 2;
		auto       quiet  = QTextCharFormat{ };
		quiet.setForeground(palette().color(QPalette::PlaceholderText));
		auto block = document()->firstBlock();
		for (std::size_t index = 0; index < Shown.Rows.size() && block.isValid(); ++index, block = block.next()) {
			const auto kind = Shown.Rows[index].Kind;
			if (const auto fill = row_fill(kind, Colours, palette()); fill.isValid()) {
				append_line_band(bands, *this, core::line_span{ .Begin = static_cast<std::uint32_t>(index), .End = static_cast<std::uint32_t>(index + 1U) }, fill);
			}
			const auto length  = kind == core::cycle_excerpt_row_kind::omitted ? block.length() - 1 : gutter;
			auto       formats = QList<QTextLayout::FormatRange>{ QTextLayout::FormatRange{ .start = 0, .length = length, .format = quiet } };
			if (const auto& place = Places[index]; place && Classified[place->Side].Spans) {
				const auto& text = Documents[place->Side];
				const auto  code = std::span{ reinterpret_cast<const std::uint8_t*>(text.data()) + place->Begin, place->Length };
				for (auto range : code_syntax_format_ranges_at(Classified[place->Side], place->Begin, code, Colours)) {
					// The code begins after the line number, the mark and a space.
					range.start += gutter + 1;
					formats.append(std::move(range));
				}
			}
			apply_syntax_format_ranges(*document(), block, formats);
		}
		setExtraSelections(bands);
	}

	void note_code_view::resizeEvent(QResizeEvent* aEvent)
	{
		QPlainTextEdit::resizeEvent(aEvent);
		// Whether a long line needs the scroll bar depends on the width just given.
		if (FitRows > 0) {
			const auto height = fitted_height(FitRows);
			if (height != this->height()) {
				setFixedHeight(height);
			}
		}
	}

	void note_code_view::fit_rows(const int aRows)
	{
		FitRows = aRows;
		setFixedHeight(fitted_height(aRows));
	}

	auto note_code_view::fitted_height(const int aRows) const -> int
	{
		const auto rows = std::clamp(static_cast<int>(Shown.Rows.size()), 1, aRows);
		// Whole pixels a line: the editor scrolls when a fraction leaves the last line short.
		// [[BPR-1161]]. A face with negative leading has a line spacing below its
		// height, and the text layout gives each line its height: on Windows 13
		// against 14 pixels, which left the tenth row out of view.
		const auto metrics = QFontMetricsF{ font() };
		const auto spacing = std::ceil(std::max(metrics.lineSpacing(), metrics.height()));
		auto       widest  = 0.0;
		for (auto block = document()->firstBlock(); block.isValid(); block = block.next()) {
			widest = std::max(widest, QFontMetricsF{ font() }.horizontalAdvance(block.text()));
		}
		const auto margins = contentsMargins().top() + contentsMargins().bottom() + 2.0 * document()->documentMargin();
		const auto bar     = widest + 2.0 * document()->documentMargin() > viewport()->width() ? horizontalScrollBar()->sizeHint().height() : 0;
		return static_cast<int>(std::ceil(spacing * rows + margins)) + bar + 2 * frameWidth();
	}

	auto note_code_view::anchor_row() const -> int
	{
		const auto found = std::ranges::find_if(Shown.Rows, [](const core::cycle_excerpt_row& aRow) {
			return aRow.Kind == core::cycle_excerpt_row_kind::noted || aRow.Kind == core::cycle_excerpt_row_kind::removed || aRow.Kind == core::cycle_excerpt_row_kind::added;
		});
		return found == Shown.Rows.end() ? 0 : static_cast<int>(found - Shown.Rows.begin());
	}

	auto note_code_view::excerpt() const noexcept -> const core::cycle_note_excerpt&
	{
		return Shown;
	}

	review_cycle_share_viewer::review_cycle_share_viewer(QWidget* aParent)
		: QDialog{ aParent }
	{
		setObjectName(QStringLiteral("cycleShareViewer"));
		setWindowModality(Qt::WindowModal);
		setWindowTitle(QStringLiteral("Note's code"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(16, 14, 16, 12);
		column->setSpacing(6);
		Heading = new QLabel{ this };
		Heading->setObjectName(QStringLiteral("cycleShareViewerHeading"));
		Heading->setTextFormat(Qt::PlainText);
		Heading->setWordWrap(true);
		auto font = Heading->font();
		font.setBold(true);
		Heading->setFont(font);
		column->addWidget(Heading);
		Detail = new QLabel{ this };
		Detail->setObjectName(QStringLiteral("cycleShareViewerDetail"));
		Detail->setTextFormat(Qt::PlainText);
		Detail->setWordWrap(true);
		column->addWidget(Detail);
		Code = new note_code_view{ this };
		Code->setObjectName(QStringLiteral("cycleShareViewerCode"));
		Code->setAccessibleName(QStringLiteral("The note's file"));
		column->addWidget(Code, 1);
		Text = new QPlainTextEdit{ this };
		Text->setObjectName(QStringLiteral("cycleShareViewerText"));
		Text->setAccessibleName(QStringLiteral("The exact text"));
		Text->setReadOnly(true);
		Text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		Text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		column->addWidget(Text, 1);
		Foot = new QLabel{ this };
		Foot->setObjectName(QStringLiteral("cycleShareViewerFoot"));
		Foot->setTextFormat(Qt::PlainText);
		Foot->setWordWrap(true);
		column->addWidget(Foot);
		auto* const buttons = new QHBoxLayout{ };
		Previous            = new QPushButton{ QStringLiteral("Previous Note"), this };
		Previous->setObjectName(QStringLiteral("cycleShareViewerPrevious"));
		Previous->setAutoDefault(false);
		Next = new QPushButton{ QStringLiteral("Next Note"), this };
		Next->setObjectName(QStringLiteral("cycleShareViewerNext"));
		Next->setAutoDefault(false);
		Done = new QPushButton{ QStringLiteral("Done"), this };
		Done->setObjectName(QStringLiteral("cycleShareViewerDone"));
		Done->setDefault(true);
		buttons->addWidget(Previous);
		buttons->addWidget(Next);
		buttons->addStretch(1);
		buttons->addWidget(Done);
		column->addLayout(buttons);
		connect(Previous, &QPushButton::clicked, this, [this] {
			if (Step) {
				Step(-1);
			}
		});
		connect(Next, &QPushButton::clicked, this, [this] {
			if (Step) {
				Step(1);
			}
		});
		connect(Done, &QPushButton::clicked, this, &QDialog::accept);
		cancel_with_control_c(*this);
		ring_keyboard_focus(*this, QStringLiteral("cycleKeyboardFocus"));
	}

	void review_cycle_share_viewer::show_code(const QString& aHeading, const QString& aDetail, const core::cycle_note_excerpt& aExcerpt, const bool aWhole, const std::string& aPath)
	{
		setWindowTitle(aHeading);
		Heading->setText(aHeading);
		Detail->setText(aDetail);
		Text->hide();
		Code->show();
		const auto changed = Code->excerpt() != aExcerpt;
		Code->show_excerpt(aExcerpt, aPath);
		if (changed) {
			// The note's own lines a third of the way down, as a reader lands on them.
			auto cursor = QTextCursor{ Code->document()->findBlockByNumber(Code->anchor_row()) };
			Code->setTextCursor(cursor);
			Code->verticalScrollBar()->setValue(std::max(0, Code->anchor_row() - 6));
		}
		Foot->setText(!aWhole            ? QStringLiteral("Reading the whole file…")
						  : aExcerpt.Cut ? QStringLiteral("The first %1 rows are shown; the exact text has every note.").arg(grouped(core::cycle_excerpt_larger_rows))
										 : QString{ });
		Foot->setVisible(!Foot->text().isEmpty());
		Previous->show();
		Next->show();
	}

	void review_cycle_share_viewer::show_text(const QString& aHeading, const QString& aText, const QString& aFoot)
	{
		setWindowTitle(aHeading);
		Heading->setText(aHeading);
		Detail->setText(QStringLiteral("Exactly the text Copy and Save send, in Markdown."));
		Code->hide();
		Text->show();
		if (Text->toPlainText() != aText) {
			Text->setPlainText(aText);
		}
		Foot->setText(aFoot);
		Foot->setVisible(!aFoot.isEmpty());
		Previous->hide();
		Next->hide();
	}

	void review_cycle_share_viewer::set_code_font(const QFont& aFont, const std::uint32_t aTabWidth)
	{
		Code->set_code_font(aFont, aTabWidth);
		Text->setFont(Code->font());
	}

	void review_cycle_share_viewer::set_colours(const code_role_colors& aColours)
	{
		Code->set_colours(aColours);
	}

	void review_cycle_share_viewer::set_steps(const bool aPrevious, const bool aNext)
	{
		Previous->setEnabled(aPrevious);
		Next->setEnabled(aNext);
	}

	void review_cycle_share_viewer::done(const int aResult)
	{
		QDialog::done(aResult);
		Centred = false;
		if (Closed) {
			Closed();
		}
	}

	void review_cycle_share_viewer::showEvent(QShowEvent* aEvent)
	{
		// Most of the window it opens over, centred on it: a larger view, not another window.
		if (!Centred) {
			const auto* const sheet = parentWidget();
			const auto* const host  = sheet != nullptr && sheet->parentWidget() != nullptr ? sheet->parentWidget()->window() : sheet;
			if (host != nullptr) {
				const auto area = host->frameGeometry();
				resize(std::max(560, area.width() * 9 / 10), std::max(420, area.height() * 9 / 10));
				move(area.center() - QPoint{ width() / 2, height() / 2 });
			}
			Centred = true;
		}
		QDialog::showEvent(aEvent);
		Done->setFocus(Qt::OtherFocusReason);
	}

	auto review_cycle_share_viewer::heading() const noexcept -> QLabel*
	{
		return Heading;
	}
	auto review_cycle_share_viewer::detail() const noexcept -> QLabel*
	{
		return Detail;
	}
	auto review_cycle_share_viewer::code() const noexcept -> note_code_view*
	{
		return Code;
	}
	auto review_cycle_share_viewer::text() const noexcept -> QPlainTextEdit*
	{
		return Text;
	}
	auto review_cycle_share_viewer::foot() const noexcept -> QLabel*
	{
		return Foot;
	}
	auto review_cycle_share_viewer::previous_button() const noexcept -> QPushButton*
	{
		return Previous;
	}
	auto review_cycle_share_viewer::next_button() const noexcept -> QPushButton*
	{
		return Next;
	}
	auto review_cycle_share_viewer::done_button() const noexcept -> QPushButton*
	{
		return Done;
	}
	auto review_cycle_share_viewer::showing_code() const noexcept -> bool
	{
		return !Code->isHidden();
	}

	review_cycle_share_sheet::review_cycle_share_sheet(QWidget* aParent)
		: QDialog{ aParent }
	{
		setObjectName(QStringLiteral("cycleShareSheet"));
		setWindowTitle(QStringLiteral("Share notes"));
		setWindowModality(Qt::WindowModal);
#ifdef Q_OS_MACOS
		// AppKit attaches it to the window, under the toolbar, over the code.
		setWindowFlag(Qt::Sheet, true);
#endif
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(20, 16, 20, 14);
		column->setSpacing(8);

		Title = new QLabel{ this };
		Title->setObjectName(QStringLiteral("cycleShareTitle"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto font = Title->font();
		font.setBold(true);
		font.setPointSizeF(font.pointSizeF() * 1.25);
		Title->setFont(font);
		// [[BPR-1285]]. The title, and at its trailing end the round's state as
		// a chip and what a round is behind an info glyph: the paragraph that
		// stood under the title. The title takes the width and wraps in it.
		auto* const heading = new QHBoxLayout{ };
		heading->setSpacing(8);
		heading->addWidget(Title, 1, Qt::AlignVCenter);
		RoundChip = new share_chip{ this };
		RoundChip->setObjectName(QStringLiteral("cycleShareRoundState"));
		heading->addWidget(RoundChip, 0, Qt::AlignVCenter);
		Intro = new info_glyph{ QString{ }, this };
		Intro->setObjectName(QStringLiteral("cycleShareIntro"));
		Intro->setAccessibleName(QStringLiteral("About this round"));
		heading->addWidget(Intro, 0, Qt::AlignVCenter);
		column->addLayout(heading);
		column->addSpacing(4);

		// What to ask for, and what it means.
		auto* const ask      = new QHBoxLayout{ };
		auto* const askLabel = new QLabel{ QStringLiteral("Ask them to"), this };
		ask->addWidget(askLabel);
		ActionGroup = new QButtonGroup{ this };
		ActionGroup->setExclusive(true);
		for (std::size_t index = 0; index < all_actions.size(); ++index) {
			const auto  action = all_actions.at(index);
			auto* const button = new QPushButton{ action_label(action), this };
			button->setObjectName(QStringLiteral("cycleShareAction_%1").arg(action_label(action)));
			button->setCheckable(true);
			button->setAutoDefault(false);
			button->setToolTip(action_meaning(action));
			button->setAccessibleDescription(action_meaning(action));
			ActionGroup->addButton(button, static_cast<int>(action));
			ask->addWidget(button);
			Actions.at(index) = button;
		}
		// What the chosen one asks for, quietly on the same line ([[BPR-1285]]).
		ActionWords = new QLabel{ this };
		ActionWords->setObjectName(QStringLiteral("cycleShareActionWords"));
		ActionWords->setTextFormat(Qt::PlainText);
		ActionWords->setForegroundRole(QPalette::PlaceholderText);
		ask->addSpacing(6);
		ask->addWidget(ActionWords, 1);
		column->addLayout(ask);
		connect(ActionGroup, &QButtonGroup::idClicked, this, [this](const int aId) {
			if (!Presenting && Dispatch.Edit) {
				Dispatch.Edit(core::set_round_action{ static_cast<core::cycle_round_action>(aId) });
			}
		});

		// [[BPR-1285]]. The field names itself: its label is its placeholder,
		// and what it is for is its tooltip and accessible description.
		Goal = new QLineEdit{ this };
		Goal->setObjectName(QStringLiteral("cycleShareGoal"));
		Goal->setAccessibleName(QStringLiteral("Note for the whole round"));
		Goal->setPlaceholderText(QStringLiteral("Note for the whole round (optional)"));
		Goal->setToolTip(QStringLiteral("What the whole round is for, in a sentence"));
		Goal->setAccessibleDescription(Goal->toolTip());
		Goal->installEventFilter(this);
		column->addWidget(Goal);
		connect(Goal, &QLineEdit::editingFinished, this, [this] {
			if (!Presenting) {
				save_draft_editor();
			}
		});

		// The notes, in the order they will be read.
		column->addSpacing(4);
		auto* const notesHeader = new QHBoxLayout{ };
		NotesHeading            = new QLabel{ this };
		NotesHeading->setObjectName(QStringLiteral("cycleShareNotesHeading"));
		NotesHeading->setTextFormat(Qt::PlainText);
		NotesHeading->setWordWrap(true);
		auto sectionFont = NotesHeading->font();
		sectionFont.setBold(true);
		NotesHeading->setFont(sectionFont);
		NotesHeading->setToolTip(QStringLiteral("In the order they will be read"));
		notesHeader->addWidget(NotesHeading, 1);
		AllCode = new QToolButton{ this };
		AllCode->setObjectName(QStringLiteral("cycleShareAllCode"));
		AllCode->setText(QStringLiteral("Show All Changes"));
		AllCode->setAutoRaise(true);
		AllCode->setCheckable(true);
		AllCode->setToolTip(QStringLiteral("Open or close every note's change at once"));
		notesHeader->addWidget(AllCode);
		column->addLayout(notesHeader);
		connect(AllCode, &QToolButton::clicked, this, [this](const bool aOpen) {
			for (auto& row : Rows) {
				if (!row.Disclose->isHidden()) {
					set_expanded(row, aOpen);
				}
			}
			refresh_all_code();
		});
		NotesArea = new QScrollArea{ this };
		NotesArea->setObjectName(QStringLiteral("cycleShareNotes"));
		NotesArea->setAccessibleName(QStringLiteral("Notes in this round"));
		NotesArea->setAccessibleDescription(QStringLiteral("In the order they will be read"));
		NotesArea->setWidgetResizable(true);
		NotesArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		// Its notes' controls take focus, and it scrolls to each; the area itself is no stop.
		NotesArea->setFocusPolicy(Qt::NoFocus);
		NotesList   = new QWidget{ NotesArea };
		NotesLayout = new QVBoxLayout{ NotesList };
		NotesLayout->setContentsMargins(4, 4, 4, 4);
		NotesLayout->setSpacing(6);
		NotesLayout->addStretch(1);
		NotesArea->setWidget(NotesList);
		column->addWidget(NotesArea, 1);
		DropMark = new QFrame{ NotesList };
		DropMark->setObjectName(QStringLiteral("cycleShareDropMark"));
		DropMark->setAutoFillBackground(true);
		DropMark->setBackgroundRole(QPalette::Highlight);
		DropMark->hide();

		// [[BPR-1285]]. The last share's words on a panel washed in what they say.
		StatusPanel = new share_panel{ this };
		StatusPanel->setObjectName(QStringLiteral("cycleShareStatusPanel"));
		Status = new QLabel{ StatusPanel };
		Status->setObjectName(QStringLiteral("cycleShareStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setAccessibleName(QStringLiteral("Last share"));
		static_cast<QHBoxLayout*>(StatusPanel->layout())->addWidget(Status, 1);
		StatusPanel->hide();
		column->addWidget(StatusPanel);
		ReplaceRow = new share_panel{ this };
		ReplaceRow->setObjectName(QStringLiteral("cycleShareReplacePanel"));
		auto* const replaceLayout = static_cast<QHBoxLayout*>(ReplaceRow->layout());
		ReplaceText               = new QLabel{ ReplaceRow };
		ReplaceText->setObjectName(QStringLiteral("cycleShareReplaceText"));
		ReplaceText->setTextFormat(Qt::PlainText);
		ReplaceText->setWordWrap(true);
		replaceLayout->addWidget(ReplaceText, 1);
		ReplaceIt = new QPushButton{ QStringLiteral("Replace It"), ReplaceRow };
		ReplaceIt->setObjectName(QStringLiteral("cycleShareReplace"));
		ReplaceIt->setAutoDefault(false);
		KeepIt = new QPushButton{ QStringLiteral("Keep It"), ReplaceRow };
		KeepIt->setObjectName(QStringLiteral("cycleShareKeep"));
		KeepIt->setAutoDefault(false);
		replaceLayout->addWidget(ReplaceIt);
		replaceLayout->addWidget(KeepIt);
		ReplaceRow->hide();
		column->addWidget(ReplaceRow);
		connect(ReplaceIt, &QPushButton::clicked, this, [this] {
			if (Dispatch.Replace) {
				Dispatch.Replace(true);
			}
		});
		connect(KeepIt, &QPushButton::clicked, this, [this] {
			if (Dispatch.Replace) {
				Dispatch.Replace(false);
			}
		});
		Invocation = new QLineEdit{ this };
		Invocation->setObjectName(QStringLiteral("cycleShareInvocation"));
		Invocation->setAccessibleName(QStringLiteral("Ask your harness to run"));
		Invocation->setReadOnly(true);
		Invocation->setToolTip(QStringLiteral("Ask your harness to run this; it reads the round through the local interface"));
		Invocation->hide();
		column->addWidget(Invocation);

		// [[BPR-1180]]. Send to Agent: which agent, which session, what happens.
		SendBox                = new QWidget{ this };
		auto* const sendLayout = new QGridLayout{ SendBox };
		sendLayout->setContentsMargins(0, 0, 0, 0);
		sendLayout->setHorizontalSpacing(8);
		sendLayout->setVerticalSpacing(4);
		auto* const agentLabel = new QLabel{ QStringLiteral("Agent"), SendBox };
		SendProfile            = new QComboBox{ SendBox };
		SendProfile->setObjectName(QStringLiteral("cycleSendProfile"));
		SendProfile->setAccessibleName(QStringLiteral("Agent"));
		agentLabel->setBuddy(SendProfile);
		SendSessionLabel = new QLabel{ QStringLiteral("Session"), SendBox };
		SendSession      = new QComboBox{ SendBox };
		SendSession->setObjectName(QStringLiteral("cycleSendSession"));
		SendSession->setAccessibleName(QStringLiteral("Session"));
		SendSessionLabel->setBuddy(SendSession);
		SendWords = new QLabel{ SendBox };
		SendWords->setObjectName(QStringLiteral("cycleSendWords"));
		SendWords->setTextFormat(Qt::PlainText);
		SendWords->setWordWrap(true);
		SendWords->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		// [[BPR-1282]]. One line says what Send does; the rest waits behind Details.
		// A flat text button, so the line stays quiet: it reads as Details
		// until opened, then as Hide details.
		SendMore = new QPushButton{ QStringLiteral("Details"), SendBox };
		SendMore->setObjectName(QStringLiteral("cycleSendMore"));
		SendMore->setAccessibleName(QStringLiteral("Send details"));
		SendMore->setFlat(true);
		SendMore->setAutoDefault(false);
		SendDetails = new QLabel{ SendBox };
		SendDetails->setObjectName(QStringLiteral("cycleSendDetails"));
		SendDetails->setTextFormat(Qt::PlainText);
		SendDetails->setWordWrap(true);
		SendDetails->setTextInteractionFlags(Qt::TextSelectableByMouse);
		SendDetails->setForegroundRole(QPalette::PlaceholderText);
		SendDetails->hide();
		connect(SendMore, &QPushButton::clicked, this, [this] {
			SendDetailsOpen = !SendDetailsOpen;
			SendMore->setText(SendDetailsOpen ? QStringLiteral("Hide details") : QStringLiteral("Details"));
			SendDetails->setVisible(SendDetailsOpen);
		});
		auto* const sendLine = new QHBoxLayout{ };
		sendLine->setContentsMargins(0, 0, 0, 0);
		sendLine->setSpacing(4);
		sendLine->addWidget(SendWords, 1);
		sendLine->addWidget(SendMore, 0, Qt::AlignVCenter);
		SendIt = new QPushButton{ QStringLiteral("Send"), SendBox };
		SendIt->setObjectName(QStringLiteral("cycleSendIt"));
		SendIt->setAutoDefault(false);
		StopSend = new QPushButton{ QStringLiteral("Stop"), SendBox };
		StopSend->setObjectName(QStringLiteral("cycleSendStop"));
		StopSend->setAutoDefault(false);
		StopSend->setToolTip(QStringLiteral("End the send under way; whether the agent read the round stays unknown"));
		sendLayout->addWidget(agentLabel, 0, 0);
		sendLayout->addWidget(SendProfile, 0, 1);
		sendLayout->addWidget(SendSessionLabel, 1, 0);
		sendLayout->addWidget(SendSession, 1, 1);
		sendLayout->addLayout(sendLine, 2, 1, 1, 2);
		sendLayout->addWidget(SendDetails, 3, 1, 1, 2);
		sendLayout->addWidget(SendIt, 0, 2);
		sendLayout->addWidget(StopSend, 1, 2);
		sendLayout->setColumnStretch(1, 1);
		// [[BPR-1182]]. The box keeps the height its wrapped words need; when
		// the window leaves the sheet short, the notes above scroll instead.
		auto sendPolicy = QSizePolicy{ QSizePolicy::Preferred, QSizePolicy::Fixed };
		sendPolicy.setHeightForWidth(true);
		SendBox->setSizePolicy(sendPolicy);
		SendBox->hide();
		column->addWidget(SendBox);
		connect(SendProfile, &QComboBox::currentIndexChanged, this, [this] {
			if (!Presenting) {
				refresh_send();
			}
		});
		connect(SendSession, &QComboBox::currentIndexChanged, this, [this] {
			if (!Presenting) {
				refresh_send();
			}
		});
		connect(SendIt, &QPushButton::clicked, this, [this] {
			if (const auto chosen = chosen_send(); chosen && Dispatch.Send) {
				Dispatch.Send(*chosen);
			}
		});
		connect(StopSend, &QPushButton::clicked, this, [this] {
			if (Dispatch.StopSend) {
				Dispatch.StopSend();
			}
		});
		ReplyBox                = new QWidget{ this };
		auto* const replyLayout = new QVBoxLayout{ ReplyBox };
		replyLayout->setContentsMargins(0, 0, 0, 0);
		replyLayout->setSpacing(4);
		ReplyLabel = new QLabel{ ReplyBox };
		ReplyLabel->setObjectName(QStringLiteral("cycleSendReplyLabel"));
		ReplyLabel->setTextFormat(Qt::PlainText);
		ReplyLabel->setWordWrap(true);
		ReplyText = new QPlainTextEdit{ ReplyBox };
		ReplyText->setObjectName(QStringLiteral("cycleSendReply"));
		ReplyText->setAccessibleName(QStringLiteral("The agent's reply"));
		ReplyText->setReadOnly(true);
		ReplyText->setMaximumHeight(120);
		ReplyLabel->setBuddy(ReplyText);
		AddReply = new QPushButton{ QStringLiteral("Add as Reply"), ReplyBox };
		AddReply->setObjectName(QStringLiteral("cycleSendAddReply"));
		AddReply->setAutoDefault(false);
		AddReply->setToolTip(QStringLiteral("Keep this reply with its round, as you would enter one by hand"));
		replyLayout->addWidget(ReplyLabel);
		replyLayout->addWidget(ReplyText);
		replyLayout->addWidget(AddReply, 0, Qt::AlignLeft);
		ReplyBox->hide();
		column->addWidget(ReplyBox);
		connect(AddReply, &QPushButton::clicked, this, [this] {
			if (Dispatch.AddReply) {
				Dispatch.AddReply();
			}
		});

		// What has happened to the text, once anything has, and what none of it means.
		Shared                   = new QWidget{ this };
		auto* const sharedColumn = new QVBoxLayout{ Shared };
		sharedColumn->setContentsMargins(0, 0, 0, 0);
		sharedColumn->setSpacing(2);
		auto* const sharedHeading = new QHBoxLayout{ };
		sharedHeading->setSpacing(4);
		auto* const sharedLabel = new QLabel{ QStringLiteral("Shared so far"), Shared };
		sharedLabel->setFont(sectionFont);
		sharedHeading->addWidget(sharedLabel);
		// [[BPR-1285]]. What none of them means, said on demand: hovering,
		// focusing or pressing the glyph, and to a screen reader.
		Meaning = new info_glyph{ QStringLiteral("These say only what happened to the bytes. None of them means the round was received, read, acted on or accepted."), Shared };
		Meaning->setObjectName(QStringLiteral("cycleShareMeaning"));
		Meaning->setAccessibleName(QStringLiteral("What these mean"));
		sharedHeading->addWidget(Meaning);
		sharedHeading->addStretch(1);
		sharedColumn->addLayout(sharedHeading);
		Deliveries = new QListWidget{ Shared };
		Deliveries->setObjectName(QStringLiteral("cycleShareDeliveries"));
		Deliveries->setAccessibleName(QStringLiteral("Shared so far"));
		Deliveries->setWordWrap(true);
		Deliveries->setSelectionMode(QAbstractItemView::NoSelection);
		Deliveries->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Deliveries->setFocusPolicy(Qt::NoFocus);
		Deliveries->setItemDelegate(new delivery_delegate{ Deliveries });
		Deliveries->setAccessibleDescription(Meaning->consequence());
		sharedLabel->setBuddy(Deliveries);
		sharedColumn->addWidget(Deliveries);
		Shared->hide();
		column->addWidget(Shared);

		// Copy, the usual way, with Save beside it; when to use each is its
		// tooltip, and a refused one says so in a chip beside it ([[BPR-1285]]).
		column->addSpacing(4);
		auto* const primary = new QHBoxLayout{ };
		primary->setSpacing(8);
		auto* const foot = new QHBoxLayout{ };
		foot->setSpacing(2);
		for (const auto route : all_routes) {
			auto* const button = new QPushButton{ route_label(route, core::cycle_round_id{ 1 }), this };
			button->setObjectName(QStringLiteral("cycleShare_%1").arg(QString::fromLatin1(core::stable_name(route).data(), static_cast<qsizetype>(core::stable_name(route).size()))));
			button->setAutoDefault(false);
			button->setToolTip(route_use(route));
			button->hide();
			Routes.at(static_cast<std::size_t>(route)) = button;
			if (route == core::cycle_share_route::copy_document || route == core::cycle_share_route::save_document) {
				const auto  line  = route == core::cycle_share_route::copy_document ? 0 : 1;
				auto* const words = new share_chip{ this };
				words->setObjectName(QStringLiteral("cycleShareWords_%1").arg(QString::fromLatin1(core::stable_name(route).data(), static_cast<qsizetype>(core::stable_name(route).size()))));
				words->hide();
				RouteWords.at(static_cast<std::size_t>(line)) = words;
				primary->addWidget(button);
				primary->addWidget(words);
			} else {
				button->setFlat(true);
				if (route == core::cycle_share_route::save_bundle) {
					ExactText = new QPushButton{ QStringLiteral("Exact Text"), this };
					ExactText->setObjectName(QStringLiteral("cycleShareExactText"));
					ExactText->setFlat(true);
					ExactText->setAutoDefault(false);
					ExactText->setAccessibleName(QStringLiteral("Read the Exact Text"));
					ExactText->setToolTip(QStringLiteral("Read exactly the text Copy and Save send, with its size and digest"));
					foot->addWidget(ExactText);
				}
				foot->addWidget(button);
			}
			connect(button, &QPushButton::clicked, this, [this, route] {
				// Send to Agent asks which agent and session first ([[BPR-1180]]).
				if (route == core::cycle_share_route::send_to_agent) {
					SendOpen = !SendOpen;
					refresh_send();
					if (SendOpen) {
						SendProfile->setFocus(Qt::TabFocusReason);
					}
					return;
				}
				if (Dispatch.Share) {
					Dispatch.Share(route);
				}
			});
		}
		primary->addStretch(1);
		column->addLayout(primary);
		foot->addStretch(1);
		Cancel = new QPushButton{ QStringLiteral("Cancel"), this };
		Cancel->setObjectName(QStringLiteral("cycleShareCancel"));
		Cancel->setAutoDefault(false);
		Cancel->setToolTip(QStringLiteral("Close; the round stays as it is for next time (Esc)"));
		foot->addWidget(Cancel);
		column->addLayout(foot);
		connect(Cancel, &QPushButton::clicked, this, &QDialog::reject);
		connect(ExactText, &QPushButton::clicked, this, [this] { open_text(); });

		// Option+Up and Option+Down move the focused note (Alt elsewhere).
		for (const auto key : { Qt::Key_Up, Qt::Key_Down }) {
			auto* const shortcut = new QShortcut{ QKeySequence{ Qt::ALT | key }, this };
			shortcut->setObjectName(key == Qt::Key_Up ? QStringLiteral("cycleShareMoveEarlier") : QStringLiteral("cycleShareMoveLater"));
			shortcut->setContext(Qt::WindowShortcut);
			connect(shortcut, &QShortcut::activated, this, [this, key] {
				auto* const row = row_of(QApplication::focusWidget());
				if (row == nullptr || !row->Included || !Idle) {
					return;
				}
				const auto at = static_cast<std::size_t>(row - Rows.data());
				move_note(at, key == Qt::Key_Up ? (at == 0U ? 0U : at - 1U) : at + 2U);
			});
		}
		cancel_with_control_c(*this);

		Viewer                       = new review_cycle_share_viewer{ this };
		Viewer->code()->SyntaxWanted = [this](const source_syntax_request& aRequest) { want_code_syntax(aRequest); };
		Viewer->Step                 = [this](const int aDirection) { step_viewer(aDirection); };
		Viewer->Closed               = [this] {
			if (Viewing) {
				Viewing.reset();
				if (Dispatch.CloseCode) {
					Dispatch.CloseCode();
				}
			}
			// Back to the sheet, on the control that opened the view.
			if (isVisible()) {
				activateWindow();
				if (ReturnFocus) {
					ReturnFocus->setFocus(Qt::OtherFocusReason);
				}
			}
		};
		ring_keyboard_focus(*this, QStringLiteral("cycleKeyboardFocus"));
	}

	void review_cycle_share_sheet::set_dispatch(review_cycle_share_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void review_cycle_share_sheet::set_code_font(const QFont& aFont, const std::uint32_t aTabWidth)
	{
		for (auto& row : Rows) {
			row.Code->set_code_font(aFont, aTabWidth);
			row.Code->fit_rows(inline_rows);
		}
		Viewer->set_code_font(aFont, aTabWidth);
		CodeFont     = aFont;
		CodeTabWidth = aTabWidth;
	}

	void review_cycle_share_sheet::set_code_colours(const code_role_colors& aColours)
	{
		Colours = aColours;
		for (auto& row : Rows) {
			row.Code->set_colours(aColours);
		}
		Viewer->set_colours(aColours);
		refresh_marks();
	}

	auto review_cycle_share_sheet::adopt_code_syntax(const std::string_view aDocument, const application::shared_token_spans& aSpans) -> bool
	{
		auto adopted = Viewer->code()->adopt_syntax(aDocument, aSpans);
		for (auto& row : Rows) {
			adopted = row.Code->adopt_syntax(aDocument, aSpans) || adopted;
		}
		return adopted;
	}

	void review_cycle_share_sheet::want_code_syntax(const source_syntax_request& aRequest) const
	{
		if (SyntaxWanted) {
			SyntaxWanted(aRequest);
		}
	}

	void review_cycle_share_sheet::set_anchor(const QPoint aGlobal)
	{
		Anchor = aGlobal;
	}

	void review_cycle_share_sheet::present(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes, const bool aIdle)
	{
		Presenting       = true;
		Idle             = aIdle;
		Shown            = aShare;
		ShownNotes       = aNotes;
		const auto round = aShare.Round.value();
		const auto notes = static_cast<std::uint64_t>(aShare.Selection.Items.size());
		if (aShare.Frozen) {
			Title->setText(shared_title(aShare));
			Intro->set_consequence(QStringLiteral("Round %1 is fixed as you shared it: copying or saving it again sends the same text, and changing it now makes round %2. When the changes come back, choose Capture revision to see them beside what you asked.")
									   .arg(round)
									   .arg(round + 1U));
		} else if (aShare.Amends) {
			Title->setText(QStringLiteral("Share round %1").arg(round));
			Intro->set_consequence(QStringLiteral("You changed this round after sharing it as round %1, which stays as it was. Sharing now sends it as round %2. Nothing leaves this window until you copy or save it.")
									   .arg(aShare.Amends->value())
									   .arg(round));
		} else {
			Title->setText(QStringLiteral("Share round %1").arg(round));
			Intro->set_consequence(notes == 0U ? QStringLiteral("Check the notes this round should carry. Nothing leaves this window until you copy or save it.")
											   : QStringLiteral("Your %1 %2 out together as one message to whoever is changing the code: an agent in your own tool, or a colleague. Nothing leaves this window until you copy or save it.")
													 .arg(notes == 1U ? QStringLiteral("note") : spelled(notes) + QStringLiteral(" notes"), notes == 1U ? QStringLiteral("goes") : QStringLiteral("go")));
		}
		setAccessibleName(Title->text());

		for (std::size_t index = 0; index < all_actions.size(); ++index) {
			Actions.at(index)->setChecked(all_actions.at(index) == aShare.Selection.Action);
			Actions.at(index)->setEnabled(aIdle);
		}
		ActionWords->setText(action_meaning(aShare.Selection.Action));

		ShownGoal = text_of(aShare.Selection.Goal);
		if (aShare.UnsavedGoal && !Goal->isModified()) {
			// Refused because the draft changed elsewhere first: kept to save again.
			Goal->setText(text_of(*aShare.UnsavedGoal));
			Goal->setModified(true);
		} else if (!aShare.UnsavedGoal && !Goal->isModified()) {
			// Typed text not yet saved stays, whether or not the sheet is the
			// active window: the reviewer may be at the terminal (BPR-1168).
			Goal->setText(ShownGoal);
			Goal->setModified(false);
		}
		Goal->setEnabled(aIdle || Goal->isModified() || Goal->hasFocus());
		rebuild_rows(aShare, aNotes);

		auto status = QStringList{ };
		if (aShare.Refusal) {
			status.append(refusal_text(*aShare.Refusal));
		}
		if (!aShare.Status.empty()) {
			status.append(text_of(aShare.Status));
		}
		Status->setText(status.join(QLatin1Char('\n')));
		StatusPanel->setVisible(!status.isEmpty());
		ReplaceRow->setVisible(aShare.Replace.has_value());
		if (aShare.Replace) {
			ReplaceText->setText(QStringLiteral("%1 already holds other content (%2). Replace exactly that file, or keep it and choose another.").arg(text_of(aShare.Replace->Name), bytes_text(aShare.Replace->Size)));
		}
		Invocation->setText(text_of(aShare.Invocation));
		Invocation->setVisible(!aShare.Invocation.empty());

		// The routes, in order; one this installation cannot reach is not shown.
		for (const auto route : all_routes) {
			auto* const button = Routes.at(static_cast<std::size_t>(route));
			const auto  offer  = std::ranges::find(aShare.Routes, route, &core::cycle_share_route_offer::Route);
			button->setText(route_label(route, aShare.Round));
			button->setAccessibleName(route_label(route, aShare.Round).remove(QStringLiteral("…")));
			button->setVisible(offer != aShare.Routes.end());
			const auto offered = offer != aShare.Routes.end() && offer->offered();
			button->setEnabled(offered && !aShare.Refusal && aIdle);
			button->setToolTip(offer == aShare.Routes.end() || offered ? route_use(route) : refused_words(offer->Decision));
			button->setAccessibleDescription(button->toolTip());
			button->setProperty("cycleShareRefused", offer != aShare.Routes.end() && !offered);
			if (route == core::cycle_share_route::copy_document || route == core::cycle_share_route::save_document) {
				// [[BPR-1285]]. Refused: a chip beside it, its reason the chip's tooltip.
				auto* const words = RouteWords.at(route == core::cycle_share_route::copy_document ? 0U : 1U);
				words->setVisible(!button->isHidden() && offer != aShare.Routes.end() && !offered);
				words->setToolTip(button->toolTip());
				words->setAccessibleDescription(button->toolTip());
			}
		}
		ExactText->setEnabled(aShare.DocumentBytes > 0U);
		Cancel->setText(aShare.Frozen ? QStringLiteral("Done") : QStringLiteral("Cancel"));
		refresh_send();
		rebuild_deliveries(aShare);
		refresh_marks();
		refresh_defaults();
		refresh_viewer();
		Presenting = false;
	}

	void review_cycle_share_sheet::refresh_send()
	{
		const auto& send  = Shown.Send;
		auto* const route = Routes.at(static_cast<std::size_t>(core::cycle_share_route::send_to_agent));
		if (!send || route->isHidden()) {
			SendOpen = false;
		}
		SendBox->setVisible(send && (SendOpen || send->Sending));
		ReplyBox->setVisible(send && !send->Reply.empty());
		if (!send) {
			return;
		}
		const auto presenting = Presenting;
		Presenting            = true;
		// The agents, refused ones listed and not choosable; the choice is kept by identity.
		const auto profile = SendProfile->currentData();
		SendProfile->clear();
		for (const auto& offer : send->Offers) {
			const auto name  = QString::fromStdString(offer.DisplayName);
			const auto label = offer_label(offer);
			SendProfile->addItem(offer.offered() ? label : QStringLiteral("%1 — not allowed here").arg(label), QVariant::fromValue(offer.Profile.value()));
			const auto at = SendProfile->count() - 1;
			// The profile's own name, for whoever wants to know which one it is.
			SendProfile->setItemData(at, offer.offered() ? name : send_refusal_text(offer.Verdict), Qt::ToolTipRole);
			if (!offer.offered()) {
				SendProfile->setItemData(at, 0, Qt::UserRole - 1);
				// [[BPR-1285]]. Marked refused as well as said; the reason is its tooltip.
				SendProfile->setItemIcon(at, RefusedMark);
			}
		}
		auto at = profile.isValid() ? SendProfile->findData(profile) : -1;
		if (at < 0 || !send->Offers.at(static_cast<std::size_t>(at)).offered()) {
			const auto first = std::ranges::find_if(send->Offers, &core::cycle_send_offer::offered);
			at               = first == send->Offers.end() ? -1 : static_cast<int>(first - send->Offers.begin());
		}
		SendProfile->setCurrentIndex(at);
		// New session first; a session this agent reported before, by name.
		const auto session = SendSession->currentData();
		SendSession->clear();
		SendSession->addItem(QStringLiteral("New session"), QVariant{ });
		const auto* offer = at >= 0 ? &send->Offers.at(static_cast<std::size_t>(at)) : nullptr;
		if (offer != nullptr) {
			for (const auto& name : offer->Sessions) {
				SendSession->addItem(QStringLiteral("Continue session %1").arg(QString::fromStdString(name)), QString::fromStdString(name));
			}
		}
		const auto kept = session.isValid() ? SendSession->findData(session) : -1;
		SendSession->setCurrentIndex(kept < 0 ? 0 : kept);
		SendSession->setEnabled(SendSession->count() > 1 && !send->Sending);
		// [[BPR-1282]]. A session to choose only where there is one to continue.
		const auto sessions = SendSession->count() > 1;
		SendSessionLabel->setVisible(sessions);
		SendSession->setVisible(sessions);
		SendProfile->setEnabled(!send->Sending);
		const auto chosen = chosen_send();
		SendMore->setVisible(offer != nullptr && !send->Sending);
		if (send->Sending) {
			SendWords->setText(QStringLiteral("Sending round %1 to %2…").arg(Shown.Round.value()).arg(QString::fromStdString(send->Profile)));
		} else if (offer == nullptr) {
			SendWords->setText(QStringLiteral("No agent is allowed for this review."));
		} else {
			SendWords->setText(send_summary(*offer, chosen ? chosen->Session.Continue : std::nullopt, send->Folder));
			SendDetails->setText(send_detail_words(*offer, send->Folder, Shown.Round));
		}
		SendDetails->setVisible(SendMore->isVisible() && SendDetailsOpen);
		SendWords->setAccessibleDescription(SendMore->isVisible() ? SendDetails->text() : QString{ });
		SendIt->setText(QStringLiteral("Send Round %1").arg(Shown.Round.value()));
		SendIt->setEnabled(chosen.has_value() && !send->Sending && Idle && !Shown.Refusal && !route->isHidden() && route->isEnabled());
		StopSend->setVisible(send->Sending);
		// The status above says it is kept only if added; the label names whose words these are.
		ReplyLabel->setText(QStringLiteral("%1's reply about round %2").arg(QString::fromStdString(send->Profile)).arg(send->ReplyRound ? send->ReplyRound->value() : 0U));
		if (ReplyText->toPlainText() != QString::fromStdString(send->Reply)) {
			ReplyText->setPlainText(QString::fromStdString(send->Reply));
		}
		AddReply->setEnabled(Idle);
		Presenting = presenting;
	}

	auto review_cycle_share_sheet::chosen_send() const -> std::optional<core::cycle_send_request>
	{
		if (!Shown.Send || SendProfile->currentIndex() < 0) {
			return std::nullopt;
		}
		const auto& offer = Shown.Send->Offers.at(static_cast<std::size_t>(SendProfile->currentIndex()));
		if (!offer.offered()) {
			return std::nullopt;
		}
		auto request = core::cycle_send_request{ .Profile = offer.Profile };
		if (const auto session = SendSession->currentData(); session.isValid()) {
			request.Session.Continue = session.toString().toStdString();
		}
		return request;
	}

	void review_cycle_share_sheet::refresh_defaults()
	{
		// Copy is the usual way; once the round is shared, Done.
		auto* chosen = Cancel;
		if (!Shown.Frozen) {
			for (const auto route : { core::cycle_share_route::copy_document, core::cycle_share_route::save_document }) {
				auto* const button = Routes.at(static_cast<std::size_t>(route));
				if (!button->isHidden() && button->isEnabled()) {
					chosen = button;
					break;
				}
			}
		}
		for (auto* const button : findChildren<QPushButton*>()) {
			if (button->window() == this) {
				button->setDefault(button == chosen);
			}
		}
		// The default button's glyph follows its text's colour.
		refresh_route_marks();
	}

	void review_cycle_share_sheet::refresh_marks()
	{
		// A palette change can arrive before the constructor has made the marks.
		if (SendIt == nullptr) {
			return;
		}
		const auto& colours = palette();
		const auto  tone    = [&](const share_tone aTone) { return share_tone_colour(aTone, Colours, colours); };
		if (Shown.Frozen) {
			RoundChip->set_mark(share_glyph::fixed, tone(share_tone::local), QStringLiteral("Shared"));
		} else if (Shown.Amends) {
			RoundChip->set_mark(share_glyph::changed, tone(share_tone::changed), QStringLiteral("Changed since round %1").arg(Shown.Amends->value()));
		} else {
			RoundChip->set_mark(share_glyph::pending, tone(share_tone::waiting), QStringLiteral("Not shared yet"));
		}
		RoundChip->setToolTip(Intro->consequence());
		StatusPanel->set_mark(Shown.Refusal ? share_glyph::refused : share_glyph::information, tone(Shown.Refusal ? share_tone::refused : share_tone::information));
		ReplaceRow->set_mark(share_glyph::caution, tone(share_tone::warning));
		for (auto* const words : RouteWords) {
			words->set_mark(share_glyph::refused, tone(share_tone::refused), QStringLiteral("Not allowed"));
		}
		if (const auto refused = tone(share_tone::refused); property("cycleShareRefusedInk").value<QColor>() != refused) {
			RefusedMark = share_glyph_icon(share_glyph::refused, refused, refused);
			setProperty("cycleShareRefusedInk", refused);
			refresh_send();
		}
		for (auto row = 0; row < Deliveries->count(); ++row) {
			auto* const item   = Deliveries->item(row);
			const auto  colour = tone(static_cast<share_tone>(item->data(delivery_tone_role).toInt()));
			if (item->data(delivery_colour_role).value<QColor>() != colour) {
				item->setData(delivery_colour_role, colour);
			}
		}
		const auto ink = colours.color(QPalette::Active, QPalette::PlaceholderText);
		if (Invocation->property("cycleShareMarkInk").value<QColor>() != ink) {
			for (auto* const action : Invocation->actions()) {
				Invocation->removeAction(action);
				action->deleteLater();
			}
			Invocation->addAction(share_glyph_icon(share_glyph::terminal, ink, ink), QLineEdit::LeadingPosition);
			Invocation->setProperty("cycleShareMarkInk", ink);
		}
		refresh_route_marks();
	}

	void review_cycle_share_sheet::refresh_route_marks()
	{
		if (SendIt == nullptr) {
			return;
		}
		const auto& colours  = palette();
		const auto  disabled = colours.color(QPalette::Disabled, QPalette::ButtonText);
		const auto  mark     = [&](QPushButton* const aButton, const share_glyph aGlyph) {
			const auto refused = aButton->property("cycleShareRefused").toBool();
			auto       ink     = colours.color(QPalette::Active, QPalette::ButtonText);
			// AppKit fills the default button with the accent and writes on it in
			// the highlighted text colour; other styles draw it as any button.
			if (aButton->isDefault() && aButton->style()->name().compare(QStringLiteral("macos"), Qt::CaseInsensitive) == 0) {
				ink = colours.color(QPalette::Active, QPalette::HighlightedText);
			}
			const auto glyph = refused ? share_glyph::refused : aGlyph;
			if (refused) {
				ink = share_tone_colour(share_tone::refused, Colours, colours);
			}
			const auto key = QStringLiteral("%1/%2/%3").arg(static_cast<int>(glyph)).arg(ink.rgba()).arg(refused ? share_tone_colour(share_tone::refused, Colours, colours).rgba() : disabled.rgba());
			if (aButton->property("cycleShareMark").toString() != key) {
				aButton->setIcon(share_glyph_icon(glyph, ink, refused ? ink : disabled));
				aButton->setProperty("cycleShareMark", key);
			}
		};
		const auto glyphs = std::array{ share_glyph::copy, share_glyph::save, share_glyph::bundle, share_glyph::terminal, share_glyph::send };
		for (const auto route : all_routes) {
			mark(Routes.at(static_cast<std::size_t>(route)), glyphs.at(static_cast<std::size_t>(route)));
		}
		mark(ExactText, share_glyph::page);
		mark(SendIt, share_glyph::send);
	}

	void review_cycle_share_sheet::changeEvent(QEvent* const aEvent)
	{
		QDialog::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::FontChange) {
			refresh_marks();
			Deliveries->viewport()->update();
		}
	}

	auto review_cycle_share_sheet::make_row(const core::cycle_feedback_item_id aItem) -> note_row
	{
		auto row = note_row{ .Item = aItem };
		row.Row  = new QWidget{ NotesList };
		row.Row->setObjectName(QStringLiteral("cycleShareNote_%1").arg(aItem.value()));
		auto* const grid = new QGridLayout{ row.Row };
		grid->setContentsMargins(0, 0, 0, 0);
		grid->setHorizontalSpacing(6);
		grid->setVerticalSpacing(2);
		row.Grip = new QLabel{ QStringLiteral("⋮⋮"), row.Row };
		row.Grip->setObjectName(QStringLiteral("cycleShareGrip"));
		row.Grip->setCursor(Qt::OpenHandCursor);
		row.Grip->setToolTip(QStringLiteral("Drag to change the order; or Option+Up and Option+Down"));
		row.Grip->installEventFilter(this);
		auto steady = row.Grip->sizePolicy();
		steady.setRetainSizeWhenHidden(true);
		row.Grip->setSizePolicy(steady);
		grid->addWidget(row.Grip, 0, 0);
		row.Include = new QCheckBox{ row.Row };
		row.Include->setObjectName(QStringLiteral("cycleShareInclude"));
		row.Include->installEventFilter(this);
		grid->addWidget(row.Include, 0, 1);
		row.Number = new QLabel{ row.Row };
		row.Number->setObjectName(QStringLiteral("cycleShareNumber"));
		// Clear of the include box, which the arrow used to stand between.
		row.Number->setContentsMargins(4, 0, 0, 0);
		grid->addWidget(row.Number, 0, 2);
		row.Heading = new QLabel{ row.Row };
		row.Heading->setObjectName(QStringLiteral("cycleShareHeading"));
		row.Heading->setTextFormat(Qt::PlainText);
		row.Heading->setWordWrap(true);
		auto bold = row.Heading->font();
		bold.setBold(true);
		row.Heading->setFont(bold);
		grid->addWidget(row.Heading, 0, 3);
		row.Kind = new QLabel{ row.Row };
		row.Kind->setObjectName(QStringLiteral("cycleShareKind"));
		row.Kind->setTextFormat(Qt::PlainText);
		grid->addWidget(row.Kind, 0, 4, Qt::AlignRight | Qt::AlignTop);
		row.Where = new QLabel{ row.Row };
		row.Where->setObjectName(QStringLiteral("cycleShareWhere"));
		row.Where->setTextFormat(Qt::PlainText);
		row.Where->setWordWrap(true);
		row.Where->setForegroundRole(QPalette::PlaceholderText);
		grid->addWidget(row.Where, 1, 3);
		// Show Change sits at the end of the second line, under what the note is,
		// in the column of Show All Changes, and away from the include box: v003's
		// arrow beside the box read as a second check mark (event 921).
		auto* const toggle = new note_code_toggle{ row.Row };
		toggle->set_line(row.Where);
		row.Disclose = toggle;
		row.Disclose->setObjectName(QStringLiteral("cycleShareDisclose"));
		// A click gives it focus, so Left and Right work on from there.
		row.Disclose->setFocusPolicy(Qt::StrongFocus);
		row.Disclose->installEventFilter(this);
		auto keep = row.Disclose->sizePolicy();
		keep.setRetainSizeWhenHidden(true);
		row.Disclose->setSizePolicy(keep);
		grid->addWidget(row.Disclose, 1, 4, Qt::AlignRight | Qt::AlignTop);
		row.Detail               = new QWidget{ row.Row };
		auto* const detailColumn = new QVBoxLayout{ row.Detail };
		detailColumn->setContentsMargins(0, 4, 0, 6);
		detailColumn->setSpacing(4);
		row.Body = new QLabel{ row.Detail };
		row.Body->setObjectName(QStringLiteral("cycleShareBody"));
		row.Body->setTextFormat(Qt::PlainText);
		row.Body->setWordWrap(true);
		row.Body->setTextInteractionFlags(Qt::TextSelectableByMouse);
		detailColumn->addWidget(row.Body);
		row.Code = new note_code_view{ row.Detail };
		row.Code->setObjectName(QStringLiteral("cycleShareCode"));
		row.Code->setFocusPolicy(Qt::ClickFocus);
		row.Code->set_colours(Colours);
		row.Code->SyntaxWanted = [this](const source_syntax_request& aRequest) { want_code_syntax(aRequest); };
		if (CodeFont) {
			row.Code->set_code_font(*CodeFont, CodeTabWidth);
		}
		detailColumn->addWidget(row.Code);
		auto* const footRow = new QHBoxLayout{ };
		row.Facts           = new QLabel{ row.Detail };
		row.Facts->setObjectName(QStringLiteral("cycleShareCodeFacts"));
		row.Facts->setTextFormat(Qt::PlainText);
		row.Facts->setWordWrap(true);
		row.Facts->setForegroundRole(QPalette::PlaceholderText);
		footRow->addWidget(row.Facts, 1);
		row.Whole = new QPushButton{ QStringLiteral("Show the Whole File…"), row.Detail };
		row.Whole->setObjectName(QStringLiteral("cycleShareWholeFile"));
		row.Whole->setAutoDefault(false);
		row.Whole->setToolTip(QStringLiteral("Open the note's whole file over this sheet, each suggested change in place"));
		footRow->addWidget(row.Whole, 0, Qt::AlignTop);
		detailColumn->addLayout(footRow);
		row.Detail->hide();
		grid->addWidget(row.Detail, 2, 3, 1, 2);
		grid->setColumnStretch(3, 1);

		const auto item = aItem;
		connect(row.Include, &QCheckBox::clicked, this, [this, item](const bool aChecked) {
			if (!Presenting && Dispatch.Edit) {
				Dispatch.Edit(core::include_round_item{ .Item = item, .Include = aChecked });
			}
		});
		connect(row.Disclose, &QToolButton::clicked, this, [this, item](const bool aOpen) {
			const auto found = std::ranges::find(Rows, item, &note_row::Item);
			if (found != Rows.end()) {
				set_expanded(*found, aOpen);
				refresh_all_code();
				reveal(item);
			}
		});
		connect(row.Whole, &QPushButton::clicked, this, [this, item] { open_code(item); });
		ring_keyboard_focus(*row.Row, QStringLiteral("cycleKeyboardFocus"));
		return row;
	}

	void review_cycle_share_sheet::fill_row(note_row& aRow, const std::size_t aNumber, const application::review_cycle_note_entry& aNote, const application::review_cycle_note_context* aContext)
	{
		const auto same = aRow.Filled && aRow.Filled->Number == aNumber && aRow.Filled->Included == aRow.Included && aRow.Filled->Idle == Idle && aRow.Filled->Note == aNote
			&& (aContext == nullptr ? !aRow.Filled->Context : aRow.Filled->Context == *aContext);
		if (same) {
			return;
		}
		aRow.Filled = note_row::filling{
			.Number   = aNumber,
			.Included = aRow.Included,
			.Idle     = Idle,
			.Note     = aNote,
			.Context  = aContext != nullptr ? std::optional{ *aContext } : std::nullopt,
		};
		auto heading = first_line(aNote.Body);
		if (heading.isEmpty()) {
			// A change with no words is named by what it changes.
			const auto file = aContext != nullptr && !aContext->Path.empty() ? text_of(aContext->Path).section(QLatin1Char('/'), -1) : QString{ };
			heading         = aNote.Suggestions == 0U ? QStringLiteral("(no text)")
				: file.isEmpty()                      ? QStringLiteral("Suggested change")
													  : QStringLiteral("%1 to %2").arg(aNote.Suggestions == 1U ? QStringLiteral("Suggested change") : QStringLiteral("Suggested changes"), file);
		}
		const auto number = aRow.Included ? QStringLiteral("%1").arg(aNumber) : QString{ };
		aRow.Number->setText(number);
		aRow.Heading->setText(heading);
		aRow.Kind->setText(aRow.Included ? kind_words(aNote) : QStringLiteral("Left out"));
		aRow.Where->setText(where_words(aNote.Primary, aContext));
		for (auto* const label : { aRow.Number, aRow.Heading, aRow.Kind }) {
			label->setEnabled(aRow.Included);
		}
		aRow.Include->setChecked(aRow.Included);
		aRow.Include->setEnabled(Idle);
		aRow.Include->setAccessibleName(aRow.Included ? QStringLiteral("Include note %1: %2").arg(aNumber).arg(heading) : QStringLiteral("Include: %1").arg(heading));
		aRow.Include->setAccessibleDescription(aRow.Included ? QStringLiteral("In this round") : QStringLiteral("Left out of this round"));
		aRow.Grip->setVisible(aRow.Included);
		aRow.Grip->setEnabled(Idle);

		const auto rest = rest_of(aNote.Body);
		aRow.Body->setText(rest);
		aRow.Body->setVisible(!rest.isEmpty());
		const auto file     = about_a_file(aNote.Primary);
		const auto readable = aContext != nullptr && aContext->Status == application::review_material_status::available && !aContext->Excerpt.Rows.empty();
		aRow.Disclose->setVisible(file || !rest.isEmpty());
		// A note about no file has only the rest of its words to show.
		static_cast<note_code_toggle*>(aRow.Disclose)->set_words(file ? QStringLiteral("Show Change") : QStringLiteral("Show More"), file ? QStringLiteral("Hide Change") : QStringLiteral("Show Less"));
		if (readable) {
			aRow.Code->show_excerpt(aContext->Excerpt, aContext->Path);
			aRow.Code->fit_rows(inline_rows);
		}
		aRow.Code->setVisible(readable);
		aRow.Code->setAccessibleName(QStringLiteral("Code of: %1").arg(heading));
		aRow.Facts->setText(!file ? QString{ } : aContext == nullptr ? QStringLiteral("Reading the code…")
																	 : code_facts(*aContext));
		aRow.Facts->setVisible(!aRow.Facts->text().isEmpty());
		aRow.Whole->setVisible(readable);
		set_expanded(aRow, Expanded.contains(aRow.Item) && !aRow.Disclose->isHidden());
	}

	void review_cycle_share_sheet::set_expanded(note_row& aRow, const bool aExpanded)
	{
		if (aExpanded) {
			Expanded.insert(aRow.Item);
		} else {
			Expanded.erase(aRow.Item);
		}
		aRow.Disclose->setChecked(aExpanded);
		// The name begins with the words shown, then says which note.
		aRow.Disclose->setAccessibleName(QStringLiteral("%1: %2").arg(aRow.Disclose->text(), aRow.Heading->text()));
		aRow.Disclose->setToolTip(QStringLiteral("%1 (%2)").arg(aRow.Disclose->text(), aExpanded ? QStringLiteral("Left") : QStringLiteral("Right")));
		aRow.Detail->setVisible(aExpanded);
		if (aExpanded) {
			aRow.Code->fit_rows(inline_rows);
		}
	}

	void review_cycle_share_sheet::reveal(const core::cycle_feedback_item_id aItem)
	{
		// Once the layout has taken the opened code, scroll the note into view, its top first.
		QTimer::singleShot(0, this, [this, aItem] {
			const auto found = std::ranges::find(Rows, aItem, &note_row::Item);
			if (found != Rows.end()) {
				NotesArea->ensureWidgetVisible(found->Row, 0, 4);
			}
		});
	}

	void review_cycle_share_sheet::refresh_all_code()
	{
		const auto openable = std::ranges::count_if(Rows, [](const note_row& aRow) { return !aRow.Disclose->isHidden(); });
		const auto open     = std::ranges::count_if(Rows, [](const note_row& aRow) { return !aRow.Disclose->isHidden() && aRow.Disclose->isChecked(); });
		const auto all      = openable > 0 && open == openable;
		AllCode->setChecked(all);
		AllCode->setText(all ? QStringLiteral("Hide All Changes") : QStringLiteral("Show All Changes"));
		AllCode->setEnabled(openable > 0);
		place();
	}

	void review_cycle_share_sheet::rebuild_rows(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes)
	{
		// The round's notes in its order, then open notes left out, in item order.
		auto listed = std::vector<std::pair<const application::review_cycle_note_entry*, bool>>{ };
		for (const auto& selected : aShare.Selection.Items) {
			const auto found = std::ranges::find(aNotes, selected.Item, [](const application::review_cycle_note_entry& aNote) { return aNote.Item.Id; });
			if (found != aNotes.end()) {
				listed.emplace_back(&*found, true);
			}
		}
		for (const auto& note : aNotes) {
			const auto chosen = std::ranges::any_of(aShare.Selection.Items, [&](const core::cycle_round_item_selection& aItem) { return aItem.Item == note.Item.Id; });
			if (!chosen && note.Lifecycle == core::private_thread_lifecycle::open) {
				listed.emplace_back(&note, false);
			}
		}
		// Rows keep their widgets, and so focus and expansion, while their note
		// is listed. [[BPR-1274]]: one pass makes at most `rows_per_pass` new
		// rows, the first listed that have none; the rest follow, a pass a
		// turn, so a long round opens without holding the window. The rows
		// made so far stand in the listed order, each numbered as it is read.
		auto next    = std::vector<note_row>{ };
		auto numbers = std::vector<std::size_t>{ };
		auto notes   = std::vector<const application::review_cycle_note_entry*>{ };
		auto made    = std::size_t{ 0 };
		// One pass of new rows a turn: a refresh in a turn whose pass is queued
		// makes none.
		const auto budget   = PassQueued ? std::size_t{ 0 } : rows_per_pass;
		auto       included = std::size_t{ 0 };
		next.reserve(listed.size());
		for (const auto& [note, chosen] : listed) {
			const auto number = chosen ? ++included : 0U;
			// Rows already in this order are found where they stand.
			const auto here  = next.size() < Rows.size() && Rows[next.size()].Item == note->Item.Id;
			const auto found = here ? Rows.begin() + static_cast<std::ptrdiff_t>(next.size()) : std::ranges::find(Rows, note->Item.Id, &note_row::Item);
			if (found != Rows.end()) {
				next.push_back(std::move(*found));
				found->Row = nullptr;
			} else if (made < budget) {
				next.push_back(make_row(note->Item.Id));
				++made;
			} else {
				continue;
			}
			next.back().Included = chosen;
			numbers.push_back(number);
			notes.push_back(note);
		}
		for (auto& row : Rows) {
			if (row.Row != nullptr) {
				Expanded.erase(row.Item);
				row.Row->deleteLater();
				row.Row->hide();
			}
		}
		// The rows that stand as they stood, in the same places: only rows
		// after them are put in. Taking every row out and putting it back
		// lays all of them out again.
		auto kept = std::size_t{ 0 };
		while (kept < next.size() && kept < Placed.size() && next[kept].Row == Placed[kept]) {
			++kept;
		}
		for (auto index = kept; index < Placed.size(); ++index) {
			NotesLayout->removeWidget(Placed[index]);
		}
		Rows = std::move(next);
		Placed.resize(kept);
		Unlisted = listed.size() - Rows.size();
		for (std::size_t index = 0; index < Rows.size(); ++index) {
			auto&      row     = Rows[index];
			const auto context = std::ranges::find(aShare.Contexts, row.Item, &application::review_cycle_note_context::Item);
			if (index >= kept) {
				NotesLayout->insertWidget(static_cast<int>(index), row.Row);
				Placed.push_back(row.Row);
			}
			fill_row(row, numbers[index], *notes[index], context == aShare.Contexts.end() ? nullptr : &*context);
		}
		// Rows put in while the sheet shows are shown in this pass, filled, not
		// in the next turn beside the next pass's; the list is laid out once,
		// after them, not once for each.
		if (NotesList->isVisible() && kept < Rows.size()) {
			NotesLayout->setEnabled(false);
			for (auto index = kept; index < Rows.size(); ++index) {
				auto* const widget = Rows[index].Row;
				if (widget->isHidden() && !widget->testAttribute(Qt::WA_WState_ExplicitShowHide)) {
					widget->show();
				}
			}
			NotesLayout->setEnabled(true);
		}
		NotesHeading->setText(QStringLiteral("%1 of %2 %3")
								  .arg(included)
								  .arg(listed.size())
								  .arg(listed.size() == 1U ? QStringLiteral("note") : QStringLiteral("notes")));
		// Tab follows what is shown: the round's choices, each note, then the buttons.
		if (kept < Rows.size() || kept < TabOrdered || Rows.empty()) {
			auto order = std::vector<QWidget*>{ Intro, Actions[0], Actions[1], Actions[2], Goal, AllCode };
			for (const auto& row : Rows) {
				order.insert(order.end(), { row.Include, row.Disclose, row.Whole });
			}
			order.insert(order.end(), { ReplaceIt, KeepIt, Invocation, SendProfile, SendSession, SendMore, SendIt, StopSend, ReplyText, AddReply, Meaning, Routes[0], Routes[1], ExactText, Routes[2], Routes[3], Routes[4], Cancel });
			for (std::size_t index = 1; index < order.size(); ++index) {
				QWidget::setTabOrder(order[index - 1U], order[index]);
			}
		}
		TabOrdered = Rows.size();
		// The next pass, once this turn is over, with whatever the sheet shows then.
		if (Unlisted > 0U && !PassQueued) {
			PassQueued = true;
			QTimer::singleShot(0, this, [this] {
				PassQueued = false;
				if (Unlisted > 0U) {
					Presenting = true;
					rebuild_rows(Shown, ShownNotes);
					Presenting = false;
				}
			});
		}
		refresh_all_code();
	}

	void review_cycle_share_sheet::rebuild_deliveries(const application::review_cycle_share_view& aShare)
	{
		// Repeats of the same observation read as one line with a count.
		struct line
		{
			QString                            Text;
			delivery_parts                     Parts;
			std::pair<share_glyph, share_tone> Mark;
			int                                Times = 1;
		};
		auto lines = std::vector<line>{ };
		for (const auto& record : aShare.Deliveries) {
			auto text = delivery_text(record);
			if (!lines.empty() && lines.back().Text == text) {
				++lines.back().Times;
			} else {
				lines.push_back({ .Text = std::move(text), .Parts = delivery_words(record), .Mark = delivery_mark(record) });
			}
		}
		Shared->setVisible(!lines.empty());
		auto texts = QStringList{ };
		for (const auto& each : lines) {
			texts.append(each.Times > 1 ? each.Text + QStringLiteral(" · %1 times").arg(each.Times) : each.Text);
		}
		auto shown = QStringList{ };
		for (auto row = 0; row < Deliveries->count(); ++row) {
			shown.append(Deliveries->item(row)->text());
		}
		if (shown != texts) {
			Deliveries->clear();
			for (std::size_t index = 0; index < lines.size(); ++index) {
				const auto& each = lines[index];
				const auto& text = texts[static_cast<qsizetype>(index)];
				auto* const item = new QListWidgetItem{ text, Deliveries };
				item->setToolTip(text);
				item->setData(delivery_glyph_role, static_cast<int>(each.Mark.first));
				item->setData(delivery_tone_role, static_cast<int>(each.Mark.second));
				item->setData(delivery_colour_role, share_tone_colour(each.Mark.second, Colours, palette()));
				item->setData(delivery_round_role, each.Parts.Round);
				item->setData(delivery_what_role, each.Parts.What);
				item->setData(delivery_size_role, each.Parts.Size);
				item->setData(delivery_times_role, each.Times);
			}
		}
		// Four lines, then the list scrolls; the caption stays in view.
		const auto rows = std::min(static_cast<int>(lines.size()), 4);
		Deliveries->setFixedHeight(rows == 0 ? 0 : Deliveries->sizeHintForRow(0) * rows + 2 * Deliveries->frameWidth() + 2);
	}

	void review_cycle_share_sheet::move_note(const std::size_t aFrom, const std::size_t aTo)
	{
		if (aFrom >= Rows.size() || !Rows[aFrom].Included || !Dispatch.Edit) {
			return;
		}
		const auto included = static_cast<std::size_t>(std::ranges::count_if(Rows, &note_row::Included));
		const auto to       = std::min(aTo, included);
		if (to == aFrom || to == aFrom + 1U) {
			return;
		}
		const auto earlier = to < aFrom;
		const auto steps   = earlier ? aFrom - to : to - aFrom - 1U;
		const auto item    = Rows[aFrom].Item;
		for (std::size_t step = 0; step < steps; ++step) {
			Dispatch.Edit(core::move_round_item{ .Item = item, .Earlier = earlier });
		}
	}

	auto review_cycle_share_sheet::row_of(const QWidget* aWidget) -> note_row*
	{
		for (auto& row : Rows) {
			if (aWidget != nullptr && (aWidget == row.Row || row.Row->isAncestorOf(aWidget))) {
				return &row;
			}
		}
		return nullptr;
	}

	void review_cycle_share_sheet::open_code(const core::cycle_feedback_item_id aItem)
	{
		if (!Viewer->isVisible()) {
			ReturnFocus = QApplication::focusWidget();
		}
		Viewing = aItem;
		if (Dispatch.ShowCode) {
			Dispatch.ShowCode(aItem);
		}
		refresh_viewer();
		if (!Viewer->isVisible()) {
			Viewer->open();
		}
	}

	void review_cycle_share_sheet::refresh_viewer()
	{
		if (!Viewing) {
			return;
		}
		const auto row = std::ranges::find(Rows, *Viewing, &note_row::Item);
		if (row == Rows.end()) {
			Viewer->reject();
			return;
		}
		const auto  context  = std::ranges::find(Shown.Contexts, *Viewing, &application::review_cycle_note_context::Item);
		const auto  whole    = Shown.Larger && Shown.Larger->Item == *Viewing;
		const auto& shown    = whole ? *Shown.Larger : context != Shown.Contexts.end() ? *context
																					   : application::review_cycle_note_context{ };
		const auto  revision = shown.Revision ? QStringLiteral("R%1").arg(shown.Revision->value()) : QString{ };
		auto        detail   = QStringLiteral("%1 at %2").arg(text_of(shown.Path), revision);
		if (shown.Excerpt.Lines) {
			detail += QStringLiteral(", %1").arg(lines_words(*shown.Excerpt.Lines));
		}
		detail += QStringLiteral(". %1 in the file.").arg(count_words(shown.Excerpt.FileLines, QStringLiteral("line"), QStringLiteral("lines")));
		if (shown.Status != application::review_material_status::available) {
			detail = code_facts(shown);
		}
		Viewer->show_code(row->Heading->text(), detail, shown.Excerpt, whole, shown.Path);
		const auto at       = static_cast<std::size_t>(row - Rows.begin());
		const auto readable = [](const note_row& aRow) { return !aRow.Whole->isHidden(); };
		Viewer->set_steps(std::any_of(Rows.begin(), Rows.begin() + static_cast<std::ptrdiff_t>(at), readable), std::any_of(Rows.begin() + static_cast<std::ptrdiff_t>(at) + 1, Rows.end(), readable));
	}

	void review_cycle_share_sheet::step_viewer(const int aDirection)
	{
		if (!Viewing) {
			return;
		}
		const auto row = std::ranges::find(Rows, *Viewing, &note_row::Item);
		if (row == Rows.end()) {
			return;
		}
		auto at = static_cast<std::ptrdiff_t>(row - Rows.begin());
		for (at += aDirection; at >= 0 && at < static_cast<std::ptrdiff_t>(Rows.size()); at += aDirection) {
			if (!Rows[static_cast<std::size_t>(at)].Whole->isHidden()) {
				open_code(Rows[static_cast<std::size_t>(at)].Item);
				return;
			}
		}
	}

	void review_cycle_share_sheet::open_text()
	{
		if (!Viewer->isVisible()) {
			ReturnFocus = QApplication::focusWidget();
		}
		if (Viewing) {
			Viewing.reset();
			if (Dispatch.CloseCode) {
				Dispatch.CloseCode();
			}
		}
		const auto text = Shown.DocumentShown ? text_of(Shown.Document)
											  : QStringLiteral("The text is %1, too large to show here. Nothing is cut short: Save as File keeps all of it.").arg(bytes_text(Shown.DocumentBytes));
		auto       foot = QStringLiteral("%1 of Markdown.").arg(bytes_text(Shown.DocumentBytes));
		if (Shown.Digest) {
			foot += QStringLiteral(" Round digest %1.").arg(QString::fromLatin1(QByteArray{ reinterpret_cast<const char*>(Shown.Digest->bytes().data()), 6 }.toHex()));
		}
		Viewer->show_text(QStringLiteral("Round %1, exactly as it is shared").arg(Shown.Round.value()), text, foot);
		if (!Viewer->isVisible()) {
			Viewer->open();
		}
	}

	void review_cycle_share_sheet::focus_first()
	{
		for (auto* const button : findChildren<QPushButton*>()) {
			if (button->isDefault() && button->isVisible()) {
				button->setFocus(Qt::OtherFocusReason);
				return;
			}
		}
		Cancel->setFocus(Qt::OtherFocusReason);
	}

	auto review_cycle_share_sheet::draft_editor() const -> core::cycle_draft_editor
	{
		if (GoalComposing) {
			return core::cycle_draft_editor::composing;
		}
		return Goal->isModified() && Goal->text() != ShownGoal ? core::cycle_draft_editor::unsaved : core::cycle_draft_editor::settled;
	}

	void review_cycle_share_sheet::save_draft_editor()
	{
		if (GoalComposing || !Dispatch.Edit || !Goal->isModified()) {
			return;
		}
		Goal->setModified(false);
		const auto bytes = Goal->text().toUtf8();
		Dispatch.Edit(core::set_round_goal{ std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) } });
	}

	auto review_cycle_share_sheet::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aWatched == Goal) {
			if (aEvent->type() == QEvent::InputMethod) {
				// Preedit text is not committed text: another interface waits for it.
				GoalComposing = !static_cast<QInputMethodEvent*>(aEvent)->preeditString().isEmpty();
			} else if (aEvent->type() == QEvent::FocusOut || aEvent->type() == QEvent::Hide) {
				GoalComposing = false;
			} else if (aEvent->type() == QEvent::KeyPress) {
				// Return saves a changed round note and stays; with nothing to save it is the default button's.
				const auto* const key = static_cast<QKeyEvent*>(aEvent);
				if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && key->modifiers() == Qt::NoModifier && Goal->isModified()) {
					save_draft_editor();
					return true;
				}
			}
			return QDialog::eventFilter(aWatched, aEvent);
		}
		auto* const widget = qobject_cast<QWidget*>(aWatched);
		auto* const row    = row_of(widget);
		if (row == nullptr) {
			return QDialog::eventFilter(aWatched, aEvent);
		}
		if (aEvent->type() == QEvent::KeyPress && (aWatched == row->Disclose || aWatched == row->Include)) {
			// Right opens a note's code and Left closes it, as a disclosure does.
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if ((key->key() == Qt::Key_Right || key->key() == Qt::Key_Left) && key->modifiers() == Qt::NoModifier && !row->Disclose->isHidden()) {
				set_expanded(*row, key->key() == Qt::Key_Right);
				refresh_all_code();
				reveal(row->Item);
				return true;
			}
		}
		if (aWatched != row->Grip || !row->Included || !Idle) {
			return QDialog::eventFilter(aWatched, aEvent);
		}
		const auto at = static_cast<std::size_t>(row - Rows.data());
		switch (aEvent->type()) {
		case QEvent::MouseButtonPress: {
			const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
			if (mouse->button() == Qt::LeftButton) {
				Dragging  = at;
				DragStart = mouse->globalPosition().toPoint();
				DragMoved = false;
				row->Grip->setCursor(Qt::ClosedHandCursor);
				return true;
			}
			break;
		}
		case QEvent::MouseMove: {
			const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
			if (!Dragging) {
				break;
			}
			const auto point = mouse->globalPosition().toPoint();
			DragMoved        = DragMoved || (point - DragStart).manhattanLength() >= QApplication::startDragDistance();
			if (DragMoved) {
				// The mark shows where the note will land: before the first included note below the pointer.
				const auto y      = NotesList->mapFromGlobal(point).y();
				auto       target = std::size_t{ 0 };
				auto       markY  = Rows.empty() ? 0 : Rows.front().Row->geometry().top() - 3;
				for (const auto& other : Rows) {
					if (!other.Included || other.Row->geometry().center().y() >= y) {
						break;
					}
					++target;
					markY = other.Row->geometry().bottom() + NotesLayout->spacing() / 2;
				}
				DropMark->setGeometry(0, markY, NotesList->width(), 2);
				DropMark->setProperty("bprTarget", static_cast<qulonglong>(target));
				DropMark->show();
				DropMark->raise();
			}
			return true;
		}
		case QEvent::MouseButtonRelease: {
			if (!Dragging) {
				break;
			}
			row->Grip->setCursor(Qt::OpenHandCursor);
			const auto from   = *Dragging;
			const auto target = DropMark->property("bprTarget").toULongLong();
			const auto moved  = DragMoved && !DropMark->isHidden();
			Dragging.reset();
			DropMark->hide();
			if (moved) {
				move_note(from, static_cast<std::size_t>(target));
			}
			return true;
		}
		default: break;
		}
		return QDialog::eventFilter(aWatched, aEvent);
	}

	void review_cycle_share_sheet::done(const int aResult)
	{
		// Cancel, Done or Escape: the host closes Share notes; the draft stays as saved.
		Viewer->hide();
		Viewing.reset();
		QDialog::done(aResult);
		if (Dispatch.Back) {
			Dispatch.Back();
		}
	}

	void review_cycle_share_sheet::showEvent(QShowEvent* aEvent)
	{
		place();
		QDialog::showEvent(aEvent);
	}

	void review_cycle_share_sheet::place()
	{
		const auto* const host = parentWidget() != nullptr ? parentWidget()->window() : nullptr;
		if (host == nullptr) {
			return;
		}
		// Wide enough for a line of code and the two buttons' words; as tall as
		// its content, within the window under the review bar. The notes'
		// layout first takes what was just opened or closed.
		NotesLayout->activate();
		layout()->activate();
		const auto area  = host->geometry();
		const auto top   = Anchor ? Anchor->y() : area.top() + 64;
		const auto room  = std::max(320, area.bottom() - top - 24);
		const auto width = std::clamp(area.width() - 96, 520, 760);
		// The scroll area keeps its last measure until a later event, so the
		// notes are measured here: the sheet grows with them, then scrolls.
		const auto notes = NotesList->sizeHint().height() + 2 * NotesArea->frameWidth();
		// Wrapped words take the lines they need at this width, not at the hint's.
		const auto natural = layout()->hasHeightForWidth() ? layout()->heightForWidth(width) : layout()->sizeHint().height();
		const auto others  = natural - NotesArea->sizeHint().height();
		const auto height  = std::clamp(others + notes, std::min(room, 240), room);
		resize(width, height);
		// AppKit places a sheet itself; elsewhere it hangs from the review bar.
		const auto centre = Anchor ? Anchor->x() : area.center().x();
		move(centre - width / 2, top);
	}

	auto review_cycle_share_sheet::title() const noexcept -> QLabel*
	{
		return Title;
	}
	auto review_cycle_share_sheet::round_chip() const noexcept -> share_chip*
	{
		return RoundChip;
	}

	auto review_cycle_share_sheet::intro() const noexcept -> info_glyph*
	{
		return Intro;
	}

	auto review_cycle_share_sheet::status_panel() const noexcept -> share_panel*
	{
		return StatusPanel;
	}
	auto review_cycle_share_sheet::status() const noexcept -> QLabel*
	{
		return Status;
	}
	auto review_cycle_share_sheet::route_button(const core::cycle_share_route aRoute) const noexcept -> QPushButton*
	{
		return Routes.at(static_cast<std::size_t>(aRoute));
	}
	auto review_cycle_share_sheet::route_words(const core::cycle_share_route aRoute) const noexcept -> share_chip*
	{
		switch (aRoute) {
		case core::cycle_share_route::copy_document: return RouteWords[0];
		case core::cycle_share_route::save_document: return RouteWords[1];
		default:                                     return nullptr;
		}
	}
	auto review_cycle_share_sheet::action_button(const core::cycle_round_action aAction) const noexcept -> QPushButton*
	{
		const auto found = std::ranges::find(all_actions, aAction);
		return Actions.at(static_cast<std::size_t>(found - all_actions.begin()));
	}
	auto review_cycle_share_sheet::action_words() const noexcept -> QLabel*
	{
		return ActionWords;
	}
	auto review_cycle_share_sheet::goal() const noexcept -> QLineEdit*
	{
		return Goal;
	}
	auto review_cycle_share_sheet::notes_heading() const noexcept -> QLabel*
	{
		return NotesHeading;
	}
	auto review_cycle_share_sheet::notes_area() const noexcept -> QScrollArea*
	{
		return NotesArea;
	}
	auto review_cycle_share_sheet::all_code_button() const noexcept -> QToolButton*
	{
		return AllCode;
	}
	auto review_cycle_share_sheet::exact_text_button() const noexcept -> QPushButton*
	{
		return ExactText;
	}
	auto review_cycle_share_sheet::cancel_button() const noexcept -> QPushButton*
	{
		return Cancel;
	}
	auto review_cycle_share_sheet::deliveries() const noexcept -> QListWidget*
	{
		return Deliveries;
	}
	auto review_cycle_share_sheet::meaning() const noexcept -> info_glyph*
	{
		return Meaning;
	}
	auto review_cycle_share_sheet::invocation() const noexcept -> QLineEdit*
	{
		return Invocation;
	}
	auto review_cycle_share_sheet::replace_button() const noexcept -> QPushButton*
	{
		return ReplaceIt;
	}
	auto review_cycle_share_sheet::keep_button() const noexcept -> QPushButton*
	{
		return KeepIt;
	}
	auto review_cycle_share_sheet::replace_words() const noexcept -> QLabel*
	{
		return ReplaceText;
	}

	auto review_cycle_share_sheet::replace_panel() const noexcept -> share_panel*
	{
		return ReplaceRow;
	}
	auto review_cycle_share_sheet::viewer() const noexcept -> review_cycle_share_viewer*
	{
		return Viewer;
	}

	auto review_cycle_share_sheet::send_box() const noexcept -> QWidget*
	{
		return SendBox;
	}

	auto review_cycle_share_sheet::send_profile() const noexcept -> QComboBox*
	{
		return SendProfile;
	}

	auto review_cycle_share_sheet::send_session() const noexcept -> QComboBox*
	{
		return SendSession;
	}

	auto review_cycle_share_sheet::send_words() const noexcept -> QLabel*
	{
		return SendWords;
	}

	auto review_cycle_share_sheet::send_details() const noexcept -> QLabel*
	{
		return SendDetails;
	}

	auto review_cycle_share_sheet::send_more() const noexcept -> QPushButton*
	{
		return SendMore;
	}

	auto review_cycle_share_sheet::send_button() const noexcept -> QPushButton*
	{
		return SendIt;
	}

	auto review_cycle_share_sheet::stop_send_button() const noexcept -> QPushButton*
	{
		return StopSend;
	}

	auto review_cycle_share_sheet::reply_box() const noexcept -> QWidget*
	{
		return ReplyBox;
	}

	auto review_cycle_share_sheet::reply_text() const noexcept -> QPlainTextEdit*
	{
		return ReplyText;
	}

	auto review_cycle_share_sheet::add_reply_button() const noexcept -> QPushButton*
	{
		return AddReply;
	}
	auto review_cycle_share_sheet::rows() const noexcept -> const std::vector<note_row>&
	{
		return Rows;
	}

	auto review_cycle_share_sheet::listing() const noexcept -> bool
	{
		return Unlisted > 0U;
	}
}
