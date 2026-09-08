#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP

#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/setup_destination_projection.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <QAbstractListModel>
#include <QColor>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

#include <optional>
#include <span>
#include <vector>

namespace bpr::qt {
	/// \brief Returns the bucket badge, readable without colour.
	[[nodiscard]] auto badge_for(core::attention_bucket aBucket) -> QString;

	/// \brief Home's three groups ([[BPR-0650]]): what needs me, what waits
	/// on somebody else, and what I follow.
	enum class home_group : std::uint8_t {
		needs_you,
		waiting_on_others,
		following,
	};

	/// \brief Which of Home's groups a bucket belongs to.
	///
	/// \details The first four buckets are the ones that need me, exactly
	/// the set the repositories drawer marks amber; authored-and-waiting is
	/// waiting on others; observed activity and quiet are following.
	[[nodiscard]] auto home_group_of(core::attention_bucket aBucket) noexcept -> home_group;
	/// \brief The group's heading, first person and one line.
	[[nodiscard]] auto home_group_label(home_group aGroup) -> QString;

	/// \brief Translates locator text through the bounded core parser, retaining its exact host.
	[[nodiscard]] auto parse_pull_request_locator(const QString& aText)
		-> std::optional<core::setup_destination>;

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
			/// [[BPR-0650]]. The action list's columns: title, author, my
			/// role, the group the row is in, and the unread dot.
			title_role,
			author_role,
			role_role,
			group_role,
			unseen_role,
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
	/// \brief Draws Home's action list ([[BPR-0650]]).
	///
	/// \details One row per pull request: "repository #n", the title, the
	/// one-word reason (amber when it needs me), the author and my role;
	/// a group heading above the first row of each group. It reads the
	/// model's roles and decides nothing; the group order is the core's
	/// bucket order, which is what the priority sort gives.
	class review_landing_delegate final : public QStyledItemDelegate
	{
	public:
		explicit review_landing_delegate(QObject* aParent = nullptr);

		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			override;
		[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
			-> QSize override;
		/// \brief True when the row is the first of its group and carries the heading.
		[[nodiscard]] static auto heads_group(const QModelIndex& aIndex) -> bool;
		void                      set_marker(const QColor& aMarker);
		void                      set_accent(const QColor& aAccent);

		static constexpr int row_height     = 26;
		static constexpr int heading_height = 22;

	private:
		QColor Marker{ 0xD2, 0x99, 0x22 };
		QColor Accent{ 0x09, 0x69, 0xDA };
	};

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
