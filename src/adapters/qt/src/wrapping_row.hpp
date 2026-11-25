#ifndef BPR_SRC_ADAPTERS_QT_SRC_WRAPPING_ROW_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_WRAPPING_ROW_HPP

#include <QLayout>
#include <QList>

/// \file
/// \brief A row of a drawer's controls that wraps inside the drawer's width
/// instead of widening it. No drawer pans sideways (owner, look
/// workflow-v002).
namespace bpr::qt::detail {
	/// \brief Lays its items out as one row when they fit at their size
	/// hints, and wraps them onto further rows when they do not.
	///
	/// \details On one row, the space left over goes to the items that
	/// expand horizontally (a state that says where a row stands, a command
	/// line), so what follows them keeps the row's right end, as a box layout
	/// with a stretch would. Wrapped, an item that expands takes a row of its
	/// own, and an item wider than the row is given the row's width and the
	/// height it asks for at that width (a word-wrapped label wraps). The row
	/// is never wider than the width it is given, unless one item cannot be
	/// narrower than its own minimum.
	///
	/// A row of a part's quiet choices that ends in its one primary action
	/// says so (`set_last_trails`): the last item then keeps the row's right
	/// end, where a stop's own primary action stands. On one row the space
	/// left over stands before it. Wrapped, it ends the last row, and goes on
	/// a row of its own when it does not fit beside what is there.
	class wrapping_row final : public QLayout
	{
	public:
		explicit wrapping_row(QWidget* aParent = nullptr, int aHorizontalSpacing = 6, int aVerticalSpacing = 4);
		~wrapping_row() override;
		wrapping_row(const wrapping_row&)                    = delete;
		auto operator=(const wrapping_row&) -> wrapping_row& = delete;

		/// \brief Whether the last item keeps the row's right end.
		void               set_last_trails(bool aTrails);
		[[nodiscard]] auto last_trails() const noexcept -> bool { return LastTrails; }
		void               addItem(QLayoutItem* aItem) override;
		[[nodiscard]] auto count() const -> int override;
		[[nodiscard]] auto itemAt(int aIndex) const -> QLayoutItem* override;
		[[nodiscard]] auto takeAt(int aIndex) -> QLayoutItem* override;
		[[nodiscard]] auto expandingDirections() const -> Qt::Orientations override;
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto minimumSize() const -> QSize override;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		void               setGeometry(const QRect& aRect) override;

	private:
		/// \brief Places the items in \p aRect (or only measures, when \p
		/// aPlace is false) and returns the height they take.
		auto arrange(const QRect& aRect, bool aPlace) const -> int;

		QList<QLayoutItem*> Items;
		int                 HorizontalSpacing;
		int                 VerticalSpacing;
		bool                LastTrails = false;
	};
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_WRAPPING_ROW_HPP
