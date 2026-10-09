#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_STEADY_COLUMNS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_STEADY_COLUMNS_HPP

#include <QObject>

#include <functional>
#include <map>
#include <optional>
#include <vector>

class QEvent;
class QModelIndex;
class QTreeView;

/// \file
/// \brief [[BPR-1363]]. Tree columns that opening or closing a folder cannot
/// move.
/// \details The owner, 2026-10-09, on the Overview's Files table: "I dont like
/// that opening this disclosure shift the column widths". Every column there
/// was sized to the rows on screen, so a file row that appeared under an
/// opened folder, or a scroll, could widen a column and move every column
/// after it. A scroll bar that took space when the rows outgrew the view
/// narrowed the last column too, which in the Files drawer moved the counts at
/// its right edge.
///
/// Here a column's width is a function of the model's whole contents, every
/// row at its depth whether its folder is open or not, and of the header's own
/// label; it is measured again only when the contents, the font or the style
/// change. One column fills what the others leave, and the space a scroll bar
/// would take is set aside whether the bar shows or not, so the bar coming
/// and going moves nothing. A width the user drags stays until the user
/// double-clicks that column's edge, which gives back the measured width.
namespace bpr::qt {
	class steady_columns final : public QObject
	{
	public:
		/// \brief Splits the cells into kinds whose delegate measure grows with
		/// the text alone, such as folder rows painted bold and file rows; one
		/// kind when empty.
		using cell_kind = std::function<int(const QModelIndex&)>;

		/// \brief Takes over `aView`'s column widths; `aFillColumn` fills the
		/// rest. The view's model must be set.
		steady_columns(QTreeView& aView, int aFillColumn, cell_kind aKind = { });

		/// \brief A width the column keeps and the user cannot drag, or none to
		/// measure it again.
		void set_fixed_width(int aColumn, std::optional<int> aWidth);

		/// \brief Measures now if the contents changed since the last measure,
		/// then lays the columns out. Model changes also do this one event
		/// later; a presenter calls it so the first paint is already right.
		void apply();

		/// \brief The space the columns share: the view's contents less a
		/// scroll bar's width wherever the style's scroll bars take space.
		[[nodiscard]] auto shared_width() const -> int;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void invalidate();
		void measure();
		void lay_out();
		void user_resized(int aColumn, int aWidth);

		QTreeView&         View;
		int                Fill;
		cell_kind          Kind;
		std::vector<int>   Natural;
		std::map<int, int> User;
		std::map<int, int> Fixed;
		bool               Dirty    = true;
		bool               Queued   = false;
		bool               Applying = false;
	};
} // namespace bpr::qt
#endif
