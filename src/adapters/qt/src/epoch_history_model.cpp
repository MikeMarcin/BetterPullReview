#include "bpr/qt/epoch_history_model.hpp"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace bpr::qt
{
namespace
{
[[nodiscard]] auto oid_text(const core::git_commit_oid& oid) -> QString
{
    const auto canonical = core::canonical_hex(oid);
    const auto view = canonical.view();
    return QString::fromLatin1(
        view.data(), static_cast<qsizetype>(view.size()));
}

[[nodiscard]] auto availability_text(
    const core::anchor_availability availability) -> QString
{
    switch(availability)
    {
    case core::anchor_availability::available:
        return QStringLiteral("Available");
    case core::anchor_availability::unavailable:
        return QStringLiteral("Unavailable");
    case core::anchor_availability::pruned:
        return QStringLiteral("Pruned");
    case core::anchor_availability::cache_deleted:
        return QStringLiteral("Cache deleted");
    case core::anchor_availability::corrupt:
        return QStringLiteral("Corrupt");
    case core::anchor_availability::never_observed:
        return QStringLiteral("Never observed");
    }
    return QStringLiteral("Unavailable");
}

[[nodiscard]] auto primary_cause_text(
    const core::epoch_primary_cause cause) -> QString
{
    switch(cause)
    {
    case core::epoch_primary_cause::initial_observation:
        return QStringLiteral("First observation");
    case core::epoch_primary_cause::explicit_historical_import:
        return QStringLiteral("Historical import");
    case core::epoch_primary_cause::base_branch_change:
        return QStringLiteral("Base branch change");
    case core::epoch_primary_cause::force_push_rebase:
        return QStringLiteral("Force-push/rebase");
    case core::epoch_primary_cause::author_upload:
        return QStringLiteral("Author upload");
    case core::epoch_primary_cause::head_change_lineage_unavailable:
        return QStringLiteral("Head change; lineage unavailable");
    case core::epoch_primary_cause::base_update:
        return QStringLiteral("Base update");
    case core::epoch_primary_cause::retention_rehydration:
        return QStringLiteral("Retention rehydration");
    }
    return QStringLiteral("Unknown change");
}

[[nodiscard]] auto base_transition_text(
    const core::epoch_base_transition transition) -> QString
{
    switch(transition)
    {
    case core::epoch_base_transition::not_applicable:
        return QStringLiteral("Base: first observation");
    case core::epoch_base_transition::unchanged:
        return QStringLiteral("Base unchanged");
    case core::epoch_base_transition::base_oid_changed:
        return QStringLiteral("Base OID changed");
    case core::epoch_base_transition::base_branch_changed:
        return QStringLiteral("Base branch changed");
    }
    return QStringLiteral("Base unavailable");
}

[[nodiscard]] auto head_transition_text(
    const core::epoch_head_transition transition) -> QString
{
    switch(transition)
    {
    case core::epoch_head_transition::not_applicable:
        return QStringLiteral("Head: first observation");
    case core::epoch_head_transition::unchanged:
        return QStringLiteral("Head unchanged");
    case core::epoch_head_transition::descendant:
        return QStringLiteral("Head descendant");
    case core::epoch_head_transition::non_fast_forward:
        return QStringLiteral("Head rewritten");
    case core::epoch_head_transition::lineage_unknown:
        return QStringLiteral("Head lineage unavailable");
    }
    return QStringLiteral("Head unavailable");
}

[[nodiscard]] auto anchor_kind_text(
    const core::comparison_anchor_kind kind) -> QString
{
    switch(kind)
    {
    case core::comparison_anchor_kind::epoch_base:
        return QStringLiteral("Base");
    case core::comparison_anchor_kind::epoch_head:
        return QStringLiteral("Head");
    case core::comparison_anchor_kind::review_commit:
        return QStringLiteral("Review commit");
    }
    return QStringLiteral("Commit");
}

[[nodiscard]] auto anchor_kind_for_row(const int row) noexcept
    -> core::comparison_anchor_kind
{
    return row % 2 == 0 ? core::comparison_anchor_kind::epoch_base
                        : core::comparison_anchor_kind::epoch_head;
}

[[nodiscard]] auto display_text(
    const application::epoch_row_projection& row,
    const core::comparison_anchor_kind kind) -> QString
{
    const auto availability = kind == core::comparison_anchor_kind::epoch_base
        ? row.BaseAvailability
        : row.HeadAvailability;
    return QStringLiteral("Epoch %1 · %2 — %3 — %4")
        .arg(row.Id.Ordinal.value())
        .arg(anchor_kind_text(kind),
            primary_cause_text(row.Cause.Primary),
            availability_text(availability));
}

[[nodiscard]] auto description_text(
    const application::epoch_row_projection& row,
    const core::comparison_anchor_kind kind) -> QString
{
    const auto& oid = kind == core::comparison_anchor_kind::epoch_base
        ? row.Base
        : row.Head;
    return QStringLiteral("%1. %2. Exact %3 OID %4.")
        .arg(base_transition_text(row.Cause.Base),
            head_transition_text(row.Cause.Head),
            anchor_kind_text(kind).toLower(),
            oid_text(oid));
}

[[nodiscard]] auto identity_text(
    const core::comparison_anchor_identity& identity) -> QString
{
    static constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    QString result;
    result.reserve(64 + 1 + 64 + 1 + 20);
    const auto append_bytes = [&](const auto& bytes)
    {
        for(const auto byte : bytes)
        {
            result.append(QLatin1Char(digits[byte >> 4U]));
            result.append(QLatin1Char(digits[byte & 0x0fU]));
        }
    };
    append_bytes(identity.Authority.bytes());
    result.append(QLatin1Char(':'));
    append_bytes(identity.Incarnation.bytes());
    result.append(QStringLiteral(":%1:%2")
                      .arg(static_cast<int>(identity.Kind))
                      .arg(identity.Epoch.value()));
    return result;
}

[[nodiscard]] auto changed_roles() -> const QList<int>&
{
    static const QList<int> roles{
        Qt::DisplayRole,
        Qt::ToolTipRole,
        Qt::AccessibleTextRole,
        Qt::AccessibleDescriptionRole,
        epoch_history_model::identity_role,
        epoch_history_model::epoch_ordinal_role,
        epoch_history_model::anchor_kind_role,
        epoch_history_model::oid_role,
        epoch_history_model::availability_role,
        epoch_history_model::primary_cause_role,
        epoch_history_model::base_transition_role,
        epoch_history_model::head_transition_role,
    };
    return roles;
}

[[nodiscard]] auto same_incarnation(
    const core::epoch_catalog_identity& left,
    const core::epoch_catalog_identity& right) noexcept -> bool
{
    return left.Incarnation == right.Incarnation;
}
}

epoch_history_model::epoch_history_model(QObject* const parent)
    : QAbstractListModel{parent}
{
}

auto epoch_history_model::rowCount(const QModelIndex& parent) const -> int
{
    if(parent.isValid()
        || Rows.size()
            > static_cast<std::size_t>(std::numeric_limits<int>::max() / 2))
    {
        return 0;
    }
    return static_cast<int>(Rows.size() * 2U);
}

auto epoch_history_model::data(
    const QModelIndex& index,
    const int requested_role) const -> QVariant
{
    if(!index.isValid() || index.column() != 0 || index.row() < 0)
    {
        return {};
    }
    const auto epoch_index = static_cast<std::size_t>(index.row()) / 2U;
    if(epoch_index >= Rows.size())
    {
        return {};
    }
    const auto& row = Rows[epoch_index];
    const auto kind = anchor_kind_for_row(index.row());
    const auto& oid = kind == core::comparison_anchor_kind::epoch_base
        ? row.Base
        : row.Head;
    const auto availability = kind == core::comparison_anchor_kind::epoch_base
        ? row.BaseAvailability
        : row.HeadAvailability;
    const auto identity = core::comparison_anchor_identity{
        .Authority = row.Id.Authority,
        .Incarnation = row.Id.Incarnation,
        .Kind = kind,
        .Epoch = row.Id.Ordinal,
    };

    switch(requested_role)
    {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        return display_text(row, kind);
    case Qt::ToolTipRole:
    case Qt::AccessibleDescriptionRole:
        return description_text(row, kind);
    case identity_role:
        return identity_text(identity);
    case epoch_ordinal_role:
        return QVariant::fromValue<qlonglong>(row.Id.Ordinal.value());
    case anchor_kind_role:
        return static_cast<int>(kind);
    case oid_role:
        return oid_text(oid);
    case availability_role:
        return static_cast<int>(availability);
    case primary_cause_role:
        return static_cast<int>(row.Cause.Primary);
    case base_transition_role:
        return static_cast<int>(row.Cause.Base);
    case head_transition_role:
        return static_cast<int>(row.Cause.Head);
    default:
        return {};
    }
}

auto epoch_history_model::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int requested_role) const -> QVariant
{
    if(section == 0 && orientation == Qt::Horizontal
        && requested_role == Qt::DisplayRole)
    {
        return QStringLiteral("Review epochs");
    }
    return {};
}

