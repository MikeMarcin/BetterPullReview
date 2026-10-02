#include "bpr/qt/windows_caption_buttons.hpp"

#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/workbench_strip.hpp"

#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStyle>

#include <algorithm>

namespace bpr::qt {
	namespace {
		// The platform's own caption glyphs: Segoe Fluent Icons on Windows 11,
		// Segoe MDL2 Assets on Windows 10, at the code points both fonts share.
		constexpr char16_t minimize_glyph = 0xE921;
		constexpr char16_t maximize_glyph = 0xE922;
		constexpr char16_t restore_glyph  = 0xE923;
		constexpr char16_t close_glyph    = 0xE8BB;
		// Windows 11's close hover and press colours.
		constexpr QRgb close_hover   = 0xFFC42B1C;
		constexpr QRgb close_pressed = 0xE6C42B1C;

		[[nodiscard]] auto caption_font() -> QFont
		{
			const auto families = QFontDatabase::families();
			auto       font     = QFont{ families.contains(QStringLiteral("Segoe Fluent Icons"))
											 ? QStringLiteral("Segoe Fluent Icons")
											 : QStringLiteral("Segoe MDL2 Assets") };
			font.setPixelSize(10);
			return font;
		}

		[[nodiscard]] auto accessible_name(const title_row_part aPart, const bool aRestore) -> QString
		{
			switch (aPart) {
			case title_row_part::minimize:
				return QStringLiteral("Minimize");
			case title_row_part::maximize:
				return aRestore ? QStringLiteral("Restore") : QStringLiteral("Maximize");
			case title_row_part::close:
				return QStringLiteral("Close");
			case title_row_part::client:
			case title_row_part::caption:
				break;
			}
			return { };
		}

		[[nodiscard]] auto accessible_description(const title_row_part aPart, const bool aRestore) -> QString
		{
			switch (aPart) {
			case title_row_part::minimize:
				return QStringLiteral("Minimize the window to the taskbar");
			case title_row_part::maximize:
				return aRestore ? QStringLiteral("Restore the window to its previous size")
								: QStringLiteral("Maximize the window to fill the screen");
			case title_row_part::close:
				return QStringLiteral("Close the window");
			case title_row_part::client:
			case title_row_part::caption:
				break;
			}
			return { };
		}
	} // namespace

