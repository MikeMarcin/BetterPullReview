#pragma once

#include "bpr/application/epoch_projection.hpp"

#include <QAbstractListModel>

#include <cstddef>
#include <optional>
#include <vector>

namespace bpr::qt
{
struct epoch_history_model_statistics
{
    std::size_t RowsInserted = 0U;
    std::size_t RowsRemoved = 0U;
    std::size_t RowsMoved = 0U;
    std::size_t RowsChanged = 0U;
    std::size_t ModelResets = 0U;
};

class epoch_history_model final : public QAbstractListModel
{
public:
    enum role : int
    {
        identity_role = Qt::UserRole + 1,
        epoch_ordinal_role,
        anchor_kind_role,
        oid_role,
        availability_role,
        primary_cause_role,
        base_transition_role,
        head_transition_role,
    };

    explicit epoch_history_model(QObject* parent = nullptr);

    [[nodiscard]] auto rowCount(const QModelIndex& parent = {}) const
        -> int override;
    [[nodiscard]] auto data(const QModelIndex& index, int role) const
        -> QVariant override;
    [[nodiscard]] auto headerData(
        int section,
        Qt::Orientation orientation,
        int role) const -> QVariant override;
    [[nodiscard]] auto flags(const QModelIndex& index) const
        -> Qt::ItemFlags override;
    [[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

    void apply(const application::epoch_catalog_projection& projection);
    void clear();
    void refresh_presentation();

    [[nodiscard]] auto anchor_at(const QModelIndex& index) const
        -> std::optional<core::comparison_anchor>;
    [[nodiscard]] auto index_for_anchor(
        const core::comparison_anchor_identity& identity) const
        -> QModelIndex;
    [[nodiscard]] auto catalog() const noexcept
        -> std::optional<core::epoch_catalog_identity>;
    [[nodiscard]] auto epoch_count() const noexcept -> std::size_t;
    [[nodiscard]] auto statistics() const noexcept
        -> epoch_history_model_statistics;

private:
    [[nodiscard]] auto epoch_index_for_id(
        const core::review_epoch_id& identity) const
        -> std::optional<std::size_t>;
    void replace(std::vector<application::epoch_row_projection> rows);
    void reconcile_page(
        std::vector<application::epoch_row_projection> rows);
    void insert_or_move(
        std::size_t index,
        application::epoch_row_projection row);
    void remove(const core::review_epoch_id& identity);
    void update(application::epoch_row_projection row);
    void update_availability(
        const std::vector<core::review_epoch_id>& identities,
        core::anchor_availability availability);
    void update_all_availability(core::anchor_availability availability);

    std::optional<core::epoch_catalog_identity> Catalog;
    std::vector<application::epoch_row_projection> Rows;
    epoch_history_model_statistics Statistics;
};
}
