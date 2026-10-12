#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP

#include "bpr/application/home_view_state.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/setup_destination_projection.hpp"
#include "bpr/application/window_review_restoration_ports.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace bpr::qt {
	/// \brief Returns the bucket badge, readable without colour.
	[[nodiscard]] auto badge_for(core::attention_bucket aBucket) -> QString;

	/// \brief The core-decided Home sections; ownership is separate from attention.
	using home_group = core::home_queue_section;

	/// \brief The attention colour group for a bucket, independent of Home ownership placement.
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

	/// \brief [[BPR-0776]]. The one word that ends a Home row, drawn in its state's colour.
	///
	/// \details Readable without colour: every bucket has its own word. No bucket
	/// means approved, so "Approved" is never produced.
	[[nodiscard]] auto home_reason_word(core::attention_bucket aBucket) -> QString;

	/// \brief [[BPR-0776]]. "owner/name#123" for a queue row.
	[[nodiscard]] auto home_identity(const core::queue_item& aItem) -> QString;

	/// \brief [[BPR-0776]]. The sentence that says why a row is where it is, in
	/// the first person.
	///
	/// \details Built only from what the core projected: the bucket, the author,
	/// the unseen comment count and, when a local rule is the only reason the
	/// row is here, that rule. Who pushed, how many updates, when and how far I
	/// read are not projected, and the sentence does not claim them.
	[[nodiscard]] auto home_why_sentence(const core::queue_item& aItem) -> QString;

	/// \brief [[BPR-0776]]. A collapsed state panel's one-line summary of its rows.
	///
	/// \details "2 requested, 1 changed; 4 new comments", "1 of mine", "3
	/// followed". Empty for a state with no row.
	[[nodiscard]] auto home_state_summary(home_group aGroup, std::span<const core::queue_item> aRows) -> QString;

	/// \brief [[BPR-0776]]. Recently viewed's collapsed summary: "2 viewed; 1 saved reading".
	[[nodiscard]] auto home_recent_summary(std::span<const application::recent_review_entry> aEntries)
		-> QString;

	/// \brief [[BPR-0776]]. What Home's list says when a ready queue is empty.
	[[nodiscard]] auto home_all_empty_sentence() -> QString;

	/// \brief [[BPR-0990]]. The clock time a discovery refresh succeeded at,
	/// in the reviewer's locale.
	[[nodiscard]] auto home_discovery_time(std::int64_t aSeconds) -> QString;

	/// \brief [[BPR-0990]]. The one sentence Home says about discovery.
	///
	/// \details When the list was last refreshed and from how many saved
	/// repositories, that a refresh is running, that the list is partial and
	/// why, that it is stale with its failure and how long GitHub asked to
	/// wait, or that nothing is saved to search. Empty with no account
	/// session: the signed-out Home carries the sign-in. Built only from the
	/// projected status; it never claims how long ago, which needs a clock.
	[[nodiscard]] auto home_discovery_sentence(const application::queue_discovery_status& aStatus) -> QString;

	/// \brief [[BPR-0783]]. One account the decided queue's rows name.
	///
	/// \details Derived from the rows themselves rather than from a session: the
	/// queue is what Home lists, and its rows carry the host and account each
	/// belongs to. Nothing here reaches a session, a vault or a provider.
	struct home_account
	{
		QString Host;
		QString Login;

		auto operator<=>(const home_account&) const = default;
	};

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
			/// [[BPR-0783]]. The host and account a row belongs to, which is what
			/// Home groups and chips by. Answered as roles so a slice of the queue
			/// can be filtered from the row itself.
			host_role,
			account_role,
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
		/// Updates an existing identity, preserving every persistent index.
		void               apply_item(const core::queue_item& aItem);
		[[nodiscard]] auto item_for(const core::pull_request_ref& aRef) const -> std::optional<core::queue_item>;
		/// \brief [[BPR-1365]]. The pull request this window has open, or none.
		///
		/// \details Its row answers `home_row_role::reviewing` and says so in its
		/// accessible description; only the rows it leaves and reaches change.
		void               set_reviewing(std::optional<core::pull_request_ref> aRef);
		[[nodiscard]] auto reviewing() const noexcept -> const std::optional<core::pull_request_ref>&;

		void clear();

		[[nodiscard]] auto item_at(const QModelIndex& aIndex) const
			-> std::optional<core::queue_item>;

		/// \brief [[BPR-0783]]. The distinct host and account of every row, in
		/// order, whatever any filter above this model admits.
		///
		/// \details The account set an account chip is decided by. It is the
		/// unfiltered queue's, so filtering to one account does not make the
		/// other one stop existing and silently drop every chip.
		[[nodiscard]] auto accounts() const noexcept -> const std::vector<home_account>&;

	private:
		[[nodiscard]] auto position_of(const core::pull_request_ref& aRef) const
			-> std::optional<std::size_t>;
		/// \brief Recounts the account set and says so when a chip appears or goes.
		void adopt_accounts();
		void index_positions(std::size_t aFirst, std::size_t aEnd);

		std::vector<core::queue_item>                 Items;
		std::map<core::pull_request_ref, std::size_t> Positions;
		std::vector<home_account>                     Accounts;
		std::optional<core::pull_request_ref>         Reviewing;
	};

	/// \brief Filters and explicitly reorders the decided landing queue.
	///
	/// \details The source model remains in the core's exact priority order.
	/// The default proxy order follows it without a comparison; alternate sorts
	/// run only when the reviewer selects one. Qt's proxy mapping retains stable
	/// source identity and emits incremental filter/layout signals rather than a
	/// routine model reset. [[BPR-0776]]: Home's state panels each admit one
	/// group of this proxy's rows, so a sort orders rows within a state.
	class review_landing_filter_model final : public QSortFilterProxyModel
	{
	public:
		explicit review_landing_filter_model(
			review_landing_model& aSource, QObject* aParent = nullptr);

		void apply(
			const application::queue_presentation_preferences& aPreferences);
		/// \brief [[BPR-0783]]. Adopts one window's Home view, whose Filter that
		/// window may own from its first Filter gesture.
		void set_home_view(const application::home_view_state& aView);
		/// \brief The profile's typed preferences, exactly as they were published.
		///
		/// \details Not what Home filters by: see `effective_preferences`. This
		/// stays the profile's answer so the Settings page and the store keep one
		/// vocabulary between them.
		[[nodiscard]] auto preferences() const noexcept
			-> const application::queue_presentation_preferences&;
		/// \brief [[BPR-0783]]. What Home actually filters by: the window's own
		/// four fields once it owns them, else the profile's, and never
		/// Repository or Bucket, which Group and the state order replaced.
		[[nodiscard]] auto effective_preferences() const noexcept
			-> const application::queue_presentation_preferences&;
		[[nodiscard]] auto home_view() const noexcept -> const application::home_view_state&;
		[[nodiscard]] auto item_at(const QModelIndex& aIndex) const
			-> std::optional<core::queue_item>;
		[[nodiscard]] auto has_active_filters() const noexcept -> bool;
		/// \brief [[BPR-0783]]. The accounts the unfiltered queue beneath names.
		///
		/// \details Deliberately the source's rather than this proxy's: filtering
		/// to one account must not make the other stop existing and silently drop
		/// every chip on the page.
		[[nodiscard]] auto accounts() const noexcept -> const std::vector<home_account>&;

	protected:
		[[nodiscard]] auto filterAcceptsRow(
			int aSourceRow, const QModelIndex& aSourceParent) const
			-> bool override;
		[[nodiscard]] auto lessThan(
			const QModelIndex& aLeft, const QModelIndex& aRight) const
			-> bool override;

	private:
		/// \brief Adopts a profile and a view together and refilters by what
		/// changed in the preferences Home reads.
		void adopt(
			const application::queue_presentation_preferences& aPreferences,
			const application::home_view_state&                aView);

		review_landing_model&                       Source;
		application::queue_presentation_preferences Preferences;
		application::home_view_state                View;
		application::queue_presentation_preferences Effective
			= application::home_preferences(application::home_view_state{ }, application::queue_presentation_preferences{ });
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_LANDING_MODEL_HPP
