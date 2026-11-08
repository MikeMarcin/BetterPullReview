#include "bpr/core/file_progress_mark.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "pull_request_overview_support.hpp"

#include <QAbstractItemModel>
#include <QChar>
#include <QColor>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QLatin1Char>
#include <QLatin1String>
#include <QLatin1StringView>
#include <QList>
#include <QListSpecialMethods>
#include <QModelIndex>
#include <QObject>
#include <QPalette>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringDecoder>
#include <QStyledItemDelegate>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto operation_name(const application::overview_file_operation aOperation)
			-> QString
		{
			switch (aOperation) {
			case application::overview_file_operation::added:
				return QStringLiteral("Added");
			case application::overview_file_operation::removed:
				return QStringLiteral("Removed");
			case application::overview_file_operation::renamed:
				return QStringLiteral("Renamed");
			case application::overview_file_operation::copied:
				return QStringLiteral("Copied");
			case application::overview_file_operation::type_changed:
				return QStringLiteral("Type changed");
			case application::overview_file_operation::unchanged:
				return QStringLiteral("Unchanged");
			case application::overview_file_operation::modified:
				break;
			}
			return QStringLiteral("Modified");
		}

		[[nodiscard]] auto changed_text(const application::overview_change_since_review aChanged)
			-> QString
		{
			switch (aChanged) {
			case application::overview_change_since_review::changed:
				return QStringLiteral("changed");
			case application::overview_change_since_review::added:
				return QStringLiteral("new");
			case application::overview_change_since_review::unknown:
				return QStringLiteral("?");
			case application::overview_change_since_review::unchanged:
				break;
			}
			return { };
		}

		/// \brief [[BPR-1339]]. The read column's word: the row's mark, where
		/// accepted is stronger than read, seen and unread; unread with no
		/// receipt.
		[[nodiscard]] auto read_text(const application::overview_file_row& aRow) -> QString
		{
			if (!aRow.Mark.has_value()) {
				return QStringLiteral("unread");
			}
			const auto word = core::name(*aRow.Mark);
			return QString::fromLatin1(word.data(), static_cast<qsizetype>(word.size()));
		}

		/// The "Not on a file" row's place among folder paths: no folder has it.
		constexpr auto remainder_path = QLatin1StringView{ "\x01not-on-a-file" };

		constexpr qreal chip_gap = 5.0;

		/// A count is its digits, wide enough for All's state bar beneath it.
		[[nodiscard]] auto count_width(const QFontMetrics& aMetrics, const std::size_t aCount) -> qreal
		{
			return std::max<qreal>(10.0, aMetrics.horizontalAdvance(QString::number(static_cast<qulonglong>(aCount))) + 2.0);
		}

		[[nodiscard]] auto mode_word(const core::feedback_resolution_mode aMode) -> QString
		{
			return aMode == core::feedback_resolution_mode::unresolved ? QStringLiteral("open") : QString{ };
		}
	}

	auto overview_operation_mark_colours(const application::overview_file_operation aOperation, const QPalette& aPalette, const overview_colours& aColours, const bool aSelected,
										 const QPalette::ColorGroup aGroup) -> overview_operation_mark
	{
		const auto base = aPalette.color(QPalette::Active, QPalette::Base);
		switch (aOperation) {
		case application::overview_file_operation::added:
			return { .Fill = aColours.Accepted, .Outline = { }, .Letter = base };
		case application::overview_file_operation::removed:
			return { .Fill = aColours.Defect, .Outline = { }, .Letter = base };
		case application::overview_file_operation::renamed:
		case application::overview_file_operation::copied:
		case application::overview_file_operation::type_changed:
			return { .Fill = aColours.Marker, .Outline = { }, .Letter = base };
		case application::overview_file_operation::modified:
		case application::overview_file_operation::unchanged:
			break;
		}
		// The selected frames' `.op`: the panel's raised tone, its rule and the
		// secondary ink, mixed from the row's ground and text. Mid is no tile
		// colour: in the dark theme it sits below the code ground, and Base's
		// letter on it read as a black square on a selected row (owner,
		// rejecting `overview-keyboard-v001`). On the highlight the mark is the
		// selection's own text, outlined, with no tile.
		const auto mixed = [](const QColor& aTone, const QColor& aGround, const qreal aShare) {
			const auto channel = [aShare](const float aTo, const float aFrom) { return aFrom + (aTo - aFrom) * static_cast<float>(aShare); };
			const auto tone    = aTone.toRgb();
			const auto ground  = aGround.toRgb();
			return QColor::fromRgbF(channel(tone.redF(), ground.redF()), channel(tone.greenF(), ground.greenF()), channel(tone.blueF(), ground.blueF()));
		};
		if (aSelected) {
			const auto ink = aPalette.color(aGroup, QPalette::HighlightedText);
			return { .Fill = { }, .Outline = mixed(ink, aPalette.color(aGroup, QPalette::Highlight), 0.6), .Letter = ink };
		}
		const auto ink = aPalette.color(QPalette::Active, QPalette::Text);
		return { .Fill = mixed(ink, base, 0.07), .Outline = mixed(ink, base, 0.32), .Letter = mixed(ink, base, 0.74) };
	}

	auto overview_row_band(const QPalette& aPalette, const QPalette::ColorGroup aGroup) -> QColor
	{
		const auto ground = aPalette.color(aGroup, QPalette::Base).toRgb();
		const auto ink    = aPalette.color(aGroup, QPalette::Text).toRgb();
		const auto toward = [&ground, &ink](const double aShare) {
			const auto channel = [aShare](const float aFrom, const float aTo) { return aFrom + ((aTo - aFrom) * static_cast<float>(aShare)); };
			return QColor::fromRgbF(channel(ground.redF(), ink.redF()), channel(ground.greenF(), ink.greenF()), channel(ground.blueF(), ink.blueF()));
		};
		const auto step = [&ground](const QColor& aColour) { return contrast_ratio(to_appearance_color(aColour), to_appearance_color(ground)); };
		if (step(ink) <= overview_row_band_contrast) {
			return ground;
		}
		// The ratio grows along the way from the ground to the text: halve the
		// interval to the first share that reaches the step.
		auto nearer  = 0.0;
		auto further = 1.0;
		for (auto pass = 0; pass < 16; ++pass) {
			const auto share                                                      = (nearer + further) / 2.0;
			(step(toward(share)) < overview_row_band_contrast ? nearer : further) = share;
		}
		return toward(further);
	}

	auto overview_count_name(const core::feedback_count_target& aTarget, const bool aAnyColumns) -> QString
	{
		switch (aTarget.Kind) {
		case core::feedback_count_kind::dedicated:
			return aTarget.Label ? feedback_label_text(*aTarget.Label) : QString{ };
		case core::feedback_count_kind::catchall:
			return aAnyColumns ? QStringLiteral("Other") : QStringLiteral("Comments");
		case core::feedback_count_kind::uncertain:
			return QStringLiteral("Not counted");
		case core::feedback_count_kind::included:
			break;
		}
		return QStringLiteral("Comments");
	}

	auto overview_comment_chips(const core::feedback_counter_projection& aCounts, const QRect& aCell, const QFont& aFont) -> std::vector<overview_comment_chip>
	{
		const auto metrics = QFontMetrics{ chip_font(aFont) };
		auto       chips   = std::vector<overview_comment_chip>{ };
		for (const auto& column : aCounts.Dedicated) {
			if (column.Count != 0U) {
				auto chip   = overview_comment_chip{ };
				chip.Kind   = overview_comment_chip::kind::label;
				chip.Label  = column.Label;
				chip.Count  = column.Count;
				chip.States = column.States;
				chips.push_back(std::move(chip));
			}
		}
		if (aCounts.Catchall != 0U) {
			auto chip   = overview_comment_chip{ };
			chip.Kind   = overview_comment_chip::kind::other;
			chip.Count  = aCounts.Catchall;
			chip.States = aCounts.CatchallStates;
			chips.push_back(std::move(chip));
		}
		const auto qualified = aCounts.Uncertain != 0U || !aCounts.Exact;
		const auto top       = static_cast<qreal>(aCell.center().y()) - chip_mark / 2.0 + 0.5;
		const auto left      = aCell.left() + 6.0;
		const auto right     = aCell.right() - 4.0 - (qualified ? chip_mark + 4.0 : 0.0);
		const auto widthOf   = [&](const overview_comment_chip& aChip) { return chip_mark + count_gap + count_width(metrics, aChip.Count); };
		auto       total     = 0.0;
		for (const auto& chip : chips) {
			total += widthOf(chip) + chip_gap;
		}
		auto shown = chips.size();
		if (left + total - chip_gap > right) {
			// Fold the tail into "+n" so the chips that are shown stay whole.
			const auto moreWidth = [&](const std::size_t aFolded) { return metrics.horizontalAdvance(QStringLiteral("+%1").arg(aFolded)) + 10.0; };
			auto       used      = 0.0;
			shown                = 0U;
			while (shown < chips.size() && left + used + widthOf(chips[shown]) + chip_gap + moreWidth(chips.size() - shown - 1U) <= right) {
				used += widthOf(chips[shown]) + chip_gap;
				++shown;
			}
		}
		auto result = std::vector<overview_comment_chip>{ };
		auto x      = left;
		for (std::size_t index = 0U; index < shown; ++index) {
			auto chip = chips[index];
			chip.Box  = QRectF{ x, top, widthOf(chip), chip_mark };
			x += chip.Box.width() + chip_gap;
			result.push_back(std::move(chip));
		}
		if (shown < chips.size()) {
			auto more   = overview_comment_chip{ };
			more.Kind   = overview_comment_chip::kind::more;
			more.Count  = chips.size() - shown;
			more.Folded = std::vector<overview_comment_chip>{ chips.begin() + static_cast<std::ptrdiff_t>(shown), chips.end() };
			more.Box    = QRectF{ x, top, metrics.horizontalAdvance(QStringLiteral("+%1").arg(more.Count)) + 10.0, chip_mark };
			result.push_back(std::move(more));
		}
		if (qualified) {
			auto qualifier  = overview_comment_chip{ };
			qualifier.Kind  = overview_comment_chip::kind::uncertain;
			qualifier.Count = aCounts.Uncertain;
			qualifier.Box   = QRectF{ aCell.right() - 4.0 - chip_mark, top, chip_mark, chip_mark };
			result.push_back(std::move(qualifier));
		}
		return result;
	}

	overview_file_model::overview_file_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
	}

	void overview_file_model::update_file(node& aNode, const application::overview_file_row& aRow)
	{
		if (aNode.File != aRow) {
			aNode.File = aRow;
			emit dataChanged(index_of(aNode), index_of(aNode, column_count - 1));
		}
	}

	void overview_file_model::build_tree(node& aRoot, const std::vector<application::overview_file_row>& aRows)
	{
		auto       folders = QHash<QString, node*>{ };
		const auto folder  = [&folders](node& aParent, const QString& aPath) {
			if (auto* found = folders.value(aPath, nullptr)) {
				return found;
			}
			auto child    = std::make_unique<node>();
			child->Path   = aPath;
			child->Parent = &aParent;
			child->Row    = static_cast<int>(aParent.Children.size());
			auto* result  = child.get();
			aParent.Children.push_back(std::move(child));
			folders.insert(aPath, result);
			return result;
		};
		// [[BPR-1213]]. Flat: one row per folder that holds files, named by
		// its whole path, so a folder with no file of its own takes no row and
		// every file sits one level down, at the same left column.
		for (const auto& row : aRows) {
			auto* const parent = folder(aRoot, text(row.Folder));
			auto        file   = std::make_unique<node>();
			file->Path         = text(row.Path);
			file->File         = row;
			file->Parent       = parent;
			file->Row          = static_cast<int>(parent->Children.size());
			parent->Children.push_back(std::move(file));
			for (auto* ancestor = parent; ancestor != nullptr; ancestor = ancestor->Parent) {
				++ancestor->FileCount;
			}
		}
	}

	void overview_file_model::update_rows(node& aParent, const int aFirst, const int aLast)
	{
		for (auto row = aFirst; row <= aLast; ++row) {
			aParent.Children[static_cast<std::size_t>(row)]->Row = row;
		}
	}

	void overview_file_model::reconcile(node& aParent, node& aDesired)
	{
		auto folders = QHash<QString, node*>{ };
		auto files   = QHash<QString, node*>{ };
		for (const auto& child : aParent.Children) {
			(child->File ? files : folders).insert(child->Path, child.get());
		}
		const auto parentIndex = index_of(aParent);
		for (std::size_t position = 0; position < aDesired.Children.size(); ++position) {
			auto&      desired  = aDesired.Children[position];
			auto*      existing = (desired->File ? files : folders).value(desired->Path, nullptr);
			const auto row      = static_cast<int>(position);
			if (existing == nullptr) {
				beginInsertRows(parentIndex, row, row);
				desired->Parent = &aParent;
				aParent.Children.insert(aParent.Children.begin() + row, std::move(desired));
				update_rows(aParent, row, static_cast<int>(aParent.Children.size()) - 1);
				endInsertRows();
				continue;
			}
			if (existing->Row != row) {
				const auto previous = existing->Row;
				beginMoveRows(parentIndex, previous, previous, parentIndex, row);
				std::rotate(aParent.Children.begin() + row, aParent.Children.begin() + previous,
							aParent.Children.begin() + previous + 1);
				update_rows(aParent, row, previous);
				endMoveRows();
			}
			if (desired->File) {
				update_file(*existing, *desired->File);
			} else {
				reconcile(*existing, *desired);
			}
		}
		const auto retained = static_cast<int>(aDesired.Children.size());
		if (aParent.Children.size() > aDesired.Children.size()) {
			beginRemoveRows(parentIndex, retained, static_cast<int>(aParent.Children.size()) - 1);
			aParent.Children.erase(aParent.Children.begin() + retained, aParent.Children.end());
			endRemoveRows();
		}
		if (aParent.FileCount != aDesired.FileCount) {
			aParent.FileCount = aDesired.FileCount;
			if (parentIndex.isValid()) {
				emit dataChanged(parentIndex, index_of(aParent, column_count - 1));
			}
		}
	}

	void overview_file_model::rebuild_lookup(const std::vector<application::overview_file_row>& aRows)
	{
		FileNodes.clear();
		FileOrder.clear();
		FolderCount        = 0U;
		LargestFolderCount = 0U;
		auto pending       = std::vector<node*>{ &Root };
		while (!pending.empty()) {
			auto* parent = pending.back();
			pending.pop_back();
			for (const auto& child : parent->Children) {
				if (child->File) {
					FileNodes.insert(child->Path, child.get());
				} else if (!child->Remainder) {
					++FolderCount;
					LargestFolderCount = std::max(LargestFolderCount, child->FileCount);
					pending.push_back(child.get());
				}
			}
		}
		FileOrder.reserve(aRows.size());
		for (const auto& row : aRows) {
			FileOrder.push_back(FileNodes.value(text(row.Path)));
		}
	}

	void overview_file_model::apply(const std::vector<application::overview_file_row>& aRows)
	{
		// Metadata-only refreshes retain the tree and allocate no replacement nodes.
		const auto sameOrder = std::ranges::equal(FileOrder, aRows,
												  [](const node* aNode, const application::overview_file_row& aRow) {
													  return aNode->File->Path == aRow.Path && aNode->File->Folder == aRow.Folder;
												  });
		if (sameOrder) {
			for (std::size_t row = 0; row < aRows.size(); ++row) {
				update_file(*FileOrder[row], aRows[row]);
			}
			return;
		}
		auto desired = node{ };
		build_tree(desired, aRows);
		// [[BPR-1089]]. The "Not on a file" row stays last, as it is.
		if (!Root.Children.empty() && Root.Children.back()->Remainder) {
			auto remainder       = std::make_unique<node>();
			remainder->Path      = remainder_path;
			remainder->Remainder = true;
			remainder->Parent    = &desired;
			remainder->Row       = static_cast<int>(desired.Children.size());
			desired.Children.push_back(std::move(remainder));
		}
		reconcile(Root, desired);
		rebuild_lookup(aRows);
	}

	void overview_file_model::apply_feedback(std::optional<application::overview_feedback_projection> aFeedback)
	{
		const auto had = Feedback.has_value();
		Feedback       = std::move(aFeedback);
		FileFeedback.clear();
		FolderFeedback.clear();
		if (Feedback) {
			for (std::size_t index = 0U; index < Feedback->Files.size(); ++index) {
				FileFeedback.insert(text(Feedback->Files[index].Path), index);
			}
			for (std::size_t index = 0U; index < Feedback->Folders.size(); ++index) {
				FolderFeedback.insert(text(Feedback->Folders[index].Path), index);
			}
		}
		sync_remainder();
		// Only the Comments column moves: one change per parent's run of rows.
		auto pending = std::vector<const node*>{ &Root };
		while (!pending.empty()) {
			const auto* parent = pending.back();
			pending.pop_back();
			if (!parent->Children.empty()) {
				emit dataChanged(index(0, comments_column, index_of(*parent)), index(static_cast<int>(parent->Children.size()) - 1, comments_column, index_of(*parent)));
			}
			for (const auto& child : parent->Children) {
				if (!child->File && !child->Remainder) {
					pending.push_back(child.get());
				}
			}
		}
		if (had != Feedback.has_value()) {
			emit headerDataChanged(Qt::Horizontal, comments_column, comments_column);
		}
	}

	void overview_file_model::sync_remainder()
	{
		const auto shown  = !Root.Children.empty() && Root.Children.back()->Remainder;
		const auto wanted = Feedback.has_value() && Feedback->Remainder.Counts.ScopeTotal != 0U;
		if (shown == wanted) {
			return;
		}
		const auto row = static_cast<int>(Root.Children.size()) - (shown ? 1 : 0);
		if (wanted) {
			beginInsertRows({ }, row, row);
			auto remainder       = std::make_unique<node>();
			remainder->Path      = remainder_path;
			remainder->Remainder = true;
			remainder->Parent    = &Root;
			remainder->Row       = row;
			Root.Children.push_back(std::move(remainder));
			endInsertRows();
		} else {
			beginRemoveRows({ }, row, row);
			Root.Children.pop_back();
			endRemoveRows();
		}
	}

	auto overview_file_model::feedback() const noexcept -> const std::optional<application::overview_feedback_projection>&
	{
		return Feedback;
	}

	auto overview_file_model::feedback_at(const QModelIndex& aIndex) const -> const core::feedback_counter_projection*
	{
		const auto* item = node_at(aIndex);
		if (item == nullptr || !Feedback) {
			return nullptr;
		}
		if (item->Remainder) {
			return &Feedback->Remainder.Counts;
		}
		const auto& lookup = item->File ? FileFeedback : FolderFeedback;
		const auto  found  = lookup.constFind(item->Path);
		if (found == lookup.cend()) {
			return nullptr;
		}
		return item->File ? &Feedback->Files[*found].Counts : &Feedback->Folders[*found].Counts;
	}

	auto overview_file_model::feedback_row(const QModelIndex& aIndex) const
		-> std::optional<std::pair<application::overview_feedback_row_kind, std::string>>
	{
		const auto* item = node_at(aIndex);
		if (item == nullptr || feedback_at(aIndex) == nullptr) {
			return std::nullopt;
		}
		if (item->Remainder) {
			return std::pair{ application::overview_feedback_row_kind::remainder, std::string{ } };
		}
		return std::pair{ item->File ? application::overview_feedback_row_kind::file : application::overview_feedback_row_kind::folder, item->Path.toStdString() };
	}

	auto overview_file_model::remainder_index(const int aColumn) const -> QModelIndex
	{
		return !Root.Children.empty() && Root.Children.back()->Remainder ? index_of(*Root.Children.back(), aColumn) : QModelIndex{ };
	}

	auto overview_file_model::comments_text(const core::feedback_counter_projection& aCounts, const bool aAccessible) const -> QString
	{
		const auto anyColumns = any_columns(Feedback);
		const auto all        = aCounts.Mode == core::feedback_resolution_mode::all;
		auto       parts      = QStringList{ };
		// In All each count says its breakdown, as its state bar shows it.
		const auto part = [&](const QString& aName, const std::size_t aCount, const core::feedback_state_counts& aStates) {
			parts.push_back(aAccessible && all ? QStringLiteral("%1 %2 (%3)").arg(aName, count_text(aCount), breakdown_text(aStates)) : QStringLiteral("%1 %2").arg(aName, count_text(aCount)));
		};
		for (const auto& column : aCounts.Dedicated) {
			if (column.Count != 0U) {
				part(feedback_label_text(column.Label), column.Count, column.States);
			}
		}
		if (aCounts.Catchall != 0U) {
			part(anyColumns ? QStringLiteral("Other") : QStringLiteral("Comments"), aCounts.Catchall, aCounts.CatchallStates);
		}
		if (!aAccessible) {
			if (aCounts.Uncertain != 0U || !aCounts.Exact) {
				parts.push_back(QStringLiteral("? %1").arg(count_text(aCounts.Uncertain)));
			}
			return parts.join(QStringLiteral(" · "));
		}
		const auto open    = mode_word(aCounts.Mode);
		auto       said    = parts.isEmpty() ? QStringLiteral("No %1threads").arg(open.isEmpty() ? QString{ } : open + QLatin1Char{ ' ' })
											 : QStringLiteral("%1 %2threads").arg(parts.join(QStringLiteral(", ")), open.isEmpty() ? QString{ } : open + QLatin1Char{ ' ' });
		const auto clauses = qualifier_clauses(aCounts);
		if (!clauses.isEmpty()) {
			said += QStringLiteral("; ") + clauses.join(QStringLiteral("; "));
		}
		return said;
	}

	void overview_file_model::update_rows(const std::span<const application::overview_file_row> aRows)
	{
		for (const auto& row : aRows) {
			if (auto* const file = FileNodes.value(text(row.Path), nullptr)) {
				update_file(*file, row);
			}
		}
	}

	auto overview_file_model::node_at(const QModelIndex& aIndex) const -> const node*
	{
		return aIndex.isValid() && aIndex.model() == this
			? static_cast<const node*>(aIndex.internalPointer())
			: nullptr;
	}

	auto overview_file_model::index_of(const node& aNode, const int aColumn) const -> QModelIndex
	{
		return &aNode == &Root ? QModelIndex{ }
							   : createIndex(aNode.Row, aColumn, const_cast<node*>(&aNode));
	}

	auto overview_file_model::index(const int aRow, const int aColumn, const QModelIndex& aParent) const
		-> QModelIndex
	{
		if (aRow < 0 || aColumn < 0 || aColumn >= column_count
			|| (aParent.isValid() && aParent.column() != 0)) {
			return { };
		}
		const auto* parent = aParent.isValid() ? node_at(aParent) : &Root;
		return parent != nullptr && static_cast<std::size_t>(aRow) < parent->Children.size()
			? index_of(*parent->Children[static_cast<std::size_t>(aRow)], aColumn)
			: QModelIndex{ };
	}

	auto overview_file_model::parent(const QModelIndex& aIndex) const -> QModelIndex
	{
		const auto* item = node_at(aIndex);
		return item != nullptr && item->Parent != nullptr ? index_of(*item->Parent) : QModelIndex{ };
	}

	auto overview_file_model::rowCount(const QModelIndex& aParent) const -> int
	{
		if (aParent.isValid() && aParent.column() != 0) {
			return 0;
		}
		const auto* parent = aParent.isValid() ? node_at(aParent) : &Root;
		return parent == nullptr ? 0 : static_cast<int>(parent->Children.size());
	}

	auto overview_file_model::columnCount(const QModelIndex& aParent) const -> int
	{
		static_cast<void>(aParent);
		return column_count;
	}

	auto overview_file_model::row_at(const QModelIndex& aIndex) const
		-> const application::overview_file_row*
	{
		const auto* item = node_at(aIndex);
		return item != nullptr && item->File ? &*item->File : nullptr;
	}

	auto overview_file_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		const auto* item = node_at(aIndex);
		if (item == nullptr) {
			return { };
		}
		if (aRole == remainder_role) {
			return item->Remainder;
		}
		const auto* counts = aIndex.column() == comments_column ? feedback_at(aIndex) : nullptr;
		if (counts != nullptr) {
			switch (aRole) {
			case Qt::DisplayRole:
				return comments_text(*counts, false);
			case Qt::AccessibleTextRole:
				return comments_text(*counts, true);
			case Qt::ToolTipRole:
				return comments_text(*counts, true) + QStringLiteral("\nTab steps onto each chip; Enter lists its threads.");
			default:
				break;
			}
		}
		if (item->Remainder) {
			if (aRole == folder_role) {
				return false;
			}
			if (aIndex.column() == file_column && aRole == Qt::DisplayRole) {
				return QStringLiteral("Not on a file");
			}
			if (aIndex.column() == file_column && (aRole == Qt::ToolTipRole || aRole == Qt::AccessibleTextRole)) {
				return QStringLiteral("Not on a file: unanchored threads, pull request comments and review summaries");
			}
			return { };
		}
		if (!item->File) {
			if (aRole == folder_role) {
				return true;
			}
			if (aRole == file_count_role) {
				return static_cast<qulonglong>(item->FileCount);
			}
			if (aIndex.column() != file_column && aIndex.column() != operation_column) {
				return { };
			}
			if (aRole == Qt::DisplayRole && aIndex.column() == file_column) {
				return item->Path.isEmpty() ? QStringLiteral("/") : item->Path;
			}
			if (aRole == Qt::ToolTipRole || aRole == Qt::AccessibleTextRole) {
				return QStringLiteral("Folder %1, %2 files").arg(item->Path.isEmpty() ? QStringLiteral("/") : item->Path).arg(item->FileCount);
			}
			return { };
		}
		const auto* const row = row_at(aIndex);
		if (row == nullptr) {
			return { };
		}
		switch (aRole) {
		case folder_role:
			return false;
		case path_role:
			return text(row->Path);
		case operation_role:
			return static_cast<int>(row->Operation);
		case changed_role:
			return static_cast<int>(row->Changed);
		case read_role:
			return row->Read.has_value() ? static_cast<int>(*row->Read) : -1;
		case progress_role:
			return row->Mark.has_value() ? static_cast<int>(*row->Mark) : -1;
		case threads_role:
			return static_cast<qulonglong>(row->ThreadCount);
		case unresolved_role:
			return static_cast<qulonglong>(row->UnresolvedThreadCount);
		case Qt::TextAlignmentRole:
			if (aIndex.column() == added_column || aIndex.column() == removed_column) {
				return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
			}
			return { };
		case Qt::ToolTipRole:
			switch (aIndex.column()) {
			case operation_column:
				return operation_name(row->Operation);
			case file_column:
				return row->PreviousPath.has_value()
					? QStringLiteral("%1 (was %2)").arg(text(row->Path), text(*row->PreviousPath))
					: text(row->Path);
			case changed_column:
				return row->Changed == application::overview_change_since_review::unknown
					? QStringLiteral("Not yet resolved: compare since my last review to find out")
					: changed_text(row->Changed);
			case read_column:
				return read_text(*row);
			case comments_column:
				return QStringLiteral("%1 threads, %2 unresolved")
					.arg(count_text(row->ThreadCount), count_text(row->UnresolvedThreadCount));
			case note_column:
				return row->Note.has_value() ? text(*row->Note) : QString{ };
			default:
				return { };
			}
		case Qt::DisplayRole:
		case Qt::AccessibleTextRole:
			switch (aIndex.column()) {
			case operation_column:
				return operation_letter(row->Operation);
			case file_column:
				return text(row->Name);
			case added_column:
				return row->Additions == 0U ? QString{ } : QStringLiteral("+%1").arg(row->Additions);
			case removed_column:
				return row->Deletions == 0U ? QString{ } : QStringLiteral("−%1").arg(row->Deletions);
			case changed_column:
				return changed_text(row->Changed);
			case read_column:
				return aRole == Qt::AccessibleTextRole ? read_text(*row) : QString{ };
			case comments_column:
				if (row->ThreadCount == 0U) {
					return QString{ };
				}
				return row->UnresolvedThreadCount == 0U
					? count_text(row->ThreadCount)
					: QStringLiteral("%1 / %2").arg(count_text(row->UnresolvedThreadCount), count_text(row->ThreadCount));
			case note_column:
				return row->Note.has_value() ? text(*row->Note) : QString{ };
			default:
				return { };
			}
		default:
			return { };
		}
	}

	auto overview_file_model::headerData(const int aSection, const Qt::Orientation aOrientation, const int aRole) const
		-> QVariant
	{
		if (aOrientation != Qt::Horizontal) {
			return { };
		}
		// [[BPR-1089]]. Comments ▾ chooses the number the rows sort by.
		if (aSection == comments_column && Feedback && (aRole == Qt::ToolTipRole || aRole == Qt::AccessibleDescriptionRole)) {
			const auto open = Feedback->Request.Mode == core::feedback_resolution_mode::unresolved;
			auto       tip  = QStringLiteral("%1 by label, in your Settings order; %2. Replies don't count.\nClick a chip, or Tab onto it and press Enter, to list its threads.")
								  .arg(open ? QStringLiteral("Open threads") : QStringLiteral("Threads"),
									   any_columns(Feedback) ? QStringLiteral("the bubble with dots holds the other labels") : QStringLiteral("no labels are ticked, so the bubble counts them all"));
			tip += Feedback->Request.Sort ? QStringLiteral("\nSorted by %1, most first. Click to change.").arg(overview_count_name(*Feedback->Request.Sort, any_columns(Feedback)))
										  : QStringLiteral("\nClick to sort by a label.");
			return tip;
		}
		if (aRole != Qt::DisplayRole) {
			return { };
		}
		if (aSection == comments_column && Feedback) {
			return Feedback->Request.Sort ? QStringLiteral("Comments ↓ ▾") : QStringLiteral("Comments ▾");
		}
		switch (aSection) {
		case operation_column:
			return QString{ };
		case file_column:
			return QStringLiteral("File");
		case added_column:
			return QStringLiteral("Added");
		case removed_column:
			return QStringLiteral("Removed");
		case changed_column:
			return QStringLiteral("Changed");
		case read_column:
			return QStringLiteral("Read");
		case comments_column:
			return QStringLiteral("Comments");
		case note_column:
			return QStringLiteral("Note");
		default:
			return { };
		}
	}

	auto overview_file_model::folder_count() const noexcept -> std::size_t
	{
		return FolderCount;
	}

	auto overview_file_model::file_count() const noexcept -> std::size_t
	{
		return FileOrder.size();
	}

	auto overview_file_model::largest_folder_file_count() const noexcept -> std::size_t
	{
		return LargestFolderCount;
	}

	auto overview_file_model::index_for_path(const QString& aPath) const -> QModelIndex
	{
		const auto* file = FileNodes.value(aPath, nullptr);
		return file == nullptr ? QModelIndex{ } : index_of(*file, file_column);
	}

	overview_row_delegate::overview_row_delegate(const overview_colours& aColours, QObject* const aParent)
		: QStyledItemDelegate{ aParent }
		, Colours{ aColours }
	{
	}
}
