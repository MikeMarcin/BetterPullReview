#pragma once

#include "bpr/application/github_projection.hpp"

#include <QAbstractItemModel>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::qt
{
struct github_conversation_model_statistics
{
    std::size_t RowsInserted = 0;
    std::size_t RowsRemoved = 0;
    std::size_t RowsMoved = 0;
    std::size_t RowsChanged = 0;
    std::size_t ModelResets = 0;
};

class github_conversation_model final : public QAbstractItemModel
{
public:
    enum role : int
    {
        identity_role = Qt::UserRole + 1,
        parent_identity_role,
        kind_role,
        author_login_role,
        body_role,
        path_role,
        line_role,
        review_state_role,
        resolved_role,
        outdated_role,
        deleted_role,
    };

    explicit github_conversation_model(QObject* parent = nullptr);
    ~github_conversation_model() override;

    github_conversation_model(const github_conversation_model&) = delete;
    auto operator=(const github_conversation_model&)
        -> github_conversation_model& = delete;
    github_conversation_model(github_conversation_model&&) = delete;
    auto operator=(github_conversation_model&&)
        -> github_conversation_model& = delete;

    [[nodiscard]] auto index(
        int row,
        int column,
        const QModelIndex& parent = {}) const -> QModelIndex override;
    [[nodiscard]] auto parent(const QModelIndex& index) const
        -> QModelIndex override;
    [[nodiscard]] auto rowCount(const QModelIndex& parent = {}) const
        -> int override;
    [[nodiscard]] auto columnCount(const QModelIndex& parent = {}) const
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

    void apply(
        const std::vector<application::github_conversation_projection>&
            conversations);
    void clear();

    [[nodiscard]] auto index_for_id(std::string_view identity) const
        -> QModelIndex;
    [[nodiscard]] auto id_for_index(const QModelIndex& index) const
        -> std::string;
    [[nodiscard]] auto statistics() const noexcept
        -> github_conversation_model_statistics;

private:
    struct node;

    [[nodiscard]] auto index_for_node(const node* value) const -> QModelIndex;
    [[nodiscard]] static auto find_node(
        const std::vector<std::unique_ptr<node>>& values,
        std::string_view identity) -> const node*;
    static void update_rows(
        std::vector<std::unique_ptr<node>>& values,
        std::size_t first) noexcept;

    std::vector<std::unique_ptr<node>> Roots;
    github_conversation_model_statistics Statistics;
};
}
