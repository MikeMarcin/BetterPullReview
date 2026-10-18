#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMPOSE_REVIEW_SHEET_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMPOSE_REVIEW_SHEET_HPP

#include <QIcon>
#include <QRect>
#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QPaintEvent;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

/// \file
/// \brief [[BPR-1590]]. RFC-BPR-0085 D6: Finish your review as a sheet from
/// the window's right edge over the current surface.
namespace bpr::qt {
	/// \brief Finish your review, beside the code: a 540-pixel sheet at the
	/// right edge of the review area, over whatever the modality shows.
	///
	/// \details The owner, 2026-10-17: "I think I like C with an expand to A."
	/// Its header is the title and two tools, Expand (to the focused page in
	/// Discuss) and Close; its body scrolls and holds the review composer,
	/// which the window lends it and takes back. It paints the panel's Base
	/// with a soft shadow on its left edge, from palette roles only, and
	/// leaves at least 320 pixels of the surface beneath it visible.
	class compose_review_sheet final : public QWidget
	{
	public:
		/// The width the sheet takes when the area allows it.
		static constexpr int preferred_width = 540;
		/// The shadow cast on the surface to its left.
		static constexpr int shadow_width = 14;

		explicit compose_review_sheet(QWidget* aParent = nullptr);

		/// \brief The tools' glyphs, their names with keys, and what they do.
		void set_tools(const QIcon& aExpand, const QString& aExpandName, std::function<void()> aOnExpand, const QIcon& aClose, const QString& aCloseName,
					   std::function<void()> aOnClose);
		/// \brief Lays the sheet at the right edge of `aArea`, in its parent's
		/// coordinates, over everything else there.
		void place(const QRect& aArea);

		[[nodiscard]] auto scroll() const noexcept -> QScrollArea*;
		/// \brief Where the borrowed composer goes.
		[[nodiscard]] auto content_layout() const noexcept -> QVBoxLayout*;
		[[nodiscard]] auto title() const noexcept -> QLabel*;
		[[nodiscard]] auto expand_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto close_button() const noexcept -> QToolButton*;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QLabel*               Title   = nullptr;
		QToolButton*          Expand  = nullptr;
		QToolButton*          Close   = nullptr;
		QScrollArea*          Scroll  = nullptr;
		QVBoxLayout*          Content = nullptr;
		std::function<void()> OnExpand;
		std::function<void()> OnClose;
	};
} // namespace bpr::qt

#endif
