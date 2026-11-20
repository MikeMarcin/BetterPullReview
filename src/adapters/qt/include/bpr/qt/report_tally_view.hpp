#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP

#include "bpr/core/agent_report.hpp"
#include "bpr/qt/report_page.hpp"

#include <QPushButton>
#include <QRect>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <vector>

namespace bpr::qt {
	class report_tally_view;

	/// \brief [[BPR-2331]]. One row of a report's summary in the Agent tab:
	/// a label in its tone, a bar as long as its share of the largest count,
	/// and the count. The whole row opens the report at the first item it
	/// counts.
	class report_tally_row final : public QPushButton
	{
	public:
		report_tally_row(const core::dressed_count& aCount, report_tally_view& aView);

		[[nodiscard]] auto count() const noexcept -> const core::dressed_count& { return Count; }
		/// \brief Where the word, the bar and the count are drawn, in the row.
		[[nodiscard]] auto word_rect() const -> QRect;
		[[nodiscard]] auto bar_rect() const -> QRect;
		[[nodiscard]] auto count_rect() const -> QRect;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		[[nodiscard]] auto track_rect() const -> QRect;

		core::dressed_count Count;
		report_tally_view&  View;
	};

	/// \brief [[BPR-2331]] (owner, 2026-11-20: "We can put a brief summary in
	/// the agents tab below the report. The line at the top of the report is
	/// a good candidate for that although we'd need a version that works in
	/// the narrow agent drawer, perhaps it gets transposed."). The report
	/// page's tally, a meter across the page and its counts in a sentence,
	/// turned on its side for the drawer: a row a label, hung from the
	/// attachment by a thin rule, the words in one column, the bars in the
	/// next and the counts right-aligned in the last.
	/// \details The columns are measured from the current font at every
	/// layout and paint, so the rows follow the font and the scale they are
	/// drawn at, and a row never asks for less width than its word, the
	/// shortest bar and its count need.
	class report_tally_view final : public QWidget
	{
	public:
		/// The least length of a bar's track, so the largest count's bar
		/// always reads as one even in the narrowest drawer at a large font;
		/// and its longest: a wide drawer keeps the summary compact rather
		/// than drawing bars across it.
		static constexpr int least_bar     = 8;
		static constexpr int preferred_bar = 120;

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

		/// \brief The widest word's and the widest count's advance, in the
		/// current font.
		[[nodiscard]] auto word_column() const -> int;
		[[nodiscard]] auto count_column() const -> int;
		[[nodiscard]] auto largest() const noexcept -> std::uint32_t { return Largest; }
		/// \brief The colour a count's word and bar wear.
		[[nodiscard]] auto ink_of(const core::dressed_count& aCount) const -> QColor;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		/// The rows' sizes follow its font: a new font measures them again.
		void changeEvent(QEvent* aEvent) override;

	private:
		std::vector<report_tally_row*>            Rows;
		std::uint32_t                             Largest = 0U;
		tone_colours                              Tones;
		bool                                      ToneSet = false;
		std::function<void(std::uint32_t aFirst)> Open;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPORT_TALLY_VIEW_HPP
