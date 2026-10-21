#include "bpr/qt/files_tree.hpp"
#include "bpr/core/file_progress_mark.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/resident_filter.hpp"
#include "bpr/qt/steady_columns.hpp"

#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QScopedValueRollback>
#include <QSet>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <algorithm>
#include <array>
#include <map>
#include <ranges>
#include <string>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr std::size_t root_node = 0U;

		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto changed_since_review(const application::overview_file_row& aRow) noexcept -> bool
		{
			return aRow.Changed == application::overview_change_since_review::changed
				|| aRow.Changed == application::overview_change_since_review::added;
		}

		[[nodiscard]] auto operation_word(const application::overview_file_operation aOperation) -> QString
		{
			switch (aOperation) {
			case application::overview_file_operation::added:
				return QStringLiteral("A");
			case application::overview_file_operation::modified:
				return QStringLiteral("M");
			case application::overview_file_operation::removed:
				return QStringLiteral("D");
			case application::overview_file_operation::renamed:
				return QStringLiteral("R");
			case application::overview_file_operation::copied:
				return QStringLiteral("C");
			case application::overview_file_operation::type_changed:
				return QStringLiteral("T");
			case application::overview_file_operation::unchanged:
				break;
			}
			return QString{ };
		}

		[[nodiscard]] auto operation_description(const application::overview_file_operation aOperation) -> QString
		{
			switch (aOperation) {
			case application::overview_file_operation::added:        return QStringLiteral("added");
			case application::overview_file_operation::modified:     return QStringLiteral("modified");
			case application::overview_file_operation::removed:      return QStringLiteral("removed");
			case application::overview_file_operation::renamed:      return QStringLiteral("renamed");
			case application::overview_file_operation::copied:       return QStringLiteral("copied");
			case application::overview_file_operation::type_changed: return QStringLiteral("type changed");
			case application::overview_file_operation::unchanged:    return QStringLiteral("unchanged");
			}
			return QString{ };
		}
	} // namespace

	// ---- files_tree_model ----------------------------------------------------

	files_tree_model::files_tree_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
		Nodes.push_back(files_tree_node{ .Folder = true });
	}

	auto files_tree_model::work_statistics() const noexcept -> files_tree_work_statistics
	{
		return WorkStatistics;
	}

	void files_tree_model::apply(const std::vector<workbench_drawer_row>& aRows)
	{
		auto knownRows = QHash<QString, const files_tree_entry*>{ };
		knownRows.reserve(static_cast<qsizetype>(Entries.size()));
		for (const auto& entry : Entries) {
			if (!knownRows.contains(entry.Path)) {
				knownRows.insert(entry.Path, &entry);
			}
		}
		auto entries = std::vector<files_tree_entry>{ };
		entries.reserve(aRows.size());
		for (const auto& row : aRows) {
			if (!row.File.has_value()) {
				continue;
			}
			auto entry = files_tree_entry{ .Path = row.Title, .File = *row.File };
			// The overview's facts survive a re-listing of the same path.
			const auto known = knownRows.constFind(row.Title);
			if (known != knownRows.constEnd()) {
				entry.Overview = (*known)->Overview;
			}
			entries.push_back(std::move(entry));
		}
		auto previous = std::exchange(Entries, std::move(entries));
		EntryIndex.clear();
		EntryIndex.reserve(Entries.size());
		for (std::size_t position = 0U; position < Entries.size(); ++position) {
			EntryIndex.emplace_back(Entries[position].File, position);
		}
		std::ranges::sort(EntryIndex);
		rebuild(previous);
	}

	void files_tree_model::apply_overview(const std::span<const application::overview_file_row> aFiles)
	{
		// [[BPR-0673]]. Each overview path is decoded once. Searching the rows
		// for every entry decoded a path per pair, a quarter of a million for
		// a five-hundred-file review, on every overview publication. The first
		// row for a path answers for it, as that search did.
		auto rows = QHash<QString, const application::overview_file_row*>{ };
		rows.reserve(static_cast<qsizetype>(aFiles.size()));
		for (const auto& row : aFiles) {
			auto path = text(row.Path);
			if (!rows.contains(path)) {
				rows.insert(std::move(path), &row);
			}
		}
		auto changed      = std::vector<core::workbench_file_id>{ };
		auto orderChanged = false;
		for (auto& entry : Entries) {
			++WorkStatistics.OverviewRowsVisited;
			const auto found = rows.constFind(entry.Path);
			auto       next  = found != rows.constEnd() ? std::optional{ **found } : std::nullopt;
			if (entry.Overview == next) {
				continue;
			}
			const auto wasChanged = entry.Overview && changed_since_review(*entry.Overview);
			const auto nowChanged = next && changed_since_review(*next);
			orderChanged          = orderChanged || (wasChanged != nowChanged);
			entry.Overview        = std::move(next);
			changed.push_back(entry.File);
		}
		if (Order == files_order::changed_first && orderChanged) {
			rebuild();
		}
		for (const auto file : changed) {
			const auto index = index_for_file(file);
			if (index.isValid()) {
				emit dataChanged(index, index);
				++WorkStatistics.ChangedRows;
			}
		}
	}

	void files_tree_model::apply_overview_rows(const std::span<const application::overview_file_row> aRows)
	{
		auto changed      = std::vector<core::workbench_file_id>{ };
		auto orderChanged = false;
		for (const auto& row : aRows) {
			const auto file = core::file_identity_of(row.Path);
			// Every entry of the file takes the row, shown or filtered away,
			// as the complete join by path does.
			for (const auto& [identity, position] : std::ranges::equal_range(EntryIndex, file, { }, &std::pair<core::workbench_file_id, std::size_t>::first)) {
				++WorkStatistics.OverviewRowsVisited;
				auto& entry = Entries[position];
				if (entry.Overview == std::optional{ row }) {
					continue;
				}
				const auto wasChanged = entry.Overview && changed_since_review(*entry.Overview);
				orderChanged          = orderChanged || wasChanged != changed_since_review(row);
				entry.Overview        = row;
				changed.push_back(identity);
			}
		}
		if (Order == files_order::changed_first && orderChanged) {
			rebuild();
		}
		for (const auto& file : changed) {
			if (const auto index = index_for_file(file); index.isValid()) {
				emit dataChanged(index, index);
				++WorkStatistics.ChangedRows;
			}
		}
	}

	void files_tree_model::apply_delta(const std::span<const core::workbench_file_id> aRemoved,
									   const std::span<const workbench_drawer_row>    aInserted)
	{
		auto whole = false;
		for (const auto& file : aRemoved) {
			for (const auto& [identity, position] : std::ranges::equal_range(EntryIndex, file, { }, &std::pair<core::workbench_file_id, std::size_t>::first)) {
				if (Entries[position].Withheld) {
					continue;
				}
				Entries[position].Withheld = true;
				const auto shown           = std::ranges::equal_range(FileIndex, file, { }, &std::pair<core::workbench_file_id, std::size_t>::first);
				auto       nodes           = std::vector<std::size_t>{ };
				for (const auto& [shownFile, node] : shown) {
					nodes.push_back(node);
				}
				for (const auto node : nodes) {
					whole = whole || !remove_file_node(node);
				}
			}
		}
		for (const auto& row : aInserted) {
			if (!row.File.has_value()) {
				continue;
			}
			const auto known = std::ranges::equal_range(EntryIndex, *row.File, { }, &std::pair<core::workbench_file_id, std::size_t>::first);
			auto       entry = std::size_t{ 0U };
			if (!known.empty()) {
				entry = known.front().second;
				if (!Entries[entry].Withheld) {
					continue;
				}
				Entries[entry].Withheld = false;
				Entries[entry].Path     = row.Title;
			} else {
				Entries.push_back(files_tree_entry{ .Path = row.Title, .File = *row.File });
				entry = Entries.size() - 1U;
				EntryIndex.insert(std::ranges::upper_bound(EntryIndex, std::pair{ *row.File, entry }), std::pair{ *row.File, entry });
			}
			if (resident_filter_matches(Entries[entry].Path, Filter)) {
				whole = whole || !insert_file_node(entry);
			}
		}
		if (whole) {
			// Folders appear, vanish or compact: the complete list decides.
			rebuild();
		}
	}

	auto files_tree_model::remove_file_node(const std::size_t aNode) -> bool
	{
		const auto parent = Nodes[aNode].Parent;
		if (Order == files_order::tree && parent != root_node) {
			// A folder left empty disappears, and one left holding a single
			// folder compacts with it: both change folders, so the complete
			// list decides.
			const auto& siblings = Nodes[parent].Children;
			if (siblings.size() < 2U) {
				return false;
			}
			if (siblings.size() == 2U && Nodes[siblings.front() == aNode ? siblings.back() : siblings.front()].Folder) {
				return false;
			}
		}
		const auto row  = Nodes[aNode].Row;
		const auto file = std::pair{ Entries[*Nodes[aNode].Entry].File, aNode };
		beginRemoveRows(index_of_node(parent), row, row);
		auto& siblings = Nodes[parent].Children;
		siblings.erase(siblings.begin() + row);
		renumber(parent, static_cast<std::size_t>(row));
		if (const auto at = std::ranges::lower_bound(FileIndex, file); at != FileIndex.end() && *at == file) {
			FileIndex.erase(at);
		}
		release(aNode);
		endRemoveRows();
		++WorkStatistics.RowsRemoved;
		count_ancestors(parent, false);
		return true;
	}

	auto files_tree_model::insert_file_node(const std::size_t aEntry) -> bool
	{
		const auto& path   = Entries[aEntry].Path;
		auto        parent = root_node;
		auto        label  = path;
		if (Order == files_order::tree) {
			// Only a folder row that ends exactly at the file's folder can take
			// it; a new or compacted folder is the complete list's to decide.
			const auto slash = path.lastIndexOf(QLatin1Char{ '/' });
			if (slash >= 0) {
				const auto folder = FolderIndex.constFind(path.left(slash));
				if (folder == FolderIndex.constEnd()) {
					return false;
				}
				parent = *folder;
				label  = path.mid(slash + 1);
			}
		}
		const auto changedFirst = Order == files_order::changed_first;
		const auto changed      = Entries[aEntry].Overview.has_value() && changed_since_review(*Entries[aEntry].Overview);
		const auto before       = [&](const std::size_t aSibling) {
			const auto& sibling = Nodes[aSibling];
			if (Order == files_order::tree && sibling.Folder) {
				return true;
			}
			if (changedFirst) {
				const auto siblingChanged = sibling.Entry && Entries[*sibling.Entry].Overview && changed_since_review(*Entries[*sibling.Entry].Overview);
				if (siblingChanged != changed) {
					return siblingChanged;
				}
			}
			return sibling.Label.compare(label, Qt::CaseInsensitive) < 0;
		};
		const auto& siblings = Nodes[parent].Children;
		const auto  position = static_cast<std::size_t>(std::distance(siblings.begin(), std::ranges::partition_point(siblings, before)));
		beginInsertRows(index_of_node(parent), static_cast<int>(position), static_cast<int>(position));
		auto node = std::size_t{ 0U };
		if (FreeNodes.empty()) {
			Nodes.emplace_back();
			node = Nodes.size() - 1U;
		} else {
			node = FreeNodes.back();
			FreeNodes.pop_back();
		}
		Nodes[node] = files_tree_node{ .Label = label, .Path = path, .Entry = aEntry, .Parent = parent };
		Nodes[parent].Children.insert(Nodes[parent].Children.begin() + static_cast<std::ptrdiff_t>(position), node);
		renumber(parent, position);
		const auto file = std::pair{ Entries[aEntry].File, node };
		FileIndex.insert(std::ranges::upper_bound(FileIndex, file), file);
		endInsertRows();
		++WorkStatistics.RowsInserted;
		count_ancestors(parent, true);
		return true;
	}

	void files_tree_model::count_ancestors(const std::size_t aNode, const bool aAdded)
	{
		for (auto ancestor = aNode;; ancestor = Nodes[ancestor].Parent) {
			Nodes[ancestor].FileCount = aAdded ? Nodes[ancestor].FileCount + 1U : Nodes[ancestor].FileCount - 1U;
			if (ancestor == root_node) {
				break;
			}
			const auto index = index_of_node(ancestor);
			emit       dataChanged(index, index);
			++WorkStatistics.ChangedRows;
		}
	}

	void files_tree_model::set_order(const files_order aOrder)
	{
		if (Order == aOrder) {
			return;
		}
		Order = aOrder;
		rebuild();
	}

	auto files_tree_model::order() const noexcept -> files_order
	{
		return Order;
	}

	void files_tree_model::set_filter(const QString& aFilter)
	{
		const auto trimmed = aFilter.trimmed();
		if (Filter == trimmed) {
			return;
		}
		Filter = trimmed;
		rebuild();
	}

	auto files_tree_model::filter() const noexcept -> const QString&
	{
		return Filter;
	}

	auto files_tree_model::build_nodes() const -> std::vector<files_tree_node>
	{
		auto built = std::vector<files_tree_node>{ };
		built.push_back(files_tree_node{ .Folder = true });
		// The files this order shows, in the order it shows them. [[BPR-0981]]:
		// the filter is RFC-BPR-0025's resident fuzzy match over the path,
		// which carries the file name; a shown file keeps its folders below.
		auto shown = std::vector<std::size_t>{ };
		for (std::size_t index = 0U; index < Entries.size(); ++index) {
			if (!Entries[index].Withheld && resident_filter_matches(Entries[index].Path, Filter)) {
				shown.push_back(index);
			}
		}
		const auto byPath = [this](const std::size_t aLeft, const std::size_t aRight) {
			return Entries[aLeft].Path.compare(Entries[aRight].Path, Qt::CaseInsensitive) < 0;
		};
		if (Order == files_order::tree) {
			// A trie of folders, then nesting-only folders compacted.
			std::ranges::sort(shown, byPath);
			auto folderOf = std::map<QString, std::size_t>{ };
			folderOf.emplace(QString{ }, root_node);
			const auto folder = [&built, &folderOf](const QString& aPrefix, auto& aSelf) -> std::size_t {
				if (const auto found = folderOf.find(aPrefix); found != folderOf.end()) {
					return found->second;
				}
				const auto slash  = aPrefix.lastIndexOf(QLatin1Char{ '/' });
				const auto parent = aSelf(slash < 0 ? QString{ } : aPrefix.left(slash), aSelf);
				built.push_back(files_tree_node{
					.Label  = slash < 0 ? aPrefix : aPrefix.mid(slash + 1),
					.Path   = aPrefix,
					.Folder = true,
					.Parent = parent,
				});
				const auto node = built.size() - 1U;
				built[parent].Children.push_back(node);
				folderOf.emplace(aPrefix, node);
				return node;
			};
			for (const auto index : shown) {
				const auto& path   = Entries[index].Path;
				const auto  slash  = path.lastIndexOf(QLatin1Char{ '/' });
				const auto  parent = slash < 0 ? root_node : folder(path.left(slash), folder);
				built.push_back(files_tree_node{
					.Label  = slash < 0 ? path : path.mid(slash + 1),
					.Path   = path,
					.Entry  = index,
					.Parent = parent,
				});
				built[parent].Children.push_back(built.size() - 1U);
			}
			// Compact: a folder whose only child is a folder takes that
			// child's name onto its own row and adopts its children.
			for (std::size_t node = 1U; node < built.size(); ++node) {
				while (built[node].Folder && built[node].Children.size() == 1U
					   && built[built[node].Children.front()].Folder) {
					const auto child     = built[node].Children.front();
					built[node].Label    = built[node].Label + QLatin1Char{ '/' } + built[child].Label;
					built[node].Path     = built[child].Path;
					built[node].Children = std::move(built[child].Children);
					for (const auto grandchild : built[node].Children) {
						built[grandchild].Parent = node;
					}
					built[child].Children.clear();
					built[child].Parent = node;
					built[child].Label.clear();
				}
			}
			// Orphaned compacted folders keep their slots (indices stay
			// stable) and are simply unreachable from the root.
			// Folders before files within each folder, each alphabetical.
			for (auto& node : built) {
				std::ranges::stable_sort(node.Children, [&built](const std::size_t aLeft, const std::size_t aRight) {
					if (built[aLeft].Folder != built[aRight].Folder) {
						return built[aLeft].Folder;
					}
					return built[aLeft].Label.compare(built[aRight].Label, Qt::CaseInsensitive) < 0;
				});
			}
		} else {
			std::ranges::sort(shown, byPath);
			if (Order == files_order::changed_first) {
				std::ranges::stable_sort(shown, [this](const std::size_t aLeft, const std::size_t aRight) {
					const auto left  = Entries[aLeft].Overview.has_value() && changed_since_review(*Entries[aLeft].Overview);
					const auto right = Entries[aRight].Overview.has_value() && changed_since_review(*Entries[aRight].Overview);
					return left && !right;
				});
			}
			for (const auto index : shown) {
				built.push_back(files_tree_node{
					.Label  = Entries[index].Path,
					.Path   = Entries[index].Path,
					.Entry  = index,
					.Parent = root_node,
				});
				built[root_node].Children.push_back(built.size() - 1U);
			}
		}
		// File counts, from the leaves up. built are created parents first,
		// so walking backwards adds each file to every reachable ancestor.
		for (std::size_t node = built.size(); node-- > 1U;) {
			if (!built[node].Folder && !built[node].Label.isEmpty()) {
				for (auto ancestor = built[node].Parent;; ancestor = built[ancestor].Parent) {
					++built[ancestor].FileCount;
					if (ancestor == root_node) {
						break;
					}
				}
			}
		}
		return built;
	}

	void files_tree_model::rebuild()
	{
		rebuild(Entries);
	}

	void files_tree_model::rebuild(const std::vector<files_tree_entry>& aPrevious)
	{
		auto       desired = build_nodes();
		const auto shown   = Nodes[root_node].Children.size();
		const auto wanted  = desired[root_node].Children.size();
		if (shown == 0U && wanted != 0U) {
			// First rows: one insertion of the whole built tree.
			beginInsertRows({ }, 0, static_cast<int>(wanted) - 1);
			Nodes = std::move(desired);
			FreeNodes.clear();
			for (std::size_t node = 1U; node < Nodes.size(); ++node) {
				if (Nodes[node].Folder && !Nodes[node].Label.isEmpty()) {
					InsertedFolders.push_back(node);
				}
			}
			rebuild_file_index();
			endInsertRows();
			WorkStatistics.RowsInserted += wanted;
			return;
		}
		if (wanted == 0U && shown != 0U) {
			beginRemoveRows({ }, 0, static_cast<int>(shown) - 1);
			Nodes.assign(1U, files_tree_node{ .Folder = true });
			FreeNodes.clear();
			rebuild_file_index();
			endRemoveRows();
			WorkStatistics.RowsRemoved += shown;
			return;
		}
		reconcile(root_node, desired, root_node, aPrevious);
		Nodes[root_node].FileCount = desired[root_node].FileCount;
		rebuild_file_index();
	}

	void files_tree_model::reconcile(
		const std::size_t                    aCurrent,
		const std::vector<files_tree_node>&  aDesired,
		const std::size_t                    aWanted,
		const std::vector<files_tree_entry>& aPrevious)
	{
		// A row is the same row when it is still a folder or file at the same
		// path under the same parent. Compaction changes a folder's path, so
		// a folder that splits or merges is replaced rather than renamed.
		const auto key = [](const files_tree_node& aNode) {
			return (aNode.Folder ? QStringLiteral("d:") : QStringLiteral("f:")) + aNode.Path;
		};
		const auto  parent   = index_of_node(aCurrent);
		const auto& children = aDesired[aWanted].Children;
		// Rows that no longer belong are removed first, so the rows after
		// them close up without being moved one by one.
		auto wanted = QSet<QString>{ };
		wanted.reserve(static_cast<qsizetype>(children.size()));
		for (const auto child : children) {
			wanted.insert(key(aDesired[child]));
		}
		for (auto row = Nodes[aCurrent].Children.size(); row-- > 0U;) {
			if (wanted.contains(key(Nodes[Nodes[aCurrent].Children[row]]))) {
				continue;
			}
			beginRemoveRows(parent, static_cast<int>(row), static_cast<int>(row));
			release(Nodes[aCurrent].Children[row]);
			Nodes[aCurrent].Children.erase(Nodes[aCurrent].Children.begin() + static_cast<std::ptrdiff_t>(row));
			renumber(aCurrent, row);
			endRemoveRows();
			++WorkStatistics.RowsRemoved;
		}
		auto retained = QHash<QString, std::size_t>{ };
		retained.reserve(static_cast<qsizetype>(Nodes[aCurrent].Children.size()));
		for (const auto child : Nodes[aCurrent].Children) {
			retained.insert(key(Nodes[child]), child);
		}
		for (std::size_t position = 0U; position < children.size(); ++position) {
			const auto& want  = aDesired[children[position]];
			const auto  found = retained.constFind(key(want));
			const auto  row   = static_cast<int>(position);
			if (found == retained.constEnd()) {
				beginInsertRows(parent, row, row);
				const auto node = adopt(aDesired, children[position], aCurrent);
				Nodes[aCurrent].Children.insert(Nodes[aCurrent].Children.begin() + static_cast<std::ptrdiff_t>(position), node);
				renumber(aCurrent, position);
				endInsertRows();
				++WorkStatistics.RowsInserted;
				continue;
			}
			const auto node = *found;
			retained.remove(found.key());
			// Every row before this one is settled, so a retained row is here
			// or after it.
			if (const auto from = Nodes[node].Row; from != row) {
				beginMoveRows(parent, from, from, parent, row);
				auto& siblings = Nodes[aCurrent].Children;
				std::rotate(siblings.begin() + row, siblings.begin() + from, siblings.begin() + from + 1);
				renumber(aCurrent, position);
				endMoveRows();
				++WorkStatistics.RowsMoved;
			}
			const auto& before    = Nodes[node].Entry ? aPrevious[*Nodes[node].Entry].Overview : std::nullopt;
			const auto& after     = want.Entry ? Entries[*want.Entry].Overview : std::nullopt;
			const auto  changed   = Nodes[node].Label != want.Label || Nodes[node].FileCount != want.FileCount || before != after;
			Nodes[node].Label     = want.Label;
			Nodes[node].FileCount = want.FileCount;
			Nodes[node].Entry     = want.Entry;
			if (changed) {
				const auto index = index_of_node(node);
				emit       dataChanged(index, index);
				++WorkStatistics.ChangedRows;
			}
			if (want.Folder) {
				reconcile(node, aDesired, children[position], aPrevious);
			}
		}
		// Only a repeated path can still be unclaimed, left at the end.
		const auto kept  = children.size();
		const auto shown = Nodes[aCurrent].Children.size();
		if (shown > kept) {
			beginRemoveRows(parent, static_cast<int>(kept), static_cast<int>(shown) - 1);
			for (auto row = kept; row < shown; ++row) {
				release(Nodes[aCurrent].Children[row]);
			}
			Nodes[aCurrent].Children.resize(kept);
			endRemoveRows();
			WorkStatistics.RowsRemoved += shown - kept;
		}
	}

	auto files_tree_model::adopt(const std::vector<files_tree_node>& aDesired, const std::size_t aWanted, const std::size_t aParent)
		-> std::size_t
	{
		auto node = std::size_t{ 0U };
		if (FreeNodes.empty()) {
			Nodes.emplace_back();
			node = Nodes.size() - 1U;
		} else {
			node = FreeNodes.back();
			FreeNodes.pop_back();
		}
		const auto& want = aDesired[aWanted];
		Nodes[node]      = files_tree_node{
			.Label     = want.Label,
			.Path      = want.Path,
			.Folder    = want.Folder,
			.FileCount = want.FileCount,
			.Entry     = want.Entry,
			.Parent    = aParent,
		};
		if (want.Folder) {
			InsertedFolders.push_back(node);
		}
		for (std::size_t row = 0U; row < aDesired[aWanted].Children.size(); ++row) {
			const auto child = adopt(aDesired, aDesired[aWanted].Children[row], node);
			Nodes[node].Children.push_back(child);
			Nodes[child].Row = static_cast<int>(row);
		}
		return node;
	}

	void files_tree_model::release(const std::size_t aNode)
	{
		for (const auto child : Nodes[aNode].Children) {
			release(child);
		}
		Nodes[aNode] = files_tree_node{ };
		FreeNodes.push_back(aNode);
	}

	void files_tree_model::renumber(const std::size_t aParent, const std::size_t aFrom)
	{
		const auto& children = Nodes[aParent].Children;
		for (auto row = aFrom; row < children.size(); ++row) {
			Nodes[children[row]].Row = static_cast<int>(row);
		}
	}

	auto files_tree_model::index_of_node(const std::size_t aNode) const -> QModelIndex
	{
		return aNode == root_node ? QModelIndex{ } : createIndex(Nodes[aNode].Row, 0, static_cast<quintptr>(aNode));
	}

	auto files_tree_model::take_inserted_folders() -> std::vector<QModelIndex>
	{
		auto folders = std::vector<QModelIndex>{ };
		folders.reserve(InsertedFolders.size());
		for (const auto node : std::exchange(InsertedFolders, { })) {
			if (node < Nodes.size() && Nodes[node].Folder) {
				folders.push_back(index_of_node(node));
			}
		}
		return folders;
	}

	auto files_tree_model::node_index(const QModelIndex& aIndex) const -> std::size_t
	{
		return aIndex.isValid() ? static_cast<std::size_t>(aIndex.internalId()) : root_node;
	}

	auto files_tree_model::index(const int aRow, const int aColumn, const QModelIndex& aParent) const -> QModelIndex
	{
		if (aRow < 0 || aColumn != 0) {
			return { };
		}
		const auto  parent   = node_index(aParent);
		const auto& children = Nodes[parent].Children;
		if (static_cast<std::size_t>(aRow) >= children.size()) {
			return { };
		}
		return createIndex(aRow, 0, static_cast<quintptr>(children[static_cast<std::size_t>(aRow)]));
	}

	auto files_tree_model::parent(const QModelIndex& aIndex) const -> QModelIndex
	{
		if (!aIndex.isValid()) {
			return { };
		}
		const auto node   = node_index(aIndex);
		const auto parent = Nodes[node].Parent;
		if (parent == root_node) {
			return { };
		}
		return createIndex(Nodes[parent].Row, 0, static_cast<quintptr>(parent));
	}

	auto files_tree_model::rowCount(const QModelIndex& aParent) const -> int
	{
		if (aParent.isValid() && aParent.column() != 0) {
			return 0;
		}
		return static_cast<int>(Nodes[node_index(aParent)].Children.size());
	}

	auto files_tree_model::columnCount(const QModelIndex& aParent) const -> int
	{
		static_cast<void>(aParent);
		return 1;
	}

	auto files_tree_model::node_at(const QModelIndex& aIndex) const -> const files_tree_node*
	{
		if (!aIndex.isValid() || aIndex.model() != this || aIndex.column() != 0) {
			return nullptr;
		}
		const auto node = node_index(aIndex);
		return node < Nodes.size() ? &Nodes[node] : nullptr;
	}

	auto files_tree_model::flags(const QModelIndex& aIndex) const -> Qt::ItemFlags
	{
		const auto* const node = node_at(aIndex);
		if (node == nullptr) {
			return Qt::NoItemFlags;
		}
		return node->Folder ? Qt::ItemIsEnabled : Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	auto files_tree_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		const auto* const node = node_at(aIndex);
		if (node == nullptr) {
			return { };
		}
		const auto* const overview = node->Entry.has_value() && Entries[*node->Entry].Overview.has_value()
			? &*Entries[*node->Entry].Overview
			: nullptr;
		switch (aRole) {
		case Qt::DisplayRole:
			return node->Label;
		case Qt::AccessibleTextRole:
			if (node->Folder) {
				return QStringLiteral("%1, %2 files").arg(node->Label).arg(node->FileCount);
			}
			return overview == nullptr ? node->Label
									   : QStringLiteral("%1, %2, %3 added lines, %4 removed lines")
											 .arg(node->Label, operation_description(overview->Operation))
											 .arg(overview->Additions)
											 .arg(overview->Deletions);
		case Qt::ToolTipRole:
		case path_role:
			return node->Path;
		case folder_role:
			return node->Folder;
		case file_count_role:
			return static_cast<qulonglong>(node->FileCount);
		case operation_role:
			return overview != nullptr ? operation_word(overview->Operation) : QString{ };
		case operation_kind_role:
			return overview != nullptr ? static_cast<int>(overview->Operation) : -1;
		case additions_role:
			return overview != nullptr ? static_cast<qulonglong>(overview->Additions) : qulonglong{ 0 };
		case deletions_role:
			return overview != nullptr ? static_cast<qulonglong>(overview->Deletions) : qulonglong{ 0 };
		case read_role:
			return overview != nullptr && overview->Read.has_value() ? static_cast<int>(*overview->Read) : -1;
		case progress_role:
			return overview != nullptr && overview->Mark.has_value() ? static_cast<int>(*overview->Mark) : -1;
		case unread_role:
			return overview != nullptr && overview->UnreadCount.has_value()
				? static_cast<int>(*overview->UnreadCount)
				: -1;
		case changed_role:
			return overview != nullptr && changed_since_review(*overview);
		case thread_count_role:
			return overview != nullptr ? static_cast<qulonglong>(overview->ThreadCount) : qulonglong{ 0 };
		default:
			return { };
		}
	}

	void files_tree_model::rebuild_file_index()
	{
		FileIndex.clear();
		FileIndex.reserve(Entries.size());
		for (auto& parent : Nodes) {
			for (std::size_t row = 0U; row < parent.Children.size(); ++row) {
				Nodes[parent.Children[row]].Row = static_cast<int>(row);
			}
		}
		FolderIndex.clear();
		for (std::size_t node = 1U; node < Nodes.size(); ++node) {
			if (Nodes[node].Entry) {
				FileIndex.emplace_back(Entries[*Nodes[node].Entry].File, node);
			} else if (Nodes[node].Folder && !Nodes[node].Label.isEmpty()) {
				FolderIndex.insert(Nodes[node].Path, node);
			}
		}
		std::ranges::sort(FileIndex);
	}

	auto files_tree_model::index_for_file(const core::workbench_file_id& aFile) const -> QModelIndex
	{
		const auto found = std::ranges::lower_bound(FileIndex, aFile, [this](const auto aLeft, const auto aRight) {
			++WorkStatistics.LookupComparisons;
			return aLeft < aRight; }, [](const auto& aEntry) { return aEntry.first; });
		if (found == FileIndex.end()) {
			return { };
		}
		++WorkStatistics.LookupComparisons;
		return found->first == aFile ? createIndex(Nodes[found->second].Row, 0, static_cast<quintptr>(found->second)) : QModelIndex{ };
	}

	auto files_tree_model::file_at(const QModelIndex& aIndex) const -> std::optional<core::workbench_file_id>
	{
		const auto* const node = node_at(aIndex);
		if (node == nullptr || node->Folder || !node->Entry.has_value()) {
			return std::nullopt;
		}
		return Entries[*node->Entry].File;
	}

	auto files_tree_model::file_count() const noexcept -> std::size_t
	{
		return Nodes[root_node].FileCount;
	}

	auto files_tree_model::nodes() const noexcept -> const std::vector<files_tree_node>&
	{
		return Nodes;
	}

	// ---- files_tree_delegate -------------------------------------------------

	files_tree_delegate::files_tree_delegate(QObject* const aParent)
		: QStyledItemDelegate{ aParent }
	{
	}

	void files_tree_delegate::set_accent(const QColor& aAccent)
	{
		Accent = aAccent;
	}

	void files_tree_delegate::set_operation_colours(const QColor& aAdded, const QColor& aRemoved)
	{
		Added   = aAdded;
		Removed = aRemoved;
	}

	void files_tree_delegate::set_progress_colours(const progress_mark_colours& aColours)
	{
		Progress = aColours;
	}

	auto files_tree_delegate::progress_colours() const noexcept -> const progress_mark_colours&
	{
		return Progress;
	}

	void files_tree_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto  folder  = aIndex.data(files_tree_model::folder_role).toBool();
		const auto  unread  = aIndex.data(files_tree_model::unread_role).toInt();
		const auto  mark    = aIndex.data(files_tree_model::progress_role).toInt();
		const auto& colours = option.palette;
		const auto  faint   = colours.color(QPalette::Active, QPalette::PlaceholderText);
		// The row's own text is drawn here so the counts can sit at the right.
		option.text.clear();
		const auto* const style = option.widget != nullptr ? option.widget->style() : nullptr;
		if (style != nullptr) {
			style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);
		} else {
			QStyledItemDelegate::paint(aPainter, option, aIndex);
		}
		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing, true);
		const auto selected      = (option.state & QStyle::State_Selected) != 0;
		const auto ink           = selected ? colours.color(QPalette::Active, QPalette::HighlightedText)
											: colours.color(QPalette::Active, QPalette::Text);
		const auto operationKind = aIndex.data(files_tree_model::operation_kind_role).toInt();
		auto       operationInk  = QColor{ };
		if (!folder && !selected) {
			if (operationKind == static_cast<int>(application::overview_file_operation::added)) {
				operationInk = Added;
			} else if (operationKind == static_cast<int>(application::overview_file_operation::removed)) {
				operationInk = Removed;
			}
		}
		auto rect = option.rect.adjusted(4, 0, -4, 0);
		// [[BPR-1331]] v003. Before a file, its mark in the review bar's
		// family, as Overview's read column draws it: one picture of a file's
		// progress wherever it shows. With the overview known and no receipt
		// the file is unread. The unread comments' accent dot ([[BPR-0656]])
		// moved from here to beside the counts, so the two never share a slot.
		const auto overviewKnown = operationKind >= 0;
		if (!folder) {
			if (overviewKnown) {
				const auto shown  = mark < 0 ? core::file_progress_mark::unread : static_cast<core::file_progress_mark>(mark);
				const auto centre = QPointF{ static_cast<qreal>(rect.left() + mark_centre), static_cast<qreal>(rect.center().y()) + 0.5 };
				paint_progress_mark(*aPainter, QRectF{ centre.x() - (mark_side / 2.0), centre.y() - (mark_side / 2.0), mark_side, mark_side }, shown, Progress);
			}
			rect.setLeft(rect.left() + name_inset);
		}
		auto small = option.font;
		small.setPointSizeF(small.pointSizeF() * 0.85);
		const auto smallMetrics = QFontMetrics{ small };
		const auto detailInk    = selected ? ink : faint;
		struct detail final
		{
			QString Text;
			QColor  Ink;
		};
		auto details = std::array<detail, 3>{ };
		if (folder) {
			// [[BPR-1530]]. An open folder shows its files beneath it, so only
			// a closed one draws its count; the accessible text keeps it.
			const auto* const tree = qobject_cast<const QTreeView*>(option.widget);
			if (tree == nullptr || !tree->isExpanded(aIndex)) {
				details[0] = { QString::number(aIndex.data(files_tree_model::file_count_role).toULongLong()), detailInk };
			}
		} else {
			const auto additions = aIndex.data(files_tree_model::additions_role).toULongLong();
			const auto deletions = aIndex.data(files_tree_model::deletions_role).toULongLong();
			details[0]           = { aIndex.data(files_tree_model::operation_role).toString(), operationInk.isValid() ? operationInk : detailInk };
			details[1]           = { additions == 0U ? QString{ } : QStringLiteral("+%1").arg(additions), !selected && Added.isValid() ? Added : detailInk };
			details[2]           = { deletions == 0U ? QString{ } : QStringLiteral("−%1").arg(deletions), !selected && Removed.isValid() ? Removed : detailInk };
		}
		auto detailLeft = rect.right() + 1;
		aPainter->setFont(small);
		for (const auto& part : std::views::reverse(details)) {
			if (part.Text.isEmpty()) {
				continue;
			}
			const auto width = smallMetrics.horizontalAdvance(part.Text);
			detailLeft -= width;
			aPainter->setPen(part.Ink);
			aPainter->drawText(QRect{ detailLeft, rect.top(), width, rect.height() }, Qt::AlignRight | Qt::AlignVCenter, part.Text);
			detailLeft -= 6;
		}
		if (!folder && unread > 0) {
			// [[BPR-0656]]. Unread comments: the accent dot, before the counts.
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(selected ? ink : Accent);
			aPainter->drawEllipse(QPointF{ static_cast<qreal>(detailLeft) - 2.0, static_cast<qreal>(rect.center().y()) + 0.5 }, 3.0, 3.0);
			detailLeft -= 11;
		}
		auto font = option.font;
		if (folder) {
			font.setWeight(QFont::DemiBold);
		}
		aPainter->setFont(font);
		// [[BPR-1529]]. A folder's name takes the ink Overview's folder rows
		// draw their paths in (`overview_row_delegate::paint_folder`), so
		// folders read apart from files the same way on both surfaces.
		const auto group = (option.state & QStyle::State_Active) != 0 ? QPalette::Active : QPalette::Inactive;
		aPainter->setPen(folder && !selected ? colours.color(group, QPalette::PlaceholderText) : ink);
		const auto label = QRect{ rect.left(), rect.top(), std::max(0, detailLeft - rect.left()), rect.height() };
		aPainter->drawText(label, Qt::AlignLeft | Qt::AlignVCenter,
						   QFontMetrics{ font }.elidedText(aIndex.data(Qt::DisplayRole).toString(), Qt::ElideMiddle, label.width()));
		aPainter->restore();
	}

	// ---- files_tree_view -----------------------------------------------------

	void files_tree_view::adopt_appearance(const appearance_profile& aProfile)
	{
		Delegate->set_accent(aProfile.color(core::appearance_role::state_unread));
		Delegate->set_operation_colours(aProfile.color(core::appearance_role::state_accepted),
										aProfile.color(core::appearance_role::state_defect));
		Delegate->set_progress_colours(progress_mark_colours_for(aProfile));
		viewport()->update();
	}

	files_tree_view::files_tree_view(QWidget* const aParent)
		: QTreeView{ aParent }
	{
		setObjectName(QStringLiteral("filesTree"));
		setAccessibleName(QStringLiteral("Files tree"));
		setAccessibleDescription(QStringLiteral(
			"The pull request's files as a tree: folders with their file counts, "
			"each file with its operation, line counts and read state; choose "
			"one to open it at its first change"));
		Model = new files_tree_model{ this };
		setModel(Model);
		Delegate = new files_tree_delegate{ this };
		setItemDelegate(Delegate);
		setHeaderHidden(true);
		setRootIsDecorated(true);
		setUniformRowHeights(true);
		setSelectionBehavior(QAbstractItemView::SelectRows);
		setSelectionMode(QAbstractItemView::SingleSelection);
		setEditTriggers(QAbstractItemView::NoEditTriggers);
		setIndentation(14);
		setFocusPolicy(Qt::StrongFocus);
		// [[BPR-1363]]. The one column fills the tree less a scroll bar's
		// width wherever scroll bars take space, so a bar that comes when
		// opened folders outgrow the tree moves no count at the right edge.
		new steady_columns{ *this, 0 };
	}

	auto files_tree_view::tree_model() const noexcept -> files_tree_model*
	{
		return Model;
	}

	auto files_tree_view::delegate() const noexcept -> files_tree_delegate*
	{
		return Delegate;
	}

	void files_tree_view::set_select_dispatch(files_tree_select_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void files_tree_view::set_new_tab_dispatch(files_tree_select_dispatch aDispatch)
	{
		NewTabDispatch = std::move(aDispatch);
	}

	auto files_tree_view::make_file_menu(const QModelIndex& aIndex, QWidget* const aParent) -> QMenu*
	{
		const auto file = Model->file_at(aIndex);
		if (!file.has_value()) {
			return nullptr;
		}
		auto* const menu   = new QMenu{ aParent };
		auto* const action = menu->addAction(QStringLiteral("Open in New Tab"));
		menu->setObjectName(QStringLiteral("filesTreeMenu"));
		action->setObjectName(QStringLiteral("filesTreeOpenInNewTab"));
		action->setEnabled(static_cast<bool>(NewTabDispatch));
		connect(action, &QAction::triggered, this, [this, chosen = *file] {
			if (NewTabDispatch) {
				NewTabDispatch(chosen);
			}
		});
		if (const auto* node = Model->node_at(aIndex); MenuExtension && node != nullptr && !node->Folder) {
			MenuExtension(*menu, node->Path);
		}
		return menu;
	}

	void files_tree_view::set_menu_extension(std::function<void(QMenu&, const QString&)> aExtension)
	{
		MenuExtension = std::move(aExtension);
	}

	void files_tree_view::mousePressEvent(QMouseEvent* const aEvent)
	{
		// [[BPR-1132]]. Cmd-click (Ctrl-click on Windows and Linux) opens the
		// file in a tab of its own and leaves the active tab's file chosen.
		if (aEvent->button() == Qt::LeftButton && (aEvent->modifiers() & Qt::ControlModifier) != 0 && NewTabDispatch) {
			if (const auto file = Model->file_at(indexAt(aEvent->position().toPoint())); file.has_value()) {
				NewTabDispatch(*file);
				aEvent->accept();
				return;
			}
		}
		// Closing the final document tab leaves the file cursor in place.
		// Clicking that same row is still an explicit request to open it;
		// currentChanged only covers a move to a different row.
		const auto repeated = aEvent->button() == Qt::LeftButton && aEvent->modifiers() == Qt::NoModifier
				&& indexAt(aEvent->position().toPoint()) == currentIndex()
			? Model->file_at(currentIndex())
			: std::nullopt;
		QTreeView::mousePressEvent(aEvent);
		if (repeated && !Applying && Dispatch) {
			Dispatch(*repeated);
		}
	}

	void files_tree_view::contextMenuEvent(QContextMenuEvent* const aEvent)
	{
		// [[BPR-1034]]. A heap menu that deletes itself on close, opened with
		// popup(), so the window can go while it is open.
		auto* const menu = make_file_menu(indexAt(aEvent->pos()), this);
		if (menu == nullptr) {
			QTreeView::contextMenuEvent(aEvent);
			return;
		}
		menu->setAttribute(Qt::WA_DeleteOnClose);
		menu->popup(aEvent->globalPos());
		aEvent->accept();
	}

	void files_tree_view::apply(const std::vector<workbench_drawer_row>& aRows)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->apply(aRows);
		expand_inserted();
		select_file(SelectedFile);
	}

	void files_tree_view::apply_overview(const std::span<const application::overview_file_row> aFiles)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->apply_overview(aFiles);
		expand_inserted();
	}

	void files_tree_view::apply_overview_rows(const std::span<const application::overview_file_row> aRows)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->apply_overview_rows(aRows);
		expand_inserted();
	}

	void files_tree_view::apply_delta(const std::span<const core::workbench_file_id> aRemoved,
									  const std::span<const workbench_drawer_row>    aInserted)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->apply_delta(aRemoved, aInserted);
		expand_inserted();
		select_file(SelectedFile);
	}

	void files_tree_view::expand_inserted()
	{
		for (const auto& folder : Model->take_inserted_folders()) {
			expand(folder);
		}
	}

	void files_tree_view::set_order(const files_order aOrder)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->set_order(aOrder);
		expand_inserted();
		select_file(SelectedFile);
	}

	void files_tree_view::set_filter(const QString& aFilter)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->set_filter(aFilter);
		expand_inserted();
		select_file(SelectedFile);
	}

	void files_tree_view::select_file(const std::optional<core::workbench_file_id>& aFile)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		SelectedFile        = aFile;
		const auto index    = aFile.has_value() ? Model->index_for_file(*aFile) : QModelIndex{ };
		setCurrentIndex(index);
		if (index.isValid()) {
			scrollTo(index);
		}
	}

	void files_tree_view::currentChanged(const QModelIndex& aCurrent, const QModelIndex& aPrevious)
	{
		QTreeView::currentChanged(aCurrent, aPrevious);
		if (Applying || !Dispatch) {
			return;
		}
		if (const auto file = Model->file_at(aCurrent); file.has_value()) {
			SelectedFile = file;
			Dispatch(*file);
		}
	}

	void files_tree_view::keyPressEvent(QKeyEvent* const aEvent)
	{
		const auto current = currentIndex();
		if (current.data(files_tree_model::folder_role).toBool() && aEvent->modifiers() == Qt::NoModifier) {
			switch (aEvent->key()) {
			case Qt::Key_Left:
				collapse(current);
				break;
			case Qt::Key_Right:
				expand(current);
				break;
			case Qt::Key_Return:
			case Qt::Key_Enter:
			case Qt::Key_Space:
				setExpanded(current, !isExpanded(current));
				break;
			default:
				QTreeView::keyPressEvent(aEvent);
				return;
			}
			aEvent->accept();
			return;
		}
		QTreeView::keyPressEvent(aEvent);
	}
} // namespace bpr::qt
