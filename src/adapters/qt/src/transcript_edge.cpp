#include "bpr/qt/transcript_edge.hpp"

#include <QEvent>
#include <QLayout>
#include <QMargins>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollArea>
#include <QScrollBar>

namespace bpr::qt {
	auto transcript_edge::attach(QScrollArea& aArea) -> transcript_edge&
	{
		if (auto* const attached = of(aArea); attached != nullptr) {
			return *attached;
		}
		return *new transcript_edge{ aArea };
	}

	auto transcript_edge::of(const QScrollArea& aArea) -> transcript_edge*
	{
		for (auto* const child : aArea.children()) {
			if (auto* const attached = dynamic_cast<transcript_edge*>(child); attached != nullptr) {
				return attached;
			}
		}
		return nullptr;
	}

	auto transcript_edge::colour(const QPalette& aPalette) -> QColor
	{
		// The cards' own hairline, 16 parts in 100 of the way from the ground
		// to the ink, on the drawer's ground instead of a card's. In whole
		// steps of a channel, so what is drawn is this colour exactly.
		const auto ground = aPalette.color(QPalette::Active, QPalette::Window);
		const auto ink    = aPalette.color(QPalette::Active, QPalette::WindowText);
		const auto mix    = [](const int aGround, const int aInk) { return (aGround * 84 + aInk * 16 + 50) / 100; };
		return QColor{ mix(ground.red(), ink.red()), mix(ground.green(), ink.green()), mix(ground.blue(), ink.blue()) };
	}

	transcript_edge::transcript_edge(QScrollArea& aArea)
		: QWidget{ &aArea }
		, Area{ &aArea }
	{
		setObjectName(QStringLiteral("transcriptEdge"));
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAttribute(Qt::WA_OpaquePaintEvent);
		setFocusPolicy(Qt::NoFocus);
		hide();
		auto* const bar = aArea.verticalScrollBar();
		connect(bar, &QScrollBar::valueChanged, this, [this] { follow(); });
		connect(bar, &QScrollBar::rangeChanged, this, [this] { follow(); });
		// The viewport is placed by the area: with its frame, a scroll bar
		// that takes room, or the area's own size.
		aArea.viewport()->installEventFilter(this);
		follow();
	}

	void transcript_edge::follow()
	{
		if (Area == nullptr) {
			return;
		}
		const auto* const bar  = Area->verticalScrollBar();
		const auto        view = Area->viewport()->geometry();
		setGeometry(view.left(), view.top(), view.width(), 1);
		// The blank margin over the first turn is not a turn: a transcript a
		// pixel or two taller than its viewport cuts nothing, and says nothing.
		const auto* const turns  = Area->widget() != nullptr ? Area->widget()->layout() : nullptr;
		const auto        margin = turns != nullptr ? turns->contentsMargins().top() : 0;
		const auto        under  = bar->value() - bar->minimum() > margin;
		if (under != !isHidden()) {
			setVisible(under);
		}
		if (under) {
			raise();
		}
	}

	void transcript_edge::paintEvent(QPaintEvent* const aEvent)
	{
		auto painter = QPainter{ this };
		painter.fillRect(aEvent->rect(), colour(palette()));
	}

	auto transcript_edge::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (Area != nullptr && aWatched == Area->viewport() && (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::Move)) {
			follow();
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