auto epoch_history_model::flags(const QModelIndex& index) const
    -> Qt::ItemFlags
{
    return index.isValid()
        ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
        : Qt::NoItemFlags;
}

auto epoch_history_model::roleNames() const -> QHash<int, QByteArray>
{
    auto roles = QAbstractListModel::roleNames();
    roles.insert(identity_role, QByteArrayLiteral("identity"));
    roles.insert(epoch_ordinal_role, QByteArrayLiteral("epochOrdinal"));
    roles.insert(anchor_kind_role, QByteArrayLiteral("anchorKind"));
    roles.insert(oid_role, QByteArrayLiteral("oid"));
    roles.insert(availability_role, QByteArrayLiteral("availability"));
    roles.insert(primary_cause_role, QByteArrayLiteral("primaryCause"));
    roles.insert(base_transition_role, QByteArrayLiteral("baseTransition"));
    roles.insert(head_transition_role, QByteArrayLiteral("headTransition"));
    return roles;
}

void epoch_history_model::apply(
    const application::epoch_catalog_projection& projection)
{
    if(projection.Replace)
    {
        Catalog = projection.Catalog;
        replace({});
    }
    else if(projection.Catalog)
    {
        if(Catalog && !same_incarnation(*Catalog, *projection.Catalog))
        {
            Catalog = projection.Catalog;
            replace({});
        }
        else
        {
            Catalog = projection.Catalog;
        }
    }

    for(const auto& change : projection.Changes)
    {
        switch(change.Kind)
        {
        case application::epoch_row_change_kind::insert:
            if(change.Row && change.Row->Id == change.Id)
            {
                insert_or_move(change.Index, *change.Row);
            }
            break;
        case application::epoch_row_change_kind::remove:
            remove(change.Id);
            break;
        case application::epoch_row_change_kind::update:
            if(change.Row && change.Row->Id == change.Id)
            {
                update(*change.Row);
            }
            break;
        }
    }
    while(Rows.size() > core::maximum_epoch_history_page_size)
    {
        remove(Rows.back().Id);
    }

    if(projection.AvailabilityChangedTo)
    {
        update_availability(projection.AvailabilityChangedEpochs,
            *projection.AvailabilityChangedTo);
    }
    if(projection.AllEpochAvailability)
    {
        update_all_availability(*projection.AllEpochAvailability);
    }

    if(projection.HistoryPage
        && projection.HistoryPage->Phase
            == core::epoch_history_page_phase::loaded
        && projection.HistoryPage->Rows.size()
            <= core::maximum_epoch_history_page_size
        && (!Catalog
            || same_incarnation(*Catalog, projection.HistoryPage->Catalog)))
    {
        Catalog = projection.HistoryPage->Catalog;
        reconcile_page(projection.HistoryPage->Rows);
    }
}

void epoch_history_model::clear()
{
    Catalog.reset();
    replace({});
}

void epoch_history_model::refresh_presentation()
{
    if(Rows.empty())
    {
        return;
    }
    const auto count = rowCount();
    emit dataChanged(index(0, 0), index(count - 1, 0), changed_roles());
    Statistics.RowsChanged += static_cast<std::size_t>(count);
}

auto epoch_history_model::anchor_at(const QModelIndex& index) const
    -> std::optional<core::comparison_anchor>
{
    if(!index.isValid() || index.column() != 0 || index.row() < 0)
    {
        return std::nullopt;
    }
    const auto epoch_index = static_cast<std::size_t>(index.row()) / 2U;
    if(epoch_index >= Rows.size())
    {
        return std::nullopt;
    }
    const auto& row = Rows[epoch_index];
    const auto kind = anchor_kind_for_row(index.row());
    return core::comparison_anchor{
        .Identity =
            {
                .Authority = row.Id.Authority,
                .Incarnation = row.Id.Incarnation,
                .Kind = kind,
                .Epoch = row.Id.Ordinal,
            },
        .Commit = kind == core::comparison_anchor_kind::epoch_base
            ? row.Base
            : row.Head,
        .Availability = kind == core::comparison_anchor_kind::epoch_base
            ? row.BaseAvailability
            : row.HeadAvailability,
    };
}

auto epoch_history_model::index_for_anchor(
    const core::comparison_anchor_identity& identity) const -> QModelIndex
{
    if(identity.Kind != core::comparison_anchor_kind::epoch_base
        && identity.Kind != core::comparison_anchor_kind::epoch_head)
    {
        return {};
    }
    const auto found = epoch_index_for_id(core::review_epoch_id{
        .Authority = identity.Authority,
        .Incarnation = identity.Incarnation,
        .Ordinal = identity.Epoch,
    });
    if(!found)
    {
        return {};
    }
    const auto offset = identity.Kind == core::comparison_anchor_kind::epoch_base
        ? 0U
        : 1U;
    return index(static_cast<int>(*found * 2U + offset), 0);
}

auto epoch_history_model::catalog() const noexcept
    -> std::optional<core::epoch_catalog_identity>
{
    return Catalog;
}

auto epoch_history_model::epoch_count() const noexcept -> std::size_t
{
    return Rows.size();
}

auto epoch_history_model::statistics() const noexcept
    -> epoch_history_model_statistics
{
    return Statistics;
}

auto epoch_history_model::epoch_index_for_id(
    const core::review_epoch_id& identity) const
    -> std::optional<std::size_t>
{
    const auto found = std::ranges::find(Rows, identity,
        &application::epoch_row_projection::Id);
    if(found == Rows.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(Rows.begin(), found));
}

void epoch_history_model::replace(
    std::vector<application::epoch_row_projection> rows)
{
    if(Rows == rows)
    {
        return;
    }
    beginResetModel();
    Rows = std::move(rows);
    endResetModel();
    ++Statistics.ModelResets;
}

void epoch_history_model::reconcile_page(
    std::vector<application::epoch_row_projection> rows)
{
    for(auto index = Rows.size(); index != 0U; --index)
    {
        const auto& identity = Rows[index - 1U].Id;
        const auto retained = std::ranges::any_of(rows,
            [&identity](const application::epoch_row_projection& row)
            {
                return row.Id == identity;
            });
        if(!retained)
        {
            remove(identity);
        }
    }
    for(std::size_t index = 0U; index < rows.size(); ++index)
    {
        insert_or_move(index, std::move(rows[index]));
    }
}

void epoch_history_model::insert_or_move(
    const std::size_t requested_index,
    application::epoch_row_projection row)
{
    const auto existing = epoch_index_for_id(row.Id);
    const auto target = std::min(requested_index, Rows.size());
    if(existing)
    {
        const auto source = *existing;
        auto current = source;
        if(source != target && target < Rows.size())
        {
            const auto source_first = static_cast<int>(source * 2U);
            const auto destination = static_cast<int>(
                (target > source ? target + 1U : target) * 2U);
            beginMoveRows({},
                source_first,
                source_first + 1,
                {},
                destination);
            auto moved = std::move(Rows[source]);
            Rows.erase(Rows.begin() + static_cast<std::ptrdiff_t>(source));
            Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(target),
                std::move(moved));
            endMoveRows();
            current = target;
            Statistics.RowsMoved += 2U;
        }
        if(Rows[current] != row)
        {
            Rows[current] = std::move(row);
            const auto first = static_cast<int>(current * 2U);
            emit dataChanged(index(first, 0),
                index(first + 1, 0),
                changed_roles());
            Statistics.RowsChanged += 2U;
        }
        return;
    }

    const auto first = static_cast<int>(target * 2U);
    beginInsertRows({}, first, first + 1);
    Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(target),
        std::move(row));
    endInsertRows();
    Statistics.RowsInserted += 2U;
}

void epoch_history_model::remove(const core::review_epoch_id& identity)
{
    const auto existing = epoch_index_for_id(identity);
    if(!existing)
    {
        return;
    }
    const auto first = static_cast<int>(*existing * 2U);
    beginRemoveRows({}, first, first + 1);
    Rows.erase(Rows.begin() + static_cast<std::ptrdiff_t>(*existing));
    endRemoveRows();
    Statistics.RowsRemoved += 2U;
}

void epoch_history_model::update(application::epoch_row_projection row)
{
    const auto existing = epoch_index_for_id(row.Id);
    if(!existing || Rows[*existing] == row)
    {
        return;
    }
    Rows[*existing] = std::move(row);
    const auto first = static_cast<int>(*existing * 2U);
    emit dataChanged(index(first, 0), index(first + 1, 0), changed_roles());
    Statistics.RowsChanged += 2U;
}

void epoch_history_model::update_availability(
    const std::vector<core::review_epoch_id>& identities,
    const core::anchor_availability availability)
{
    for(const auto& identity : identities)
    {
        const auto existing = epoch_index_for_id(identity);
        if(!existing)
        {
            continue;
        }
        auto& row = Rows[*existing];
        if(row.BaseAvailability == availability
            && row.HeadAvailability == availability)
        {
            continue;
        }
        row.BaseAvailability = availability;
        row.HeadAvailability = availability;
        const auto first = static_cast<int>(*existing * 2U);
        emit dataChanged(index(first, 0), index(first + 1, 0), changed_roles());
        Statistics.RowsChanged += 2U;
    }
}

void epoch_history_model::update_all_availability(
    const core::anchor_availability availability)
{
    for(std::size_t position = 0U; position < Rows.size(); ++position)
    {
        auto& row = Rows[position];
        if(row.BaseAvailability == availability
            && row.HeadAvailability == availability)
        {
            continue;
        }
        row.BaseAvailability = availability;
        row.HeadAvailability = availability;
        const auto first = static_cast<int>(position * 2U);
        emit dataChanged(index(first, 0), index(first + 1, 0), changed_roles());
        Statistics.RowsChanged += 2U;
    }
}
}
