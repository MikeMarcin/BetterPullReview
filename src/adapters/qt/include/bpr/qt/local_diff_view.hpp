#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DIFF_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DIFF_VIEW_HPP

#include "bpr/core/review_cycle_views.hpp"

#include <QAbstractScrollArea>
#include <QFont>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

class QPushButton;

namespace bpr::qt {
	/// \brief A band above one change: its number, whether it is staged, and
	/// the one action it offers (Stage Change or Unstage Change), if any.
	struct local_diff_band
	{
		/// The index into the rows of the change's first row.
		std::size_t Row = 0U;
		QString     Title;
		/// Shown after the title with a green dot: "Staged".
		QString State;
		/// "Stage Change", "Unstage Change", or empty for none.
		QString Action;
		/// Which change the action is for, as the host knows it.
		std::size_t Change = 0U;
		bool        Stage  = true;
	};

	/// \brief The local draft's and author state's comparison, side by side
	/// or in one pane, with a band per change (RFC-BPR-0108 as amended and
	/// RFC-BPR-0114 as amended 2026-11-12).
	/// \details Draws the rows it is given: equal lines once on each side,
	/// removed lines on the left against hatching, added on the right.
	/// Decides nothing: a band's action is reported through `set_action`.
	/// Selecting rows selects new-side lines for a tour stop.
	class local_diff_view final : public QAbstractScrollArea
	{
	public:
		explicit local_diff_view(QWidget* aParent = nullptr);

		void present(std::vector<core::cycle_diff_row> aRows, std::vector<local_diff_band> aBands, QString aLeft, QString aPath, QString aRight);
		void clear(const QString& aStatus = { });
		void set_one_pane(bool aOnePane);
		[[nodiscard]] auto one_pane() const noexcept -> bool;
		void               set_code_font(const QFont& aFont);
		/// \brief A band's button was pressed: the change and whether it stages.
		void set_action(std::function<void(std::size_t, bool)> aAction);
		/// \brief The new-side lines selected (one-based, inclusive), if any.
		[[nodiscard]] auto selected_lines() const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>;
		/// \brief Scrolls to and selects new-side lines.
		void reveal(std::uint32_t aFirst, std::uint32_t aLast);
		[[nodiscard]] auto band_buttons() const -> std::vector<QPushButton*>;
		[[nodiscard]] auto rows() const noexcept -> const std::vector<core::cycle_diff_row>&;
		[[nodiscard]] auto header_text() const -> QString;
		/// \brief The rows as text, one per line, prefixed as a unified diff is.
		[[nodiscard]] auto text() const -> QString;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void scrollContentsBy(int aDx, int aDy) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		/// One drawn line: a pair of sides, or a band.
		struct line
		{
			std::optional<std::size_t> Left;
			std::optional<std::size_t> Right;
			std::optional<std::size_t> Band;
		};
		void layout_lines();
		void place_buttons();
		void update_scrollbars();
		[[nodiscard]] auto line_height() const -> int;
		[[nodiscard]] auto band_height() const -> int;
		[[nodiscard]] auto header_height() const -> int;
		[[nodiscard]] auto top_of(std::size_t aLine) const -> int;
		[[nodiscard]] auto line_at(int aY) const -> std::optional<std::size_t>;
		[[nodiscard]] auto right_line_of(std::size_t aLine) const -> std::optional<std::uint32_t>;

		std::vector<core::cycle_diff_row>       Rows;
		std::vector<local_diff_band>            Bands;
		std::vector<line>                       Lines;
		std::vector<int>                        Tops;
		std::vector<QPushButton*>               Buttons;
		QString                                 Left;
		QString                                 Path;
		QString                                 Right;
		QString                                 Status;
		QFont                                   Code;
		bool                                    OnePane = false;
		std::optional<std::size_t>              Anchor;
		std::optional<std::size_t>              Cursor;
		std::function<void(std::size_t, bool)>  Action;
	};
}

#endif
