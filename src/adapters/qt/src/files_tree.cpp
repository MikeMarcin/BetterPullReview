#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/resident_filter.hpp"

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

	void files_tree_model::apply(const std::vector<workbench_drawer_row>& aRows)
	{
		auto entries = std::vector<files_tree_entry>{ };
		entries.reserve(aRows.size());
		for (const auto& row : aRows) {
			if (!row.File.has_value()) {
				continue;
			}
			auto entry = files_tree_entry{ .Path = row.Title, .File = *row.File };
			// The overview's facts survive a re-listing of the same path.
			const auto known = std::ranges::find(Entries, row.Title, &files_tree_entry::Path);
			if (known != Entries.end()) {
				entry.Overview = known->Overview;
			}
			entries.push_back(std::move(entry));
		}
		Entries = std::move(entries);
		rebuild();
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
		for (auto& entry : Entries) {
			const auto found = rows.constFind(entry.Path);
			entry.Overview   = found != rows.constEnd() ? std::optional{ **found } : std::nullopt;
		}
		rebuild();
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

	void files_tree_model::rebuild()
	{
		beginResetModel();
		Nodes.clear();
		Nodes.push_back(files_tree_node{ .Folder = true });
		// The files this order shows, in the order it shows them. [[BPR-0981]]:
		// the filter is RFC-BPR-0025's resident fuzzy match over the path,
		// which carries the file name; a shown file keeps its folders below.
		auto shown = std::vector<std::size_t>{ };
		for (std::size_t index = 0U; index < Entries.size(); ++index) {
			if (resident_filter_matches(Entries[index].Path, Filter)) {
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
			const auto folder = [this, &folderOf](const QString& aPrefix, auto& aSelf) -> std::size_t {
				if (const auto found = folderOf.find(aPrefix); found != folderOf.end()) {
					return found->second;
				}
				const auto slash  = aPrefix.lastIndexOf(QLatin1Char{ '/' });
				const auto parent = aSelf(slash < 0 ? QString{ } : aPrefix.left(slash), aSelf);
				Nodes.push_back(files_tree_node{
					.Label  = slash < 0 ? aPrefix : aPrefix.mid(slash + 1),
					.Path   = aPrefix,
					.Folder = true,
					.Parent = parent,
				});
				const auto node = Nodes.size() - 1U;
				Nodes[parent].Children.push_back(node);
				folderOf.emplace(aPrefix, node);
				return node;
			};
			for (const auto index : shown) {
				const auto& path   = Entries[index].Path;
				const auto  slash  = path.lastIndexOf(QLatin1Char{ '/' });
				const auto  parent = slash < 0 ? root_node : folder(path.left(slash), folder);
				Nodes.push_back(files_tree_node{
					.Label  = slash < 0 ? path : path.mid(slash + 1),
					.Path   = path,
					.Entry  = index,
					.Parent = parent,
				});
				Nodes[parent].Children.push_back(Nodes.size() - 1U);
			}
			// Compact: a folder whose only child is a folder takes that
			// child's name onto its own row and adopts its children.
			for (std::size_t node = 1U; node < Nodes.size(); ++node) {
				while (Nodes[node].Folder && Nodes[node].Children.size() == 1U
					   && Nodes[Nodes[node].Children.front()].Folder) {
					const auto child     = Nodes[node].Children.front();
					Nodes[node].Label    = Nodes[node].Label + QLatin1Char{ '/' } + Nodes[child].Label;
					Nodes[node].Path     = Nodes[child].Path;
					Nodes[node].Children = std::move(Nodes[child].Children);
					for (const auto grandchild : Nodes[node].Children) {
						Nodes[grandchild].Parent = node;
					}
					Nodes[child].Children.clear();
					Nodes[child].Parent = node;
					Nodes[child].Label.clear();
				}
			}
			// Orphaned compacted folders keep their slots (indices stay
			// stable) and are simply unreachable from the root.
			// Folders before files within each folder, each alphabetical.
			for (auto& node : Nodes) {
				std::ranges::stable_sort(node.Children, [this](const std::size_t aLeft, const std::size_t aRight) {
					if (Nodes[aLeft].Folder != Nodes[aRight].Folder) {
						return Nodes[aLeft].Folder;
					}
					return Nodes[aLeft].Label.compare(Nodes[aRight].Label, Qt::CaseInsensitive) < 0;
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
				Nodes.push_back(files_tree_node{
					.Label  = Entries[index].Path,
					.Path   = Entries[index].Path,
					.Entry  = index,
					.Parent = root_node,
				});
				Nodes[root_node].Children.push_back(Nodes.size() - 1U);
			}
		}
		// File counts, from the leaves up. Nodes are created parents first,
		// so walking backwards adds each file to every reachable ancestor.
		for (std::size_t node = Nodes.size(); node-- > 1U;) {
			if (!Nodes[node].Folder && !Nodes[node].Label.isEmpty()) {
				for (auto ancestor = Nodes[node].Parent;; ancestor = Nodes[ancestor].Parent) {
					++Nodes[ancestor].FileCount;
					if (ancestor == root_node) {
						break;
					}
				}
			}
		}
		endResetModel();
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
		const auto& siblings = Nodes[Nodes[parent].Parent].Children;
		const auto  row      = std::ranges::find(siblings, parent) - siblings.begin();
		return createIndex(static_cast<int>(row), 0, static_cast<quintptr>(parent));
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

	auto files_tree_model::index_for_file(const core::workbench_file_id& aFile) const -> QModelIndex
	{
		for (std::size_t node = 1U; node < Nodes.size(); ++node) {
			if (Nodes[node].Entry.has_value() && Entries[*Nodes[node].Entry].File == aFile) {
				const auto& siblings = Nodes[Nodes[node].Parent].Children;
				const auto  row      = std::ranges::find(siblings, node) - siblings.begin();
				return createIndex(static_cast<int>(row), 0, static_cast<quintptr>(node));
			}
		}
		return { };
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

	void files_tree_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto  folder  = aIndex.data(files_tree_model::folder_role).toBool();
		const auto  unread  = aIndex.data(files_tree_model::unread_role).toInt();
		const auto  read    = aIndex.data(files_tree_model::read_role).toInt();
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
		// [[BPR-0656]] [[BPR-0658]]. The read-state dot before a file: the
		// accent while comments are unread, a faint ring while the file itself
		// is unread, nothing once read.
		if (!folder) {
			const auto centre = QPointF{ static_cast<qreal>(rect.left()) + 4.0, static_cast<qreal>(rect.center().y()) + 0.5 };
			if (unread > 0) {
				aPainter->setPen(Qt::NoPen);
				aPainter->setBrush(Accent);
				aPainter->drawEllipse(centre, 3.0, 3.0);
			} else if (read == static_cast<int>(core::file_read_state::unread)) {
				aPainter->setPen(QPen{ faint, 1.0 });
				aPainter->setBrush(Qt::NoBrush);
				aPainter->drawEllipse(centre, 3.0, 3.0);
			}
			rect.setLeft(rect.left() + 12);
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
			details[0] = { QString::number(aIndex.data(files_tree_model::file_count_role).toULongLong()), detailInk };
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
		auto font = option.font;
		if (folder) {
			font.setWeight(QFont::DemiBold);
		}
		aPainter->setFont(font);
		aPainter->setPen(ink);
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
		header()->setStretchLastSection(true);
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
		return menu;
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
		expandAll();
		select_file(SelectedFile);
	}

	void files_tree_view::apply_overview(const std::span<const application::overview_file_row> aFiles)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->apply_overview(aFiles);
		expandAll();
		select_file(SelectedFile);
	}

	void files_tree_view::set_order(const files_order aOrder)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->set_order(aOrder);
		expandAll();
		select_file(SelectedFile);
	}

	void files_tree_view::set_filter(const QString& aFilter)
	{
		const auto applying = QScopedValueRollback{ Applying, true };
		Model->set_filter(aFilter);
		expandAll();
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
