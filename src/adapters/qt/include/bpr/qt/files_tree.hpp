#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FILES_TREE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FILES_TREE_HPP

#include "bpr/application/pull_request_overview.hpp"
#include "bpr/core/review_ledger.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAbstractItemModel>
#include <QColor>
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
#include <vector>

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
		std::vector<std::size_t>   Children;
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
			/// -1 unknown, else the unread comment count.
			unread_role,
			/// True when the overview says the file changed since my last review.
			changed_role,
			thread_count_role,
		};

		explicit files_tree_model(QObject* aParent = nullptr);

		void               apply(const std::vector<workbench_drawer_row>& aRows);
		void               apply_overview(std::span<const application::overview_file_row> aFiles);
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

	private:
		void               rebuild();
		[[nodiscard]] auto node_index(const QModelIndex& aIndex) const -> std::size_t;

		std::vector<files_tree_entry> Entries;
		std::vector<files_tree_node>  Nodes;
		files_order                   Order = files_order::tree;
		QString                       Filter;
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

	private:
		QColor Accent{ 0x09, 0x69, 0xDA };
		QColor Added;
		QColor Removed;
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
		void               apply(const std::vector<workbench_drawer_row>& aRows);
		void               apply_overview(std::span<const application::overview_file_row> aFiles);
		void               adopt_appearance(const appearance_profile& aProfile);
		void               set_order(files_order aOrder);
		void               set_filter(const QString& aFilter);
		/// \brief Makes the file's row current without reporting it.
		void select_file(const std::optional<core::workbench_file_id>& aFile);

	protected:
		void currentChanged(const QModelIndex& aCurrent, const QModelIndex& aPrevious) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		files_tree_model*                      Model    = nullptr;
		files_tree_delegate*                   Delegate = nullptr;
		files_tree_select_dispatch             Dispatch;
		std::optional<core::workbench_file_id> SelectedFile;
		bool                                   Applying = false;
	};
} // namespace bpr::qt

#endif
