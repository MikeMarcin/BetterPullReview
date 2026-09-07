#include "bpr/qt/review_landing_model.hpp"

#include "bpr/qt/repositories_drawer.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRect>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionViewItem>

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
		-> std::optional<core::open_pull_request_intent>
	{
		auto text = aText.trimmed();
		// A link: keep the path after the host.
		if (const auto scheme = text.indexOf(QStringLiteral("://")); scheme >= 0) {
			const auto afterHost = text.indexOf(QLatin1Char{ '/' }, scheme + 3);
			if (afterHost < 0) {
				return std::nullopt;
			}
			text = text.mid(afterHost + 1);
		}
		auto owner  = QString{ };
		auto name   = QString{ };
		auto number = QString{ };
		if (const auto hash = text.indexOf(QLatin1Char{ '#' }); hash >= 0) {
			// owner/name#123
			const auto repository = text.left(hash);
			const auto slash      = repository.indexOf(QLatin1Char{ '/' });
			if (slash <= 0) {
				return std::nullopt;
			}
			owner  = repository.left(slash);
			name   = repository.mid(slash + 1);
			number = text.mid(hash + 1);
		} else {
			// owner/name/pull/123, with anything after the number ignored.
			const auto parts = text.split(QLatin1Char{ '/' }, Qt::SkipEmptyParts);
			if (parts.size() < 4 || parts[2] != QStringLiteral("pull")) {
				return std::nullopt;
			}
			owner  = parts[0];
			name   = parts[1];
			number = parts[3];
		}
		auto       ok    = false;
		const auto value = number.trimmed().toUInt(&ok);
		if (!ok || value == 0U || owner.isEmpty() || name.isEmpty() || owner.contains(QLatin1Char{ '/' })
			|| name.contains(QLatin1Char{ '/' }) || name.contains(QLatin1Char{ '#' })) {
			return std::nullopt;
		}
		return core::open_pull_request_intent{
			.Repository = core::repository_locator{
				.Owner = owner.trimmed().toStdString(),
				.Name  = name.trimmed().toStdString(),
			},
			.Number = core::pull_request_number{ value },
		};
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

	// ---- review_landing_delegate --------------------------------------------

	review_landing_delegate::review_landing_delegate(QObject* const aParent)
		: QStyledItemDelegate{ aParent }
	{
	}

	void review_landing_delegate::set_marker(const QColor& aMarker)
	{
		Marker = aMarker;
	}

	void review_landing_delegate::set_accent(const QColor& aAccent)
	{
		Accent = aAccent;
	}

	auto review_landing_delegate::heads_group(const QModelIndex& aIndex) -> bool
	{
		if (!aIndex.isValid()) {
			return false;
		}
		if (aIndex.row() == 0) {
			return true;
		}
		const auto above = aIndex.model()->index(aIndex.row() - 1, aIndex.column(), aIndex.parent());
		return above.data(review_landing_model::group_role).toInt()
			!= aIndex.data(review_landing_model::group_role).toInt();
	}

	auto review_landing_delegate::sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QSize
	{
		static_cast<void>(aOption);
		return { 320, row_height + (heads_group(aIndex) ? heading_height : 0) };
	}

	void review_landing_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto& colours = option.palette;
		const auto  ink     = colours.color(QPalette::Active, QPalette::Text);
		const auto  faint   = colours.color(QPalette::Active, QPalette::PlaceholderText);
		auto        rect    = option.rect;
		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing, true);
		if (heads_group(aIndex)) {
			const auto heading = QRect{ rect.left(), rect.top(), rect.width(), heading_height };
			auto       font    = option.font;
			font.setWeight(QFont::DemiBold);
			font.setPointSizeF(font.pointSizeF() * 0.9);
			aPainter->setFont(font);
			aPainter->setPen(faint);
			aPainter->drawText(heading.adjusted(6, 0, -6, 0), Qt::AlignLeft | Qt::AlignBottom,
							   home_group_label(static_cast<home_group>(aIndex.data(review_landing_model::group_role).toInt())));
			rect.setTop(rect.top() + heading_height);
		}
		const auto selected = (option.state & QStyle::State_Selected) != 0;
		if (selected) {
			aPainter->fillRect(rect, colours.color(QPalette::Active, QPalette::Highlight));
		}
		const auto textInk = selected ? colours.color(QPalette::Active, QPalette::HighlightedText) : ink;
		const auto needsMe = static_cast<home_group>(aIndex.data(review_landing_model::group_role).toInt())
			== home_group::needs_you;
		const auto unseen  = aIndex.data(review_landing_model::unseen_role).toBool();
		const auto metrics = QFontMetrics{ option.font };
		auto       x       = rect.left() + 6;
		const auto y       = rect.top();
		const auto h       = rect.height();
		if (unseen) {
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(Accent);
			aPainter->drawEllipse(QPointF{ static_cast<qreal>(x) + 3.0, static_cast<qreal>(y) + h / 2.0 }, 3.0, 3.0);
		}
		x += 12;
		// Columns: the pull request, the title (what is left), then the
		// reason, author and role at fixed widths from the right.
		const auto reasonWidth = 76;
		const auto authorWidth = std::min(120, rect.width() / 6);
		const auto roleWidth   = std::min(110, rect.width() / 6);
		const auto right       = rect.right() - 6;
		auto       demi        = option.font;
		demi.setWeight(QFont::DemiBold);
		const auto pullRequest = QStringLiteral("%1 #%2")
									 .arg(aIndex.data(review_landing_model::repository_role).toString())
									 .arg(aIndex.data(review_landing_model::number_role).toLongLong());
		const auto prWidth     = std::min(QFontMetrics{ demi }.horizontalAdvance(pullRequest) + 8, rect.width() / 3);
		aPainter->setFont(demi);
		aPainter->setPen(textInk);
		aPainter->drawText(QRect{ x, y, prWidth, h }, Qt::AlignLeft | Qt::AlignVCenter,
						   QFontMetrics{ demi }.elidedText(pullRequest, Qt::ElideMiddle, prWidth));
		x += prWidth + 8;
		const auto titleRight = right - reasonWidth - authorWidth - roleWidth - 24;
		aPainter->setFont(option.font);
		aPainter->setPen(textInk);
		aPainter->drawText(QRect{ x, y, std::max(0, titleRight - x), h }, Qt::AlignLeft | Qt::AlignVCenter,
						   metrics.elidedText(aIndex.data(review_landing_model::title_role).toString(), Qt::ElideRight,
											  std::max(0, titleRight - x)));
		auto small = option.font;
		small.setPointSizeF(small.pointSizeF() * 0.9);
		aPainter->setFont(small);
		const auto smallMetrics = QFontMetrics{ small };
		const auto reason       = QRect{ titleRight + 8, y, reasonWidth, h };
		aPainter->setPen(selected ? textInk : needsMe ? Marker
													  : faint);
		aPainter->drawText(reason, Qt::AlignLeft | Qt::AlignVCenter,
						   smallMetrics.elidedText(repository_reason_word(
													   static_cast<core::attention_bucket>(aIndex.data(review_landing_model::bucket_role).toInt())),
												   Qt::ElideRight, reasonWidth));
		aPainter->setPen(selected ? textInk : faint);
		const auto author = QRect{ reason.right() + 8, y, authorWidth, h };
		aPainter->drawText(author, Qt::AlignLeft | Qt::AlignVCenter,
						   smallMetrics.elidedText(aIndex.data(review_landing_model::author_role).toString(), Qt::ElideRight, authorWidth));
		const auto role = QRect{ author.right() + 8, y, roleWidth, h };
		aPainter->drawText(role, Qt::AlignLeft | Qt::AlignVCenter,
						   smallMetrics.elidedText(aIndex.data(review_landing_model::role_role).toString(), Qt::ElideRight, roleWidth));
		aPainter->restore();
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
