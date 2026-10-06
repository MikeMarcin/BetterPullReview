#include "bpr/qt/suggestion_mark.hpp"

#include "bpr/qt/feedback_label_marks.hpp"

#include <QEvent>
#include <QPainter>
#include <QPen>

#include <algorithm>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto reason_text(const core::suggestion_text_reason aReason) -> QString
		{
			using reason = core::suggestion_text_reason;
			switch (aReason) {
			case reason::base_side:
				return QStringLiteral("the line is on the base side");
			case reason::file_subject:
				return QStringLiteral("this thread is a file comment");
			case reason::outdated_line:
				return QStringLiteral("this thread's line is outdated");
			case reason::unavailable_anchor:
				return QStringLiteral("this thread has no current line");
			case reason::conversation:
				return QStringLiteral("a pull request comment has no line");
			case reason::inline_comment_unsupported:
				return QStringLiteral("GitHub refused an inline comment on this line, and a suggestion cannot go as a file comment");
			case reason::none:
			case reason::invalid_observation:
				break;
			}
			return QStringLiteral("where it goes is not known");
		}
	} // namespace

	auto suggestion_mark_name(const core::suggestion_applicability_decision& aDecision) -> QString
	{
		switch (aDecision.Applicability) {
		case core::suggestion_applicability::native:
			return QStringLiteral("Suggested change, native");
		case core::suggestion_applicability::textual_only:
			return QStringLiteral("Suggested change, text only");
		case core::suggestion_applicability::no_patch:
			break;
		}
		return { };
	}

	auto suggestion_mark_tooltip(const core::suggestion_applicability_decision& aDecision) -> QString
	{
		switch (aDecision.Applicability) {
		case core::suggestion_applicability::native:
			return QStringLiteral("Suggested change: posted on this line as GitHub's own suggestion, which is applied on GitHub.");
		case core::suggestion_applicability::textual_only:
			return QStringLiteral("Suggested change, text only: %1. GitHub cannot apply it here. Your text is kept as written, and nothing moves to another line.")
				.arg(reason_text(aDecision.Reason));
		case core::suggestion_applicability::no_patch:
			break;
		}
		return { };
	}

	suggestion_mark::suggestion_mark(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("suggestionMark"));
		setFocusPolicy(Qt::NoFocus);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		setProperty("suggestionApplicability", QString::fromLatin1(core::stable_name(Decision.Applicability).data()));
		setProperty("suggestionReason", QString::fromLatin1(core::stable_name(Decision.Reason).data()));
		hide();
	}

	void suggestion_mark::present(const core::suggestion_applicability_decision& aDecision)
	{
		const auto shown = aDecision.Applicability != core::suggestion_applicability::no_patch;
		if (aDecision == Decision && isHidden() != shown) {
			return;
		}
		Decision = aDecision;
		setProperty("suggestionApplicability", QString::fromLatin1(core::stable_name(Decision.Applicability).data()));
		setProperty("suggestionReason", QString::fromLatin1(core::stable_name(Decision.Reason).data()));
		setAccessibleName(suggestion_mark_name(Decision));
		setAccessibleDescription(suggestion_mark_tooltip(Decision));
		setToolTip(suggestion_mark_tooltip(Decision));
		setVisible(shown);
		update();
	}

	auto suggestion_mark::decision() const noexcept -> const core::suggestion_applicability_decision&
	{
		return Decision;
	}

	auto suggestion_mark::sizeHint() const -> QSize
	{
		return QSize{ 16, 16 };
	}

	auto suggestion_mark::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void suggestion_mark::paintEvent(QPaintEvent* /*aEvent*/)
	{
		if (Decision.Applicability == core::suggestion_applicability::no_patch) {
			return;
		}
		auto painter = QPainter{ this };
		paint_suggestion_mark(painter, QRectF{ rect() }, Decision.Applicability, palette().color(QPalette::Active, QPalette::WindowText));
	}

	void paint_suggestion_mark(QPainter& aPainter, const QRectF& aBox, const core::suggestion_applicability aApplicability, const QColor& aInk)
	{
		if (aApplicability == core::suggestion_applicability::no_patch) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		// Drawn on the 16 px mark's own units, centred in the box.
		const auto side = std::min(aBox.width(), aBox.height());
		aPainter.translate(aBox.center() - QPointF{ side / 2.0, side / 2.0 });
		aPainter.scale(side / feedback_mark_size, side / feedback_mark_size);
		// A state, so a ring (BPR-1083: labels are square, states round), with
		// the marks' border. Text only opens it into the accepted dashes, as a
		// non-blocking mark's border: the same change, a lighter form.
		auto ring = QPen{ aInk, feedback_mark_border };
		if (aApplicability == core::suggestion_applicability::textual_only) {
			ring.setCapStyle(Qt::FlatCap);
			ring.setDashPattern({ feedback_mark_dash / feedback_mark_border, feedback_mark_dash_gap / feedback_mark_border });
		}
		aPainter.setPen(ring);
		aPainter.setBrush(Qt::NoBrush);
		const auto inset = feedback_mark_border / 2.0;
		aPainter.drawEllipse(QRectF{ inset, inset, feedback_mark_size - feedback_mark_border, feedback_mark_size - feedback_mark_border });
		// A plus over a minus: the change and what it replaces, in the
		// glyphs' stroke (2.6 units of BPR-1080's 24-unit grid in a 12 px box).
		aPainter.setPen(QPen{ aInk, feedback_glyph_pen * feedback_mark_glyph_size / feedback_glyph_grid, Qt::SolidLine, Qt::RoundCap });
		// The bars sit on pixel centres, so both are crisp at 1x, with a clear
		// pixel between them, and the pair is centred in the ring.
		constexpr auto centre = feedback_mark_size / 2.0;
		constexpr auto arm    = 2.3;
		constexpr auto plus   = 6.5;
		constexpr auto minus  = 11.5;
		aPainter.drawLine(QPointF{ centre - arm, plus }, QPointF{ centre + arm, plus });
		aPainter.drawLine(QPointF{ centre, plus - arm }, QPointF{ centre, plus + arm });
		aPainter.drawLine(QPointF{ centre - arm, minus }, QPointF{ centre + arm, minus });
		aPainter.restore();
	}

	void suggestion_mark::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			update();
		}
	}
} // namespace bpr::qt
