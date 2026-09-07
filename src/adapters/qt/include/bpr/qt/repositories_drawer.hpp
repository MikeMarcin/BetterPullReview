#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPOSITORIES_DRAWER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REPOSITORIES_DRAWER_HPP

#include "bpr/core/review_queue.hpp"
#include "bpr/core/review_queue_attention.hpp"

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
#include <string>
#include <vector>

namespace bpr::qt {
	/// \brief The one-word reason a pull request is in the tree.
	[[nodiscard]] auto repository_reason_word(core::attention_bucket aBucket) -> QString;
	/// \brief Whether a bucket is one that needs me now (amber).
	[[nodiscard]] auto repository_needs_me(core::attention_bucket aBucket) noexcept -> bool;

	/// \brief One repository of the tree, with its pull requests of interest.
	struct repository_node
	{
		std::string                   Host;
		std::string                   Account;
		std::string                   Repository;
		std::size_t                   NeedsMe = 0U;
		std::vector<core::queue_item> Items;
	};

	/// \brief RFC-BPR-0028's repositories tree: repositories, then their pull requests.
	///
	/// \details [[BPR-0651]]. Built from the queue landing's items in Home's
	/// order: each repository the items name is a row with "needs you of
	/// total", and its pull requests hang beneath it, one per row, with the
	/// number and a one-word reason at the right, amber when the bucket is
	/// one that needs me. Titles and unread dots wait on the projection that
	/// carries them (BPR-0659); the tree decides nothing and holds only what
	/// the projection said.
	class repositories_tree_model final : public QAbstractItemModel
	{
	public:
		enum column : int {
			name_column,
			reason_column,
			column_count,
		};

		enum roles : int {
			repository_role = Qt::UserRole + 1,
			number_role,
			reason_role,
			needs_me_role,
			is_repository_role,
			host_role,
			account_role,
		};

		explicit repositories_tree_model(QObject* aParent = nullptr);

		void apply(std::span<const core::queue_item> aItems);
		void clear();

		[[nodiscard]] auto index(int aRow, int aColumn, const QModelIndex& aParent = { }) const
			-> QModelIndex override;
		[[nodiscard]] auto parent(const QModelIndex& aIndex) const -> QModelIndex override;
		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const -> int override;
		[[nodiscard]] auto columnCount(const QModelIndex& aParent = { }) const -> int override;
		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRole) const -> QVariant override;

		[[nodiscard]] auto repositories() const noexcept -> const std::vector<repository_node>&;
		[[nodiscard]] auto item_at(const QModelIndex& aIndex) const
			-> std::optional<core::queue_item>;
		[[nodiscard]] auto index_for(const core::pull_request_ref& aRef) const -> QModelIndex;
		[[nodiscard]] auto needs_me_count() const noexcept -> std::size_t;
		[[nodiscard]] auto item_count() const noexcept -> std::size_t;

	private:
		std::vector<repository_node> Repositories;
	};

	/// \brief Paints the reason word in amber when it needs me.
	class repositories_tree_delegate final : public QStyledItemDelegate
	{
	public:
		explicit repositories_tree_delegate(QObject* aParent = nullptr);

		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			override;

		void set_marker(const QColor& aMarker);

	private:
		QColor Marker{ 0xD2, 0x99, 0x22 };
	};

	using repository_open_dispatch = std::function<void(const core::queue_item&)>;

	/// \brief The repositories drawer's tree view.
	///
	/// \details Selecting a pull request row reports the item; the window
	/// opens it on Overview. Repository rows expand and collapse and report
	/// nothing.
	class repositories_tree_view final : public QTreeView
	{
	public:
		explicit repositories_tree_view(QWidget* aParent = nullptr);

		[[nodiscard]] auto tree_model() const noexcept -> repositories_tree_model*;
		void               set_open_dispatch(repository_open_dispatch aDispatch);
		void               apply(std::span<const core::queue_item> aItems);
		[[nodiscard]] auto delegate() const noexcept -> repositories_tree_delegate*;

	protected:
		void currentChanged(const QModelIndex& aCurrent, const QModelIndex& aPrevious) override;

	private:
		repositories_tree_model*    Model    = nullptr;
		repositories_tree_delegate* Delegate = nullptr;
		repository_open_dispatch    OpenDispatch;
	};
} // namespace bpr::qt

#endif
