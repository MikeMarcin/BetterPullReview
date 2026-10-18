#include "bpr/qt/compose_review_sheet.hpp"

#include "bpr/qt/drawer_parts.hpp"

#include <QBoxLayout>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	compose_review_sheet::compose_review_sheet(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("composeReviewSheet"));
		setAccessibleName(QStringLiteral("Finish your review"));
		setAttribute(Qt::WA_StyledBackground, false);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(shadow_width, 0, 0, 0);
		column->setSpacing(0);
		auto* const header = new QWidget{ this };
		Header             = header;
		header->setObjectName(QStringLiteral("composeReviewSheetHeader"));
		auto* const row = new QHBoxLayout{ header };
		row->setContentsMargins(16, 12, 10, 10);
		row->setSpacing(4);
		Title = new QLabel{ QStringLiteral("Finish your review"), header };
		Title->setObjectName(QStringLiteral("composeReviewSheetTitle"));
		Title->setTextFormat(Qt::PlainText);
		auto font = Title->font();
		font.setPointSizeF(font.pointSizeF() * 1.25);
		font.setWeight(QFont::DemiBold);
		Title->setFont(font);
		row->addWidget(Title, 1);
		// Esc closes it, said where the eye looks for the way out (layout C).
		auto* const key = new QLabel{ QStringLiteral("Esc"), header };
		key->setObjectName(QStringLiteral("composeReviewSheetKey"));
		key->setForegroundRole(QPalette::PlaceholderText);
		key->setAccessibleName(QStringLiteral("Esc closes this sheet"));
		row->addWidget(key);
		Expand = make_drawer_tool(header, QIcon{ }, QStringLiteral("Expand"), QStringLiteral("composeReviewSheetExpand"));
		Close  = make_drawer_tool(header, QIcon{ }, QStringLiteral("Close"), QStringLiteral("composeReviewSheetClose"));
		row->addWidget(Expand);
		row->addWidget(Close);
		column->addWidget(header);
		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("composeReviewSheetScroll"));
		Scroll->setWidgetResizable(true);
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const body = new QWidget{ Scroll };
		body->setObjectName(QStringLiteral("composeReviewSheetBody"));
		Content = new QVBoxLayout{ body };
		Content->setContentsMargins(16, 12, 16, 20);
		Content->setSpacing(10);
		Scroll->setWidget(body);
		column->addWidget(Scroll, 1);
		connect(Expand, &QToolButton::clicked, this, [this] {
			if (OnExpand) {
				OnExpand();
			}
		});
		connect(Close, &QToolButton::clicked, this, [this] {
			if (OnClose) {
				OnClose();
			}
		});
		hide();
	}

	void compose_review_sheet::set_tools(const QIcon& aExpand, const QString& aExpandName, std::function<void()> aOnExpand, const QIcon& aClose, const QString& aCloseName,
										 std::function<void()> aOnClose)
	{
		Expand->setIcon(aExpand);
		Expand->setToolTip(aExpandName);
		Expand->setAccessibleName(aExpandName);
		Close->setIcon(aClose);
		Close->setToolTip(aCloseName);
		Close->setAccessibleName(aCloseName);
		OnExpand = std::move(aOnExpand);
		OnClose  = std::move(aOnClose);
	}

	void compose_review_sheet::place(const QRect& aArea)
	{
		// Leave the surface beneath at least 320 pixels, so the code stays
		// beside the sheet; never narrower than 360.
		// Large text can need more than 540: the sheet grows to its content's
		// minimum rather than scrolling sideways.
		const auto needed    = Scroll->widget()->minimumSizeHint().width() + Scroll->verticalScrollBar()->sizeHint().width();
		const auto preferred = std::max(preferred_width, needed);
		const auto width     = std::clamp(aArea.width() - 320, std::min(360, preferred), preferred) + shadow_width;
		setGeometry(QRect{ aArea.right() + 1 - std::min(width, aArea.width()), aArea.top(), std::min(width, aArea.width()), aArea.height() });
		raise();
	}

	auto compose_review_sheet::scroll() const noexcept -> QScrollArea*
	{
		return Scroll;
	}

	auto compose_review_sheet::content_layout() const noexcept -> QVBoxLayout*
	{
		return Content;
	}

	auto compose_review_sheet::title() const noexcept -> QLabel*
	{
		return Title;
	}

	auto compose_review_sheet::expand_button() const noexcept -> QToolButton*
	{
		return Expand;
	}

	auto compose_review_sheet::close_button() const noexcept -> QToolButton*
	{
		return Close;
	}

	void compose_review_sheet::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto        painter = QPainter{ this };
		const auto& colours = palette();
		// The shadow: the window's text colour fading to nothing over the
		// surface, so it reads in Light, Dark and both high-contrast themes.
		auto dark = colours.color(QPalette::Active, QPalette::WindowText);
		dark.setAlpha(46);
		auto clear = dark;
		clear.setAlpha(0);
		auto shade = QLinearGradient{ QPointF{ static_cast<qreal>(shadow_width), 0.0 }, QPointF{ 0.0, 0.0 } };
		shade.setColorAt(0.0, dark);
		shade.setColorAt(1.0, clear);
		painter.fillRect(QRect{ 0, 0, shadow_width, height() }, shade);
		painter.fillRect(QRect{ shadow_width, 0, width() - shadow_width, height() }, colours.color(QPalette::Active, QPalette::Base));
		painter.setPen(colours.color(QPalette::Active, QPalette::Mid));
		painter.drawLine(shadow_width, 0, shadow_width, height());
		// The header's rule.
		if (Header != nullptr) {
			const auto y = Header->geometry().bottom() + 1;
			painter.drawLine(shadow_width, y, width(), y);
		}
	}
} // namespace bpr::qt
