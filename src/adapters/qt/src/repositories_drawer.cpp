#include "bpr/qt/repositories_drawer.hpp"
#include "bpr/qt/resident_filter.hpp"

#include <QFont>
#include <QHeaderView>
#include <QPainter>
#include <QPalette>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		constexpr quintptr repository_id = 0U;
		/// The room the unread dot takes before a pull request's number.
		constexpr int unread_dot_inset = 12;
	} // namespace

	auto repository_reason_word(const core::attention_bucket aBucket) -> QString
	{
		switch (aBucket) {
		case core::attention_bucket::initial_review_requested:
			return QStringLiteral("requested");
		case core::attention_bucket::changed_since_my_review:
			return QStringLiteral("changed");
		case core::attention_bucket::new_comments_or_author_replies:
			return QStringLiteral("replied");
		case core::attention_bucket::fixes_awaiting_verification:
			return QStringLiteral("verify");
		case core::attention_bucket::authored_awaiting_reviewer_action:
			return QStringLiteral("waiting");
		case core::attention_bucket::observed_with_new_activity:
			return QStringLiteral("following");
		case core::attention_bucket::detail_unknown:
			return QStringLiteral("unknown");
		case core::attention_bucket::quiet:
			break;
		}
		return QStringLiteral("quiet");
	}

	auto repository_needs_me(const core::attention_bucket aBucket) noexcept -> bool
	{
		switch (aBucket) {
		case core::attention_bucket::initial_review_requested:
		case core::attention_bucket::changed_since_my_review:
		case core::attention_bucket::new_comments_or_author_replies:
		case core::attention_bucket::fixes_awaiting_verification:
			return true;
		case core::attention_bucket::authored_awaiting_reviewer_action:
		case core::attention_bucket::observed_with_new_activity:
		case core::attention_bucket::quiet:
		case core::attention_bucket::detail_unknown:
			break;
		}
		return false;
	}

	// ---- repositories_tree_model --------------------------------------------

	repositories_tree_model::repositories_tree_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
	}

	void repositories_tree_model::apply(
		const std::span<const core::queue_item>              aItems,
		const std::span<const application::saved_repository> aSaved)
	{
		Items.assign(aItems.begin(), aItems.end());
		CataloguePositions.clear();
		NeedsMe = 0U;
		for (std::size_t position = 0U; position < Items.size(); ++position) {
			CataloguePositions.emplace(Items[position].Ref, position);
			NeedsMe += repository_needs_me(Items[position].Bucket) ? 1U : 0U;
		}
		Saved.assign(aSaved.begin(), aSaved.end());
		rebuild();
	}

	void repositories_tree_model::clear()
	{
		apply({ });
	}

	void repositories_tree_model::set_filter(const QString& aFilter)
	{
		const auto trimmed = aFilter.trimmed();
		if (Filter == trimmed) {
			return;
		}
		Filter = trimmed;
		rebuild();
	}

	auto repositories_tree_model::filter() const noexcept -> const QString&
	{
		return Filter;
	}

	auto repositories_tree_model::repository_shown(const std::string& aHost, const std::string& aRepository) const -> bool
	{
		return Filter.isEmpty() || resident_filter_matches(text(aRepository), Filter)
			|| resident_filter_matches(text(aHost), Filter);
	}

	auto repositories_tree_model::shown(const core::queue_item& aItem) const -> bool
	{
		return repository_shown(aItem.Ref.Host, aItem.Ref.Repository)
			|| resident_filter_matches(text(aItem.Facts.Title), Filter)
			|| resident_filter_matches(QStringLiteral("#%1").arg(aItem.Ref.Number), Filter);
	}

	void repositories_tree_model::rebuild()
	{
		++Work.FullBuilds;
		auto       desired   = std::vector<repository_node>{ };
		auto       positions = std::map<repository_key, std::size_t>{ };
		const auto nodeFor   = [&](const std::string& aHost, const std::string& aAccount, const std::string& aRepository) -> repository_node& {
			const auto key                  = repository_key{ aHost, aAccount, aRepository };
			const auto [position, inserted] = positions.try_emplace(key, desired.size());
			if (inserted) {
				const auto [identity, newIdentity] = Identities.try_emplace(key, NextIdentity);
				if (newIdentity) {
					++NextIdentity;
				}
				desired.push_back(repository_node{ .Host = aHost, .Account = aAccount, .Repository = aRepository, .Identity = identity->second });
			}
			return desired[position->second];
		};
		for (const auto& item : core::ordered_queue(Items)) {
			++Work.CatalogueRowsVisited;
			if (shown(item)) {
				auto& node = nodeFor(item.Ref.Host, item.Ref.Account, item.Ref.Repository);
				node.Items.push_back(item);
				node.NeedsMe += repository_needs_me(item.Bucket) ? 1U : 0U;
			}
		}
		for (const auto& saved : Saved) {
			if (positions.contains(repository_key{ saved.Host, saved.Account, saved.Repository }) || repository_shown(saved.Host, saved.Repository)) {
				nodeFor(saved.Host, saved.Account, saved.Repository).Saved = true;
			}
		}
		reconcile(std::move(desired));
	}

	void repositories_tree_model::index_roots()
	{
		RootPositions.clear();
		for (std::size_t row = 0U; row < Repositories.size(); ++row) {
			RootPositions.emplace(Repositories[row].Identity, row);
		}
	}

	void repositories_tree_model::index_items(const std::size_t aPosition, const std::size_t aFirst, const std::size_t aEnd)
	{
		const auto& node = Repositories[aPosition];
		for (std::size_t row = aFirst; row < std::min(aEnd, node.Items.size()); ++row) {
			VisiblePositions.insert_or_assign(node.Items[row].Ref, std::pair{ node.Identity, row });
			++Work.PositionalRowsUpdated;
		}
	}

	void repositories_tree_model::reconcile_items(const std::size_t aPosition, std::vector<core::queue_item> aItems)
	{
		auto&      node        = Repositories[aPosition];
		auto&      items       = node.Items;
		const auto parentIndex = index(static_cast<int>(aPosition), 0);
		auto       desired     = std::set<core::pull_request_ref>{ };
		for (const auto& item : aItems) {
			desired.insert(item.Ref);
		}
		Work.ChangedRepositoryRowsVisited += items.size();
		for (auto row = items.size(); row > 0U; --row) {
			const auto position = row - 1U;
			if (!desired.contains(items[position].Ref)) {
				beginRemoveRows(parentIndex, static_cast<int>(position), static_cast<int>(position));
				VisiblePositions.erase(items[position].Ref);
				items.erase(items.begin() + static_cast<std::ptrdiff_t>(position));
				index_items(aPosition);
				endRemoveRows();
			}
		}
		for (std::size_t target = 0U; target < aItems.size(); ++target) {
			const auto& wanted = aItems[target];
			const auto  found  = VisiblePositions.find(wanted.Ref);
			if (found == VisiblePositions.end()) {
				beginInsertRows(parentIndex, static_cast<int>(target), static_cast<int>(target));
				items.insert(items.begin() + static_cast<std::ptrdiff_t>(target), wanted);
				index_items(aPosition);
				endInsertRows();
				continue;
			}
			const auto position = found->second.second;
			if (position != target) {
				beginMoveRows(parentIndex, static_cast<int>(position), static_cast<int>(position), parentIndex, static_cast<int>(target));
				auto moved = std::move(items[position]);
				items.erase(items.begin() + static_cast<std::ptrdiff_t>(position));
				items.insert(items.begin() + static_cast<std::ptrdiff_t>(target), std::move(moved));
				index_items(aPosition);
				endMoveRows();
			}
			if (items[target] != wanted) {
				items[target] = wanted;
				emit dataChanged(index(static_cast<int>(target), 0, parentIndex), index(static_cast<int>(target), column_count - 1, parentIndex));
			}
		}
		node.NeedsMe = static_cast<std::size_t>(std::ranges::count_if(items, [](const core::queue_item& aItem) { return repository_needs_me(aItem.Bucket); }));
		emit dataChanged(parentIndex, index(static_cast<int>(aPosition), column_count - 1));
	}

	void repositories_tree_model::reconcile(std::vector<repository_node> aDesired)
	{
		auto identities = std::set<quintptr>{ };
		for (const auto& node : aDesired) {
			identities.insert(node.Identity);
		}
		for (auto row = Repositories.size(); row > 0U; --row) {
			const auto position = row - 1U;
			if (!identities.contains(Repositories[position].Identity)) {
				beginRemoveRows({ }, static_cast<int>(position), static_cast<int>(position));
				for (const auto& item : Repositories[position].Items) {
					VisiblePositions.erase(item.Ref);
				}
				Repositories.erase(Repositories.begin() + static_cast<std::ptrdiff_t>(position));
				index_roots();
				endRemoveRows();
			}
		}
		for (std::size_t target = 0U; target < aDesired.size(); ++target) {
			auto&      wanted = aDesired[target];
			const auto found  = RootPositions.find(wanted.Identity);
			if (found == RootPositions.end()) {
				beginInsertRows({ }, static_cast<int>(target), static_cast<int>(target));
				Repositories.insert(Repositories.begin() + static_cast<std::ptrdiff_t>(target), std::move(wanted));
				index_roots();
				index_items(target);
				endInsertRows();
				continue;
			}
			const auto position = found->second;
			if (position != target) {
				beginMoveRows({ }, static_cast<int>(position), static_cast<int>(position), { }, static_cast<int>(target));
				auto moved = std::move(Repositories[position]);
				Repositories.erase(Repositories.begin() + static_cast<std::ptrdiff_t>(position));
				Repositories.insert(Repositories.begin() + static_cast<std::ptrdiff_t>(target), std::move(moved));
				index_roots();
				endMoveRows();
			}
			Repositories[target].Saved = wanted.Saved;
			reconcile_items(target, std::move(wanted.Items));
		}
	}

	void repositories_tree_model::apply_item(const core::queue_item& aItem)
	{
		const auto found = CataloguePositions.find(aItem.Ref);
		if (found == CataloguePositions.end() || Items[found->second] == aItem) {
			return;
		}
		NeedsMe -= repository_needs_me(Items[found->second].Bucket) ? 1U : 0U;
		NeedsMe += repository_needs_me(aItem.Bucket) ? 1U : 0U;
		Items[found->second]            = aItem;
		const auto key                  = repository_key{ aItem.Ref.Host, aItem.Ref.Account, aItem.Ref.Repository };
		const auto [identity, inserted] = Identities.try_emplace(key, NextIdentity);
		if (inserted) {
			++NextIdentity;
		}
		const auto root = RootPositions.find(identity->second);
		if (root == RootPositions.end()) {
			if (!shown(aItem)) {
				return;
			}
			const auto target = static_cast<std::size_t>(std::lower_bound(Repositories.begin(), Repositories.end(), aItem,
																		  [](const repository_node& aNode, const core::queue_item& aValue) { return !aNode.Items.empty() && core::queue_precedes(aNode.Items.front(), aValue); })
														 - Repositories.begin());
			auto       node   = repository_node{ .Host = aItem.Ref.Host, .Account = aItem.Ref.Account, .Repository = aItem.Ref.Repository, .NeedsMe = repository_needs_me(aItem.Bucket) ? 1U : 0U, .Identity = identity->second, .Saved = std::ranges::contains(Saved, application::saved_repository{ aItem.Ref.Host, aItem.Ref.Account, aItem.Ref.Repository }), .Items = { aItem } };
			beginInsertRows({ }, static_cast<int>(target), static_cast<int>(target));
			Repositories.insert(Repositories.begin() + static_cast<std::ptrdiff_t>(target), std::move(node));
			index_roots();
			index_items(target);
			endInsertRows();
			return;
		}
		const auto position    = root->second;
		auto&      node        = Repositories[position];
		auto&      items       = node.Items;
		const auto parentIndex = index(static_cast<int>(position), 0);
		const auto visible     = VisiblePositions.find(aItem.Ref);
		const auto old         = visible == VisiblePositions.end() ? std::optional<std::size_t>{ } : std::optional{ visible->second.second };
		++Work.ChangedRepositoryRowsVisited;
		if (shown(aItem)) {
			auto target = static_cast<std::size_t>(std::ranges::lower_bound(items, aItem, core::queue_precedes) - items.begin());
			if (old && target > *old) {
				--target;
			}
			if (!old) {
				beginInsertRows(parentIndex, static_cast<int>(target), static_cast<int>(target));
				items.insert(items.begin() + static_cast<std::ptrdiff_t>(target), aItem);
				index_items(position, target);
				node.NeedsMe += repository_needs_me(aItem.Bucket) ? 1U : 0U;
				endInsertRows();
			} else {
				node.NeedsMe -= repository_needs_me(items[*old].Bucket) ? 1U : 0U;
				node.NeedsMe += repository_needs_me(aItem.Bucket) ? 1U : 0U;
				if (target != *old) {
					beginMoveRows(parentIndex, static_cast<int>(*old), static_cast<int>(*old), parentIndex, static_cast<int>(target > *old ? target + 1U : target));
					auto moved = std::move(items[*old]);
					items.erase(items.begin() + static_cast<std::ptrdiff_t>(*old));
					items.insert(items.begin() + static_cast<std::ptrdiff_t>(target), std::move(moved));
					index_items(position, std::min(target, *old), std::max(target, *old) + 1U);
					endMoveRows();
				}
				items[target] = aItem;
				emit dataChanged(index(static_cast<int>(target), 0, parentIndex), index(static_cast<int>(target), column_count - 1, parentIndex));
			}
		} else if (old) {
			beginRemoveRows(parentIndex, static_cast<int>(*old), static_cast<int>(*old));
			node.NeedsMe -= repository_needs_me(items[*old].Bucket) ? 1U : 0U;
			VisiblePositions.erase(aItem.Ref);
			items.erase(items.begin() + static_cast<std::ptrdiff_t>(*old));
			index_items(position, *old);
			endRemoveRows();
		}
		if (items.empty() && (!node.Saved || !repository_shown(aItem.Ref.Host, aItem.Ref.Repository))) {
			beginRemoveRows({ }, static_cast<int>(position), static_cast<int>(position));
			Repositories.erase(Repositories.begin() + static_cast<std::ptrdiff_t>(position));
			index_roots();
			endRemoveRows();
			return;
		}
		emit dataChanged(parentIndex, index(static_cast<int>(position), column_count - 1));
		// Repository order follows its first visible item; empty saved rows follow
		// in their saved order. Only roots, never other repositories' items, are read.
		const auto precedes = [this](const repository_node& aLeft, const repository_node& aRight) {
			if (!aLeft.Items.empty() && !aRight.Items.empty()) {
				return core::queue_precedes(aLeft.Items.front(), aRight.Items.front());
			}
			if (aLeft.Items.empty() != aRight.Items.empty()) {
				return !aLeft.Items.empty();
			}
			const auto savedPosition = [this](const repository_node& aNode) {
				return std::ranges::find(Saved, application::saved_repository{ aNode.Host, aNode.Account, aNode.Repository }) - Saved.begin();
			};
			return savedPosition(aLeft) < savedPosition(aRight);
		};
		auto target = position;
		while (target > 0U && precedes(Repositories[position], Repositories[target - 1U])) {
			--target;
		}
		while (target == position && target + 1U < Repositories.size() && precedes(Repositories[target + 1U], Repositories[position])) {
			++target;
		}
		// If moving down, keep following the same original node, not its neighbours.
		while (target > position && target + 1U < Repositories.size() && precedes(Repositories[target + 1U], Repositories[position])) {
			++target;
		}
		if (target != position) {
			beginMoveRows({ }, static_cast<int>(position), static_cast<int>(position), { }, static_cast<int>(target > position ? target + 1U : target));
			auto moved = std::move(Repositories[position]);
			Repositories.erase(Repositories.begin() + static_cast<std::ptrdiff_t>(position));
			Repositories.insert(Repositories.begin() + static_cast<std::ptrdiff_t>(target), std::move(moved));
			index_roots();
			endMoveRows();
		}
	}

	auto repositories_tree_model::work() const noexcept -> repositories_projection_work { return Work; }

	auto repositories_tree_model::index(const int aRow, const int aColumn, const QModelIndex& aParent) const
		-> QModelIndex
	{
		if (aRow < 0 || aColumn < 0 || aColumn >= column_count) {
			return { };
		}
		if (!aParent.isValid()) {
			return static_cast<std::size_t>(aRow) < Repositories.size()
				? createIndex(aRow, aColumn, repository_id)
				: QModelIndex{ };
		}
		if (aParent.internalId() != repository_id || aParent.column() != 0) {
			return { };
		}
		const auto repository = static_cast<std::size_t>(aParent.row());
		if (repository >= Repositories.size()
			|| static_cast<std::size_t>(aRow) >= Repositories[repository].Items.size()) {
			return { };
		}
		return createIndex(aRow, aColumn, Repositories[repository].Identity);
	}

	auto repositories_tree_model::parent(const QModelIndex& aIndex) const -> QModelIndex
	{
		if (!aIndex.isValid() || aIndex.internalId() == repository_id) {
			return { };
		}
		const auto found = RootPositions.find(aIndex.internalId());
		return found == RootPositions.end() ? QModelIndex{ } : createIndex(static_cast<int>(found->second), 0, repository_id);
	}

	auto repositories_tree_model::rowCount(const QModelIndex& aParent) const -> int
	{
		if (!aParent.isValid()) {
			return static_cast<int>(Repositories.size());
		}
		if (aParent.internalId() != repository_id || aParent.column() != 0) {
			return 0;
		}
		const auto repository = static_cast<std::size_t>(aParent.row());
		return repository < Repositories.size()
			? static_cast<int>(Repositories[repository].Items.size())
			: 0;
	}

	auto repositories_tree_model::columnCount(const QModelIndex& aParent) const -> int
	{
		static_cast<void>(aParent);
		return column_count;
	}

	auto repositories_tree_model::item_at(const QModelIndex& aIndex) const
		-> std::optional<core::queue_item>
	{
		if (!aIndex.isValid() || aIndex.internalId() == repository_id) {
			return std::nullopt;
		}
		const auto found = RootPositions.find(aIndex.internalId());
		if (found == RootPositions.end()) {
			return std::nullopt;
		}
		const auto& items = Repositories[found->second].Items;
		const auto  row   = static_cast<std::size_t>(aIndex.row());
		return row < items.size() ? std::optional{ items[row] } : std::nullopt;
	}

	auto repositories_tree_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		if (!aIndex.isValid()) {
			return { };
		}
		if (aIndex.internalId() == repository_id) {
			const auto& node = Repositories[static_cast<std::size_t>(aIndex.row())];
			switch (aRole) {
			case is_repository_role:
				return true;
			case repository_role:
				return text(node.Repository);
			case host_role:
				return text(node.Host);
			case account_role:
				return text(node.Account);
			case needs_me_role:
				return node.NeedsMe != 0U;
			case saved_role:
				return node.Saved;
			case Qt::DisplayRole:
			case Qt::AccessibleTextRole:
				if (aIndex.column() == name_column) {
					return text(node.Repository);
				}
				if (node.Items.empty()) {
					return node.Saved ? QStringLiteral("saved") : QString{ };
				}
				return node.NeedsMe == 0U
					? QString::number(node.Items.size())
					: QStringLiteral("%1 of %2").arg(node.NeedsMe).arg(node.Items.size());
			case Qt::ToolTipRole:
				return QStringLiteral("%1 on %2 as %3: %4 of %5 need you%6")
					.arg(text(node.Repository), text(node.Host), text(node.Account))
					.arg(node.NeedsMe)
					.arg(node.Items.size())
					.arg(node.Saved ? QStringLiteral(", saved") : QString{ });
			case Qt::FontRole: {
				auto font = QFont{ };
				font.setWeight(QFont::DemiBold);
				return QVariant{ std::move(font) };
			}
			default:
				return { };
			}
		}
		const auto item = item_at(aIndex);
		if (!item.has_value()) {
			return { };
		}
		switch (aRole) {
		case is_repository_role:
			return false;
		case repository_role:
			return text(item->Ref.Repository);
		case number_role:
			return static_cast<qlonglong>(item->Ref.Number);
		case reason_role:
			return repository_reason_word(item->Bucket);
		case needs_me_role:
			return repository_needs_me(item->Bucket);
		case host_role:
			return text(item->Ref.Host);
		case account_role:
			return text(item->Ref.Account);
		case title_role:
			return text(item->Facts.Title);
		case unseen_role:
			return item->Facts.UnseenComments > 0U;
		case Qt::DisplayRole:
		case Qt::AccessibleTextRole:
			// [[BPR-0659]]. "#n title" when the host read a title; the view
			// elides a long one.
			return aIndex.column() == name_column
				? (item->Facts.Title.empty()
					   ? QStringLiteral("#%1").arg(item->Ref.Number)
					   : QStringLiteral("#%1 %2").arg(item->Ref.Number).arg(text(item->Facts.Title)))
				: repository_reason_word(item->Bucket);
		case Qt::ToolTipRole: {
			auto tip = QStringLiteral("%1 #%2: %3")
						   .arg(text(item->Ref.Repository))
						   .arg(item->Ref.Number)
						   .arg(text(std::string{ core::explanation(item->Bucket) }));
			if (!item->Facts.Title.empty()) {
				tip += QStringLiteral("\n%1").arg(text(item->Facts.Title));
			}
			if (!item->Facts.Author.empty()) {
				tip += QStringLiteral("\nby %1").arg(text(item->Facts.Author));
			}
			if (item->Facts.UnseenComments > 0U) {
				tip += QStringLiteral("\n%1 unseen comments").arg(item->Facts.UnseenComments);
			}
			return tip;
		}
		default:
			return { };
		}
	}

	auto repositories_tree_model::repositories() const noexcept -> const std::vector<repository_node>&
	{
		return Repositories;
	}

	auto repositories_tree_model::index_for(const core::pull_request_ref& aRef) const -> QModelIndex
	{
		const auto found = VisiblePositions.find(aRef);
		return found == VisiblePositions.end() ? QModelIndex{ } : createIndex(static_cast<int>(found->second.second), name_column, found->second.first);
	}

	auto repositories_tree_model::index_for(const std::string_view aRepository, const std::int64_t aNumber) const
		-> QModelIndex
	{
		for (std::size_t repository = 0U; repository < Repositories.size(); ++repository) {
			const auto& items = Repositories[repository].Items;
			for (std::size_t row = 0U; row < items.size(); ++row) {
				if (items[row].Ref.Repository == aRepository && items[row].Ref.Number == aNumber) {
					return createIndex(static_cast<int>(row), name_column, Repositories[repository].Identity);
				}
			}
		}
		return { };
	}

	auto repositories_tree_model::needs_me_count() const noexcept -> std::size_t
	{
		return NeedsMe;
	}

	auto repositories_tree_model::item_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& node : Repositories) {
			count += node.Items.size();
		}
		return count;
	}

	// ---- repositories_tree_delegate -----------------------------------------

	repositories_tree_delegate::repositories_tree_delegate(QObject* const aParent)
		: QStyledItemDelegate{ aParent }
	{
	}

	void repositories_tree_delegate::set_marker(const QColor& aMarker)
	{
		Marker = aMarker;
	}

	void repositories_tree_delegate::set_accent(const QColor& aAccent)
	{
		Accent = aAccent;
	}

	void repositories_tree_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		if (aIndex.column() == repositories_tree_model::reason_column) {
			option.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
			const auto repository   = aIndex.data(repositories_tree_model::is_repository_role).toBool();
			const auto needsMe      = aIndex.data(repositories_tree_model::needs_me_role).toBool();
			auto       font         = option.font;
			font.setPointSizeF(font.pointSizeF() * 0.9);
			option.font = std::move(font);
			option.palette.setColor(
				QPalette::Text,
				needsMe && !repository ? Marker
									   : option.palette.color(QPalette::Active, QPalette::PlaceholderText));
			if (repository && needsMe) {
				option.palette.setColor(QPalette::Text, option.palette.color(QPalette::Active, QPalette::Text));
			}
		}
		// [[BPR-0659]]. The unread dot before a pull request with comments I
		// have not seen; the text moves right to leave it room.
		const auto unseen = aIndex.column() == repositories_tree_model::name_column
			&& !aIndex.data(repositories_tree_model::is_repository_role).toBool()
			&& aIndex.data(repositories_tree_model::unseen_role).toBool();
		if (unseen) {
			option.rect.adjust(unread_dot_inset, 0, 0, 0);
		}
		const auto* const style = option.widget != nullptr ? option.widget->style() : nullptr;
		if (style != nullptr) {
			style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);
		} else {
			QStyledItemDelegate::paint(aPainter, option, aIndex);
		}
		if (unseen) {
			aPainter->save();
			aPainter->setRenderHint(QPainter::Antialiasing, true);
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(Accent);
			const auto centre = QPointF{ static_cast<qreal>(aOption.rect.left()) + unread_dot_inset / 2.0,
										 static_cast<qreal>(aOption.rect.center().y()) + 0.5 };
			aPainter->drawEllipse(centre, 3.0, 3.0);
			aPainter->restore();
		}
	}

	// ---- repositories_tree_view ---------------------------------------------

	repositories_tree_view::repositories_tree_view(QWidget* const aParent)
		: QTreeView{ aParent }
	{
		setObjectName(QStringLiteral("repositoriesTree"));
		setAccessibleName(QStringLiteral("Repositories tree"));
		setAccessibleDescription(QStringLiteral(
			"Each repository with its pull requests of interest: number and "
			"why it is here; choose one to open it on its overview"));
		Model = new repositories_tree_model{ this };
		setModel(Model);
		connect(Model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& aParentIndex, const int aFirst, const int aLast) {
			if (!aParentIndex.isValid()) {
				for (auto row = aFirst; row <= aLast; ++row) {
					expand(Model->index(row, 0));
				}
			}
		});
		Delegate = new repositories_tree_delegate{ this };
		setItemDelegate(Delegate);
		setHeaderHidden(true);
		setRootIsDecorated(true);
		setUniformRowHeights(true);
		setSelectionBehavior(QAbstractItemView::SelectRows);
		setSelectionMode(QAbstractItemView::SingleSelection);
		setEditTriggers(QAbstractItemView::NoEditTriggers);
		setIndentation(14);
		setFocusPolicy(Qt::StrongFocus);
		header()->setStretchLastSection(false);
		header()->setSectionResizeMode(repositories_tree_model::name_column, QHeaderView::Stretch);
		header()->setSectionResizeMode(repositories_tree_model::reason_column, QHeaderView::ResizeToContents);
		// [[BPR-0659]]. Save and Forget on the current repository row, as the
		// tree's own context menu; the window carries them to the queue host.
		SaveAction = new QAction{ QStringLiteral("Save repository"), this };
		SaveAction->setObjectName(QStringLiteral("saveRepositoryAction"));
		SaveAction->setStatusTip(QStringLiteral("Keep this repository in the drawer, even with nothing to review."));
		ForgetAction = new QAction{ QStringLiteral("Forget repository"), this };
		ForgetAction->setObjectName(QStringLiteral("forgetRepositoryAction"));
		ForgetAction->setStatusTip(QStringLiteral("Stop keeping this repository in the drawer."));
		addAction(SaveAction);
		addAction(ForgetAction);
		setContextMenuPolicy(Qt::ActionsContextMenu);
		connect(SaveAction, &QAction::triggered, this, [this] { dispatch_saved(application::saved_repository_action::add); });
		connect(ForgetAction, &QAction::triggered, this, [this] { dispatch_saved(application::saved_repository_action::remove); });
		update_saved_actions();
	}

	void repositories_tree_view::set_saved_repository_dispatch(saved_repository_dispatch aDispatch)
	{
		SavedDispatch = std::move(aDispatch);
		update_saved_actions();
	}

	auto repositories_tree_view::save_repository_action() const noexcept -> QAction*
	{
		return SaveAction;
	}

	auto repositories_tree_view::forget_repository_action() const noexcept -> QAction*
	{
		return ForgetAction;
	}

	void repositories_tree_view::update_saved_actions()
	{
		// Qt can report a current change while the view is still being built.
		if (SaveAction == nullptr || ForgetAction == nullptr) {
			return;
		}
		const auto current    = currentIndex();
		const auto repository = current.isValid()
			&& current.data(repositories_tree_model::is_repository_role).toBool();
		const auto saved = repository && current.data(repositories_tree_model::saved_role).toBool();
		SaveAction->setEnabled(repository && !saved && static_cast<bool>(SavedDispatch));
		ForgetAction->setEnabled(saved && static_cast<bool>(SavedDispatch));
	}

	void repositories_tree_view::dispatch_saved(const application::saved_repository_action aAction)
	{
		const auto current = currentIndex();
		if (!SavedDispatch || !current.isValid()
			|| !current.data(repositories_tree_model::is_repository_role).toBool()) {
			return;
		}
		SavedDispatch(application::saved_repository_intent{
			.Action     = aAction,
			.Repository = application::saved_repository{
				.Host       = current.data(repositories_tree_model::host_role).toString().toStdString(),
				.Account    = current.data(repositories_tree_model::account_role).toString().toStdString(),
				.Repository = current.data(repositories_tree_model::repository_role).toString().toStdString(),
			},
		});
	}

	auto repositories_tree_view::tree_model() const noexcept -> repositories_tree_model*
	{
		return Model;
	}

	auto repositories_tree_view::delegate() const noexcept -> repositories_tree_delegate*
	{
		return Delegate;
	}

	void repositories_tree_view::set_open_dispatch(repository_open_dispatch aDispatch)
	{
		OpenDispatch = std::move(aDispatch);
	}

	void repositories_tree_view::apply(
		const std::span<const core::queue_item>              aItems,
		const std::span<const application::saved_repository> aSaved)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->apply(aItems, aSaved);

		restore_current();
		update_saved_actions();
	}

	void repositories_tree_view::apply_item(const core::queue_item& aItem)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->apply_item(aItem);
		restore_current();
		update_saved_actions();
	}

	void repositories_tree_view::set_filter(const QString& aFilter)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->set_filter(aFilter);

		restore_current();
		update_saved_actions();
	}

	void repositories_tree_view::follow_open_pull_request(const std::string& aRepository, const std::int64_t aNumber)
	{
		// The row the reviewer chose here keeps its exact host and account.
		if (Current.has_value() && Current->Repository == aRepository && Current->Number == aNumber) {
			return;
		}
		Current             = core::pull_request_ref{ .Repository = aRepository, .Number = aNumber };
		const auto applying = QScopedValueRollback{ Applying, true };
		restore_current();
	}

	void repositories_tree_view::forget_open_pull_request()
	{
		if (!Current.has_value()) {
			return;
		}
		Current.reset();
		const auto applying = QScopedValueRollback{ Applying, true };
		restore_current();
	}

	auto repositories_tree_view::current_pull_request() const noexcept -> const std::optional<core::pull_request_ref>&
	{
		return Current;
	}

	void repositories_tree_view::restore_current()
	{
		// The row is found again by the pull request's identity, exactly when
		// it was chosen here and by repository and number when the window
		// opened it; a filter that hides it leaves nothing current until it
		// shows again.
		auto index = QModelIndex{ };
		if (Current.has_value()) {
			index = Model->index_for(*Current);
			if (!index.isValid()) {
				index = Model->index_for(Current->Repository, Current->Number);
			}
		}
		setCurrentIndex(index);
		if (index.isValid()) {
			scrollTo(index);
		}
	}

	void repositories_tree_view::currentChanged(const QModelIndex& aCurrent, const QModelIndex& aPrevious)
	{
		QTreeView::currentChanged(aCurrent, aPrevious);
		update_saved_actions();
		if (Applying) {
			return;
		}
		// A pull request row chosen by the reviewer opens it; a repository
		// row is a heading and reports nothing.
		if (const auto item = Model->item_at(aCurrent); item.has_value()) {
			Current = item->Ref;
			if (OpenDispatch) {
				OpenDispatch(*item);
			}
		}
	}
} // namespace bpr::qt
