#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP

#include "bpr/core/agent_report.hpp"
#include "bpr/qt/report_page.hpp"

#include <QPainterPath>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace bpr::qt {
	class report_tally_view;

	/// \brief [[BPR-2331]], [[BPR-2344]]. One count of a report's summary in
	/// the Agent tab, as its row of the donut's legend: its swatch in its
	/// slice's colour, its word in its tone and its count. The row opens the
	/// report at the first item it counts.
	class report_tally_row final : public QPushButton
	{
	public:
		report_tally_row(const core::dressed_count& aCount, std::size_t aIndex, report_tally_view& aView);

		[[nodiscard]] auto count() const noexcept -> const core::dressed_count& { return Count; }
		[[nodiscard]] auto index() const noexcept -> std::size_t { return Index; }
		/// \brief Where the swatch, the word and the count are drawn, in the
		/// row. The count stands at the row's right, so the legend's counts
		/// make one column.
		[[nodiscard]] auto swatch_rect() const -> QRect;
		[[nodiscard]] auto word_rect() const -> QRect;
		[[nodiscard]] auto count_rect() const -> QRect;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void focusOutEvent(QFocusEvent* aEvent) override;

	private:
		core::dressed_count Count;
		std::size_t         Index;
		report_tally_view&  View;
	};

	/// \brief [[BPR-2344]]. The summary's donut: a slice a count, clockwise
	/// from twelve o'clock in the report's order, each exactly its share of
	/// the whole, and the total in its hole. A press on a slice opens the
	/// report at the first item it counts; hovering one highlights it and its
	/// legend row.
	class report_tally_pie final : public QWidget
	{
	public:
		/// The ring's outer diameter; the hole is `hole` of it.
		static constexpr int diameter = 72;
		/// The diameter where the drawer is too narrow for the ring at
		/// `diameter` beside its legend.
		static constexpr int    compact_diameter = 56;
		static constexpr double hole             = 0.56;
		/// How far a highlighted slice stands out of the ring.
		static constexpr int lift = 3;

		explicit report_tally_pie(report_tally_view& aView);

		/// \brief The ring's centre and radii, in the pie.
		[[nodiscard]] auto centre() const -> QPointF;
		[[nodiscard]] auto outer_radius() const -> double;
		[[nodiscard]] auto inner_radius() const -> double;
		/// \brief The slice under `aPoint`, if any: the ring only, a gap
		/// belonging to the slice either side of it.
		[[nodiscard]] auto slice_at(QPointF aPoint) const -> std::optional<std::size_t>;
		/// \brief The middle of the slice at `aIndex`'s ring: half way
		/// round it, half way across it.
		[[nodiscard]] auto slice_middle(std::size_t aIndex) const -> QPointF;
		/// \brief The ring's outer diameter now: `diameter`, or
		/// `compact_diameter` in a narrow drawer.
		[[nodiscard]] auto ring_diameter() const noexcept -> int { return Diameter; }
		void               set_ring_diameter(int aDiameter);

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;

	private:
		/// The slice at `aIndex` as drawn: its wedge of the ring, out by
		/// `aLift`, less the gaps either side of it.
		[[nodiscard]] auto slice_path(std::size_t aIndex, double aLift) const -> QPainterPath;

		report_tally_view&         View;
		std::optional<std::size_t> Pressed;
		int                        Diameter = diameter;
	};

	/// \brief [[BPR-2331]] (owner, 2026-11-20: "We can put a brief summary in
	/// the agents tab below the report."), redrawn for [[BPR-2344]] (owner,
	/// 2026-11-21: "let's do a pie chart instead of a vertical line for
	/// report findings in the agents tab."). A donut of the report's counts
	/// and beside it their legend, over the report's attachment (owner,
	/// 2026-11-25: "I'm thinking that I want to move the report findings
	/// donut & counts above the attachment.").
	/// \details The slices and the legend rows are in the report's order and
	/// its tones. The legend's rows are measured from the current font; the
	/// donut keeps its size where it has the room, and is smaller only where
	/// the drawer is too narrow for it beside the legend (Ubuntu's larger
	/// font in the narrowest drawer, build 76), so the summary fits.
	class report_tally_view final : public QWidget
	{
	public:
		/// Between the donut and its legend.
		static constexpr int legend_gap = 12;
		/// Under the summary, before the attachment row it stands over.
		static constexpr int attachment_gap = 10;

		explicit report_tally_view(QWidget* aParent = nullptr);
		/// The rows go first: a row that loses focus tells the view, and the
		/// view's own part must still be there to hear it.
		~report_tally_view() override;
		report_tally_view(const report_tally_view&)                    = delete;
		auto operator=(const report_tally_view&) -> report_tally_view& = delete;

		/// \brief The counts, in the report's order; none hides the view.
		void set_counts(const std::vector<core::dressed_count>& aCounts);
		/// \brief The theme's tones, as the report page draws them.
		void set_tones(const tone_colours& aTones);
		/// \brief What pressing a row or a slice does: the menu row of the
		/// first item it counts.
		void set_open(std::function<void(std::uint32_t aFirst)> aOpen) { Open = std::move(aOpen); }

		[[nodiscard]] auto rows() const -> std::vector<report_tally_row*>;
		[[nodiscard]] auto counts() const -> std::vector<core::dressed_count>;
		[[nodiscard]] auto pie() const noexcept -> report_tally_pie* { return Pie; }
		/// \brief The least width: the smaller donut beside the legend.
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

		/// \brief A legend row's height: one line of the current font.
		[[nodiscard]] auto line_height() const -> int;
		/// \brief Where the slice at `aIndex` starts and how far it goes, in
		/// degrees clockwise from twelve o'clock: its count's exact share of
		/// 360. The slices meet end to start and make the whole circle.
		[[nodiscard]] auto slice_start(std::size_t aIndex) const -> double;
		[[nodiscard]] auto slice_span(std::size_t aIndex) const -> double;
		[[nodiscard]] auto total() const noexcept -> std::uint32_t { return Total; }
		/// \brief The colour a count's word wears.
		[[nodiscard]] auto ink_of(const core::dressed_count& aCount) const -> QColor;
		/// \brief The colour a count's slice and swatch wear: its ink, or
		/// for a neutral label the window's ink at 45%.
		[[nodiscard]] auto fill_of(const core::dressed_count& aCount) const -> QColor;

		/// \brief The count whose slice and row stand out: the one under
		/// the pointer, else the row with focus.
		[[nodiscard]] auto highlighted() const -> std::optional<std::size_t>;
		/// \brief The pointer is over the slice or row of `aIndex`, or over
		/// none.
		void set_hovered(std::optional<std::size_t> aIndex);
		/// \brief Opens the report at the first item the count at `aIndex`
		/// counts.
		void open(std::size_t aIndex) const;
		/// \brief A row's focus changed: the donut shows which.
		void focus_changed();

	protected:
		/// The rows' sizes follow its font: a new font measures them again.
		void changeEvent(QEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// The width the summary needs with a ring of `aDiameter`.
		[[nodiscard]] auto width_for(int aDiameter) const -> int;
		/// The ring at its full size where the width holds it.
		void fit_ring();
		/// The rows in the focus order where the summary stands.
		void                                      order_rows();
		std::vector<report_tally_row*>            Rows;
		report_tally_pie*                         Pie    = nullptr;
		QWidget*                                  Legend = nullptr;
		std::uint32_t                             Total  = 0U;
		std::optional<std::size_t>                Hovered;
		tone_colours                              Tones;
		bool                                      ToneSet = false;
		std::function<void(std::uint32_t aFirst)> Open;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP
