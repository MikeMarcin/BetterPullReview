#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMPOSE_REVIEW_SHEET_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMPOSE_REVIEW_SHEET_HPP

#include <QIcon>
#include <QRect>
#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QMouseEvent;
class QPaintEvent;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

/// \file
/// \brief [[BPR-1590]]. RFC-BPR-0085 D6: Finish your review over the current
/// surface; [[BPR-2175]]: in a lightbox.
namespace bpr::qt {
	/// \brief Finish your review in a lightbox over the window, below its
	/// title row.
	///
	/// \details The owner, 2026-10-17: "I think I like C with an expand to A."
	/// Its header is the title and two tools, Expand (to the focused page in
	/// Discuss) and Close; its body scrolls and holds the review composer,
	/// which the window lends it and takes back.
	///
	/// [[BPR-2175]] (owner, mockup round 2, option D): the panel is centred
	/// over a dimmed window, as wide as the window allows up to
	/// `lightbox_width` and as tall as its content up to the area. The widget
	/// covers the whole area it is placed on and paints the scrim there; a
	/// press on the scrim closes it (the window's title row, with its mode
	/// tabs and window controls, lies outside it and stays live). Colours
	/// come from palette roles only.
	class compose_review_sheet final : public QWidget
	{
	public:
		/// The widest the panel grows.
		static constexpr int lightbox_width = 1180;
		/// The narrowest it shrinks to.
		static constexpr int minimum_width = 360;
		/// The scrim kept around the panel on each side.
		static constexpr int margin = 24;
		/// A panel at least this wide sets the composer in two columns.
		static constexpr int two_column_width = 820;

		explicit compose_review_sheet(QWidget* aParent = nullptr);

		/// \brief The tools' glyphs, their names with keys, and what they do.
		void set_tools(const QIcon& aExpand, const QString& aExpandName, std::function<void()> aOnExpand, const QIcon& aClose, const QString& aCloseName,
					   std::function<void()> aOnClose);
		/// \brief Covers `aArea`, in its parent's coordinates, over everything
		/// else there, with the panel centred in it.
		void place(const QRect& aArea);
		/// \brief The panel, in the sheet's own coordinates.
		[[nodiscard]] auto panel() const noexcept -> QRect;

		[[nodiscard]] auto scroll() const noexcept -> QScrollArea*;
		/// \brief Where the borrowed composer goes.
		[[nodiscard]] auto content_layout() const noexcept -> QVBoxLayout*;
		[[nodiscard]] auto title() const noexcept -> QLabel*;
		[[nodiscard]] auto expand_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto close_button() const noexcept -> QToolButton*;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;

	private:
		QWidget*              Header  = nullptr;
		QLabel*               Title   = nullptr;
		QToolButton*          Expand  = nullptr;
		QToolButton*          Close   = nullptr;
		QScrollArea*          Scroll  = nullptr;
		QVBoxLayout*          Content = nullptr;
		QRect                 Panel;
		std::function<void()> OnExpand;
		std::function<void()> OnClose;
	};
} // namespace bpr::qt

#endif
