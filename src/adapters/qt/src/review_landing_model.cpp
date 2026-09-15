#include "bpr/qt/review_landing_model.hpp"

#include "bpr/qt/home_row_delegate.hpp"

#include <QStringList>

#include <algorithm>
#include <array>
#include <cstdint>
#include <tuple>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text(const std::string_view aValue) -> QString
		{
			return QString::fromUtf8(
				aValue.data(), static_cast<qsizetype>(aValue.size()));
		}
	} // namespace

	auto badge_for(const core::attention_bucket aBucket) -> QString
	{
		return QStringLiteral("[%1]").arg(text(core::name(aBucket)));
	}

	auto home_group_of(const core::attention_bucket aBucket) noexcept -> home_group
	{
		switch (aBucket) {
		case core::attention_bucket::initial_review_requested:
		case core::attention_bucket::changed_since_my_review:
		case core::attention_bucket::new_comments_or_author_replies:
		case core::attention_bucket::fixes_awaiting_verification:
			return home_group::needs_you;
		case core::attention_bucket::authored_awaiting_reviewer_action:
			return home_group::waiting_on_others;
		case core::attention_bucket::observed_with_new_activity:
		case core::attention_bucket::quiet:
			break;
		}
		return home_group::following;
	}

	auto home_group_label(const home_group aGroup) -> QString
	{
		switch (aGroup) {
		case home_group::needs_you:
			return QStringLiteral("Needs me");
		case home_group::waiting_on_others:
			return QStringLiteral("Waiting on others");
		case home_group::following:
			break;
		}
		return QStringLiteral("Following");
	}

	auto parse_pull_request_locator(const QString& aText)
		-> std::optional<core::setup_destination>
	{
		if (aText.size() > 4096) {
			return std::nullopt;
		}
		return application::parse_setup_destination(aText.toStdString());
	}

	auto reason_for(const core::queue_item& aItem) -> QString
	{
		auto reason = text(core::explanation(aItem.Bucket));
		// The rule is named only when it is why the item is here at all. Saying
		// "you watch src/parser" about a review someone asked for would bury
		// the fact that they were asked.
		if (aItem.Membership.MatchedRule
			&& aItem.Membership.Role == core::github_role::none) {
			reason += QStringLiteral(" You watch %1 %2.")
						  .arg(text(core::name(aItem.Membership.Rule.Kind)))
						  .arg(text(aItem.Membership.Rule.Pattern));
		}
		return reason;
	}

	namespace {
		[[nodiscard]] auto counted(const std::uint64_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return QStringLiteral("%1 %2").arg(QString::number(aCount), aCount == 1U ? aOne : aMany);
		}

		[[nodiscard]] auto new_comments(const std::uint64_t aCount) -> QString
		{
			return counted(aCount, QStringLiteral("new comment"), QStringLiteral("new comments"));
		}
	} // namespace

	auto home_reason_word(const core::attention_bucket aBucket) -> QString
	{
		switch (aBucket) {
		case core::attention_bucket::initial_review_requested:
			return QStringLiteral("Requested");
		case core::attention_bucket::changed_since_my_review:
			return QStringLiteral("Changed");
		case core::attention_bucket::new_comments_or_author_replies:
			return QStringLiteral("Replied");
		case core::attention_bucket::fixes_awaiting_verification:
			return QStringLiteral("Verify");
		case core::attention_bucket::authored_awaiting_reviewer_action:
			return QStringLiteral("Waiting");
		case core::attention_bucket::observed_with_new_activity:
			return QStringLiteral("Following");
		case core::attention_bucket::quiet:
			break;
		}
		return QStringLiteral("Quiet");
	}

	auto home_identity(const core::queue_item& aItem) -> QString
	{
		return QStringLiteral("%1#%2").arg(text(aItem.Ref.Repository), QString::number(aItem.Ref.Number));
	}

	auto home_why_sentence(const core::queue_item& aItem) -> QString
	{
		const auto unseen  = static_cast<std::uint64_t>(aItem.Facts.UnseenComments);
		const auto author  = text(aItem.Facts.Author);
		auto       clauses = QStringList{ };
		switch (aItem.Bucket) {
		case core::attention_bucket::initial_review_requested:
			if (author.isEmpty()) {
				clauses.push_back(QStringLiteral("My review is requested"));
			} else {
				clauses.push_back(QStringLiteral("Opened by %1").arg(author));
				clauses.push_back(QStringLiteral("my review is requested"));
			}
			break;
		case core::attention_bucket::changed_since_my_review:
			clauses.push_back(QStringLiteral("The code moved since my last review"));
			break;
		case core::attention_bucket::new_comments_or_author_replies:
			clauses.push_back(unseen > 0U ? new_comments(unseen) + QStringLiteral(" I have not read")
										  : QStringLiteral("Something here is unread"));
			break;
		case core::attention_bucket::fixes_awaiting_verification:
			clauses.push_back(QStringLiteral("A defect I filed is marked fixed"));
			clauses.push_back(QStringLiteral("I have not checked it"));
			break;
		case core::attention_bucket::authored_awaiting_reviewer_action:
			clauses.push_back(QStringLiteral("My pull request waits on reviewers"));
			break;
		case core::attention_bucket::observed_with_new_activity:
			clauses.push_back(QStringLiteral("New activity on a pull request I follow"));
			break;
		case core::attention_bucket::quiet:
			clauses.push_back(QStringLiteral("Nothing here is waiting on anyone"));
			break;
		}
		if (aItem.Bucket != core::attention_bucket::new_comments_or_author_replies && unseen > 0U) {
			clauses.push_back(new_comments(unseen));
		}
		// As `reason_for`: the rule is named only when it is why the row is here.
		if (aItem.Membership.MatchedRule && aItem.Membership.Role == core::github_role::none) {
			clauses.push_back(QStringLiteral("I watch %1 %2")
								  .arg(text(core::name(aItem.Membership.Rule.Kind)), text(aItem.Membership.Rule.Pattern)));
		}
		return clauses.join(QStringLiteral("; ")) + QLatin1Char{ '.' };
	}

	auto home_state_summary(const home_group aGroup, const std::span<const core::queue_item> aRows) -> QString
	{
		if (aRows.empty()) {
			return { };
		}
		auto unseen = std::uint64_t{ 0U };
		for (const auto& row : aRows) {
			unseen += row.Facts.UnseenComments;
		}
		auto summary = QString{ };
		switch (aGroup) {
		case home_group::needs_you: {
			auto parts = QStringList{ };
			for (const auto bucket : std::array{ core::attention_bucket::initial_review_requested,
												 core::attention_bucket::changed_since_my_review,
												 core::attention_bucket::new_comments_or_author_replies,
												 core::attention_bucket::fixes_awaiting_verification }) {
				const auto count = std::ranges::count(aRows, bucket, &core::queue_item::Bucket);
				if (count > 0) {
					parts.push_back(QStringLiteral("%1 %2").arg(
						QString::number(count),
						bucket == core::attention_bucket::fixes_awaiting_verification
							? QStringLiteral("to verify")
							: home_reason_word(bucket).toLower()));
				}
			}
			summary = parts.join(QStringLiteral(", "));
			break;
		}
		case home_group::waiting_on_others:
			summary = QStringLiteral("%1 of mine").arg(QString::number(aRows.size()));
			break;
		case home_group::following:
			summary = QStringLiteral("%1 followed").arg(QString::number(aRows.size()));
			break;
		}
		if (unseen > 0U) {
			summary += QStringLiteral("; ") + new_comments(unseen);
		}
		return summary;
	}

	auto home_recent_summary(const std::span<const application::recent_review_entry> aEntries) -> QString
	{
		if (aEntries.empty()) {
			return { };
		}
		auto       summary = QStringLiteral("%1 viewed").arg(QString::number(aEntries.size()));
		const auto saved   = std::ranges::count(aEntries, true, &application::recent_review_entry::Resumable);
		if (saved > 0) {
			summary += QStringLiteral("; ")
				+ counted(static_cast<std::uint64_t>(saved), QStringLiteral("saved reading"), QStringLiteral("saved readings"));
		}
		return summary;
	}

	auto home_all_empty_sentence() -> QString
	{
		return QStringLiteral("Nothing needs me. Pull requests from saved repositories and observer rules appear here.");
	}

	review_landing_model::review_landing_model(QObject* const aParent)
		: QAbstractListModel{ aParent }
	{
	}

	auto review_landing_model::rowCount(const QModelIndex& aParent) const -> int
	{
		return aParent.isValid() ? 0 : static_cast<int>(Items.size());
	}

	auto review_landing_model::data(
		const QModelIndex& aIndex, const int aRequestedRole) const -> QVariant
	{
		const auto item = item_at(aIndex);
		if (!item) {
			return { };
		}
		switch (aRequestedRole) {
		case Qt::DisplayRole:
			return QStringLiteral("%1 %2 #%3")
				.arg(badge_for(item->Bucket))
				.arg(text(item->Ref.Repository))
				.arg(item->Ref.Number);
		case badge_role:
			return badge_for(item->Bucket);
		case reason_role:
			return reason_for(*item);
		case Qt::AccessibleTextRole:
			return item->Facts.Title.empty()
				? home_identity(*item)
				: QStringLiteral("%1 %2").arg(home_identity(*item), text(item->Facts.Title));
		case Qt::AccessibleDescriptionRole:
			return QStringLiteral("%1 %2").arg(home_why_sentence(*item), home_reason_word(item->Bucket));
		case repository_role:
			return text(item->Ref.Repository);
		case number_role:
			return QVariant::fromValue(item->Ref.Number);
		case bucket_role:
			return static_cast<int>(item->Bucket);
		case title_role:
			return text(item->Facts.Title);
		case author_role:
			return text(item->Facts.Author);
		case role_role:
			return item->Membership.Role == core::github_role::none
				? QString{ }
				: text(core::name(item->Membership.Role));
		case group_role:
			return static_cast<int>(home_group_of(item->Bucket));
		case unseen_role:
			return item->Facts.UnseenComments > 0U;
		// [[BPR-0776]]. The roles Home's one row anatomy reads. Time and
		// progress are not projected for a queue row, so they are empty and
		// nothing is drawn for them.
		case home_row_role::identity:
			return home_identity(*item);
		case home_row_role::title:
			return text(item->Facts.Title);
		case home_row_role::why:
			return home_why_sentence(*item);
		case home_row_role::reason_word:
			return home_reason_word(item->Bucket);
		case home_row_role::state:
			return static_cast<int>(home_group_of(item->Bucket));
		case home_row_role::time:
		case home_row_role::progress:
		case home_row_role::account_chip:
		case home_row_role::row_action:
			return QString{ };
		default:
			break;
		}
		return { };
	}

	auto review_landing_model::roleNames() const -> QHash<int, QByteArray>
	{
		auto names = QAbstractListModel::roleNames();
		names.insert(badge_role, QByteArrayLiteral("badge"));
		names.insert(reason_role, QByteArrayLiteral("reason"));
		names.insert(repository_role, QByteArrayLiteral("repository"));
		names.insert(number_role, QByteArrayLiteral("number"));
		names.insert(bucket_role, QByteArrayLiteral("bucket"));
		names.insert(title_role, QByteArrayLiteral("title"));
		names.insert(author_role, QByteArrayLiteral("author"));
		names.insert(role_role, QByteArrayLiteral("role"));
		names.insert(group_role, QByteArrayLiteral("group"));
		names.insert(unseen_role, QByteArrayLiteral("unseen"));
		return names;
	}

	auto review_landing_model::position_of(
		const core::pull_request_ref& aRef) const -> std::optional<std::size_t>
	{
		const auto found = std::ranges::find(Items, aRef, &core::queue_item::Ref);
		if (found == Items.end()) {
			return std::nullopt;
		}
		return static_cast<std::size_t>(found - Items.begin());
	}

	void review_landing_model::apply(
		const std::span<const core::queue_item> aQueue)
	{
		// Removals first, from the back, so every index stays valid while they
		// are issued. Doing them forwards would leave each subsequent index
		// pointing at the wrong row.
		for (auto index = Items.size(); index > 0U; --index) {
			const auto position = index - 1U;
			const auto present  = std::ranges::any_of(
				aQueue, [&](const core::queue_item& aCandidate) {
					return aCandidate.Ref == Items[position].Ref;
				});
			if (present) {
				continue;
			}
			beginRemoveRows({ }, static_cast<int>(position), static_cast<int>(position));
			Items.erase(Items.begin() + static_cast<std::ptrdiff_t>(position));
			endRemoveRows();
		}

		// Then place each item where the core put it, matching by identity. An
		// item that changed bucket moves rather than being removed and
		// re-inserted, so its persistent index survives and a view keeps the
		// reviewer's place.
		for (auto target = std::size_t{ 0U }; target < aQueue.size(); ++target) {
			const auto& wanted = aQueue[target];
			const auto  found  = position_of(wanted.Ref);

			if (!found) {
				beginInsertRows(
					{ }, static_cast<int>(target), static_cast<int>(target));
				Items.insert(
					Items.begin() + static_cast<std::ptrdiff_t>(target), wanted);
				endInsertRows();
				continue;
			}

			if (*found != target) {
				beginMoveRows(
					{ },
					static_cast<int>(*found),
					static_cast<int>(*found),
					{ },
					static_cast<int>(target));
				auto moved = Items[*found];
				Items.erase(Items.begin() + static_cast<std::ptrdiff_t>(*found));
				Items.insert(
					Items.begin() + static_cast<std::ptrdiff_t>(target),
					std::move(moved));
				endMoveRows();
			}

			if (Items[target] != wanted) {
				Items[target]  = wanted;
				const auto row = index(static_cast<int>(target));
				emit       dataChanged(row, row);
			}
		}
	}

	void review_landing_model::clear()
	{
		if (Items.empty()) {
			return;
		}
		beginRemoveRows({ }, 0, static_cast<int>(Items.size() - 1U));
		Items.clear();
		endRemoveRows();
	}

	auto review_landing_model::item_at(const QModelIndex& aIndex) const
		-> std::optional<core::queue_item>
	{
		if (!aIndex.isValid() || aIndex.parent().isValid()) {
			return std::nullopt;
		}
		const auto row = static_cast<std::size_t>(aIndex.row());
		if (row >= Items.size()) {
			return std::nullopt;
		}
		return Items[row];
	}

	review_landing_filter_model::review_landing_filter_model(
		review_landing_model& aSource, QObject* const aParent)
		: QSortFilterProxyModel{ aParent }
		, Source{ aSource }
	{
		setDynamicSortFilter(true);
		setSourceModel(&Source);
		sort(-1);
	}

	void review_landing_filter_model::apply(
		const application::queue_presentation_preferences& aPreferences)
	{
		if (!application::valid(aPreferences) || aPreferences == Preferences) {
			return;
		}
		const auto sortChanged = aPreferences.Sort != Preferences.Sort;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		beginFilterChange();
		Preferences = aPreferences;
		endFilterChange(Direction::Rows);
#else
		Preferences = aPreferences;
		invalidateFilter();
#endif
		if (sortChanged) {
			// Qt returns early when the requested column and order are unchanged,
			// even though our explicit sort vocabulary changed the comparator.
			// Disabling the proxy sort first makes that transition observable on
			// every supported Qt version.
			sort(-1);
			if (Preferences.Sort
				!= application::queue_sort_order::core_priority) {
				sort(0, Qt::AscendingOrder);
			}
		}
	}

	auto review_landing_filter_model::preferences() const noexcept
		-> const application::queue_presentation_preferences&
	{
		return Preferences;
	}

	auto review_landing_filter_model::item_at(const QModelIndex& aIndex) const
		-> std::optional<core::queue_item>
	{
		return Source.item_at(mapToSource(aIndex));
	}

	auto review_landing_filter_model::has_active_filters() const noexcept -> bool
	{
		return Preferences.Role.has_value()
			|| Preferences.ObserverReason.has_value()
			|| Preferences.Bucket.has_value()
			|| !Preferences.Repository.empty()
			|| Preferences.Quiet != application::queue_quiet_filter::all;
	}

	auto review_landing_filter_model::filterAcceptsRow(
		const int aSourceRow, const QModelIndex& aSourceParent) const -> bool
	{
		const auto item = Source.item_at(Source.index(aSourceRow, 0, aSourceParent));
		if (!item) {
			return false;
		}
		if (Preferences.Role.has_value()
			&& item->Membership.Role != *Preferences.Role) {
			return false;
		}
		if (Preferences.ObserverReason.has_value()
			&& (!item->Membership.MatchedRule
				|| item->Membership.Rule.Kind != *Preferences.ObserverReason)) {
			return false;
		}
		if (Preferences.Bucket.has_value()
			&& item->Bucket != *Preferences.Bucket) {
			return false;
		}
		if (!Preferences.Repository.empty()
			&& item->Ref.Repository != Preferences.Repository) {
			return false;
		}
		if (Preferences.Quiet == application::queue_quiet_filter::attention
			&& item->Bucket == core::attention_bucket::quiet) {
			return false;
		}
		if (Preferences.Quiet == application::queue_quiet_filter::quiet
			&& item->Bucket != core::attention_bucket::quiet) {
			return false;
		}
		return true;
	}

	auto review_landing_filter_model::lessThan(
		const QModelIndex& aLeft, const QModelIndex& aRight) const -> bool
	{
		const auto left  = Source.item_at(aLeft);
		const auto right = Source.item_at(aRight);
		if (!left || !right) {
			return aLeft.row() < aRight.row();
		}
		const auto& leftRef  = left->Ref;
		const auto& rightRef = right->Ref;
		switch (Preferences.Sort) {
		case application::queue_sort_order::repository:
			return std::tie(
					   leftRef.Repository,
					   leftRef.Number,
					   leftRef.Host,
					   leftRef.Account)
				< std::tie(
					   rightRef.Repository,
					   rightRef.Number,
					   rightRef.Host,
					   rightRef.Account);
		case application::queue_sort_order::pull_request_number:
			return std::tie(
					   leftRef.Number,
					   leftRef.Repository,
					   leftRef.Host,
					   leftRef.Account)
				< std::tie(
					   rightRef.Number,
					   rightRef.Repository,
					   rightRef.Host,
					   rightRef.Account);
		case application::queue_sort_order::core_priority:
			return aLeft.row() < aRight.row();
		}
		return aLeft.row() < aRight.row();
	}
} // namespace bpr::qt
