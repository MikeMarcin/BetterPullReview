#include "bpr/qt/github_conversation_model.hpp"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace bpr::qt
{
namespace
{
using conversation = application::github_conversation_projection;

[[nodiscard]] auto text(const std::string& value) -> QString
{
    return QString::fromUtf8(
        value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] auto review_state_text(
    const application::github_review_state_projection state) -> QString
{
    switch(state)
    {
    case application::github_review_state_projection::pending:
        return QStringLiteral("Pending review");
    case application::github_review_state_projection::commented:
        return QStringLiteral("Commented");
    case application::github_review_state_projection::approved:
        return QStringLiteral("Approved");
    case application::github_review_state_projection::changes_requested:
        return QStringLiteral("Changes requested");
    case application::github_review_state_projection::dismissed:
        return QStringLiteral("Dismissed review");
    case application::github_review_state_projection::none:
        return QStringLiteral("Review");
    }
    return QStringLiteral("Review");
}

[[nodiscard]] auto location_text(const conversation& value) -> QString
{
    auto result = text(value.Path);
    if(value.Line.has_value())
    {
        if(!result.isEmpty())
        {
            result.append(QLatin1Char(':'));
        }
        result.append(QString::number(*value.Line));
    }
    return result;
}

[[nodiscard]] auto author_text(const conversation& value) -> QString
{
    if(value.AuthorLogin.empty())
    {
        return QStringLiteral("Unknown author");
    }
    return QStringLiteral("@%1").arg(text(value.AuthorLogin));
}

[[nodiscard]] auto display_text(const conversation& value) -> QString
{
    QString heading;
    switch(value.Kind)
    {
    case application::github_conversation_kind::review:
        heading = QStringLiteral("%1 by %2")
                      .arg(review_state_text(value.ReviewState),
                           author_text(value));
        break;
    case application::github_conversation_kind::review_thread:
        heading = value.Resolved
            ? QStringLiteral("Resolved review thread")
            : QStringLiteral("Open review thread");
        if(const auto location = location_text(value); !location.isEmpty())
        {
            heading.append(QStringLiteral(" — %1").arg(location));
        }
        if(value.Outdated)
        {
            heading.append(QStringLiteral(" — Outdated"));
        }
        break;
    case application::github_conversation_kind::review_comment:
        heading = QStringLiteral("Review comment by %1").arg(author_text(value));
        break;
    case application::github_conversation_kind::issue_comment:
        heading =
            QStringLiteral("Pull request comment by %1").arg(author_text(value));
        break;
    }

    if(value.Deleted)
    {
        heading.append(QStringLiteral("\nDeleted comment"));
    }
    else if(!value.Body.empty())
    {
        heading.append(QLatin1Char('\n'));
        heading.append(text(value.Body));
    }

    if(value.Kind != application::github_conversation_kind::review_thread)
    {
        if(const auto location = location_text(value); !location.isEmpty())
        {
            heading.append(QStringLiteral("\n%1").arg(location));
        }
        if(value.Outdated)
        {
            heading.append(QStringLiteral("\nOutdated"));
        }
    }
    return heading;
}

[[nodiscard]] auto description_text(const conversation& value) -> QString
{
    QStringList facts;
    if(value.Resolved)
    {
        facts.push_back(QStringLiteral("Resolved"));
    }
    if(value.Outdated)
    {
        facts.push_back(QStringLiteral("Outdated"));
    }
    if(value.Deleted)
    {
        facts.push_back(QStringLiteral("Deleted"));
    }
    if(value.ParentId.has_value())
    {
        facts.push_back(QStringLiteral("Reply in a review thread"));
    }
    return facts.join(QStringLiteral(", "));
}

[[nodiscard]] auto changed_roles() -> const QList<int>&
{
    static const QList<int> roles{
        Qt::DisplayRole,
        Qt::AccessibleTextRole,
        Qt::AccessibleDescriptionRole,
        github_conversation_model::parent_identity_role,
        github_conversation_model::kind_role,
        github_conversation_model::author_login_role,
        github_conversation_model::body_role,
        github_conversation_model::path_role,
        github_conversation_model::line_role,
        github_conversation_model::review_state_role,
        github_conversation_model::resolved_role,
        github_conversation_model::outdated_role,
        github_conversation_model::deleted_role,
    };
    return roles;
}
}

struct github_conversation_model::node
{
    application::github_conversation_projection Value;
    node* Parent = nullptr;
    std::size_t Row = 0;
    std::vector<std::unique_ptr<node>> Children;
};

github_conversation_model::github_conversation_model(QObject* const parent)
    : QAbstractItemModel{parent}
{
}

github_conversation_model::~github_conversation_model() = default;

auto github_conversation_model::index(
    const int row,
    const int column,
    const QModelIndex& parent_index) const -> QModelIndex
{
    if(!hasIndex(row, column, parent_index) || column != 0)
    {
        return {};
    }

    const auto* const parent_node = parent_index.isValid()
        ? static_cast<const node*>(parent_index.internalPointer())
        : nullptr;
    const auto& values =
        parent_node == nullptr ? Roots : parent_node->Children;
    return createIndex(
        row, column, values[static_cast<std::size_t>(row)].get());
}

auto github_conversation_model::parent(const QModelIndex& child) const
    -> QModelIndex
{
    if(!child.isValid())
    {
        return {};
    }
    const auto* const value =
        static_cast<const node*>(child.internalPointer());
    return value == nullptr ? QModelIndex{} : index_for_node(value->Parent);
}

auto github_conversation_model::rowCount(
    const QModelIndex& parent_index) const -> int
{
    if(parent_index.isValid() && parent_index.column() != 0)
    {
        return 0;
    }
    const auto* const parent_node = parent_index.isValid()
        ? static_cast<const node*>(parent_index.internalPointer())
        : nullptr;
    const auto& values =
        parent_node == nullptr ? Roots : parent_node->Children;
    return static_cast<int>(values.size());
}

auto github_conversation_model::columnCount(
    const QModelIndex& parent_index) const -> int
{
    static_cast<void>(parent_index);
    return 1;
}

auto github_conversation_model::data(
    const QModelIndex& model_index,
    const int requested_role) const -> QVariant
{
    if(!model_index.isValid() || model_index.column() != 0)
    {
        return {};
    }
    const auto* const value =
        static_cast<const node*>(model_index.internalPointer());
    if(value == nullptr)
    {
        return {};
    }
    const auto& conversation = value->Value;

    switch(requested_role)
    {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        return display_text(conversation);
    case Qt::AccessibleDescriptionRole:
        return description_text(conversation);
    case identity_role:
        return text(conversation.Id);
    case parent_identity_role:
        return conversation.ParentId.has_value()
            ? QVariant{text(*conversation.ParentId)}
            : QVariant{};
    case kind_role:
        return static_cast<int>(conversation.Kind);
    case author_login_role:
        return text(conversation.AuthorLogin);
    case body_role:
        return conversation.Deleted ? QString{} : text(conversation.Body);
    case path_role:
        return text(conversation.Path);
    case line_role:
        return conversation.Line.has_value()
            ? QVariant::fromValue(static_cast<qulonglong>(*conversation.Line))
            : QVariant{};
    case review_state_role:
        return static_cast<int>(conversation.ReviewState);
    case resolved_role:
        return conversation.Resolved;
    case outdated_role:
        return conversation.Outdated;
    case deleted_role:
        return conversation.Deleted;
    default:
        return {};
    }
}

auto github_conversation_model::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int requested_role) const -> QVariant
{
    if(section == 0 && orientation == Qt::Horizontal
       && requested_role == Qt::DisplayRole)
    {
        return QStringLiteral("Conversations");
    }
    return {};
}

auto github_conversation_model::flags(const QModelIndex& model_index) const
    -> Qt::ItemFlags
{
    return model_index.isValid()
        ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
        : Qt::NoItemFlags;
}

auto github_conversation_model::roleNames() const -> QHash<int, QByteArray>
{
    auto roles = QAbstractItemModel::roleNames();
    roles.insert(identity_role, QByteArrayLiteral("identity"));
    roles.insert(parent_identity_role, QByteArrayLiteral("parentIdentity"));
    roles.insert(kind_role, QByteArrayLiteral("kind"));
    roles.insert(author_login_role, QByteArrayLiteral("authorLogin"));
    roles.insert(body_role, QByteArrayLiteral("body"));
    roles.insert(path_role, QByteArrayLiteral("path"));
    roles.insert(line_role, QByteArrayLiteral("line"));
    roles.insert(review_state_role, QByteArrayLiteral("reviewState"));
    roles.insert(resolved_role, QByteArrayLiteral("resolved"));
    roles.insert(outdated_role, QByteArrayLiteral("outdated"));
    roles.insert(deleted_role, QByteArrayLiteral("deleted"));
    return roles;
}

void github_conversation_model::apply(
    const std::vector<application::github_conversation_projection>&
        conversations)
{
    const auto& roles = changed_roles();
    std::unordered_map<std::string, const conversation*> desired_by_identity;
    desired_by_identity.reserve(conversations.size());
    for(const auto& value : conversations)
    {
        if(!value.Id.empty())
        {
            desired_by_identity.try_emplace(value.Id, &value);
        }
    }

    std::unordered_map<std::string, std::optional<std::string>> desired_parent;
    desired_parent.reserve(desired_by_identity.size());
    for(const auto& [identity, value] : desired_by_identity)
    {
        std::optional<std::string> parent_identity;
        if(
            value->Kind == application::github_conversation_kind::review_comment
            && value->ParentId.has_value() && *value->ParentId != identity)
        {
            if(const auto parent = desired_by_identity.find(*value->ParentId);
               parent != desired_by_identity.end()
               && parent->second->Kind
                      == application::github_conversation_kind::review_thread)
            {
                parent_identity = *value->ParentId;
            }
        }
        desired_parent.emplace(identity, std::move(parent_identity));
    }

    std::vector<const conversation*> desired_roots;
    std::unordered_map<std::string, std::vector<const conversation*>>
        desired_children;
    desired_roots.reserve(desired_by_identity.size());
    desired_children.reserve(desired_by_identity.size());
    std::unordered_set<std::string> ordered;
    ordered.reserve(desired_by_identity.size());
    for(const auto& value : conversations)
    {
        const auto selected = desired_by_identity.find(value.Id);
        if(
            selected == desired_by_identity.end()
            || selected->second != &value || !ordered.insert(value.Id).second)
        {
            continue;
        }
        const auto& parent_identity = desired_parent.at(value.Id);
        if(parent_identity.has_value())
        {
            desired_children[*parent_identity].push_back(&value);
        }
        else
        {
            desired_roots.push_back(&value);
        }
    }

    std::function<void(
        std::vector<std::unique_ptr<node>>&,
        const QModelIndex&,
        const std::optional<std::string>&)>
        remove_obsolete;
    remove_obsolete =
        [this, &desired_parent, &remove_obsolete](
            std::vector<std::unique_ptr<node>>& values,
            const QModelIndex& parent_index,
            const std::optional<std::string>& actual_parent)
    {
        for(std::size_t offset = values.size(); offset != 0U; --offset)
        {
            const auto row = offset - 1U;
            auto* const value = values[row].get();
            const auto desired = desired_parent.find(value->Value.Id);
            if(
                desired == desired_parent.end()
                || desired->second != actual_parent)
            {
                beginRemoveRows(
                    parent_index,
                    static_cast<int>(row),
                    static_cast<int>(row));
                values.erase(
                    values.begin() + static_cast<std::ptrdiff_t>(row));
                update_rows(values, row);
                endRemoveRows();
                ++Statistics.RowsRemoved;
                continue;
            }
            remove_obsolete(
                value->Children,
                index_for_node(value),
                std::optional<std::string>{value->Value.Id});
        }
    };
    remove_obsolete(Roots, {}, std::nullopt);

    static const std::vector<const conversation*> no_children;
    std::function<void(
        std::vector<std::unique_ptr<node>>&,
        node*,
        const QModelIndex&,
        const std::vector<const conversation*>&)>
        synchronize;
    synchronize =
        [this, &desired_children, &roles, &synchronize](
            std::vector<std::unique_ptr<node>>& values,
            node* const parent_node,
            const QModelIndex& parent_index,
            const std::vector<const conversation*>& desired)
    {
        values.reserve(desired.size());

        std::unordered_map<node*, std::size_t> desired_existing_positions;
        desired_existing_positions.reserve(values.size());
        std::unordered_map<std::string_view, node*> existing_by_identity;
        existing_by_identity.reserve(values.size());
        for(const auto& value : values)
        {
            existing_by_identity.emplace(value->Value.Id, value.get());
        }

        std::vector<node*> desired_existing;
        desired_existing.reserve(values.size());
        for(const auto* const desired_value : desired)
        {
            const auto existing =
                existing_by_identity.find(desired_value->Id);
            if(existing != existing_by_identity.end())
            {
                desired_existing_positions.emplace(
                    existing->second, desired_existing.size());
                desired_existing.push_back(existing->second);
            }
        }

        std::vector<std::size_t> current_ranks;
        current_ranks.reserve(values.size());
        for(const auto& value : values)
        {
            current_ranks.push_back(
                desired_existing_positions.at(value.get()));
        }

        std::vector<std::size_t> tail_ranks;
        std::vector<std::size_t> tail_indices;
        std::vector<std::size_t> predecessors(
            current_ranks.size(), current_ranks.size());
        tail_ranks.reserve(current_ranks.size());
        tail_indices.reserve(current_ranks.size());
        for(std::size_t index = 0; index < current_ranks.size(); ++index)
        {
            const auto insertion = std::lower_bound(
                tail_ranks.begin(), tail_ranks.end(), current_ranks[index]);
            const auto length = static_cast<std::size_t>(
                std::distance(tail_ranks.begin(), insertion));
            if(length == tail_ranks.size())
            {
                tail_ranks.push_back(current_ranks[index]);
                tail_indices.push_back(index);
            }
            else
            {
                *insertion = current_ranks[index];
                tail_indices[length] = index;
            }
            if(length != 0U)
            {
                predecessors[index] = tail_indices[length - 1U];
            }
        }

        std::vector<bool> retained(desired_existing.size(), false);
        if(!tail_indices.empty())
        {
            auto index = tail_indices.back();
            while(index != current_ranks.size())
            {
                retained[current_ranks[index]] = true;
                index = predecessors[index];
            }
        }

        node* anchor = nullptr;
        for(auto offset = desired_existing.size(); offset != 0U; --offset)
        {
            const auto desired_row = offset - 1U;
            auto* const value = desired_existing[desired_row];
            if(retained[desired_row])
            {
                anchor = value;
                continue;
            }

            const auto source_row = value->Row;
            const auto destination_child =
                anchor == nullptr ? values.size() : anchor->Row;
            const auto insertion_row = source_row < destination_child
                ? destination_child - 1U
                : destination_child;
            if(source_row != insertion_row)
            {
                if(!beginMoveRows(
                       parent_index,
                       static_cast<int>(source_row),
                       static_cast<int>(source_row),
                       parent_index,
                       static_cast<int>(destination_child)))
                {
                    return;
                }
                auto moved = std::move(values[source_row]);
                values.erase(
                    values.begin()
                    + static_cast<std::ptrdiff_t>(source_row));
                values.insert(
                    values.begin()
                        + static_cast<std::ptrdiff_t>(insertion_row),
                    std::move(moved));
                update_rows(values, std::min(source_row, insertion_row));
                endMoveRows();
                ++Statistics.RowsMoved;
            }
            anchor = value;
        }

        for(std::size_t row = 0; row < desired.size(); ++row)
        {
            if(
                row < values.size()
                && values[row]->Value.Id == desired[row]->Id)
            {
                continue;
            }

            auto inserted = std::make_unique<node>();
            inserted->Value = *desired[row];
            inserted->Parent = parent_node;
            inserted->Row = row;
            if(const auto children =
                   desired_children.find(inserted->Value.Id);
               children != desired_children.end())
            {
                inserted->Children.reserve(children->second.size());
            }
            beginInsertRows(
                parent_index,
                static_cast<int>(row),
                static_cast<int>(row));
            values.insert(
                values.begin() + static_cast<std::ptrdiff_t>(row),
                std::move(inserted));
            update_rows(values, row);
            endInsertRows();
            ++Statistics.RowsInserted;
        }

        for(std::size_t row = 0; row < desired.size(); ++row)
        {
            auto* const current = values[row].get();
            if(current->Value != *desired[row])
            {
                auto replacement = *desired[row];
                current->Value = std::move(replacement);
                const auto changed_index = index_for_node(current);
                emit dataChanged(changed_index, changed_index, roles);
                ++Statistics.RowsChanged;
            }

            const auto children =
                desired_children.find(current->Value.Id);
            synchronize(
                current->Children,
                current,
                index_for_node(current),
                children == desired_children.end()
                    ? no_children
                    : children->second);
        }
    };
    synchronize(Roots, nullptr, {}, desired_roots);
}

void github_conversation_model::clear()
{
    apply({});
}

auto github_conversation_model::index_for_id(
    const std::string_view identity) const -> QModelIndex
{
    return index_for_node(find_node(Roots, identity));
}

auto github_conversation_model::id_for_index(
    const QModelIndex& model_index) const -> std::string
{
    if(!model_index.isValid())
    {
        return {};
    }
    const auto* const value =
        static_cast<const node*>(model_index.internalPointer());
    return value == nullptr ? std::string{} : value->Value.Id;
}

auto github_conversation_model::statistics() const noexcept
    -> github_conversation_model_statistics
{
    return Statistics;
}

auto github_conversation_model::index_for_node(const node* const value) const
    -> QModelIndex
{
    return value == nullptr
        ? QModelIndex{}
        : createIndex(static_cast<int>(value->Row), 0, value);
}

auto github_conversation_model::find_node(
    const std::vector<std::unique_ptr<node>>& values,
    const std::string_view identity) -> const node*
{
    for(const auto& value : values)
    {
        if(value->Value.Id == identity)
        {
            return value.get();
        }
        if(const auto* const child = find_node(value->Children, identity);
           child != nullptr)
        {
            return child;
        }
    }
    return nullptr;
}

void github_conversation_model::update_rows(
    std::vector<std::unique_ptr<node>>& values,
    const std::size_t first) noexcept
{
    for(auto row = first; row < values.size(); ++row)
    {
        values[row]->Row = row;
    }
}
}
