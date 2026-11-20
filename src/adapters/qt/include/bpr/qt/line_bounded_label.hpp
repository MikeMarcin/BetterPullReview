#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LINE_BOUNDED_LABEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LINE_BOUNDED_LABEL_HPP

#include <QLabel>
#include <QStringList>

namespace bpr::qt {
	/// \brief [[BPR-2297]]. A wrapped label of at most a set number of whole
	/// lines: text that needs more ends its last line in "…".
	/// \details Its height for a width is its shown lines at its current
	/// font's line spacing, so whatever the font, style or scale, every line
	/// it draws is whole. A maximum height on a wrapping QLabel does not do
	/// that: the label draws all its lines into the shorter box, centred, and
	/// cuts the first and last through their glyphs. `text()` stays whole
	/// for accessibility and for callers; only the painting shortens it. A
	/// word wider than the label breaks anywhere, so no line overflows it.
	class line_bounded_label final : public QLabel
	{
	public:
		explicit line_bounded_label(int aMaximumLines, QWidget* aParent = nullptr);

		/// \brief The lines drawn at a content width of `aWidth` pixels, the
		/// last ending in "…" when the text needs more than the maximum.
		[[nodiscard]] auto shown_lines(int aWidth) const -> QStringList;
		/// \brief Whether the text is shortened at a content width of
		/// `aWidth` pixels.
		[[nodiscard]] auto elided(int aWidth) const -> bool;
		/// \brief The most lines it draws.
		[[nodiscard]] auto maximum_lines() const noexcept -> int { return MaximumLines; }

		[[nodiscard]] auto hasHeightForWidth() const -> bool override { return true; }
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		int MaximumLines = 1;
	};
} // namespace bpr::qt

#endif
