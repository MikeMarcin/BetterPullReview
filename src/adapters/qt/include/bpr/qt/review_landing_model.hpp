#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP

#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include <optional>
#include <span>
#include <vector>

namespace bpr::qt {
	/// \brief Returns the bucket badge, readable without colour.
	[[nodiscard]] auto badge_for(core::attention_bucket aBucket) -> QString;

	/// \brief Returns the sentence saying why an item is in the queue.
	///
	/// \details Both halves: what the bucket asks, and — when a local rule is
	/// why the item is here at all — which rule matched. A reviewer looking at
	/// an unfamiliar pull request in their own queue is owed the second.
	[[nodiscard]] auto reason_for(const core::queue_item& aItem) -> QString;

	/// \brief Presents the reviewer's landing page.
	///
	/// \details The model holds what the core ordered and renders it. It
	/// decides no bucket and no order: a queue that sorted itself would
	/// disagree with the core the moment either changed, and the reviewer would
	/// have no way to tell which was right.
	///
	/// There is no mutating entry point beyond `apply` and `clear`. The
	/// presentation cannot enroll, depart, or re-bucket anything, because there
	/// is no method by which it could.
	class review_landing_model final : public QAbstractListModel
	{
	public:
		enum role : int {
			badge_role = Qt::UserRole + 1,
			reason_role,
			repository_role,
			number_role,
			bucket_role,
		};

		explicit review_landing_model(QObject* aParent = nullptr);

		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const
			-> int override;
		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRequestedRole)
			const -> QVariant override;
		[[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

		/// \brief Adopts one ordered queue, signalling only what moved.
		///
		/// \details Rows are matched by pull-request identity rather than by
		/// position. An item that changes bucket moves; it is not removed and
		/// re-inserted, and every other row's persistent index survives.
		///
		/// A positional diff would report a move as a long run of data changes,
		/// which is correct only in the sense that a reset is correct: the view
		/// re-reads everything, the selection is meaningless, and assistive
		/// technology is told the list it was reading no longer exists.
		void apply(std::span<const core::queue_item> aQueue);

		void clear();

		[[nodiscard]] auto item_at(const QModelIndex& aIndex) const
			-> std::optional<core::queue_item>;

	private:
		[[nodiscard]] auto position_of(const core::pull_request_ref& aRef) const
			-> std::optional<std::size_t>;

		std::vector<core::queue_item> Items;
	};

	/// \brief Filters and explicitly reorders the decided landing queue.
	///
	/// \details The source model remains in the core's exact priority order.
	/// The default proxy order follows it without a comparison; alternate sorts
	/// run only when the reviewer selects one. Qt's proxy mapping retains stable
	/// source identity and emits incremental filter/layout signals rather than a
	/// routine model reset.
	class review_landing_filter_model final : public QSortFilterProxyModel
	{
	public:
		explicit review_landing_filter_model(
			review_landing_model& aSource, QObject* aParent = nullptr);

		void apply(
			const application::queue_presentation_preferences& aPreferences);
		[[nodiscard]] auto preferences() const noexcept
			-> const application::queue_presentation_preferences&;
		[[nodiscard]] auto item_at(const QModelIndex& aIndex) const
			-> std::optional<core::queue_item>;
		[[nodiscard]] auto has_active_filters() const noexcept -> bool;

	protected:
		[[nodiscard]] auto filterAcceptsRow(
			int aSourceRow, const QModelIndex& aSourceParent) const
			-> bool override;
		[[nodiscard]] auto lessThan(
			const QModelIndex& aLeft, const QModelIndex& aRight) const
			-> bool override;

	private:
		review_landing_model&                       Source;
		application::queue_presentation_preferences Preferences;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP
