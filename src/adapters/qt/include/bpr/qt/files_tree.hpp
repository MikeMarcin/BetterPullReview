#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FILES_TREE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FILES_TREE_HPP

#include "bpr/application/pull_request_overview.hpp"
#include "bpr/core/review_ledger.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVariant>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

class QContextMenuEvent;
class QMenu;
class QMouseEvent;

namespace bpr::qt {
	class appearance_profile;
	/// \brief The files drawer's three orders ([[BPR-0658]]).
	enum class files_order : std::uint8_t {
		/// Folders as the paths nest, nesting-only folders compacted.
		tree,
		/// Every file by its full path, no folders.
		flat,
		/// Flat, with the files changed since my last review first.
		changed_first,
	};

	/// \brief One file of the drawer, joined with what the overview says of it.
	struct files_tree_entry
	{
		QString                                       Path;
		core::workbench_file_id                       File;
		std::optional<application::overview_file_row> Overview;
		/// [[BPR-0972]]. Left the drawer's order; kept so its overview facts
		/// return with it.
		bool Withheld = false;
	};

	/// \brief One row of the tree: a folder or a file.
	struct files_tree_node
	{
		/// The name drawn: a file's name, or a folder's path segments (several
		/// when nesting-only folders are compacted into one row).
		QString Label;
		/// The full path of a file; the folder prefix of a folder.
		QString                    Path;
		bool                       Folder    = false;
		std::size_t                FileCount = 0U;
		std::optional<std::size_t> Entry;
		std::size_t                Parent = 0U;
		int                        Row    = 0;
		std::vector<std::size_t>   Children;
	};

	/// \brief Counts file-tree work without inferring whole-process allocations.
	struct files_tree_work_statistics
	{
		std::uint64_t ModelResets         = 0U;
		std::uint64_t ChangedRows         = 0U;
		std::uint64_t OverviewRowsVisited = 0U;
		std::uint64_t LookupComparisons   = 0U;
		/// [[BPR-0972]]. Structural notifications of a reconciled order.
		std::uint64_t RowsInserted = 0U;
		std::uint64_t RowsRemoved  = 0U;
		std::uint64_t RowsMoved    = 0U;
	};

	/// \brief RFC-BPR-0028's files tree ([[BPR-0658]]).
	///
	/// \details A second view of the files drawer's rows, as the repositories
	/// tree is of the queue: the flat `workbench_drawer_model` stays the
	/// authority the composition's courses select in, and this model draws
	/// the same files as a tree joined with the overview's per-file operation,
	/// counts and read state. It decides nothing: the order and the filter
	/// are presentation, and selecting a row is carried to the list, whose
	/// selection is what dispatches the intent. Nesting-only folders are
	/// compacted into one row, folders carry their file count, and a filter
	/// keeps a matching file's parents visible.
	///
	/// [[BPR-0972]]. A changed file list, order or filter is reconciled with
	/// the rows already shown by folder and path: rows that remain keep their
	/// identity, persistent indexes and expansion, and only inserted, removed,
	/// moved or changed rows are signalled. The model never resets.
	class files_tree_model final : public QAbstractItemModel
	{
	public:
		enum role : int {
			path_role = Qt::UserRole + 1,
			folder_role,
			file_count_role,
			/// The overview's operation name, empty when the overview has not said.
			operation_role,
			/// -1 unknown, else `application::overview_file_operation`.
			operation_kind_role,
			additions_role,
			deletions_role,
			/// -1 unknown, else `core::file_read_state`.
			read_role,
			/// [[BPR-1339]]. -1 unknown, else the `core::file_progress_mark`
			/// the overview decided.
			progress_role,
			/// -1 unknown, else the unread comment count.
			unread_role,
			/// True when the overview says the file changed since my last review.
			changed_role,
			thread_count_role,
		};

		explicit files_tree_model(QObject* aParent = nullptr);

