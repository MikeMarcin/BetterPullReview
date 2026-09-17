#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WINDOWS_CAPTION_BUTTONS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WINDOWS_CAPTION_BUTTONS_HPP

#include <QAbstractButton>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>

class QPaintEvent;

namespace bpr::qt {
	/// \brief The parts of the Windows title row a point can fall on.
	///
	/// \details [[BPR-0653]]. Presentation answers which part is under a point;
	/// the host turns the answer into the platform's hit-test code and performs
	/// what the part asks for. `client` is any control, `caption` the strip's
	/// bare surface that moves and zooms the window as a title bar would.
	enum class title_row_part : std::uint8_t {
		client,
		caption,
		minimize,
		maximize,
		close,
	};

	/// \brief One Windows caption button, drawn in the strip's ink.
	class windows_caption_button final : public QAbstractButton
	{
	public:
		windows_caption_button(title_row_part aPart, QWidget* aParent = nullptr);

		/// \brief The width Windows gives each caption button.
		static constexpr int width_pixels = 46;

		[[nodiscard]] auto part() const noexcept -> title_row_part;
		/// \brief Hover and press as the platform reports them for the non-client area.
		void               set_native_hot(bool aHot);
		void               set_native_pressed(bool aPressed);
		[[nodiscard]] auto native_hot() const noexcept -> bool;
		/// \brief Shows the restore glyph and name while the window is maximized.
		void set_restore(bool aRestore);

		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;

	private:
		title_row_part Part;
		bool           NativeHot     = false;
		bool           NativePressed = false;
		bool           Hovered       = false;
		bool           Restore       = false;
	};

	/// \brief Minimise, maximise-or-restore and close, in that order.
	///
	/// \details Qt presents: the buttons draw the platform's current caption
	/// glyphs and take keyboard focus as the strip's last three stops, and a
	/// press asks the host to act through the dispatch it installed. Without a
	/// dispatch a press does nothing; the window is the host's to change.
	class windows_caption_buttons final : public QWidget
	{
	public:
		explicit windows_caption_buttons(QWidget* aParent = nullptr);

		using dispatch = std::function<void(title_row_part)>;
		void set_dispatch(dispatch aDispatch);

		[[nodiscard]] auto button(title_row_part aPart) const -> windows_caption_button*;
		[[nodiscard]] auto buttons() const -> std::array<windows_caption_button*, 3U>;
		/// \brief The caption part under a point in this widget, or `client`.
		[[nodiscard]] auto part_at(QPoint aPoint) const -> title_row_part;
		/// \brief Presents the platform's non-client hover and press over the buttons.
		void set_native_state(title_row_part aHot, title_row_part aPressed);
		void set_maximized(bool aMaximized);
		/// \brief The toolbar item that holds the buttons' place in the row.
		///
		/// \details The buttons lie over the row's right end, the full row
		/// height and flush with the window's corner, as Windows draws them; the
		/// reserve is what the row lays out in their place.
		[[nodiscard]] auto reserve() const noexcept -> QWidget*;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void place();

		std::array<windows_caption_button*, 3U> Buttons{ };
		dispatch                                Dispatch;
		QWidget*                                Reserve = nullptr;
	};

	/// \brief The compact menu control's three-line glyph, in the strip's ink.
	[[nodiscard]] auto windows_menu_icon(const QPalette& aPalette) -> QIcon;
} // namespace bpr::qt

#endif
