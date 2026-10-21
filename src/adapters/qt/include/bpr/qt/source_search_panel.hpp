#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_SEARCH_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_SEARCH_PANEL_HPP
#include "bpr/core/source_matcher.hpp"
#include "bpr/qt/source_find_bar.hpp"
#include <QFont>
#include <QModelIndex>
#include <QRect>
class QAbstractButton;
class QTreeView;
class QLineEdit;
class QLabel;
class QMenu;
class QToolButton;
class QWidget;
namespace bpr::qt {
	class appearance_profile;
	class source_search_result_model;
	class source_search_result_delegate;
	/// \brief Item roles the result model answers ([[BPR-1504]]).
	/// \details File rows answer the file roles and match rows the match
	/// roles. The highlight range indexes `preview_role`'s text in UTF-16 units.
	enum source_search_role : int {
		/// One plus the global match index on a match row; nothing on a file row.
		source_search_match_role = Qt::UserRole,
		/// True on a file row.
		source_search_file_role,
		/// The file's name, `old → new` when a rename changes it.
		source_search_name_role,
		/// The file's folder, `old → new` when a rename moves it.
		source_search_folder_role,
		/// The file's shown match count under the side filter.
		source_search_count_role,
		/// "added", "removed", "renamed" or empty.
		source_search_change_role,
		/// Bit 1 for a left location, bit 2 for a right location.
		source_search_sides_role,
		/// The one-based line number text.
		source_search_line_role,
		/// The snippet as drawn: newlines as ↵, leading blanks removed.
		source_search_preview_role,
		source_search_highlight_start_role,
		source_search_highlight_length_role,
		/// [[BPR-PENDING-T]]. A match row as Copy writes it: `path:line: text`,
		/// with the path and line of the side the row shows.
		source_search_copy_role,
		/// [[BPR-PENDING-T]]. The file's path, on a file row and its match rows.
		source_search_path_role,
	};
	/// \brief A match row's three fixed columns in viewport coordinates.
	struct source_search_match_columns
	{
		QRect Tags;
		QRect Number;
		QRect Snippet;
	};
	/// \brief Native PR search projection, preserving result indexes between ordered batches.
	///
	/// \details [[BPR-PENDING-T]]. The results take an extended selection
	/// (Shift and Command or Ctrl); a click without either, Return and the
	/// menu's Open still act on one result. The row menu (right-click, the
	/// menu key, Shift+F10) offers Open, Open in New Tab, Copy and Copy Path.
	/// Copy writes the selected results to the clipboard, the one effect the
	/// panel performs itself; opening is the controller's.
	class source_search_panel final : public QWidget
	{
	public:
		explicit source_search_panel(QWidget* aParent);
		std::function<void(source_find_gesture)> Dispatch;
		std::function<void(std::uint32_t)>       Activate;
		/// [[BPR-PENDING-T]]. Open in New Tab: the result in a tab of its own.
		std::function<void(std::uint32_t)>            ActivateInNewTab;
		std::function<void(std::uint32_t)>            Select;
		std::function<void()>                         Changed;
		std::function<void(core::source_side_filter)> SideFilterChanged;
		void                                          present(const application::source_search_projection& aProjection);
		void                                          set_scope_labels(QString aLabel, QString aIdentity);
		void                                          set_snippet_font(const QFont& aFont);
		/// \brief The side tags, file tags and match wash take the theme's role colours.
		void               adopt_appearance(const appearance_profile& aProfile);
		void               set_side_filter(core::source_side_filter aFilter);
		void               reset();
		void               cancel_pending();
		[[nodiscard]] auto bar() const -> source_find_bar*;
		[[nodiscard]] auto include() const -> std::string;
		[[nodiscard]] auto exclude() const -> std::string;
		[[nodiscard]] auto side_filter() const -> core::source_side_filter;
		[[nodiscard]] auto summary_text() const -> QString;
		[[nodiscard]] auto projection() const -> const application::source_search_projection&;
		[[nodiscard]] auto tree() const -> QTreeView*;
		/// \brief The delegate's column geometry for a match row; empty for a file row.
		[[nodiscard]] auto match_columns(const QModelIndex& aIndex) const -> source_search_match_columns;
		/// \brief [[BPR-PENDING-T]]. The row menu for `aIndex`, built as a
		/// right-click builds it; the caller owns it. None off a row.
		[[nodiscard]] auto make_result_menu(const QModelIndex& aIndex, QWidget* aParent) -> QMenu*;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		source_find_bar*                      Bar;
		QAbstractButton*                      Sides[2];
		QToolButton*                          CollapseAll;
		QToolButton*                          Details;
		QWidget*                              DetailsBody;
		QLabel*                               Scope;
		QLabel*                               Identity;
		QLabel*                               Coverage;
		QLabel*                               Hint;
		QToolButton*                          SkippedToggle;
		QLabel*                               Skipped;
		QToolButton*                          Retry;
		QLineEdit*                            Include;
		QLineEdit*                            Exclude;
		QTreeView*                            Tree;
		source_search_result_model*           Model;
		source_search_result_delegate*        Delegate;
		QTimer*                               Debounce;
		application::source_search_projection Projection;
		QString                               LastAnnouncement;
		void                                  append_results(const application::source_search_projection& aProjection);
		void                                  present_summary();
		void                                  select_current();
		void                                  refresh_icons();
		void                                  popup_result_menu(const QModelIndex& aIndex, const QPoint& aGlobal);
		/// The selected results as Copy writes them: one `path:line: text`
		/// line each, in the list's order. A selected file row stands for all
		/// of its shown results; with nothing selected, the current result.
		[[nodiscard]] auto selected_text() const -> QString;
		void               copy_selected() const;
		/// The rows Copy and Copy Path read: the selection, else the current row.
		[[nodiscard]] auto chosen_rows() const -> QModelIndexList;
	};
}
#endif
