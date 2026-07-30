#include "bpr/qt/github_conversation_model.hpp"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QModelIndex>
#include <QObject>
#include <QPersistentModelIndex>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
using namespace bpr;
using conversation = application::github_conversation_projection;
using conversation_kind = application::github_conversation_kind;

[[nodiscard]] auto text(const std::string_view value) -> QString
{
    return QString::fromUtf8(
        value.data(), static_cast<qsizetype>(value.size()));
}

[[noreturn]] auto fail(
    const std::string_view suite,
    const std::size_t case_number,
    const std::string_view message) -> void
{
    std::cerr << "FAILED [" << suite << " case " << case_number
              << "]: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

auto expect(
    const bool condition,
    const std::string_view suite,
    const std::size_t case_number,
    const std::string_view message) -> void
{
    if(!condition)
    {
        fail(suite, case_number, message);
    }
}

class fixed_generator
{
public:
    explicit fixed_generator(const std::uint64_t seed) : State{seed} {}

    [[nodiscard]] auto bounded(const std::size_t bound) -> std::size_t
    {
        State += UINT64_C(0x9e3779b97f4a7c15);
        auto value = State;
        value = (value ^ (value >> 30U))
            * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27U))
            * UINT64_C(0x94d049bb133111eb);
        value ^= value >> 31U;
        return static_cast<std::size_t>(
            value % static_cast<std::uint64_t>(bound));
    }

private:
    std::uint64_t State;
};

struct expected_tree
{
    std::vector<std::string> Roots;
    std::unordered_map<std::string, std::vector<std::string>> Children;
    std::unordered_map<std::string, std::optional<std::string>> ParentById;
    std::unordered_map<std::string, conversation> ValueById;
};

[[nodiscard]] auto expected_for(
    const std::vector<conversation>& values) -> expected_tree
{
    expected_tree result;
    result.ValueById.reserve(values.size());
    result.ParentById.reserve(values.size());
    result.Children.reserve(values.size());

    for(const auto& value : values)
    {
        if(!value.Id.empty())
        {
            result.ValueById.try_emplace(value.Id, value);
        }
    }

    for(const auto& [identity, value] : result.ValueById)
    {
        std::optional<std::string> parent;
        if(
            value.Kind == conversation_kind::review_comment
            && value.ParentId.has_value() && *value.ParentId != identity)
        {
            if(const auto candidate =
                   result.ValueById.find(*value.ParentId);
               candidate != result.ValueById.end()
               && candidate->second.Kind
                      == conversation_kind::review_thread)
            {
                parent = *value.ParentId;
            }
        }
        result.ParentById.emplace(identity, std::move(parent));
    }

    std::unordered_set<std::string> ordered;
    ordered.reserve(result.ValueById.size());
    result.Roots.reserve(result.ValueById.size());
    for(const auto& value : values)
    {
        if(
            value.Id.empty()
            || !ordered.insert(value.Id).second)
        {
            continue;
        }

        const auto& parent = result.ParentById.at(value.Id);
        if(parent.has_value())
        {
            result.Children[*parent].push_back(value.Id);
        }
        else
        {
            result.Roots.push_back(value.Id);
        }
    }
    return result;
}

struct signal_audit
{
    std::size_t InsertedRows = 0;
    std::size_t RemovedRows = 0;
    std::size_t MovedRows = 0;
    std::size_t MoveEvents = 0;
    std::size_t ChangedRows = 0;
    std::size_t ModelResets = 0;

    void clear_operations() noexcept
    {
        InsertedRows = 0;
        RemovedRows = 0;
        MovedRows = 0;
        MoveEvents = 0;
        ChangedRows = 0;
    }
};

