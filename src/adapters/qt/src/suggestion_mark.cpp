#include "bpr/qt/suggestion_mark.hpp"

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
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto text    = Decision.Applicability == core::suggestion_applicability::textual_only;
		const auto ink     = palette().color(QPalette::Active, text ? QPalette::PlaceholderText : QPalette::WindowText);
		const auto side    = static_cast<qreal>(std::min(width(), height()));
		const auto unit    = side / 16.0;
		const auto box     = QRectF{ (width() - side) / 2.0, (height() - side) / 2.0, side, side }.adjusted(1.5 * unit, 1.5 * unit, -1.5 * unit, -1.5 * unit);
		auto       outline = QPen{ ink, 1.2 * unit };
		if (text) {
			outline.setStyle(Qt::DashLine);
		}
		painter.setPen(outline);
		painter.setBrush(Qt::NoBrush);
		painter.drawRoundedRect(box, 2.5 * unit, 2.5 * unit);
		auto stroke = QPen{ ink, 1.4 * unit, Qt::SolidLine, Qt::RoundCap };
		painter.setPen(stroke);
		const auto centre = box.center();
		const auto arm    = 2.6 * unit;
		// A plus above a minus: the change and what it replaces.
		painter.drawLine(QPointF{ centre.x() - arm, centre.y() - 2.2 * unit }, QPointF{ centre.x() + arm, centre.y() - 2.2 * unit });
		painter.drawLine(QPointF{ centre.x(), centre.y() - 2.2 * unit - arm }, QPointF{ centre.x(), centre.y() - 2.2 * unit + arm });
		painter.drawLine(QPointF{ centre.x() - arm, centre.y() + 3.0 * unit }, QPointF{ centre.x() + arm, centre.y() + 3.0 * unit });
		if (text) {
			// Struck through: not applicable here.
			painter.drawLine(box.bottomLeft(), box.topRight());
		}
	}

	void suggestion_mark::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			update();
		}
	}
} // namespace bpr::qt