	windows_caption_button::windows_caption_button(const title_row_part aPart, QWidget* const aParent)
		: QAbstractButton{ aParent }
		, Part{ aPart }
	{
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover, true);
		setFixedWidth(width_pixels);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		setAccessibleName(accessible_name(Part, Restore));
		setAccessibleDescription(accessible_description(Part, Restore));
		setToolTip(accessibleName());
	}

	auto windows_caption_button::part() const noexcept -> title_row_part
	{
		return Part;
	}

	void windows_caption_button::set_native_hot(const bool aHot)
	{
		if (NativeHot != aHot) {
			NativeHot = aHot;
			update();
		}
	}

	void windows_caption_button::set_native_pressed(const bool aPressed)
	{
		if (NativePressed != aPressed) {
			NativePressed = aPressed;
			update();
		}
	}

	auto windows_caption_button::native_hot() const noexcept -> bool
	{
		return NativeHot;
	}

	void windows_caption_button::set_restore(const bool aRestore)
	{
		if (Restore == aRestore) {
			return;
		}
		Restore = aRestore;
		setAccessibleName(accessible_name(Part, Restore));
		setAccessibleDescription(accessible_description(Part, Restore));
		setToolTip(accessibleName());
		update();
	}

	auto windows_caption_button::sizeHint() const -> QSize
	{
		return { width_pixels, workbench_strip::height_pixels };
	}

	void windows_caption_button::enterEvent(QEnterEvent* const aEvent)
	{
		Hovered = true;
		update();
		QAbstractButton::enterEvent(aEvent);
	}

	void windows_caption_button::leaveEvent(QEvent* const aEvent)
	{
		Hovered = false;
		update();
		QAbstractButton::leaveEvent(aEvent);
	}

	void windows_caption_button::paintEvent(QPaintEvent*)
	{
		auto       painter  = QPainter{ this };
		const auto pressed  = NativePressed || isDown();
		const auto hot      = NativeHot || Hovered;
		const auto ink      = palette().color(QPalette::ButtonText);
		auto       glyphInk = ink;
		if (Part == title_row_part::close && (hot || pressed)) {
			painter.fillRect(rect(), QColor::fromRgba(pressed ? close_pressed : close_hover));
			glyphInk = Qt::white;
		} else if (pressed || hot) {
			auto wash = ink;
			wash.setAlpha(pressed ? 0x33 : 0x1A);
			painter.fillRect(rect(), wash);
		}
		auto glyph = char16_t{ close_glyph };
		switch (Part) {
		case title_row_part::minimize:
			glyph = minimize_glyph;
			break;
		case title_row_part::maximize:
			glyph = Restore ? restore_glyph : maximize_glyph;
			break;
		case title_row_part::close:
		case title_row_part::client:
		case title_row_part::caption:
			break;
		}
		painter.setFont(caption_font());
		painter.setPen(glyphInk);
		painter.drawText(rect(), Qt::AlignCenter, QString{ QChar{ glyph } });
		if (hasFocus()) {
			// [[BPR-1157]]. The window's focus ring over the title row, as the
			// review bar's buttons draw it, square like the button itself.
			constexpr auto inset = focus_ring_width / 2.0;
			painter.setPen(QPen{ focus_ring_colour(palette(), QPalette::Window), focus_ring_width });
			painter.setBrush(Qt::NoBrush);
			painter.drawRect(QRectF{ rect() }.adjusted(inset, inset, -inset, -inset));
		}
	}

	windows_caption_buttons::windows_caption_buttons(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchCaptionButtons"));
		setFocusPolicy(Qt::NoFocus);
		auto* const layout = new QHBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		constexpr auto parts = std::array{ title_row_part::minimize, title_row_part::maximize, title_row_part::close };
		constexpr auto names = std::array{ "workbenchMinimizeButton", "workbenchMaximizeButton", "workbenchCloseButton" };
		for (std::size_t index = 0U; index < parts.size(); ++index) {
			auto* const button = new windows_caption_button{ parts[index], this };
			button->setObjectName(QLatin1StringView{ names[index] });
			layout->addWidget(button);
			Buttons[index] = button;
			connect(button, &QAbstractButton::clicked, this, [this, part = parts[index]] {
				if (Dispatch) {
					Dispatch(part);
				}
			});
		}
		setFixedWidth(3 * windows_caption_button::width_pixels);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		// Windows draws its caption buttons the row's full height and into the
		// window's corner, so the top-right pixel is close. A toolbar item sits
		// inside the toolbar's item margin instead, so the buttons are laid over
		// the row's right end, and a reserve takes their place among the
		// toolbar's items: the strip's width accounting and its action order are
		// unchanged.
		Reserve = new QWidget{ aParent };
		Reserve->setObjectName(QStringLiteral("workbenchCaptionButtonsReserve"));
		Reserve->setFocusPolicy(Qt::NoFocus);
		Reserve->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		Reserve->setFixedWidth(3 * windows_caption_button::width_pixels);
		if (aParent != nullptr) {
			aParent->installEventFilter(this);
		}
		Reserve->installEventFilter(this);
	}

	auto windows_caption_buttons::reserve() const noexcept -> QWidget*
	{
		return Reserve;
	}

	auto windows_caption_buttons::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == parentWidget() || aWatched == Reserve) {
			switch (aEvent->type()) {
			case QEvent::Resize:
			case QEvent::Show:
			case QEvent::Hide:
			case QEvent::Move:
			case QEvent::LayoutRequest:
			case QEvent::StyleChange:
				place();
				break;
			default:
				break;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void windows_caption_buttons::place()
	{
		auto* const row = parentWidget();
		if (row == nullptr || Reserve == nullptr) {
			return;
		}
		// The reserve ends where the toolbar's item margin begins; the buttons
		// cover the reserve and that margin, flush with the row's end.
		const auto width  = 3 * windows_caption_button::width_pixels;
		const auto margin = row->style()->pixelMetric(QStyle::PM_ToolBarItemMargin, nullptr, row);
		if (const auto reserved = std::max(width - margin, 0); Reserve->width() != reserved) {
			Reserve->setFixedWidth(reserved);
		}
		// The buttons follow their reserve rather than the row's end: a row too
		// narrow for its items puts the reserve in the overflow, and buttons
		// left at the end would then cover whatever the row did lay out. The
		// strip's rule forbids that width in the product, and the window's
		// floor keeps it out; a window forced narrower than its floor -- a
		// zoomed window on a small offscreen screen, say -- shows the row it can
		// and no caption buttons over it.
		const auto shown = Reserve->isVisible() && Reserve->x() >= 0
			&& Reserve->x() + Reserve->width() <= row->width();
		setVisible(shown);
		if (!shown) {
			return;
		}
		setGeometry(Reserve->x(), 0, row->width() - Reserve->x(), row->height());
		raise();
	}

	void windows_caption_buttons::set_dispatch(dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto windows_caption_buttons::button(const title_row_part aPart) const -> windows_caption_button*
	{
		for (auto* const button : Buttons) {
			if (button->part() == aPart) {
				return button;
			}
		}
		return nullptr;
	}

	auto windows_caption_buttons::buttons() const -> std::array<windows_caption_button*, 3U>
	{
		return Buttons;
	}

	auto windows_caption_buttons::part_at(const QPoint aPoint) const -> title_row_part
	{
		for (auto* const button : Buttons) {
			if (button->geometry().contains(aPoint)) {
				return button->part();
			}
		}
		return title_row_part::client;
	}

	void windows_caption_buttons::set_native_state(const title_row_part aHot, const title_row_part aPressed)
	{
		for (auto* const button : Buttons) {
			button->set_native_hot(button->part() == aHot);
			button->set_native_pressed(button->part() == aPressed);
		}
	}

	void windows_caption_buttons::set_maximized(const bool aMaximized)
	{
		if (auto* const maximize = button(title_row_part::maximize); maximize != nullptr) {
			maximize->set_restore(aMaximized);
		}
	}

	auto windows_menu_icon(const QPalette& aPalette) -> QIcon
	{
		auto       icon  = QIcon{ };
		const auto paint = [&aPalette](const qreal aRatio) {
			auto pixmap = QPixmap{ static_cast<int>(16 * aRatio), static_cast<int>(16 * aRatio) };
			pixmap.setDevicePixelRatio(aRatio);
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing, true);
			auto pen = QPen{ aPalette.color(QPalette::ButtonText) };
			pen.setWidthF(1.25);
			pen.setCapStyle(Qt::RoundCap);
			painter.setPen(pen);
			for (const auto y : { 4.5, 8.0, 11.5 }) {
				painter.drawLine(QPointF{ 3.0, y }, QPointF{ 13.0, y });
			}
			return pixmap;
		};
		for (const auto ratio : { 1.0, 1.25, 1.5, 2.0 }) {
			icon.addPixmap(paint(ratio));
		}
		return icon;
	}
} // namespace bpr::qt
