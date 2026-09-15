#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_PANEL_HPP

#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/review_landing_model.hpp"

#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QVBoxLayout;

namespace bpr::qt {
	class home_panel;

	/// \brief How a Home panel is drawn.
	enum class home_panel_style : std::uint8_t {
		/// A bordered card with one 8-point radius.
		panel,
		/// A slim tinted band nested inside a group panel ([[BPR-0783]]).
		band,
	};

	/// \brief [[BPR-0776]]. A Home panel's header: the chevron, the name, the
	/// count and, while collapsed, the one-line summary.
	///
	/// \details The whole header is the toggle. Space, Return and Enter toggle,
	/// Left collapses and Right expands; Up and Down leave the header through the
	/// panel's navigator. It is announced as an expandable button whose expanded
	/// or collapsed state is part of its accessible state.
	class home_panel_header final : public QWidget
	{
	public:
		explicit home_panel_header(home_panel& aPanel);

		[[nodiscard]] auto panel() const noexcept -> home_panel&;
		/// \brief The chevron's rectangle, in header coordinates.
		[[nodiscard]] auto chevron_rect() const -> QRect;

		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		home_panel& Panel;
	};

	/// \brief [[BPR-0776]]. One collapsible Home panel with a tinted sticky header
	/// and, for a state, a rail in its colour.
	///
	/// \details Used for Home's states, for Recently viewed and, nesting slim
	/// state bands inside a neutral group panel, for [[BPR-0783]]'s groups. The
	/// collapsed state is Qt-local presentation state: it moves no row and asks
	/// nothing of the core. The header is a child placed over the panel's top
	/// margin rather than laid out, so a scroll area can move it down the panel
	/// while the panel is in view (`set_header_offset`).
	class home_panel final : public QWidget
	{
	public:
		explicit home_panel(
			std::optional<home_group> aState,
			home_panel_style          aStyle  = home_panel_style::panel,
			QWidget*                  aParent = nullptr);

		void set_title(const QString& aTitle);
		void set_count(int aCount);
		/// \brief The one-line summary shown, and described, only while collapsed.
		void set_summary(const QString& aSummary);
		/// \brief Adopts the panel's content; hidden while collapsed.
		void set_content(QWidget* aContent);
		void set_collapsed(bool aCollapsed);
		void set_collapse_observer(std::function<void(bool)> aObserver);
		/// \brief Receives Up and Down from the header; answers whether it moved focus.
		void set_header_navigator(std::function<bool(int aKey)> aNavigator);
		void set_state_colours(const home_state_colours& aColours);
		/// \brief Moves the header down the panel, clamped to the panel's height.
		void set_header_offset(int aOffset);

		[[nodiscard]] auto state() const noexcept -> std::optional<home_group>;
		[[nodiscard]] auto style_kind() const noexcept -> home_panel_style;
		[[nodiscard]] auto title() const -> QString;
		[[nodiscard]] auto count() const noexcept -> int;
		[[nodiscard]] auto summary() const -> QString;
		[[nodiscard]] auto collapsed() const noexcept -> bool;
		[[nodiscard]] auto header() const noexcept -> home_panel_header*;
		[[nodiscard]] auto header_offset() const noexcept -> int;
		[[nodiscard]] auto header_height() const noexcept -> int;
		[[nodiscard]] auto content() const noexcept -> QWidget*;
		/// \brief The state colour at text strength, or an invalid colour for a neutral panel.
		[[nodiscard]] auto tone() const -> QColor;
		[[nodiscard]] auto navigate(int aKey) const -> bool;

		static constexpr qreal radius     = 8.0;
		static constexpr int   rail_width = 3;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void refresh_header();
		void place_header();

		std::optional<home_group>     State;
		home_panel_style              Style   = home_panel_style::panel;
		home_panel_header*            Header  = nullptr;
		QVBoxLayout*                  Column  = nullptr;
		QWidget*                      Content = nullptr;
		QString                       Title;
		QString                       Summary;
		int                           Count        = 0;
		int                           HeaderOffset = 0;
		int                           HeaderHeight = 0;
		bool                          Collapsed    = false;
		home_state_colours            Colours;
		std::function<void(bool)>     CollapseObserver;
		std::function<bool(int aKey)> Navigator;
	};

	/// \brief Sticks one panel's header, and its nested bands' headers, under a line.
	///
	/// \details `aLine` is in `aViewport` coordinates: the top of the scroll area
	/// for a top-level panel, the bottom of the group header for a band. A header
	/// moves down its panel while the panel's top has scrolled above the line and
	/// stops at the panel's bottom, so it never covers the next panel.
	void stick_home_header(home_panel& aPanel, const QWidget& aViewport, int aLine);
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_PANEL_HPP