		[[nodiscard]] auto work_statistics() const noexcept -> files_tree_work_statistics;
		void               apply(const std::vector<workbench_drawer_row>& aRows);
		void               apply_overview(std::span<const application::overview_file_row> aFiles);
		/// \brief [[BPR-0972]]. Takes the overview facts of these files only.
		void apply_overview_rows(std::span<const application::overview_file_row> aRows);
		/// \brief [[BPR-0972]]. Removes and inserts single files in place.
		/// \details A file whose folder already holds other rows, and whose
		/// removal leaves no folder to compact, moves only its own row and its
		/// folders' counts; anything else is reconciled as a complete list.
		void               apply_delta(std::span<const core::workbench_file_id> aRemoved,
									   std::span<const workbench_drawer_row>    aInserted);
		void               set_order(files_order aOrder);
		[[nodiscard]] auto order() const noexcept -> files_order;
		void               set_filter(const QString& aFilter);
		[[nodiscard]] auto filter() const noexcept -> const QString&;

		[[nodiscard]] auto index(int aRow, int aColumn, const QModelIndex& aParent = { }) const
			-> QModelIndex override;
		[[nodiscard]] auto parent(const QModelIndex& aIndex) const -> QModelIndex override;
		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const -> int override;
		[[nodiscard]] auto columnCount(const QModelIndex& aParent = { }) const -> int override;
		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRole) const -> QVariant override;
		[[nodiscard]] auto flags(const QModelIndex& aIndex) const -> Qt::ItemFlags override;

		[[nodiscard]] auto index_for_file(const core::workbench_file_id& aFile) const -> QModelIndex;
		[[nodiscard]] auto file_at(const QModelIndex& aIndex) const -> std::optional<core::workbench_file_id>;
		[[nodiscard]] auto node_at(const QModelIndex& aIndex) const -> const files_tree_node*;
		[[nodiscard]] auto file_count() const noexcept -> std::size_t;
		[[nodiscard]] auto nodes() const noexcept -> const std::vector<files_tree_node>&;
		/// \brief Returns, once, the folder rows inserted since the last call.
		/// \details A view expands these and leaves every retained folder as
		/// the reviewer left it.
		[[nodiscard]] auto take_inserted_folders() -> std::vector<QModelIndex>;

	private:
		void               rebuild();
		void               rebuild(const std::vector<files_tree_entry>& aPrevious);
		[[nodiscard]] auto build_nodes() const -> std::vector<files_tree_node>;
		void               reconcile(std::size_t aCurrent, const std::vector<files_tree_node>& aDesired,
									 std::size_t aWanted, const std::vector<files_tree_entry>& aPrevious);
		[[nodiscard]] auto adopt(const std::vector<files_tree_node>& aDesired, std::size_t aWanted, std::size_t aParent)
			-> std::size_t;
		void               release(std::size_t aNode);
		void               renumber(std::size_t aParent, std::size_t aFrom);
		[[nodiscard]] auto index_of_node(std::size_t aNode) const -> QModelIndex;
		[[nodiscard]] auto remove_file_node(std::size_t aNode) -> bool;
		[[nodiscard]] auto insert_file_node(std::size_t aEntry) -> bool;
		void               count_ancestors(std::size_t aNode, bool aAdded);
		void               rebuild_file_index();
		[[nodiscard]] auto node_index(const QModelIndex& aIndex) const -> std::size_t;

