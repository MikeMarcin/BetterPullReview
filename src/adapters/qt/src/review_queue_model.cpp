#include "bpr/qt/review_queue_model.hpp"

#include <QHash>
#include <QVariant>

#include <algorithm>

namespace bpr::qt {
	namespace {
		/// \brief Renders one file identity as a short stable label.
		///
		/// \details The queue names files by identity because a receipt attaches
		/// to material rather than to a path, and the path is not reachable from
		/// here. A prefix of the digest is enough to tell two rows apart and is
		/// stable across runs.
		[[nodiscard]] auto short_identity(const core::file_identity& aFile) -> QString
		{
			auto text = QString{ };
			for (auto index = std::size_t{ 0U }; index < 4U; ++index) {
				text += QString::number(aFile.bytes()[index], 16).rightJustified(2, QLatin1Char{ '0' });
			}
			return text;
		}
	} // namespace

	auto explanation_for(const core::actionability_reason aReason) -> QString
	{
		switch (aReason) {
		case core::actionability_reason::unresolved_defect:
			return QStringLiteral("A defect here is still open.");
		case core::actionability_reason::awaiting_verification:
			return QStringLiteral("A fix here is claimed but not verified.");
		case core::actionability_reason::acceptance_outdated:
			return QStringLiteral("This changed after you accepted it.");
		case core::actionability_reason::unread_conversation:
			return QStringLiteral("A thread here has unread comments.");
		case core::actionability_reason::unread_material:
			return QStringLiteral("You have not finished reading this.");
		case core::actionability_reason::nothing_to_do:
			return QStringLiteral("Nothing is outstanding here.");
		case core::actionability_reason::excluded:
			return QStringLiteral("You excluded this from review.");
		}
		return QStringLiteral("Nothing is outstanding here.");
	}

	auto marker_for(const core::actionability_reason aReason) -> QString
	{
		switch (aReason) {
		case core::actionability_reason::unresolved_defect:
			return QStringLiteral("[defect]");
		case core::actionability_reason::awaiting_verification:
			return QStringLiteral("[unverified]");
		case core::actionability_reason::acceptance_outdated:
			return QStringLiteral("[changed]");
		case core::actionability_reason::unread_conversation:
			return QStringLiteral("[comments]");
		case core::actionability_reason::unread_material:
			return QStringLiteral("[unread]");
		case core::actionability_reason::nothing_to_do:
			return QStringLiteral("[done]");
		case core::actionability_reason::excluded:
			return QStringLiteral("[excluded]");
		}
		return QStringLiteral("[done]");
	}

	review_queue_model::review_queue_model(QObject* const aParent)
		: QAbstractListModel{ aParent }
	{
	}

	auto review_queue_model::rowCount(const QModelIndex& aParent) const -> int
	{
		return aParent.isValid() ? 0 : static_cast<int>(Items.size());
	}

	auto review_queue_model::data(
		const QModelIndex& aIndex,
		const int          aRequestedRole) const -> QVariant
	{
		const auto item = item_at(aIndex);
		if (!item) {
			return { };
		}

		switch (aRequestedRole) {
		case Qt::DisplayRole:
			// The marker leads the label, so the state is legible without
			// colour and is the first thing a screen reader announces.
			return marker_for(item->Reason) + QLatin1Char{ ' ' }
			+ short_identity(item->File);
		case Qt::AccessibleTextRole:
			return short_identity(item->File) + QStringLiteral(". ")
				+ explanation_for(item->Reason);
		case Qt::ToolTipRole:
		case reason_role:
			return explanation_for(item->Reason);
		case marker_role:
			return marker_for(item->Reason);
		case file_role:
			return short_identity(item->File);
		default:
			break;
		}
		return { };
	}

	auto review_queue_model::roleNames() const -> QHash<int, QByteArray>
	{
		auto names = QAbstractListModel::roleNames();
		names.insert(file_role, "file");
		names.insert(reason_role, "reason");
		names.insert(marker_role, "marker");
		return names;
	}

	auto review_queue_model::item_at(const QModelIndex& aIndex) const
		-> std::optional<core::actionable_item>
	{
		if (!aIndex.isValid() || aIndex.row() < 0
			|| static_cast<std::size_t>(aIndex.row()) >= Items.size()) {
			return std::nullopt;
		}
		return Items[static_cast<std::size_t>(aIndex.row())];
	}

	auto review_queue_model::statistics() const noexcept
		-> review_queue_model_statistics
	{
		return Statistics;
	}

	void review_queue_model::apply(
		const std::span<const core::actionable_item> aRanked)
	{
		const auto common = std::min(Items.size(), aRanked.size());

		if (aRanked.size() > Items.size()) {
			beginInsertRows(
				{ },
				static_cast<int>(Items.size()),
				static_cast<int>(aRanked.size() - 1U));
			Items.insert(
				Items.end(),
				aRanked.begin() + static_cast<std::ptrdiff_t>(common),
				aRanked.end());
			endInsertRows();
			Statistics.RowsInserted += aRanked.size() - common;
		} else if (aRanked.size() < Items.size()) {
			const auto removed = Items.size() - aRanked.size();
			beginRemoveRows(
				{ },
				static_cast<int>(aRanked.size()),
				static_cast<int>(Items.size() - 1U));
			Items.erase(
				Items.begin() + static_cast<std::ptrdiff_t>(aRanked.size()),
				Items.end());
			endRemoveRows();
			Statistics.RowsRemoved += removed;
		}

		// Only the prefix that actually differs is reported changed. Emitting a
		// change for every row would be correct and useless: a view cannot tell
		// which row to re-read.
		auto first = std::optional<std::size_t>{ };
		auto last  = std::optional<std::size_t>{ };
		for (auto index = std::size_t{ 0U }; index < common; ++index) {
			if (Items[index] == aRanked[index]) {
				continue;
			}
			Items[index] = aRanked[index];
			first        = first.value_or(index);
			last         = index;
		}
		if (first && last) {
			emit dataChanged(
				index(static_cast<int>(*first)),
				index(static_cast<int>(*last)));
			Statistics.RowsChanged += *last - *first + 1U;
		}
	}

	void review_queue_model::clear()
	{
		if (Items.empty()) {
			return;
		}
		beginRemoveRows({ }, 0, static_cast<int>(Items.size() - 1U));
		const auto removed = Items.size();
		Items.clear();
		endRemoveRows();
		Statistics.RowsRemoved += removed;
	}
} // namespace bpr::qt
