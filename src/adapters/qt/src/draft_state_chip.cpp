#include "bpr/qt/draft_state_chip.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPen>

#include <algorithm>

namespace bpr::qt {
	namespace {
		constexpr int pad_x  = 7;
		constexpr int gap    = 5;
		constexpr int pad_y  = 2;
		constexpr int ring_d = 8;
	} // namespace

	draft_state_chip::draft_state_chip(QWidget* const aParent)
		: QLabel{ QStringLiteral("Draft"), aParent }
	{
		setObjectName(QStringLiteral("draftStateChip"));
		setTextFormat(Qt::PlainText);
		setFocusPolicy(Qt::NoFocus);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		auto bold = font();
		bold.setWeight(QFont::DemiBold);
		setFont(bold);
		setAccessibleName(QStringLiteral("Draft pull request"));
		setToolTip(QStringLiteral("This pull request is a draft: GitHub does not ask anyone to review it until it is marked ready for review."));
	}

	auto draft_state_chip::sizeHint() const -> QSize
	{
		const auto metrics = QFontMetrics{ font() };
		return { pad_x + ring_d + gap + metrics.horizontalAdvance(text()) + pad_x + 1, metrics.height() + 2 * pad_y };
	}

	auto draft_state_chip::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void draft_state_chip::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
		// GitHub's draft grey is the theme's quiet ink; the word is the paper's
		// colour on it, so it holds its contrast in light and dark themes.
		const auto fill = palette().color(group, QPalette::PlaceholderText);
		const auto ink  = palette().color(group, QPalette::Base);
		const auto hint = sizeHint();
		const auto box  = QRectF{ 0.0, std::max(0.0, (height() - hint.height()) / 2.0), static_cast<qreal>(hint.width()), static_cast<qreal>(hint.height()) };
		painter.setPen(Qt::NoPen);
		painter.setBrush(fill);
		painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
		// A dashed ring: the circle GitHub's open mark is, not yet closed.
		auto ring = QPen{ ink, 1.4 };
		ring.setDashPattern({ 1.6, 1.4 });
		painter.setPen(ring);
		painter.setBrush(Qt::NoBrush);
		const auto ringBox = QRectF{ box.left() + pad_x, box.center().y() - ring_d / 2.0, static_cast<qreal>(ring_d), static_cast<qreal>(ring_d) };
		painter.drawEllipse(ringBox.adjusted(0.7, 0.7, -0.7, -0.7));
		painter.setPen(ink);
		painter.setFont(font());
		const auto textLeft = box.left() + pad_x + ring_d + gap;
		painter.drawText(QRectF{ textLeft, box.top(), box.right() - textLeft, box.height() }, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
	}
} // namespace bpr::qt