		mutable files_tree_work_statistics WorkStatistics;
		std::vector<std::size_t>           FreeNodes;
		/// [[BPR-0972]]. Every entry by file, shown or filtered away.
		std::vector<std::pair<core::workbench_file_id, std::size_t>> EntryIndex;
		/// [[BPR-0972]]. Each folder row by the path it ends at.
		QHash<QString, std::size_t>                                  FolderIndex;
		std::vector<std::size_t>                                     InsertedFolders;
		std::vector<std::pair<core::workbench_file_id, std::size_t>> FileIndex;
		std::vector<files_tree_entry>                                Entries;
		std::vector<files_tree_node>                                 Nodes;
		files_order                                                  Order = files_order::tree;
		QString                                                      Filter;
	};

	/// \brief Draws the tree's rows: guide lines, the read dot, the counts.
	class files_tree_delegate final : public QStyledItemDelegate
	{
	public:
		explicit files_tree_delegate(QObject* aParent = nullptr);

		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			override;
		void set_accent(const QColor& aAccent);
		void set_operation_colours(const QColor& aAdded, const QColor& aRemoved);
		/// \brief [[BPR-1331]] v003. The file marks' colours, the review bar's
		/// family in the theme on screen.
		void               set_progress_colours(const progress_mark_colours& aColours);
		[[nodiscard]] auto progress_colours() const noexcept -> const progress_mark_colours&;

		/// \brief [[BPR-1331]] v003. The side of the design box a file's mark
		/// is painted in: ten pixels across, as Overview's.
		static constexpr qreal mark_side = 15.0;
		/// \brief Where a file's mark is centred, from the row's left, and
		/// where its name starts.
		static constexpr int mark_centre = 7;
		static constexpr int name_inset  = 17;

	private:
		QColor                Accent{ 0x09, 0x69, 0xDA };
		QColor                Added;
		QColor                Removed;
		progress_mark_colours Progress = builtin_progress_mark_colours(core::appearance_variant::light);
	};

	using files_tree_select_dispatch = std::function<void(const core::workbench_file_id&)>;

	/// \brief The files drawer's tree view.
	///
	/// \details Choosing a file row reports its identity; the window turns
	/// that into the list's selection, which dispatches the intent. A folder
	/// row expands and collapses and reports nothing.
	class files_tree_view final : public QTreeView
	{
	public:
		explicit files_tree_view(QWidget* aParent = nullptr);

		[[nodiscard]] auto tree_model() const noexcept -> files_tree_model*;
		[[nodiscard]] auto delegate() const noexcept -> files_tree_delegate*;
		void               set_select_dispatch(files_tree_select_dispatch aDispatch);
		/// \brief [[BPR-1132]]. Cmd-click (Ctrl-click) on a file row, and its
		/// menu's Open in New Tab, report the file here instead of choosing it.
		void set_new_tab_dispatch(files_tree_select_dispatch aDispatch);
		/// \brief The menu a right-click on a file row opens; the caller owns it.
		[[nodiscard]] auto make_file_menu(const QModelIndex& aIndex, QWidget* aParent) -> QMenu*;
		void               apply(const std::vector<workbench_drawer_row>& aRows);
		void               apply_overview(std::span<const application::overview_file_row> aFiles);
		/// \brief [[BPR-0972]]. Takes the overview facts of these files only.
		void apply_overview_rows(std::span<const application::overview_file_row> aRows);
		/// \brief [[BPR-0972]]. Removes and inserts single files in place.
		void apply_delta(std::span<const core::workbench_file_id> aRemoved,
						 std::span<const workbench_drawer_row>    aInserted);
		void adopt_appearance(const appearance_profile& aProfile);
		void set_order(files_order aOrder);
		void set_filter(const QString& aFilter);
		/// \brief Makes the file's row current without reporting it.
		void select_file(const std::optional<core::workbench_file_id>& aFile);

	protected:
		void currentChanged(const QModelIndex& aCurrent, const QModelIndex& aPrevious) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void contextMenuEvent(QContextMenuEvent* aEvent) override;

	private:
		/// \brief Expands only the folder rows the model just inserted.
		void expand_inserted();

		files_tree_model*                      Model    = nullptr;
		files_tree_delegate*                   Delegate = nullptr;
		files_tree_select_dispatch             Dispatch;
		files_tree_select_dispatch             NewTabDispatch;
		std::optional<core::workbench_file_id> SelectedFile;
		bool                                   Applying = false;
	};
} // namespace bpr::qt

#endif
