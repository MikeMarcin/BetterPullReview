#ifndef BPR_SRC_ADAPTERS_QT_SRC_SETUP_STEP_PARTS_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_SETUP_STEP_PARTS_HPP

#include <QColor>
#include <QEnterEvent>
#include <QFont>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>

/// \file
/// \brief The setup surfaces' shared parts: the numbered step marker, the
/// step header, the whole-card choice and the link-coloured button.
///
/// \details RFC-BPR-0092 drew them for the Agent tab's setup; RFC-BPR-0032
/// amendment r1 draws the account setup with the same parts, so both setups
/// read as one language. Presentation only, private to this adapter.
namespace bpr::qt::setup_parts {
	[[nodiscard]] inline auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
	{
		const auto keep = 1.0 - aShare;
		return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
								static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
	}

	[[nodiscard]] inline auto dark(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Window).lightness() < 128; }

	[[nodiscard]] inline auto passed_colour(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 }; }
	[[nodiscard]] inline auto failed_colour(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E }; }

	/// \brief A step's marker: its number while open or ahead, a tick once
	/// done. The steps are a real sequence, so they are numbered.
	class step_marker final : public QWidget
	{
	public:
		enum class state : std::uint8_t {
			ahead,
			open,
			done,
		};
		step_marker(const int aNumber, const state aState, QWidget* aParent)
			: QWidget{ aParent }
			, Number{ aNumber }
			, State{ aState }
		{
			setFixedSize(24, 24);
			setAccessibleName(aState == state::done ? QObject::tr("Done") : QObject::tr("Step %1").arg(aNumber));
		}

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto box    = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
			const auto accent = palette().color(QPalette::Active, QPalette::Highlight);
			const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
			if (State == state::done) {
				painter.setPen(Qt::NoPen);
				painter.setBrush(passed_colour(palette()));
				painter.drawEllipse(box);
				auto tick = QPen{ QColor{ Qt::white }, 2.0 };
				tick.setCapStyle(Qt::RoundCap);
				tick.setJoinStyle(Qt::RoundJoin);
				painter.setPen(tick);
				const auto c = box.center();
				painter.drawPolyline(std::array{ QPointF{ c.x() - 4.5, c.y() }, QPointF{ c.x() - 1.5, c.y() + 3.0 }, QPointF{ c.x() + 4.5, c.y() - 3.5 } }.data(), 3);
				return;
			}
			painter.setPen(State == state::open ? QPen{ Qt::NoPen } : QPen{ mixed(palette().color(QPalette::Window), ink, 0.35), 1.2 });
			painter.setBrush(State == state::open ? accent : QBrush{ Qt::NoBrush });
			painter.drawEllipse(box);
			auto number = font();
			number.setWeight(QFont::DemiBold);
			number.setPointSizeF(std::max(1.0, number.pointSizeF() * 0.85));
			painter.setFont(number);
			painter.setPen(State == state::open ? palette().color(QPalette::Active, QPalette::HighlightedText) : mixed(palette().color(QPalette::Window), ink, 0.55));
			painter.drawText(box, Qt::AlignCenter, QString::number(Number));
		}

	private:
		int   Number;
		state State;
	};

	/// \brief A choice card: an agent, a clone, a way to sign in or an
	/// account, the chosen one outlined.
	/// \details Owner look v002 ("Why do I have to click a little circle
	/// instead of the whole card"): the whole card chooses, by a click or,
	/// with the keyboard focus on it, Space or Return. The radio inside only
	/// shows the choice.
	class choice_card final : public QWidget
	{
	public:
		explicit choice_card(QWidget* aParent)
			: QWidget{ aParent }
		{
			setFocusPolicy(Qt::StrongFocus);
			setCursor(Qt::PointingHandCursor);
		}
		bool                  Chosen   = false;
		bool                  Disabled = false;
		std::function<void()> Activate;

		void settle()
		{
			setFocusPolicy(Disabled ? Qt::NoFocus : Qt::StrongFocus);
			setCursor(Disabled ? Qt::ArrowCursor : Qt::PointingHandCursor);
			for (auto* const radio : findChildren<QRadioButton*>()) {
				radio->setFocusPolicy(Qt::NoFocus);
				radio->setAttribute(Qt::WA_TransparentForMouseEvents, true);
			}
		}

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto base   = palette().color(QPalette::Active, QPalette::Base);
			const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
			const auto accent = palette().color(QPalette::Active, QPalette::Highlight);
			const auto box    = QRectF{ rect() }.adjusted(0.75, 0.75, -0.75, -0.75);
			const auto ring   = Chosen || hasFocus();
			painter.setPen(QPen{ ring ? accent : mixed(base, ink, 0.16), hasFocus() ? 2.0 : (Chosen ? 1.5 : 1.0) });
			painter.setBrush(Chosen ? mixed(base, accent, 0.06) : (Disabled ? mixed(base, ink, 0.03) : (Hovered ? mixed(base, accent, 0.03) : base)));
			painter.drawRoundedRect(box, 8.0, 8.0);
		}
		void mouseReleaseEvent(QMouseEvent* aEvent) override
		{
			if (aEvent->button() == Qt::LeftButton && rect().contains(aEvent->position().toPoint()) && !Disabled && Activate) {
				Activate();
				return;
			}
			QWidget::mouseReleaseEvent(aEvent);
		}
		void keyPressEvent(QKeyEvent* aEvent) override
		{
			if ((aEvent->key() == Qt::Key_Space || aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) && !Disabled && Activate) {
				Activate();
				return;
			}
			QWidget::keyPressEvent(aEvent);
		}
		void enterEvent(QEnterEvent* aEvent) override
		{
			Hovered = true;
			update();
			QWidget::enterEvent(aEvent);
		}
		void leaveEvent(QEvent* aEvent) override
		{
			Hovered = false;
			update();
			QWidget::leaveEvent(aEvent);
		}
		void focusInEvent(QFocusEvent* aEvent) override
		{
			update();
			QWidget::focusInEvent(aEvent);
		}
		void focusOutEvent(QFocusEvent* aEvent) override
		{
			update();
			QWidget::focusOutEvent(aEvent);
		}

	private:
		bool Hovered = false;
	};

	/// \brief A step's header row. A step already reached opens with a
	/// click or, focused, Space or Return (owner look v002: "click (2) to
	/// continue").
	class step_header final : public QWidget
	{
	public:
		explicit step_header(QWidget* aParent)
			: QWidget{ aParent }
		{ }
		std::function<void()> Activate;

		void settle()
		{
			setFocusPolicy(Activate ? Qt::StrongFocus : Qt::NoFocus);
			setCursor(Activate ? Qt::PointingHandCursor : Qt::ArrowCursor);
		}

	protected:
		void mouseReleaseEvent(QMouseEvent* aEvent) override
		{
			if (aEvent->button() == Qt::LeftButton && Activate && rect().contains(aEvent->position().toPoint())) {
				Activate();
				return;
			}
			QWidget::mouseReleaseEvent(aEvent);
		}
		void keyPressEvent(QKeyEvent* aEvent) override
		{
			if ((aEvent->key() == Qt::Key_Space || aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) && Activate) {
				Activate();
				return;
			}
			QWidget::keyPressEvent(aEvent);
		}
		void paintEvent(QPaintEvent* aEvent) override
		{
			QWidget::paintEvent(aEvent);
			if (hasFocus()) {
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 1.5 });
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), 6.0, 6.0);
			}
		}
		void focusInEvent(QFocusEvent* aEvent) override
		{
			update();
			QWidget::focusInEvent(aEvent);
		}
		void focusOutEvent(QFocusEvent* aEvent) override
		{
			update();
			QWidget::focusOutEvent(aEvent);
		}
	};

	[[nodiscard]] inline auto muted(const QString& aText, QWidget* aParent) -> QLabel*
	{
		auto* const label = new QLabel{ aText, aParent };
		label->setWordWrap(true);
		label->setTextFormat(Qt::PlainText);
		label->setForegroundRole(QPalette::PlaceholderText);
		return label;
	}

	[[nodiscard]] inline auto words(const QString& aText, QWidget* aParent) -> QLabel*
	{
		auto* const label = new QLabel{ aText, aParent };
		label->setWordWrap(true);
		label->setTextFormat(Qt::PlainText);
		return label;
	}

	[[nodiscard]] inline auto code_font(const QFont& aFont) -> QFont
	{
		auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		code.setPointSizeF(std::max(1.0, aFont.pointSizeF() * 0.92));
		return code;
	}

	/// \brief A flat button drawn in the palette's link colour, read at
	/// each paint so it follows light and dark.
	class link_push final : public QPushButton
	{
	public:
		link_push(const QString& aText, QWidget* aParent)
			: QPushButton{ aText, aParent }
		{
			setFlat(true);
			setCursor(Qt::PointingHandCursor);
		}

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QStylePainter{ this };
			auto option  = QStyleOptionButton{ };
			initStyleOption(&option);
			option.palette.setColor(QPalette::ButtonText, palette().color(QPalette::Active, QPalette::Link));
			painter.drawControl(QStyle::CE_PushButton, option);
		}
	};

	[[nodiscard]] inline auto link_button(const QString& aText, QWidget* aParent, const QString& aName) -> QPushButton*
	{
		auto* const button = new link_push{ aText, aParent };
		button->setObjectName(aName);
		return button;
	}

	[[nodiscard]] inline auto primary_button(const QString& aText, QWidget* aParent, const QString& aName) -> QPushButton*
	{
		auto* const button = new QPushButton{ aText, aParent };
		button->setObjectName(aName);
		button->setDefault(true);
		return button;
	}
} // namespace bpr::qt::setup_parts

#endif // BPR_SRC_ADAPTERS_QT_SRC_SETUP_STEP_PARTS_HPP
