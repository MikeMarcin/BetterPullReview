#include "bpr/qt/review_landing_model.hpp"

#include <algorithm>
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
		case Qt::AccessibleDescriptionRole:
			return reason_for(*item);
		case repository_role:
			return text(item->Ref.Repository);
		case number_role:
			return QVariant::fromValue(item->Ref.Number);
		case bucket_role:
			return static_cast<int>(item->Bucket);
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
