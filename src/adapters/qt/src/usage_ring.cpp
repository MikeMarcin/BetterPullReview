#include "bpr/qt/usage_ring.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionFocusRect>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int ring_extent_pixels = 18;

		[[nodiscard]] auto reset_phrase(
			const std::uint64_t aResetUnixSeconds,
			const std::uint64_t aNowUnixSeconds) -> QString
		{
			if (aResetUnixSeconds <= aNowUnixSeconds) {
				return QStringLiteral("Resets now");
			}
			const auto remaining = aResetUnixSeconds - aNowUnixSeconds;
			const auto hours     = remaining / 3'600U;
			const auto minutes   = (remaining % 3'600U) / 60U;
			if (hours == 0U) {
				return QStringLiteral("Resets in %1 min")
					.arg(static_cast<qulonglong>(minutes));
			}
			return QStringLiteral("Resets in %1 hr %2 min")
				.arg(static_cast<qulonglong>(hours))
				.arg(static_cast<qulonglong>(minutes));
		}

		[[nodiscard]] auto consumed(
			const application::public_preview_budget& aBudget) -> double
		{
			if (aBudget.Limit == 0U) {
				return 0.0;
			}
			const auto spent = aBudget.Limit > aBudget.Remaining
				? aBudget.Limit - aBudget.Remaining
				: 0U;
			return std::clamp(
				static_cast<double>(spent) / static_cast<double>(aBudget.Limit),
				0.0,
				1.0);
		}

		[[nodiscard]] auto reason_text(
			const application::public_preview_refusal_reason         aReason,
			const std::optional<application::public_preview_budget>& aBudget,
			const std::uint64_t                                      aNowUnixSeconds) -> QString
		{
			switch (aReason) {
			case application::public_preview_refusal_reason::budget_exhausted:
				// A budget refusal names the limit and the reset. Without a
				// budget fact it still refuses, because a refusal that stayed
				// silent about why would be worse than one short of a number.
				return aBudget.has_value()
					? QStringLiteral(
						  "The shared hourly limit of %1 requests is spent. %2.")
						  .arg(static_cast<qulonglong>(aBudget->Limit))
						  .arg(reset_phrase(
							  aBudget->ResetUnixSeconds, aNowUnixSeconds))
					: QStringLiteral("The shared hourly limit is spent.");
			case application::public_preview_refusal_reason::private_or_missing:
				return QStringLiteral(
					"This repository is private, or it does not exist. Public "
					"preview cannot tell the two apart.");
			case application::public_preview_refusal_reason::
				enterprise_host_unavailable:
				return QStringLiteral(
					"GitHub Enterprise Server is unavailable in public "
					"preview. GitHub.com only.");
			case application::public_preview_refusal_reason::
				review_threads_unavailable:
				return QStringLiteral(
					"Review threads are unavailable in public preview.");
			case application::public_preview_refusal_reason::
				viewer_identity_unavailable:
				return QStringLiteral(
					"Viewer identity is unavailable in public preview.");
			case application::public_preview_refusal_reason::
				triage_or_publishing_refused:
				return QStringLiteral(
					"Triage and publishing are unavailable in public preview.");
			case application::public_preview_refusal_reason::mode_unavailable:
				break;
			}
			return QStringLiteral("Public preview is unavailable in this build.");
		}
	} // namespace

	auto public_preview_mode_text() -> QString
	{
		return QStringLiteral("Public preview — signed out");
	}

	auto public_preview_budget_text(
		const application::public_preview_budget& aBudget,
		const std::uint64_t                       aNowUnixSeconds) -> QString
	{
		const auto percent = static_cast<qulonglong>(
			std::llround(consumed(aBudget) * 100.0));
		return QStringLiteral("%1% of the hourly limit · %2")
			.arg(percent)
			.arg(reset_phrase(aBudget.ResetUnixSeconds, aNowUnixSeconds));
	}

	auto public_preview_limitation_text(
		const application::public_preview_presentation& aPresentation)
		-> QString
	{
		QStringList lines;
		for (const auto& limitation : aPresentation.Limitations) {
			lines.append(QString::fromStdString(limitation));
		}
		return lines.join(QStringLiteral("\n"));
	}

	auto public_preview_sign_in_text() -> QString
	{
		return QStringLiteral("Sign in to review with your account.");
	}

	auto public_preview_refusal_text(
		const application::public_preview_refusal_presentation& aRefusal,
		const std::uint64_t                                     aNowUnixSeconds) -> QString
	{
		auto text = reason_text(
			aRefusal.Reason, aRefusal.Budget, aNowUnixSeconds);
		// `SignInOffered` is a property of the refusal type rather than a flag
		// a caller sets, so the way out is appended unconditionally here.
		static_assert(
			application::public_preview_refusal_presentation::SignInOffered);
		text.append(QStringLiteral("\n"));
		text.append(public_preview_sign_in_text());
		return text;
	}

	auto public_preview_unavailable_text(const QString& aControl) -> QString
	{
		return QStringLiteral("%1 is unavailable in public preview.")
			.arg(aControl);
	}

	usage_ring::usage_ring(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("publicPreviewUsageRing"));
		setAccessibleName(QStringLiteral("Public preview hourly limit"));
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::PointingHandCursor);
		setMinimumSize(sizeHint());
		refresh_text();
	}

	void usage_ring::apply(
		const application::public_preview_presentation& aPresentation,
		const std::uint64_t                             aNowUnixSeconds)
	{
		Presentation   = aPresentation;
		NowUnixSeconds = aPresentation.NowUnixSeconds != 0U
			? aPresentation.NowUnixSeconds
			: aNowUnixSeconds;
		refresh_text();
		update();
	}

	void usage_ring::set_dispatch(
		application::public_preview_gesture_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto usage_ring::consumed_fraction() const noexcept -> double
	{
		return Presentation.Budget.has_value() ? consumed(*Presentation.Budget)
											   : 0.0;
	}

	auto usage_ring::has_budget() const noexcept -> bool
	{
		return Presentation.Budget.has_value();
	}

	auto usage_ring::tooltip_text() const -> QString
	{
		QStringList lines;
		lines.append(
			Presentation.Budget.has_value()
				? public_preview_budget_text(*Presentation.Budget, NowUnixSeconds)
				: QStringLiteral("The hourly limit has not been observed yet."));
		lines.append(public_preview_limitation_text(Presentation));
		lines.append(public_preview_sign_in_text());
		return lines.join(QStringLiteral("\n"));
	}

	auto usage_ring::sizeHint() const -> QSize
	{
		return { ring_extent_pixels, ring_extent_pixels };
	}

	void usage_ring::refresh_text()
	{
		const auto text = tooltip_text();
		setToolTip(text);
		setAccessibleDescription(text);
	}

	void usage_ring::emit_sign_in()
	{
		// Sign-in is offered by the signed-out mode, so a ring that was never
		// told which mode it is in offers nothing. A gesture that escaped an
		// unpublished ring would be a way into the device flow that no
		// presentation asked for.
		if (Presentation.Mode != application::public_preview_mode::signed_out) {
			return;
		}
		if (Dispatch) {
			Dispatch(application::public_preview_sign_in_gesture{ });
		}
	}

	void usage_ring::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter
			|| aEvent->key() == Qt::Key_Space) {
			emit_sign_in();
			aEvent->accept();
			return;
		}
		QWidget::keyPressEvent(aEvent);
	}

	void usage_ring::mouseReleaseEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() == Qt::LeftButton
			&& rect().contains(aEvent->position().toPoint())) {
			emit_sign_in();
			aEvent->accept();
			return;
		}
		QWidget::mouseReleaseEvent(aEvent);
	}

	void usage_ring::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		QPainter painter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto side      = std::min(width(), height());
		const auto thickness = std::max(2, side / 6);
		auto       arc       = QRect{ 0, 0, side, side };
		arc.moveCenter(rect().center());
		arc.adjust(thickness, thickness, -thickness, -thickness);

		// Palette-derived, never overridden: the ring follows whatever theme
		// the application and the operating system are in.
		QPen track{ palette().color(QPalette::Mid) };
		track.setWidth(thickness);
		track.setCapStyle(Qt::FlatCap);
		painter.setPen(track);
		painter.drawArc(arc, 0, 360 * 16);

		if (Presentation.Budget.has_value()) {
			QPen spent{ palette().color(QPalette::Highlight) };
			spent.setWidth(thickness);
			spent.setCapStyle(Qt::FlatCap);
			painter.setPen(spent);
			const auto span = static_cast<int>(
				std::llround(consumed_fraction() * 360.0 * 16.0));
			// Clockwise from twelve o'clock, so a fuller ring reads as less
			// budget left rather than more.
			painter.drawArc(arc, 90 * 16, -span);
		}

		if (hasFocus()) {
			QStyleOptionFocusRect option;
			option.initFrom(this);
			option.rect = rect();
			style()->drawPrimitive(
				QStyle::PE_FrameFocusRect, &option, &painter, this);
		}
	}
} // namespace bpr::qt
