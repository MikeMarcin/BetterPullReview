#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP

#include "bpr/core/agent_report.hpp"
#include "bpr/qt/report_page.hpp"

#include <QPushButton>
#include <QRect>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace bpr::qt {
	class report_tally_view;

	/// \brief [[BPR-2331]], [[BPR-2336]]. One count of a report's summary in
	/// the Agent tab: its segment of the summary's one bar, in its tone and
	/// as tall as its band, with its label and count beside it. The whole
	/// band (segment and label) opens the report at the first item it
	/// counts.
	class report_tally_row final : public QPushButton
	{
	public:
		report_tally_row(const core::dressed_count& aCount, std::size_t aIndex, report_tally_view& aView);

		[[nodiscard]] auto count() const noexcept -> const core::dressed_count& { return Count; }
		/// \brief Where the segment, the word and the count are drawn, in the
		/// row.
		[[nodiscard]] auto segment_rect() const -> QRect;
		[[nodiscard]] auto word_rect() const -> QRect;
		[[nodiscard]] auto count_rect() const -> QRect;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		core::dressed_count Count;
		std::size_t         Index;
		report_tally_view&  View;
	};

	/// \brief [[BPR-2331]] (owner, 2026-11-20: "We can put a brief summary in
	/// the agents tab below the report. The line at the top of the report is
	/// a good candidate for that although we'd need a version that works in
	/// the narrow agent drawer, perhaps it gets transposed."), as the owner
	/// asked again in [[BPR-2336]] ("I wish this was a single vertical bar
	/// with colored segments instead."). The report page's meter turned on
	/// its side: one bar dropping from under the attachment's page, a
	/// segment a count in its tone, each as tall as its band, and beside each
	/// segment its word and its count.
	/// \details A band is one line of the current font plus its share, by
	/// count, of as much again for the whole summary, so every label has its
	/// line and a larger count stands taller. The bands are measured from the
	/// current font at every layout and paint.
	class report_tally_view final : public QWidget
	{
	public:
		/// The bar's thickness, and the room between it and the labels.
		static constexpr int bar_width = 6;
		static constexpr int label_gap = 8;

		explicit report_tally_view(QWidget* aParent = nullptr);

		/// \brief The counts, in the report's order; none hides the view.
		void set_counts(const std::vector<core::dressed_count>& aCounts);
		/// \brief The theme's tones, as the report page draws them.
		void set_tones(const tone_colours& aTones);
		/// \brief What pressing a row does: the menu row of the first item
		/// it counts.
		void set_open(std::function<void(std::uint32_t aFirst)> aOpen) { Open = std::move(aOpen); }

		[[nodiscard]] auto rows() const -> std::vector<report_tally_row*>;
		[[nodiscard]] auto counts() const -> std::vector<core::dressed_count>;

		/// \brief The least band: one line of the current font.
		[[nodiscard]] auto line_height() const -> int;
		/// \brief The height of the band of the count at `aIndex`: one line,
		/// and its share of as much again for the summary, by count. The
		/// bands sum to `bar_height`.
		[[nodiscard]] auto band_height(std::size_t aIndex) const -> int;
		[[nodiscard]] auto bar_height() const -> int;
		[[nodiscard]] auto total() const noexcept -> std::uint32_t { return Total; }
		/// \brief The colour a count's word and segment wear.
		[[nodiscard]] auto ink_of(const core::dressed_count& aCount) const -> QColor;

	protected:
		/// The rows' sizes follow its font: a new font measures them again.
		void changeEvent(QEvent* aEvent) override;

	private:
		/// Where the band at `aIndex` starts, from the bar's top.
		[[nodiscard]] auto band_top(std::size_t aIndex) const -> int;

		std::vector<report_tally_row*>            Rows;
		std::uint32_t                             Total = 0U;
		tone_colours                              Tones;
		bool                                      ToneSet = false;
		std::function<void(std::uint32_t aFirst)> Open;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP
