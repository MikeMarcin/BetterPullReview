#include "bpr/qt/compose_review_sheet.hpp"

#include "bpr/qt/drawer_parts.hpp"

#include <QBoxLayout>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
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
		column->setContentsMargins(0, 0, 0, 0);
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
		// [[BPR-2175]]. The whole area, dimmed; the panel centred in it, as
		// wide as the area allows up to its widest, never narrower than the
		// composer's own minimum, and as tall as its content.
		setGeometry(aArea);
		const auto needed = Scroll->widget()->minimumSizeHint().width() + Scroll->verticalScrollBar()->sizeHint().width();
		const auto room   = std::max(0, aArea.width() - (2 * margin));
		const auto width  = std::min(room, std::max({ std::min(room, lightbox_width), minimum_width, needed }));
		// The body's height at the panel's width: rows that wrap are as tall
		// as their words there.
		auto* const body    = Scroll->widget();
		const auto  inner   = std::max(0, width - 2);
		const auto  tall    = body->hasHeightForWidth() ? std::max(body->heightForWidth(inner), body->minimumSizeHint().height()) : body->sizeHint().height();
		const auto  content = Header->sizeHint().height() + tall + 12;
		const auto  height  = std::clamp(content, std::min(240, aArea.height() - (2 * margin)), std::max(0, aArea.height() - (2 * margin)));
		Panel               = QRect{ (aArea.width() - width) / 2, (aArea.height() - height) / 2, width, height };
		// The scroll stops short of the panel's rounded foot.
		layout()->setContentsMargins(Panel.left() + 1, Panel.top() + 1, aArea.width() - Panel.right(), aArea.height() - Panel.bottom() + 9);
		raise();
		update();
	}

	auto compose_review_sheet::panel() const noexcept -> QRect
	{
		return Panel;
	}

	void compose_review_sheet::mousePressEvent(QMouseEvent* const aEvent)
	{
		// [[BPR-2175]]. A press on the dimmed window closes the lightbox,
		// its words kept; it goes no further.
		if (!Panel.contains(aEvent->position().toPoint()) && OnClose) {
			OnClose();
		}
		aEvent->accept();
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
		painter.setRenderHint(QPainter::Antialiasing, true);
		// The scrim: the window's text colour over everything outside the
		// panel, so it dims in Light, Dark and both high-contrast themes.
		const auto light = colours.color(QPalette::Active, QPalette::Window).lightness() > 128;
		auto       scrim = light ? colours.color(QPalette::Active, QPalette::WindowText) : QColor{ Qt::black };
		scrim.setAlpha(light ? 74 : 132);
		auto outside = QPainterPath{ };
		outside.addRect(QRectF{ rect() });
		auto inside = QPainterPath{ };
		inside.addRoundedRect(QRectF{ Panel }, 12.0, 12.0);
		painter.fillPath(outside.subtracted(inside), scrim);
		// A soft shadow under the panel, then the panel on Base.
		for (auto ring = 1; ring <= 6; ++ring) {
			auto shade = QColor{ Qt::black };
			shade.setAlpha(std::max(0, 22 - (ring * 3)));
			painter.setPen(QPen{ shade, 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ Panel }.adjusted(-ring, -ring + 2.0, ring, ring + 2.0), 12.0 + ring, 12.0 + ring);
		}
		painter.setPen(QPen{ colours.color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(colours.color(QPalette::Active, QPalette::Base));
		painter.drawRoundedRect(QRectF{ Panel }.adjusted(0.5, 0.5, -0.5, -0.5), 12.0, 12.0);
		// The header's rule.
		if (Header != nullptr) {
			const auto y = Header->geometry().bottom() + 1;
			painter.setRenderHint(QPainter::Antialiasing, false);
			painter.drawLine(Panel.left() + 1, y, Panel.right() - 1, y);
		}
	}
} // namespace bpr::qt
