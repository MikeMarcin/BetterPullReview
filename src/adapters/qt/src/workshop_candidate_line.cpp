#include "bpr/qt/workshop_candidate_line.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/suggestion_mark.hpp"

#include <QEvent>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		/// \brief Between the pieces of one facet, and between facets.
		constexpr qreal piece_gap = 4.0;
		constexpr qreal facet_gap = 18.0;
		/// \brief The arrow between a before and an after mark.
		constexpr qreal arrow_width = 10.0;
		/// \brief The wavy underline of an unreadable header: its period and height.
		constexpr qreal wave_period = 4.0;
		constexpr qreal wave_height = 1.5;

		/// \brief One body's header as written: the label token and its
		/// decorations, and why the shared recognizer could not read it.
		[[nodiscard]] auto describe_header(const core::workshop_label_facts& aFacts) -> QString
		{
			auto text = aFacts.RawLabel.empty() ? QObject::tr("a header") : QString::fromStdString(aFacts.RawLabel);
			if (!aFacts.Decorations.empty()) {
				auto decorations = QStringList{ };
				for (const auto& decoration : aFacts.Decorations) {
					decorations.push_back(QString::fromStdString(decoration));
				}
				text += QStringLiteral(" (") + decorations.join(QStringLiteral(", ")) + QStringLiteral(")");
			}
			switch (aFacts.Status) {
			case core::feedback_prefix_status::recognized:
			case core::feedback_prefix_status::plain:
				return text;
			case core::feedback_prefix_status::unknown_label:
				return QObject::tr("%1, not a known label").arg(text);
			case core::feedback_prefix_status::malformed:
				return QObject::tr("%1, a header that cannot be read").arg(text);
			case core::feedback_prefix_status::limited:
				return QObject::tr("%1, a header too long to read").arg(text);
			}
			return text;
		}

		[[nodiscard]] auto label_sentence(const core::workshop_candidate_change& aChange) -> QString
		{
			switch (aChange.Label) {
			case core::workshop_label_change::none:
				break;
			case core::workshop_label_change::kept:
				return QObject::tr("Label kept: %1").arg(describe_header(aChange.After));
			case core::workshop_label_change::added:
				return QObject::tr("Label added: %1").arg(describe_header(aChange.After));
			case core::workshop_label_change::removed:
				return QObject::tr("Label removed: %1").arg(describe_header(aChange.Before));
			case core::workshop_label_change::changed:
				return QObject::tr("Label changed: %1 → %2").arg(describe_header(aChange.Before), describe_header(aChange.After));
			}
			return { };
		}

		[[nodiscard]] auto question_sentence(const core::workshop_question_change aChange) -> QString
		{
			switch (aChange) {
			case core::workshop_question_change::none:
				break;
			case core::workshop_question_change::kept:
				return QObject::tr("Still asks a question");
			case core::workshop_question_change::added:
				return QObject::tr("Now asks a question");
			case core::workshop_question_change::removed:
				return QObject::tr("No longer asks a question");
			}
			return { };
		}

		[[nodiscard]] auto patch_sentence(const core::workshop_patch_change aChange) -> QString
		{
			switch (aChange) {
			case core::workshop_patch_change::none:
				break;
			case core::workshop_patch_change::kept:
				return QObject::tr("Suggested change kept byte for byte");
			case core::workshop_patch_change::added:
				return QObject::tr("Adds a suggested change");
			case core::workshop_patch_change::removed:
				return QObject::tr("Removes the suggested change");
			case core::workshop_patch_change::changed:
				return QObject::tr("Changes the suggested change");
			}
			return { };
		}

		/// \brief The one word a changed facet carries beside its glyph.
		[[nodiscard]] auto change_word(const bool aAdded, const bool aRemoved) -> QString
		{
			return aAdded ? QObject::tr("added") : aRemoved ? QObject::tr("removed")
															: QObject::tr("changed");
		}

		[[nodiscard]] auto is_decoration(const std::string& aDecoration, const QLatin1StringView aName) -> bool
		{
			return QString::fromStdString(aDecoration).compare(aName, Qt::CaseInsensitive) == 0;
		}

		/// \brief The question facet: a speech bubble holding a `?`, in the
		/// marks' border weight. Not the `question` label's square, because
		/// it is about what the prose asks, whatever its label says.
		void paint_question_bubble(QPainter& aPainter, const QRectF& aBox, const QColor& aInk)
		{
			aPainter.save();
			aPainter.translate(aBox.topLeft());
			aPainter.scale(aBox.width() / feedback_mark_size, aBox.height() / feedback_mark_size);
			constexpr auto inset = feedback_mark_border / 2.0;
			auto           body  = QPainterPath{ };
			body.addRoundedRect(QRectF{ inset, inset, feedback_mark_size - feedback_mark_border, 12.0 }, 4.5, 4.5);
			auto tail = QPainterPath{ };
			tail.moveTo(3.5, 10.0);
			tail.lineTo(3.0, 15.25);
			tail.lineTo(8.0, 10.0);
			tail.closeSubpath();
			aPainter.setPen(QPen{ aInk, feedback_mark_border, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawPath(body.united(tail));
			// The `?` on the glyphs' stroke: an arc and a dot.
			aPainter.setPen(QPen{ aInk, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			auto hook = QPainterPath{ };
			hook.moveTo(6.0, 5.0);
			hook.cubicTo(6.0, 2.6, 10.0, 2.6, 10.0, 5.0);
			hook.cubicTo(10.0, 6.4, 8.0, 6.6, 8.0, 8.0);
			aPainter.drawPath(hook);
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(aInk);
			aPainter.drawEllipse(QPointF{ 8.0, 10.1 }, 0.95, 0.95);
			aPainter.restore();
		}
	} // namespace

	auto workshop_candidate_words(const core::workshop_candidate_change& aChange) -> QString
	{
		auto parts = QStringList{ };
		for (auto sentence : { label_sentence(aChange), question_sentence(aChange.Question), patch_sentence(aChange.Patch) }) {
			if (!sentence.isEmpty()) {
				parts.push_back(std::move(sentence));
			}
		}
		return parts.isEmpty() ? QObject::tr("Wording only") : parts.join(QStringLiteral(" · "));
	}

	workshop_candidate_line::workshop_candidate_line(QWidget* const aParent)
		: QWidget{ aParent }
		, Theme{ default_feedback_mark_theme(palette()) }
	{
		setFocusPolicy(Qt::NoFocus);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		setAccessibleName(tr("What the proposed revision changes"));
		// One tooltip per facet, right below it; the gaps show none.
		anchor_tooltips(*this, [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
			const auto index = facet_at(QPointF{ aPosition });
			if (!index) {
				return std::nullopt;
			}
			return anchored_tooltip_target{ .Anchor = facet_rect(*index).toAlignedRect(), .Text = facet_tooltip(*index) };
		});
	}

	void workshop_candidate_line::present(const core::workshop_candidate_change& aChange)
	{
		if (Change == aChange) {
			return;
		}
		Change = aChange;
		rebuild();
	}

	void workshop_candidate_line::clear()
	{
		if (!Change) {
			return;
		}
		Change.reset();
		rebuild();
	}

	void workshop_candidate_line::set_theme(const feedback_mark_theme& aTheme)
	{
		Theme = aTheme;
		update();
	}

	auto workshop_candidate_line::theme() const noexcept -> const feedback_mark_theme&
	{
		return Theme;
	}

	auto workshop_candidate_line::text() const -> QString
	{
		return Words;
	}

	auto workshop_candidate_line::facet_count() const noexcept -> std::size_t
	{
		return Facets.size();
	}

	auto workshop_candidate_line::facet(const std::size_t aIndex) const -> facet_kind
	{
		return Facets.at(aIndex).Kind;
	}

	auto workshop_candidate_line::facet_rect(const std::size_t aIndex) const -> QRectF
	{
		return Facets.at(aIndex).Box;
	}

	auto workshop_candidate_line::facet_tooltip(const std::size_t aIndex) const -> QString
	{
		return Facets.at(aIndex).Tooltip;
	}

	auto workshop_candidate_line::facet_changed(const std::size_t aIndex) const -> bool
	{
		return Facets.at(aIndex).Changed;
	}

	auto workshop_candidate_line::facet_at(const QPointF aPoint) const -> std::optional<std::size_t>
	{
		for (std::size_t index = 0U; index < Facets.size(); ++index) {
			if (Facets[index].Box.contains(aPoint)) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto workshop_candidate_line::sizeHint() const -> QSize
	{
		const auto height = std::max(feedback_mark_size, QFontMetricsF{ font() }.height()) + 4.0;
		return QSize{ static_cast<int>(std::ceil(Width)), static_cast<int>(std::ceil(height)) };
	}

	auto workshop_candidate_line::minimumSizeHint() const -> QSize
	{
		return QSize{ 0, sizeHint().height() };
	}

	void workshop_candidate_line::append_header(facet_layout& aFacet, const core::workshop_label_facts& aFacts, const bool aFaint) const
	{
		using kind = piece::kind;
		switch (aFacts.Status) {
		case core::feedback_prefix_status::plain:
			aFacet.Pieces.push_back(piece{ .Kind = kind::plain_mark });
			return;
		case core::feedback_prefix_status::recognized:
			if (aFacts.Label) {
				auto custom   = QStringList{ };
				auto blocking = false;
				auto lighter  = false;
				auto ifMinor  = false;
				for (const auto& decoration : aFacts.Decorations) {
					if (is_decoration(decoration, QLatin1StringView{ "blocking" })) {
						blocking = true;
					} else if (is_decoration(decoration, QLatin1StringView{ "non-blocking" })) {
						lighter = true;
					} else if (is_decoration(decoration, QLatin1StringView{ "if-minor" })) {
						ifMinor = true;
					} else {
						custom.push_back(QString::fromStdString(decoration));
					}
				}
				aFacet.Pieces.push_back(piece{ .Kind = kind::mark, .Label = *aFacts.Label, .Emphasis = feedback_mark_emphasis_of(blocking, lighter, ifMinor) });
				// The mark shows the three standard decorations; a custom one
				// is said as written.
				if (!custom.isEmpty()) {
					aFacet.Pieces.push_back(piece{ .Kind = kind::quiet_text, .Text = custom.join(QStringLiteral(", ")), .Faint = true });
				}
				return;
			}
			break;
		case core::feedback_prefix_status::unknown_label:
		case core::feedback_prefix_status::malformed:
		case core::feedback_prefix_status::limited:
			break;
		}
		auto token = aFacts.RawLabel.empty() ? tr("header") : QString::fromStdString(aFacts.RawLabel);
		if (token.size() > 24) {
			token = token.first(23) + QChar{ 0x2026 };
		}
		aFacet.Pieces.push_back(piece{ .Kind = kind::unreadable, .Text = token, .Faint = aFaint });
	}

	void workshop_candidate_line::rebuild()
	{
		using kind = piece::kind;
		Facets.clear();
		Words.clear();
		if (Change) {
			const auto& change = *Change;
			Words              = workshop_candidate_words(change);
			if (change.Label != core::workshop_label_change::none) {
				auto facet    = facet_layout{ .Kind = facet_kind::label, .Changed = change.Label != core::workshop_label_change::kept };
				facet.Tooltip = label_sentence(change) + QLatin1Char{ '\n' } + tr("Read the way Publish reads it.");
				if (facet.Changed) {
					append_header(facet, change.Before, false);
					facet.Pieces.push_back(piece{ .Kind = kind::arrow });
				}
				append_header(facet, change.After, false);
				Facets.push_back(std::move(facet));
			}
			if (change.Question != core::workshop_question_change::none) {
				const auto changed = change.Question != core::workshop_question_change::kept;
				auto       facet   = facet_layout{ .Kind = facet_kind::question, .Changed = changed, .Tooltip = question_sentence(change.Question) };
				facet.Pieces.push_back(piece{ .Kind = kind::question, .Faint = !changed });
				if (changed) {
					facet.Pieces.push_back(piece{ .Kind = kind::word, .Text = change_word(change.Question == core::workshop_question_change::added, change.Question == core::workshop_question_change::removed) });
				}
				Facets.push_back(std::move(facet));
			}
			if (change.Patch != core::workshop_patch_change::none) {
				const auto changed = change.Patch != core::workshop_patch_change::kept;
				auto       facet   = facet_layout{ .Kind = facet_kind::patch, .Changed = changed, .Tooltip = patch_sentence(change.Patch) };
				facet.Pieces.push_back(piece{ .Kind = kind::patch, .Faint = !changed });
				if (changed) {
					facet.Pieces.push_back(piece{ .Kind = kind::word, .Text = change_word(change.Patch == core::workshop_patch_change::added, change.Patch == core::workshop_patch_change::removed) });
				}
				Facets.push_back(std::move(facet));
			}
			if (Facets.empty()) {
				auto facet = facet_layout{ .Kind = facet_kind::wording, .Tooltip = tr("The label, the question and any suggested change are as they were.") };
				facet.Pieces.push_back(piece{ .Kind = kind::quiet_text, .Text = Words, .Faint = true });
				Facets.push_back(std::move(facet));
			}
		}
		setAccessibleDescription(Words);
		place();
	}

	void workshop_candidate_line::place()
	{
		using kind         = piece::kind;
		const auto metrics = QFontMetricsF{ font() };
		const auto row     = std::max(feedback_mark_size, metrics.height()) + 4.0;
		const auto top     = (row - feedback_mark_size) / 2.0;
		auto       x       = 1.0;
		for (auto& facet : Facets) {
			const auto left  = x;
			auto       first = true;
			for (auto& shown : facet.Pieces) {
				if (!first) {
					x += piece_gap;
				}
				first = false;
				switch (shown.Kind) {
				case kind::mark:
				case kind::plain_mark:
				case kind::patch:
				case kind::question:
					shown.Box = QRectF{ x, top, feedback_mark_size, feedback_mark_size };
					break;
				case kind::arrow:
					shown.Box = QRectF{ x, top, arrow_width, feedback_mark_size };
					break;
				case kind::quiet_text:
				case kind::word:
				case kind::unreadable:
					shown.Box = QRectF{ x, (row - metrics.height()) / 2.0, metrics.horizontalAdvance(shown.Text), metrics.height() };
					break;
				}
				x = shown.Box.right();
			}
			facet.Box = QRectF{ left - 2.0, 0.0, (x - left) + 4.0, row };
			x += facet_gap;
		}
		Width = Facets.empty() ? 0.0 : x - facet_gap + 1.0;
		updateGeometry();
		update();
	}

	void workshop_candidate_line::paintEvent(QPaintEvent* /*aEvent*/)
	{
		using kind   = piece::kind;
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  ink     = colours.color(QPalette::Active, QPalette::WindowText);
		const auto  faint   = feedback_state_faint(colours);
		const auto  warning = default_feedback_state_colours(colours).Open;
		for (const auto& facet : Facets) {
			for (const auto& shown : facet.Pieces) {
				const auto colour = shown.Faint ? faint : ink;
				switch (shown.Kind) {
				case kind::mark:
					paint_feedback_mark(painter, shown.Box, shown.Label, shown.Emphasis, feedback_mark_colours_for(Theme, shown.Label, colours));
					break;
				case kind::plain_mark:
					painter.drawPixmap(shown.Box.topLeft(), feedback_plain_mark_pixmap(colours, devicePixelRatioF()));
					break;
				case kind::patch:
					paint_suggestion_mark(painter, shown.Box, core::suggestion_applicability::native, colour);
					break;
				case kind::question:
					paint_question_bubble(painter, shown.Box, colour);
					break;
				case kind::arrow: {
					// A thin arrow in the faint ink: the change is the marks.
					const auto middle = shown.Box.center().y();
					auto       pen    = QPen{ faint, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin };
					painter.setPen(pen);
					painter.setBrush(Qt::NoBrush);
					painter.drawLine(QPointF{ shown.Box.left() + 1.0, middle }, QPointF{ shown.Box.right() - 1.0, middle });
					auto head = QPainterPath{ };
					head.moveTo(shown.Box.right() - 4.0, middle - 3.0);
					head.lineTo(shown.Box.right() - 1.0, middle);
					head.lineTo(shown.Box.right() - 4.0, middle + 3.0);
					painter.drawPath(head);
					break;
				}
				case kind::quiet_text:
				case kind::word:
					painter.setPen(colour);
					painter.drawText(shown.Box, Qt::AlignLeft | Qt::AlignVCenter, shown.Text);
					break;
				case kind::unreadable: {
					painter.setPen(colour);
					painter.drawText(shown.Box, Qt::AlignLeft | Qt::AlignVCenter, shown.Text);
					// The editor's own sign for text it cannot read.
					const auto baseline = shown.Box.top() + QFontMetricsF{ font() }.ascent() + 2.0;
					auto       wave     = QPainterPath{ };
					wave.moveTo(shown.Box.left(), baseline);
					const auto waveCount = static_cast<int>(std::ceil(shown.Box.width() / wave_period));
					for (auto index = 0; index < waveCount; ++index) {
						const auto at = shown.Box.left() + index * wave_period;
						wave.quadTo(QPointF{ at + wave_period / 4.0, baseline - wave_height }, QPointF{ at + wave_period / 2.0, baseline });
						wave.quadTo(QPointF{ at + 3.0 * wave_period / 4.0, baseline + wave_height }, QPointF{ at + wave_period, baseline });
					}
					painter.setPen(QPen{ warning, 1.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawPath(wave);
					break;
				}
				}
			}
		}
	}

	void workshop_candidate_line::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange) {
			place();
		} else if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			update();
		}
	}
} // namespace bpr::qt