auto observe(qt::github_conversation_model& model, signal_audit& audit) -> void
{
    QObject::connect(
        &model,
        &QAbstractItemModel::rowsInserted,
        [&audit](const QModelIndex&, const int first, const int last) {
            audit.InsertedRows +=
                static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::rowsRemoved,
        [&audit](const QModelIndex&, const int first, const int last) {
            audit.RemovedRows +=
                static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::rowsMoved,
        [&audit](
            const QModelIndex&,
            const int first,
            const int last,
            const QModelIndex&,
            const int) {
            audit.MovedRows +=
                static_cast<std::size_t>(last - first + 1);
            ++audit.MoveEvents;
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::dataChanged,
        [&audit](
            const QModelIndex& top_left,
            const QModelIndex& bottom_right,
            const QList<int>&) {
            audit.ChangedRows += static_cast<std::size_t>(
                bottom_right.row() - top_left.row() + 1);
        });
    QObject::connect(
        &model,
        &QAbstractItemModel::modelReset,
        [&audit] { ++audit.ModelResets; });
}

auto validate_index(
    const qt::github_conversation_model& model,
    const QModelIndex& index,
    const std::string& identity,
    const expected_tree& expected,
    const std::string_view suite,
    const std::size_t case_number,
    const bool validate_lookup) -> void
{
    const auto& projection = expected.ValueById.at(identity);
    expect(index.isValid(), suite, case_number, "expected index is valid");
    expect(index.column() == 0, suite, case_number, "index uses column zero");
    expect(
        model.id_for_index(index) == identity,
        suite,
        case_number,
        "index identity matches expected order");
    const auto identity_value =
        model.data(index, qt::github_conversation_model::identity_role);
    expect(
        identity_value.isValid()
            && identity_value.toString() == text(identity),
        suite,
        case_number,
        "identity role matches stable identity");
    const auto kind_value =
        model.data(index, qt::github_conversation_model::kind_role);
    expect(
        kind_value.isValid()
            && kind_value.toInt() == static_cast<int>(projection.Kind),
        suite,
        case_number,
        "kind role matches the first unique projection");

    const auto parent_value =
        model.data(index, qt::github_conversation_model::parent_identity_role);
    const auto& projected_parent = projection.ParentId;
    expect(
        projected_parent.has_value()
            ? parent_value.isValid()
                && parent_value.toString() == text(*projected_parent)
            : !parent_value.isValid(),
        suite,
        case_number,
        "parent identity role matches the first unique projection");

    const auto author_value =
        model.data(index, qt::github_conversation_model::author_login_role);
    expect(
        author_value.isValid()
            && author_value.toString() == text(projection.AuthorLogin),
        suite,
        case_number,
        "author role matches the first unique projection");

    const auto body_value =
        model.data(index, qt::github_conversation_model::body_role);
    const auto expected_body =
        projection.Deleted ? QString{} : text(projection.Body);
    expect(
        body_value.isValid() && body_value.toString() == expected_body,
        suite,
        case_number,
        "body role matches deletion-aware projection semantics");

    const auto path_value =
        model.data(index, qt::github_conversation_model::path_role);
    expect(
        path_value.isValid()
            && path_value.toString() == text(projection.Path),
        suite,
        case_number,
        "path role matches the first unique projection");

    const auto line_value =
        model.data(index, qt::github_conversation_model::line_role);
    expect(
        projection.Line.has_value()
            ? line_value.isValid()
                && line_value.toULongLong()
                    == static_cast<qulonglong>(*projection.Line)
            : !line_value.isValid(),
        suite,
        case_number,
        "line role preserves exact optional presence and value");

    const auto review_state_value =
        model.data(index, qt::github_conversation_model::review_state_role);
    expect(
        review_state_value.isValid()
            && review_state_value.toInt()
                == static_cast<int>(projection.ReviewState),
        suite,
        case_number,
        "review-state role matches the first unique projection");

    const auto resolved_value =
        model.data(index, qt::github_conversation_model::resolved_role);
    const auto outdated_value =
        model.data(index, qt::github_conversation_model::outdated_role);
    const auto deleted_value =
        model.data(index, qt::github_conversation_model::deleted_role);
    expect(
        resolved_value.isValid()
            && resolved_value.toBool() == projection.Resolved
            && outdated_value.isValid()
            && outdated_value.toBool() == projection.Outdated
            && deleted_value.isValid()
            && deleted_value.toBool() == projection.Deleted,
        suite,
        case_number,
        "resolved, outdated, and deleted roles match the projection");

    const auto display =
        model.data(index, Qt::DisplayRole).toString();
    const auto accessible_text =
        model.data(index, Qt::AccessibleTextRole).toString();
    expect(
        !display.isEmpty() && accessible_text == display,
        suite,
        case_number,
        "accessible text is the complete nonempty display text");
    if(projection.Deleted)
    {
        expect(
            display.contains(QStringLiteral("Deleted comment")),
            suite,
            case_number,
            "deleted display text exposes the deletion state");
    }
    else if(!projection.Body.empty())
    {
        expect(
            display.contains(text(projection.Body)),
            suite,
            case_number,
            "nondeleted display text contains the literal projected body");
    }

    const auto accessible_description_value =
        model.data(index, Qt::AccessibleDescriptionRole);
    const auto accessible_description =
        accessible_description_value.toString();
    const auto has_description = projection.Resolved || projection.Outdated
        || projection.Deleted || projection.ParentId.has_value();
    expect(
        accessible_description_value.isValid()
            && accessible_description.isEmpty() == !has_description
            && accessible_description.contains(QStringLiteral("Resolved"))
                == projection.Resolved
            && accessible_description.contains(QStringLiteral("Outdated"))
                == projection.Outdated
            && accessible_description.contains(QStringLiteral("Deleted"))
                == projection.Deleted
            && accessible_description.contains(
                   QStringLiteral("Reply in a review thread"))
                == projection.ParentId.has_value(),
        suite,
        case_number,
        "accessible description exposes exactly the applicable state facts");

    if(validate_lookup)
    {
        expect(
            model.index_for_id(identity) == index,
            suite,
            case_number,
            "stable identity lookup returns the exact index");
    }
}

auto validate_model(
    const qt::github_conversation_model& model,
    const expected_tree& expected,
    const std::string_view suite,
    const std::size_t case_number) -> void
{
    expect(
        model.columnCount() == 1,
        suite,
        case_number,
        "root exposes exactly one column");
    expect(
        model.rowCount()
            == static_cast<int>(expected.Roots.size()),
        suite,
        case_number,
        "root row count matches normalized projection");

    const auto validate_every_lookup = expected.ValueById.size() <= 512U;
    std::size_t visited = 0;
    for(std::size_t root_row = 0; root_row < expected.Roots.size(); ++root_row)
    {
        const auto root =
            model.index(static_cast<int>(root_row), 0, QModelIndex{});
        const auto& identity = expected.Roots[root_row];
        expect(
            root.row() == static_cast<int>(root_row),
            suite,
            case_number,
            "root index reports its exact row");
        expect(
            !model.parent(root).isValid(),
            suite,
            case_number,
            "root index has no parent");
        validate_index(
            model,
            root,
            identity,
            expected,
            suite,
            case_number,
            validate_every_lookup);
        ++visited;

        const auto children = expected.Children.find(identity);
        const auto child_count = children == expected.Children.end()
            ? 0U
            : children->second.size();
        expect(
            model.rowCount(root) == static_cast<int>(child_count),
            suite,
            case_number,
            "child row count matches normalized projection");
        expect(
            model.columnCount(root) == 1,
            suite,
            case_number,
            "root child collection exposes exactly one column");

        for(std::size_t child_row = 0; child_row < child_count; ++child_row)
        {
            const auto child =
                model.index(static_cast<int>(child_row), 0, root);
            const auto& child_identity = children->second[child_row];
            expect(
                child.row() == static_cast<int>(child_row),
                suite,
                case_number,
                "child index reports its exact row");
            expect(
                model.parent(child) == root,
                suite,
                case_number,
                "child index returns the exact root parent");
            expect(
                model.rowCount(child) == 0,
                suite,
                case_number,
                "normalized conversation tree has only root and child levels");
            validate_index(
                model,
                child,
                child_identity,
                expected,
                suite,
                case_number,
                validate_every_lookup);
            ++visited;
        }
    }

    expect(
        visited == expected.ValueById.size(),
        suite,
        case_number,
        "every unique nonempty identity is reachable exactly once");
    expect(
        !model.index_for_id("property-test:absent").isValid(),
        suite,
        case_number,
        "unknown identity lookup is invalid");

    if(!validate_every_lookup && !expected.Roots.empty())
    {
        const std::size_t sampled_rows[]{
            0U,
            expected.Roots.size() / 2U,
            expected.Roots.size() - 1U,
        };
        for(const auto row : sampled_rows)
        {
            expect(
                model.index_for_id(expected.Roots[row])
                    == model.index(static_cast<int>(row), 0),
                suite,
                case_number,
                "sampled large-model identity lookup returns the exact row");
        }
    }
}

using persistent_indexes =
    std::unordered_map<std::string, QPersistentModelIndex>;

[[nodiscard]] auto capture_persistent_indexes(
    const qt::github_conversation_model& model,
    const expected_tree& expected) -> persistent_indexes
{
    persistent_indexes result;
    result.reserve(expected.ValueById.size());
    for(const auto& entry : expected.ValueById)
    {
        result.emplace(entry.first, model.index_for_id(entry.first));
    }
    return result;
}

[[nodiscard]] auto preserves_persistent_index(
    const expected_tree& before,
    const expected_tree& after,
    const std::string& identity) -> bool
{
    const auto prior_parent = before.ParentById.find(identity);
    const auto next_parent = after.ParentById.find(identity);
    return prior_parent != before.ParentById.end()
        && next_parent != after.ParentById.end()
        && prior_parent->second == next_parent->second;
}

struct varied_role_coverage
{
    bool Kind = false;
    bool ParentId = false;
    bool AuthorLogin = false;
    bool Body = false;
    bool Path = false;
    bool Line = false;
    bool ReviewState = false;
    bool Resolved = false;
    bool Outdated = false;
    bool Deleted = false;
};

[[nodiscard]] auto visible_body(const conversation& value) noexcept
    -> std::string_view
{
    return value.Deleted ? std::string_view{} : value.Body;
}

[[nodiscard]] auto observe_preserved_role_changes(
    varied_role_coverage& coverage,
    const expected_tree& before,
    const expected_tree& after) -> std::size_t
{
    std::size_t changed_rows = 0U;
    for(const auto& [identity, prior] : before.ValueById)
    {
        if(!preserves_persistent_index(before, after, identity))
        {
            continue;
        }

        const auto& next = after.ValueById.at(identity);
        coverage.Kind = coverage.Kind || prior.Kind != next.Kind;
        coverage.ParentId =
            coverage.ParentId || prior.ParentId != next.ParentId;
        coverage.AuthorLogin =
            coverage.AuthorLogin || prior.AuthorLogin != next.AuthorLogin;
        coverage.Body =
            coverage.Body || visible_body(prior) != visible_body(next);
        coverage.Path = coverage.Path || prior.Path != next.Path;
        coverage.Line = coverage.Line || prior.Line != next.Line;
        coverage.ReviewState =
            coverage.ReviewState || prior.ReviewState != next.ReviewState;
        coverage.Resolved =
            coverage.Resolved || prior.Resolved != next.Resolved;
        coverage.Outdated =
            coverage.Outdated || prior.Outdated != next.Outdated;
        coverage.Deleted =
            coverage.Deleted || prior.Deleted != next.Deleted;
        changed_rows += prior != next ? 1U : 0U;
    }
    return changed_rows;
}

auto validate_persistent_indexes(
    const qt::github_conversation_model& model,
    const expected_tree& before,
    const expected_tree& after,
    const persistent_indexes& indexes,
    const std::string_view suite,
    const std::size_t case_number) -> void
{
    for(const auto& [identity, index] : indexes)
    {
        if(preserves_persistent_index(before, after, identity))
        {
            expect(
                index.isValid(),
                suite,
                case_number,
                "same-parent stable identity preserves persistent index");
            expect(
                QModelIndex{index} == model.index_for_id(identity),
                suite,
                case_number,
                "persistent index follows exact insert and move position");
        }
        else
        {
            expect(
                !index.isValid(),
                suite,
                case_number,
                "removed or reparented identity invalidates old index");
        }
    }
}

[[nodiscard]] auto mixed_projection(
    fixed_generator& generator,
    const std::size_t case_number,
    const std::size_t slot) -> conversation
{
    const auto identity_number = generator.bounded(96U);
    auto identity = std::string{"mixed:"} + std::to_string(identity_number);
    if(generator.bounded(29U) == 0U)
    {
        identity.clear();
    }

    std::optional<std::string> parent;
    if(generator.bounded(4U) != 0U)
    {
        if(generator.bounded(7U) == 0U)
        {
            parent =
                std::string{"missing:"}
                + std::to_string(generator.bounded(16U));
        }
        else if(generator.bounded(11U) == 0U)
        {
            parent = identity;
        }
        else
        {
            parent =
                std::string{"mixed:"}
                + std::to_string(generator.bounded(96U));
        }
    }

    return conversation{
        .Id = std::move(identity),
        .ParentId = std::move(parent),
        .Kind = static_cast<conversation_kind>(generator.bounded(4U)),
        .AuthorLogin =
            std::string{"author:"}
            + std::to_string(generator.bounded(13U)),
        .Body =
            std::string{"body:"} + std::to_string(case_number % 17U) + ':'
            + std::to_string(slot) + ':'
            + std::to_string(generator.bounded(9U)),
        .Path =
            std::string{"src/property-"}
            + std::to_string(generator.bounded(11U)) + ".cpp",
        .Line = generator.bounded(3U) == 0U
            ? std::nullopt
            : std::optional<std::uint32_t>{
                  static_cast<std::uint32_t>(
                      1U + generator.bounded(500U))},
        .ReviewState =
            static_cast<application::github_review_state_projection>(
                generator.bounded(6U)),
        .Resolved = generator.bounded(2U) != 0U,
        .Outdated = generator.bounded(2U) != 0U,
        .Deleted = generator.bounded(5U) == 0U,
    };
}

[[nodiscard]] auto mixed_case(
    fixed_generator& generator,
    const std::size_t case_number) -> std::vector<conversation>
{
    if(case_number % 31U == 0U)
    {
        return {};
    }

    std::vector<conversation> result;
    const auto random_count = 20U + generator.bounded(81U);
    result.reserve(random_count + 3U);
    for(std::size_t slot = 0; slot < random_count; ++slot)
    {
        result.push_back(mixed_projection(generator, case_number, slot));
    }

    auto anchor_thread = conversation{
        .Id = "mixed:anchor-thread",
        .Kind = case_number % 23U == 0U
            ? conversation_kind::issue_comment
            : conversation_kind::review_thread,
        .AuthorLogin = "anchor",
        .Body = std::string{"anchor-thread:"}
            + std::to_string(case_number % 7U),
    };
    auto anchor_child = conversation{
        .Id = "mixed:anchor-child",
        .ParentId = case_number % 19U == 0U
            ? std::optional<std::string>{"missing:anchor"}
            : std::optional<std::string>{"mixed:anchor-thread"},
        .Kind = conversation_kind::review_comment,
        .AuthorLogin = "anchor",
        .Body = std::string{"anchor-child:"}
            + std::to_string(case_number % 5U),
    };
    result.push_back(std::move(anchor_thread));
    result.push_back(std::move(anchor_child));

    if(!result.empty() && case_number % 5U == 0U)
    {
        result.push_back(result[generator.bounded(result.size())]);
    }
    if(result.size() > 1U)
    {
        std::rotate(
            result.begin(),
            result.begin()
                + static_cast<std::ptrdiff_t>(
                    generator.bounded(result.size())),
            result.end());
    }
    return result;
}

auto test_mixed_cases() -> void
{
    constexpr std::size_t case_count = 3'000U;
    qt::github_conversation_model model;
    signal_audit audit;
    observe(model, audit);
    fixed_generator generator{UINT64_C(0x4250522d4d495845)};
    expected_tree prior;

    std::size_t cases_with_inserts = 0;
    std::size_t cases_with_removes = 0;
    std::size_t cases_with_moves = 0;
    std::size_t cases_with_changes = 0;
    std::size_t preserved_indexes = 0;
    std::size_t invalidated_indexes = 0;
    varied_role_coverage varied_roles;

    for(std::size_t case_number = 0; case_number < case_count; ++case_number)
    {
        const auto values = mixed_case(generator, case_number);
        auto expected = expected_for(values);
        const auto expected_changed_rows =
            observe_preserved_role_changes(varied_roles, prior, expected);
        const auto persistent =
            capture_persistent_indexes(model, prior);
        audit.clear_operations();

        model.apply(values);

        validate_persistent_indexes(
            model,
            prior,
            expected,
            persistent,
            "mixed",
            case_number);
        for(const auto& [identity, index] : persistent)
        {
            static_cast<void>(index);
            if(preserves_persistent_index(prior, expected, identity))
            {
                ++preserved_indexes;
            }
            else
            {
                ++invalidated_indexes;
            }
        }
        validate_model(model, expected, "mixed", case_number);
        expect(
            audit.ChangedRows == expected_changed_rows
                && audit.ModelResets == 0U
                && model.statistics().ModelResets == 0U,
            "mixed",
            case_number,
            "mixed projection changes emit exact data rows without reset");

        cases_with_inserts += audit.InsertedRows != 0U ? 1U : 0U;
        cases_with_removes += audit.RemovedRows != 0U ? 1U : 0U;
        cases_with_moves += audit.MovedRows != 0U ? 1U : 0U;
        cases_with_changes += audit.ChangedRows != 0U ? 1U : 0U;
        prior = std::move(expected);
    }

    expect(
        cases_with_inserts != 0U && cases_with_removes != 0U
            && cases_with_moves != 0U && cases_with_changes != 0U,
        "mixed",
        case_count,
        "fixed mixed cases exercise insert, remove, move, and change paths");
    expect(
        preserved_indexes != 0U && invalidated_indexes != 0U,
        "mixed",
        case_count,
        "fixed mixed cases exercise persistent preservation and invalidation");
    expect(
        varied_roles.Kind && varied_roles.ParentId
            && varied_roles.AuthorLogin && varied_roles.Body
            && varied_roles.Path && varied_roles.Line
            && varied_roles.ReviewState && varied_roles.Resolved
            && varied_roles.Outdated && varied_roles.Deleted,
        "mixed",
        case_count,
        "fixed mixed cases vary every public projection role on preserved "
        "rows");
}

[[nodiscard]] auto permutation_rows()
    -> std::vector<conversation>
{
    std::vector<conversation> result;
    result.reserve(100U);
    for(std::size_t index = 0; index < 100U; ++index)
    {
        result.push_back({
            .Id = std::string{"permutation:"} + std::to_string(index),
            .Kind = conversation_kind::issue_comment,
            .AuthorLogin = "permutation",
            .Body = std::string{"row:"} + std::to_string(index),
        });
    }
    return result;
}

[[nodiscard]] auto projections_in_order(
    const std::vector<conversation>& rows,
    const std::vector<std::size_t>& order) -> std::vector<conversation>
{
    std::vector<conversation> result;
    result.reserve(order.size());
    for(const auto row : order)
    {
        result.push_back(rows[row]);
    }
    return result;
}

auto shuffle(
    std::vector<std::size_t>& values,
    fixed_generator& generator) -> void
{
    for(auto remaining = values.size(); remaining > 1U; --remaining)
    {
        std::swap(
            values[remaining - 1U],
            values[generator.bounded(remaining)]);
    }
}

[[nodiscard]] auto minimum_moves(
    const std::vector<std::size_t>& current,
    const std::vector<std::size_t>& desired) -> std::size_t
{
    std::vector<std::size_t> desired_rank(desired.size());
    for(std::size_t rank = 0; rank < desired.size(); ++rank)
    {
        desired_rank[desired[rank]] = rank;
    }

    std::vector<std::size_t> tails;
    tails.reserve(current.size());
    for(const auto identity : current)
    {
        const auto rank = desired_rank[identity];
        const auto insertion =
            std::lower_bound(tails.begin(), tails.end(), rank);
        if(insertion == tails.end())
        {
            tails.push_back(rank);
        }
        else
        {
            *insertion = rank;
        }
    }
    return current.size() - tails.size();
}

auto test_arbitrary_permutations() -> void
{
    constexpr std::size_t case_count = 5'000U;
    const auto rows = permutation_rows();
    std::vector<std::size_t> current(rows.size());
    for(std::size_t index = 0; index < current.size(); ++index)
    {
        current[index] = index;
    }

    qt::github_conversation_model model;
    signal_audit audit;
    observe(model, audit);
    const auto initial = projections_in_order(rows, current);
    model.apply(initial);
    std::vector<QPersistentModelIndex> persistent;
    persistent.reserve(rows.size());
    for(std::size_t row = 0; row < rows.size(); ++row)
    {
        persistent.emplace_back(model.index(static_cast<int>(row), 0));
    }

    fixed_generator generator{UINT64_C(0x4250522d5045524d)};
    for(std::size_t case_number = 0; case_number < case_count; ++case_number)
    {
        auto desired_order = current;
        shuffle(desired_order, generator);
        const auto expected_moves =
            minimum_moves(current, desired_order);
        const auto desired =
            projections_in_order(rows, desired_order);
        const auto expected = expected_for(desired);
        const auto before_statistics = model.statistics();
        audit.clear_operations();

        model.apply(desired);

        const auto after_statistics = model.statistics();
        expect(
            audit.MoveEvents == expected_moves
                && audit.MovedRows == expected_moves
                && after_statistics.RowsMoved
                       - before_statistics.RowsMoved
                    == expected_moves,
            "permutation",
            case_number,
            "reconciliation emits the LIS-minimum number of one-row moves");
        expect(
            audit.InsertedRows == 0U && audit.RemovedRows == 0U
                && audit.ChangedRows == 0U,
            "permutation",
            case_number,
            "pure permutation emits no insert, remove, or data change");
        expect(
            audit.ModelResets == 0U
                && model.statistics().ModelResets == 0U,
            "permutation",
            case_number,
            "arbitrary permutations never reset the model");
        validate_model(
            model, expected, "permutation", case_number);

        for(std::size_t identity = 0; identity < rows.size(); ++identity)
        {
            expect(
                persistent[identity].isValid()
                    && QModelIndex{persistent[identity]}
                           == model.index_for_id(rows[identity].Id),
                "permutation",
                case_number,
                "every stable row preserves its persistent index");
        }
        current = std::move(desired_order);
    }
}

[[nodiscard]] auto rotation_rows(
    const std::size_t count) -> std::vector<conversation>
{
    std::vector<conversation> result;
    result.reserve(count);
    for(std::size_t index = 0; index < count; ++index)
    {
        result.push_back({
            .Id = std::string{"rotation:"} + std::to_string(index),
            .Kind = conversation_kind::issue_comment,
        });
    }
    return result;
}

auto expect_one_rotation_move(
    qt::github_conversation_model& model,
    signal_audit& audit,
    const std::vector<conversation>& desired,
    const std::string_view suite,
    const std::size_t case_number) -> void
{
    const auto before = model.statistics();
    audit.clear_operations();
    model.apply(desired);
    const auto after = model.statistics();

    expect(
        audit.MoveEvents == 1U && audit.MovedRows == 1U
            && after.RowsMoved - before.RowsMoved == 1U,
        suite,
        case_number,
        "50,000-row rotation emits exactly one one-row move");
    expect(
        audit.InsertedRows == 0U && audit.RemovedRows == 0U
            && audit.ChangedRows == 0U,
        suite,
        case_number,
        "50,000-row rotation emits no insert, remove, or data change");
    expect(
        audit.ModelResets == 0U && after.ModelResets == 0U,
        suite,
        case_number,
        "50,000-row rotation never resets the model");
    validate_model(
        model, expected_for(desired), suite, case_number);
}

auto test_large_rotations() -> void
{
    constexpr std::size_t row_count = 50'000U;
    auto rows = rotation_rows(row_count);
    qt::github_conversation_model model;
    signal_audit audit;
    observe(model, audit);
    model.apply(rows);
    validate_model(model, expected_for(rows), "rotation", 0U);

    const QPersistentModelIndex first{model.index(0, 0)};
    const QPersistentModelIndex middle{
        model.index(static_cast<int>(row_count / 2U), 0)};
    const QPersistentModelIndex last{
        model.index(static_cast<int>(row_count - 1U), 0)};

    std::rotate(rows.begin(), rows.begin() + 1, rows.end());
    expect_one_rotation_move(model, audit, rows, "left-rotation", 0U);
    expect(
        first.isValid()
            && QModelIndex{first}
                   == model.index(static_cast<int>(row_count - 1U), 0)
            && middle.isValid()
            && QModelIndex{middle}
                   == model.index(
                       static_cast<int>(row_count / 2U - 1U), 0)
            && last.isValid()
            && QModelIndex{last}
                   == model.index(static_cast<int>(row_count - 2U), 0),
        "left-rotation",
        0U,
        "large left rotation preserves sampled persistent indexes");

    std::rotate(rows.begin(), rows.end() - 1, rows.end());
    expect_one_rotation_move(model, audit, rows, "right-rotation", 0U);
    expect(
        first.isValid() && QModelIndex{first} == model.index(0, 0)
            && middle.isValid()
            && QModelIndex{middle}
                   == model.index(static_cast<int>(row_count / 2U), 0)
            && last.isValid()
            && QModelIndex{last}
                   == model.index(static_cast<int>(row_count - 1U), 0),
        "right-rotation",
        0U,
        "large right rotation preserves sampled persistent indexes");
}
}

auto main(int argc, char** argv) -> int
{
    QCoreApplication application{argc, argv};
    application.setApplicationName(
        QStringLiteral("BetterPullReviewConversationModelPropertyTests"));

    test_mixed_cases();
    test_arbitrary_permutations();
    test_large_rotations();

    std::cout << "github-conversation-model-property-tests:pass\n";
    return EXIT_SUCCESS;
}
