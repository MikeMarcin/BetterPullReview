#include "bpr/qt/repositories_drawer.hpp"

#include <QFont>
#include <QHeaderView>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <algorithm>
#include <cstddef>
#include <optional>
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
			break;
		}
		return false;
	}

	// ---- repositories_tree_model --------------------------------------------

	repositories_tree_model::repositories_tree_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
	}

	void repositories_tree_model::apply(const std::span<const core::queue_item> aItems)
	{
		beginResetModel();
		Repositories.clear();
		// Home's order, repositories in the order they first appear in it.
		const auto ordered = core::ordered_queue(aItems);
		for (const auto& item : ordered) {
			auto found = std::ranges::find_if(Repositories, [&item](const repository_node& aNode) {
				return aNode.Host == item.Ref.Host && aNode.Account == item.Ref.Account
					&& aNode.Repository == item.Ref.Repository;
			});
			if (found == Repositories.end()) {
				Repositories.push_back(repository_node{
					.Host       = item.Ref.Host,
					.Account    = item.Ref.Account,
					.Repository = item.Ref.Repository,
				});
				found = std::prev(Repositories.end());
			}
			found->Items.push_back(item);
			if (repository_needs_me(item.Bucket)) {
				++found->NeedsMe;
			}
		}
		endResetModel();
	}

	void repositories_tree_model::clear()
	{
		apply({ });
	}

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
		return createIndex(aRow, aColumn, static_cast<quintptr>(repository) + 1U);
	}

	auto repositories_tree_model::parent(const QModelIndex& aIndex) const -> QModelIndex
	{
		if (!aIndex.isValid() || aIndex.internalId() == repository_id) {
			return { };
		}
		return createIndex(static_cast<int>(aIndex.internalId() - 1U), 0, repository_id);
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
		const auto repository = static_cast<std::size_t>(aIndex.internalId() - 1U);
		if (repository >= Repositories.size()) {
			return std::nullopt;
		}
		const auto& items = Repositories[repository].Items;
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
			case Qt::DisplayRole:
			case Qt::AccessibleTextRole:
				if (aIndex.column() == name_column) {
					return text(node.Repository);
				}
				return node.NeedsMe == 0U
					? QString::number(node.Items.size())
					: QStringLiteral("%1 of %2").arg(node.NeedsMe).arg(node.Items.size());
			case Qt::ToolTipRole:
				return QStringLiteral("%1 on %2 as %3: %4 of %5 need you")
					.arg(text(node.Repository), text(node.Host), text(node.Account))
					.arg(node.NeedsMe)
					.arg(node.Items.size());
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
		for (std::size_t repository = 0U; repository < Repositories.size(); ++repository) {
			const auto& items = Repositories[repository].Items;
			for (std::size_t row = 0U; row < items.size(); ++row) {
				if (items[row].Ref == aRef) {
					return createIndex(static_cast<int>(row), name_column, static_cast<quintptr>(repository) + 1U);
				}
			}
		}
		return { };
	}

	auto repositories_tree_model::needs_me_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& node : Repositories) {
			count += node.NeedsMe;
		}
		return count;
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

	void repositories_tree_view::apply(const std::span<const core::queue_item> aItems)
	{
		Model->apply(aItems);
		expandAll();
	}

	void repositories_tree_view::currentChanged(const QModelIndex& aCurrent, const QModelIndex& aPrevious)
	{
		QTreeView::currentChanged(aCurrent, aPrevious);
		// A pull request row chosen by the reviewer opens it; a repository
		// row is a heading and reports nothing.
		if (const auto item = Model->item_at(aCurrent); item.has_value() && OpenDispatch) {
			OpenDispatch(*item);
		}
	}
} // namespace bpr::qt
