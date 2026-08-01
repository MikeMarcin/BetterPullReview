#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_window.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFont>
#include <QLineEdit>
#include <QPersistentModelIndex>
#include <QPixmap>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace bpr;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Identity>
[[nodiscard]] auto identity(const std::uint8_t seed) -> Identity
{
    std::array<std::uint8_t, 32> bytes{};
    bytes.front() = seed;
    bytes.back() = static_cast<std::uint8_t>(seed ^ 0xa5U);
    return Identity{bytes};
}

[[nodiscard]] auto commit(
    const char digit,
    const std::size_t length = 40U) -> core::git_commit_oid
{
    const auto parsed = core::parse_git_commit_oid(std::string(length, digit));
    expect(parsed.has_value(), "fixture commit OID parses");
    return *parsed;
}

[[nodiscard]] auto catalog(const std::int64_t revision = 7)
    -> core::epoch_catalog_identity
{
    return {
        .Incarnation = identity<core::epoch_catalog_incarnation_id>(2U),
        .Revision = core::epoch_catalog_revision{revision},
    };
}

[[nodiscard]] auto epoch_id(const std::int64_t ordinal)
    -> core::review_epoch_id
{
    return {
        .Authority = identity<core::pull_request_authority_scope_id>(1U),
        .Incarnation = catalog().Incarnation,
        .Ordinal = core::epoch_ordinal{ordinal},
    };
}

[[nodiscard]] auto epoch_row(
    const std::int64_t ordinal,
    const char base,
    const char head,
    const core::epoch_primary_cause primary =
        core::epoch_primary_cause::author_upload)
    -> application::epoch_row_projection
{
    return {
        .Id = epoch_id(ordinal),
        .Snapshot = core::provider_snapshot_id{
            static_cast<std::uint64_t>(ordinal)},
        .IdentityGeneration = core::github_identity_generation{3U},
        .PullRequestGeneration = core::request_generation{4U},
        .BaseBranch = identity<core::base_branch_id>(5U),
        .Base = commit(base),
        .Head = commit(head),
        .Cause =
            {
                .Origin = core::epoch_origin::live_observation,
                .Base = core::epoch_base_transition::unchanged,
                .Head = primary
                        == core::epoch_primary_cause::force_push_rebase
                    ? core::epoch_head_transition::non_fast_forward
                    : core::epoch_head_transition::descendant,
                .Material = core::epoch_material_relation::different,
                .Primary = primary,
            },
        .ObservedAt = core::observed_utc_milliseconds{1'700'000'000'000LL
            + ordinal},
        .BaseAvailability = core::anchor_availability::available,
        .HeadAvailability = core::anchor_availability::available,
    };
}

[[nodiscard]] auto anchor(
    const application::epoch_row_projection& row,
    const core::comparison_anchor_kind kind,
    const core::anchor_availability availability =
        core::anchor_availability::available) -> core::comparison_anchor
{
    return {
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
        .Availability = availability,
    };
}

[[nodiscard]] auto loaded_catalog_update(
    std::vector<application::epoch_row_projection> rows,
    const std::optional<core::epoch_history_cursor>& continuation =
        std::nullopt) -> application::epoch_presentation_update
{
    const auto current_catalog = catalog();
    return {
        .Catalog =
            application::epoch_catalog_projection{
                .Catalog = current_catalog,
                .HistoryPage =
                    application::epoch_history_page_projection{
                        .Phase = core::epoch_history_page_phase::loaded,
                        .Catalog = current_catalog,
                        .Limit = core::maximum_epoch_history_page_size,
                        .Rows = std::move(rows),
                        .Continuation = continuation,
                    },
            },
    };
}

[[nodiscard]] auto enabled_commands() -> core::epoch_command_delta
{
    return {
        .Commands =
            {
                core::epoch_command_projection{
                    .Id = core::epoch_command_id::refresh_epochs,
                    .Enabled = true,
                    .DisabledReason = core::epoch_reason_code::none,
                },
                core::epoch_command_projection{
                    .Id = core::epoch_command_id::select_comparison,
                    .Enabled = true,
                    .DisabledReason = core::epoch_reason_code::none,
                },
                core::epoch_command_projection{
                    .Id = core::epoch_command_id::cancel_epoch_refresh,
                    .Enabled = true,
                    .DisabledReason = core::epoch_reason_code::none,
                },
            },
    };
}

[[nodiscard]] auto comparison_update(
    const core::comparison_selection& selection,
    const core::comparison_pair& pair,
    const bool available = true,
    const core::epoch_reason_code reason = core::epoch_reason_code::none)
    -> application::epoch_presentation_update
{
    return {
        .Transition =
            application::epoch_transition_projection{
                .Changed = core::epoch_changed_domain::selection
                    | core::epoch_changed_domain::comparison
                    | core::epoch_changed_domain::status
                    | core::epoch_changed_domain::commands,
                .Selection = core::comparison_selection_delta{
                    .Selection = selection,
                },
                .Comparison =
                    application::epoch_comparison_projection{
                        .Selection = selection,
                        .Pair = pair,
                        .Generation = core::comparison_generation{9U},
                        .Revision = core::comparison_revision{3},
                        .AvailableForDiff = available,
                        .Reason = reason,
                    },
                .Status = core::epoch_status_delta{
                    .Status = core::epoch_status::ready,
                },
                .Commands = enabled_commands(),
            },
    };
}

auto test_model_applies_stable_incremental_rows() -> void
{
    qt::epoch_history_model model;
    std::size_t inserted = 0U;
    std::size_t removed = 0U;
    std::size_t moved = 0U;
    std::size_t changed = 0U;
    QObject::connect(&model,
        &QAbstractItemModel::rowsInserted,
        [&inserted](const QModelIndex&, const int first, const int last)
        {
            inserted += static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(&model,
        &QAbstractItemModel::rowsRemoved,
        [&removed](const QModelIndex&, const int first, const int last)
        {
            removed += static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(&model,
        &QAbstractItemModel::rowsMoved,
        [&moved](const QModelIndex&,
            const int first,
            const int last,
            const QModelIndex&,
            const int)
        {
            moved += static_cast<std::size_t>(last - first + 1);
        });
    QObject::connect(&model,
        &QAbstractItemModel::dataChanged,
        [&changed](const QModelIndex& first, const QModelIndex& last)
        {
            changed += static_cast<std::size_t>(last.row() - first.row() + 1);
        });

    const auto third = epoch_row(3, '1', '2');
    const auto second = epoch_row(2, '1', '3',
        core::epoch_primary_cause::force_push_rebase);
    model.apply(*loaded_catalog_update({third, second}).Catalog);
    expect(model.rowCount() == 4 && inserted == 4U,
        "a loaded page inserts two exact anchors for each stable epoch");
    expect(model.data(model.index(2, 0), Qt::DisplayRole)
                   .toString()
                   .contains(QStringLiteral("Force-push/rebase"))
            && model.data(model.index(2, 0),
                         qt::epoch_history_model::base_transition_role)
                   .toInt()
                == static_cast<int>(core::epoch_base_transition::unchanged)
            && model.data(model.index(3, 0),
                         qt::epoch_history_model::head_transition_role)
                   .toInt()
                == static_cast<int>(
                    core::epoch_head_transition::non_fast_forward),
        "each anchor exposes the primary and independent base/head facts");

    const QPersistentModelIndex stable_second_head{
        model.index_for_anchor(anchor(second,
            core::comparison_anchor_kind::epoch_head)
                                   .Identity)};
    const auto fourth = epoch_row(4, '1', '4');
    model.apply(application::epoch_catalog_projection{
        .Catalog = catalog(8),
        .Changes =
            {
                application::epoch_row_change_projection{
                    .Kind = application::epoch_row_change_kind::insert,
                    .Index = 0U,
                    .Id = fourth.Id,
                    .Row = fourth,
                },
            },
    });
    expect(stable_second_head.isValid() && stable_second_head.row() == 5
            && model.statistics().ModelResets == 0U,
        "an insertion preserves exact persistent anchor identity without reset");

    model.apply(application::epoch_catalog_projection{
        .Catalog = catalog(9),
        .Changes =
            {
                application::epoch_row_change_projection{
                    .Kind = application::epoch_row_change_kind::insert,
                    .Index = 0U,
                    .Id = second.Id,
                    .Row = second,
                },
            },
    });
    expect(moved == 2U && stable_second_head.isValid()
            && stable_second_head.row() == 1,
        "an existing stable epoch moves by an exact two-anchor move signal");

    model.apply(application::epoch_catalog_projection{
        .Catalog = catalog(10),
        .AvailabilityChangedEpochs = {second.Id},
        .AvailabilityChangedTo = core::anchor_availability::pruned,
    });
    expect(model.data(model.index(0, 0),
                     qt::epoch_history_model::availability_role)
                   .toInt()
                == static_cast<int>(core::anchor_availability::pruned)
            && model.data(model.index(1, 0), Qt::DisplayRole)
                   .toString()
                   .contains(QStringLiteral("Pruned"))
            && changed >= 2U,
        "exact availability deltas render pruned state without Qt inference");

    model.apply(application::epoch_catalog_projection{
        .Catalog = catalog(11),
        .Changes =
            {
                application::epoch_row_change_projection{
                    .Kind = application::epoch_row_change_kind::remove,
                    .Id = third.Id,
                },
            },
    });
    expect(removed == 2U && model.index_for_anchor(anchor(third,
                core::comparison_anchor_kind::epoch_head)
                                                            .Identity)
                                  .isValid()
            == false,
        "removal emits an exact two-anchor removal and removes only its ID");

    model.apply(application::epoch_catalog_projection{
        .Catalog = catalog(12),
        .AllEpochAvailability = core::anchor_availability::cache_deleted,
    });
    expect(model.data(model.index(0, 0),
                     qt::epoch_history_model::availability_role)
                   .toInt()
                == static_cast<int>(core::anchor_availability::cache_deleted)
            && model.statistics().ModelResets == 0U,
        "bulk cache deletion updates resident rows without a model reset");
}

auto test_control_translates_exact_selection_and_preserves_projection() -> void
{
    std::vector<application::epoch_intent> intents;
    qt::epoch_comparison_control control{
        [&intents](application::epoch_intent intent)
        {
            intents.push_back(std::move(intent));
        },
    };
    const auto third = epoch_row(3, '1', '2');
    const auto second = epoch_row(2, '1', '3');
    const auto continuation = core::epoch_history_cursor{
        .Authority = third.Id.Authority,
        .Incarnation = third.Id.Incarnation,
        .Revision = catalog().Revision,
        .Before = core::epoch_ordinal{2},
    };
    control.publish(loaded_catalog_update({third, second}, continuation));
    const auto whole_pair = core::comparison_pair{
        .Left = anchor(third, core::comparison_anchor_kind::epoch_base),
        .Right = anchor(third, core::comparison_anchor_kind::epoch_head),
    };
    control.publish(comparison_update(
        core::named_selection(core::named_comparison::whole_pr), whole_pair));

    expect(control.left_selector()->currentIndex() == 0
            && control.right_selector()->currentIndex() == 1
            && control.left_oid_editor()->text() == QString(40, QLatin1Char('1'))
            && control.right_oid_editor()->text() == QString(40, QLatin1Char('2'))
            && control.left_oid_editor()->text().size() == 40
            && !control.left_oid_editor()->hasSelectedText(),
        "resolved selection exposes exact non-elided copyable OIDs and anchors");
    expect(control.whole_pr_action()->isChecked()
            && control.whole_pr_action()->isEnabled()
            && !control.whole_pr_action()->shortcut().isEmpty()
            && !control.load_history_action()->shortcut().isEmpty(),
        "named comparisons and history paging expose keyboard actions");

    control.previous_upload_action()->trigger();
    const auto* named = std::get_if<core::select_named_comparison_intent>(
        &intents.back());
    expect(named != nullptr
            && named->Alias
                == core::named_comparison::previous_upload_to_latest
            && named->ExpectedCatalogRevision == catalog().Revision
            && control.whole_pr_action()->isChecked(),
        "an alias action dispatches exact catalog-bound semantics and waits "
        "for projection");

    control.left_selector()->setCurrentIndex(3);
    const auto* selected = std::get_if<core::select_comparison_anchor_intent>(
        &intents.back());
    expect(selected != nullptr
            && selected->Side == core::comparison_side::left
            && selected->Anchor
                == anchor(second, core::comparison_anchor_kind::epoch_head)
                       .Identity
            && selected->ExpectedCatalogRevision == catalog().Revision
            && control.left_selector()->currentIndex() == 0,
        "one selector dispatches only its exact typed anchor and is not authority");

    control.load_history_action()->trigger();
    const auto* page = std::get_if<core::request_epoch_history_page_intent>(
        &intents.back());
    expect(page != nullptr && page->Catalog == catalog()
            && page->Cursor == continuation
            && page->Limit == core::maximum_epoch_history_page_size,
        "history action dispatches the exact bounded continuation");

    control.publish(application::epoch_presentation_update{
        .Catalog = application::epoch_catalog_projection{
            .Catalog = catalog(8),
        },
    });
    control.load_history_action()->trigger();
    page = std::get_if<core::request_epoch_history_page_intent>(
        &intents.back());
    expect(page != nullptr && page->Catalog == catalog(8) && !page->Cursor,
        "a catalog revision change discards the stale cursor and reloads "
        "from the exact new revision");
    control.refresh_action()->trigger();
    expect(std::holds_alternative<core::refresh_epochs_intent>(intents.back()),
        "refresh action emits only the semantic refresh intent");
    control.cancel_action()->trigger();
    expect(std::holds_alternative<core::cancel_epoch_refresh_intent>(
               intents.back()),
        "cancel action emits only the semantic cancellation intent");

    const auto reverse = core::comparison_pair{
        .Left = anchor(third, core::comparison_anchor_kind::epoch_head),
        .Right = anchor(second, core::comparison_anchor_kind::epoch_head),
    };
    control.publish(comparison_update(core::explicit_selection(
        reverse.Left.Identity, reverse.Right.Identity), reverse));
    expect(control.left_selector()->currentIndex() == 1
            && control.right_selector()->currentIndex() == 3
            && !control.whole_pr_action()->isChecked()
            && control.comparison_text().contains(
                QStringLiteral("Exact epoch pair")),
        "reverse exact pairs remain visible without alias remapping");
    const auto identical = core::comparison_pair{
        .Left = reverse.Right,
        .Right = reverse.Right,
    };
    control.publish(comparison_update(core::explicit_selection(
        identical.Left.Identity, identical.Right.Identity), identical));
    expect(control.left_oid_editor()->text()
                == control.right_oid_editor()->text()
            && control.left_selector()->currentIndex()
                == control.right_selector()->currentIndex(),
        "identical exact pairs remain representable");

    const auto unavailable = core::comparison_pair{
        .Left = anchor(second,
            core::comparison_anchor_kind::epoch_head,
            core::anchor_availability::pruned),
        .Right = reverse.Left,
    };
    control.publish(comparison_update(core::named_selection(
        core::named_comparison::previous_upload_to_latest),
        unavailable,
        false,
        core::epoch_reason_code::unavailable));
    expect(control.previous_upload_action()->isChecked()
            && control.left_oid_editor()->text() == QString(40, QLatin1Char('3'))
            && control.comparison_text().contains(QStringLiteral("pruned"))
            && control.comparison_text().contains(QStringLiteral("unavailable")),
        "an unavailable alias retains its exact selected candidate and reason");

    const auto review_anchor = core::comparison_anchor{
        .Identity =
            {
                .Authority = third.Id.Authority,
                .Incarnation = third.Id.Incarnation,
                .Kind = core::comparison_anchor_kind::review_commit,
                .Review = identity<core::review_association_id>(19U),
            },
        .Commit = commit('4'),
        .Availability = core::anchor_availability::available,
    };
    control.publish(comparison_update(core::named_selection(
        core::named_comparison::my_last_reviewed_to_latest),
        {
            .Left = review_anchor,
            .Right = reverse.Left,
        }));
    expect(control.last_reviewed_action()->isChecked()
            && control.left_selector()->currentIndex() == -1
            && control.left_oid_editor()->text() == QString(40, QLatin1Char('4')),
        "a distinct review anchor stays selected and copyable without being "
        "invented as an epoch row");

    auto sha256_left = review_anchor;
    sha256_left.Identity.Review = identity<core::review_association_id>(20U);
    sha256_left.Commit = commit('a', 64U);
    auto sha256_right = review_anchor;
    sha256_right.Identity.Review = identity<core::review_association_id>(21U);
    sha256_right.Commit = commit('b', 64U);
    control.publish(comparison_update(core::explicit_selection(
        sha256_left.Identity, sha256_right.Identity),
        {.Left = sha256_left, .Right = sha256_right}));
    expect(control.left_oid_editor()->text().size() == 64
            && control.right_oid_editor()->text().size() == 64
            && control.left_oid_editor()->text() == QString(64, QLatin1Char('a')),
        "SHA-256 comparisons preserve every exact OID character");
}

auto test_explicit_loading_empty_failure_and_availability_states() -> void
{
    qt::epoch_comparison_control control;
    control.publish(application::epoch_presentation_update{
        .Transition =
            application::epoch_transition_projection{
                .Status = core::epoch_status_delta{
                    .Status = core::epoch_status::loading_catalog,
                },
            },
    });
    expect(control.status_text().contains(QStringLiteral("Loading")),
        "catalog loading has an explicit visible state");

    control.publish(loaded_catalog_update({}));
    expect(control.status_text().contains(QStringLiteral("No review epochs")),
        "an exhausted empty catalog has an explicit visible state");

    auto never_observed = epoch_row(3, '1', '2');
    never_observed.HeadAvailability =
        core::anchor_availability::never_observed;
    control.publish(loaded_catalog_update({never_observed}));
    control.publish(comparison_update(
        core::named_selection(core::named_comparison::previous_upload_to_latest),
        {
            .Left = anchor(never_observed,
                core::comparison_anchor_kind::epoch_head,
                core::anchor_availability::never_observed),
            .Right = anchor(never_observed,
                core::comparison_anchor_kind::epoch_base),
        },
        false,
        core::epoch_reason_code::unavailable));
    expect(control.history_model()
                   ->data(control.history_model()->index(1, 0), Qt::DisplayRole)
                   .toString()
                   .contains(QStringLiteral("Never observed"))
            && control.comparison_text().contains(
                QStringLiteral("never observed")),
        "never-observed material is explicit and never remapped");

    control.publish(application::epoch_presentation_update{
        .Transition =
            application::epoch_transition_projection{
                .Status = core::epoch_status_delta{
                    .Status = core::epoch_status::preparing,
                },
            },
        .Catalog =
            application::epoch_catalog_projection{
                .Catalog = catalog(),
                .HistoryPage =
                    application::epoch_history_page_projection{
                        .Phase = core::epoch_history_page_phase::loading,
                        .Catalog = catalog(),
                    },
            },
    });
    expect(control.status_text().contains(QStringLiteral("retained")),
        "refresh and page loading retain the accepted history visibly");

    control.publish(application::epoch_presentation_update{
        .Catalog =
            application::epoch_catalog_projection{
                .Catalog = catalog(),
                .HistoryPage =
                    application::epoch_history_page_projection{
                        .Phase = core::epoch_history_page_phase::failed,
                        .Catalog = catalog(),
                        .Reason = core::epoch_reason_code::quota_exceeded,
                    },
            },
    });
    expect(control.status_text().contains(QStringLiteral("could not be loaded"))
            && control.status_text().contains(QStringLiteral("limit"))
            && control.history_model()->epoch_count() == 1U,
        "a failed page retains rows and presents the typed reason");

    control.publish(application::epoch_presentation_update{
        .Catalog =
            application::epoch_catalog_projection{
                .Catalog = catalog(),
                .HistoryPage =
                    application::epoch_history_page_projection{
                        .Phase = core::epoch_history_page_phase::cancelled,
                        .Catalog = catalog(),
                        .Reason = core::epoch_reason_code::cancelled,
                    },
            },
    });
    expect(control.status_text().contains(QStringLiteral("canceled"))
            && control.history_model()->epoch_count() == 1U,
        "a canceled page retains rows and has an explicit visible state");

    control.publish(application::epoch_presentation_update{
        .Catalog =
            application::epoch_catalog_projection{
                .Catalog = catalog(8),
                .AllEpochAvailability =
                    core::anchor_availability::cache_deleted,
            },
    });
    expect(control.history_model()
                   ->data(control.history_model()->index(0, 0), Qt::DisplayRole)
                   .toString()
                   .contains(QStringLiteral("Cache deleted")),
        "whole-cache deletion is visible on every retained anchor row");
}

auto test_focus_appearance_accessibility_and_announcements() -> void
{
    qt::epoch_comparison_control control;
    control.resize(960, 240);
    control.show();
    const auto third = epoch_row(3, '1', '2');
    const auto second = epoch_row(2, '1', '3');
    control.publish(loaded_catalog_update({third, second}));
    control.publish(comparison_update(
        core::named_selection(core::named_comparison::whole_pr),
        {
            .Left = anchor(third, core::comparison_anchor_kind::epoch_base),
            .Right = anchor(third, core::comparison_anchor_kind::epoch_head),
        }));
    QApplication::processEvents();

    control.left_selector()->setFocus(Qt::OtherFocusReason);
    const QPersistentModelIndex stable{
        control.history_model()->index_for_anchor(
            anchor(second, core::comparison_anchor_kind::epoch_head).Identity)};
    const auto resets_before = control.history_model()->statistics().ModelResets;
    const auto fourth = epoch_row(4, '1', '4');
    control.publish(application::epoch_presentation_update{
        .Catalog =
            application::epoch_catalog_projection{
                .Catalog = catalog(8),
                .Changes =
                    {
                        application::epoch_row_change_projection{
                            .Kind = application::epoch_row_change_kind::insert,
                            .Index = 0U,
                            .Id = fourth.Id,
                            .Row = fourth,
                        },
                    },
            },
    });
    expect(QApplication::focusWidget() == control.left_selector()
            && stable.isValid() && stable.row() == 5
            && control.history_model()->statistics().ModelResets
                == resets_before,
        "routine projection preserves focus, identity, and zero-reset model state");

    auto* const left_accessible =
        QAccessible::queryAccessibleInterface(control.left_selector());
    auto* const oid_accessible =
        QAccessible::queryAccessibleInterface(control.left_oid_editor());
    expect(left_accessible != nullptr
            && !left_accessible->text(QAccessible::Name).isEmpty()
            && left_accessible->state().focusable && oid_accessible != nullptr
            && !oid_accessible->text(QAccessible::Name).isEmpty()
            && oid_accessible->state().readOnly,
        "selectors and exact OIDs expose named focusable/read-only accessibility");

    const auto appearance_before = control.statistics().AppearanceUpdates;
    const auto resets_at_appearance =
        control.history_model()->statistics().ModelResets;
    auto changed_font = control.font();
    changed_font.setPointSizeF(changed_font.pointSizeF() + 1.0);
    control.setFont(changed_font);
    QApplication::processEvents();
    expect(control.statistics().AppearanceUpdates > appearance_before
            && control.history_model()->statistics().ModelResets
                == resets_at_appearance,
        "system appearance changes refresh presentation roles without a reset");

    const auto announcements = control.statistics().Announcements;
    const auto same = comparison_update(
        core::named_selection(core::named_comparison::whole_pr),
        {
            .Left = anchor(third, core::comparison_anchor_kind::epoch_base),
            .Right = anchor(third, core::comparison_anchor_kind::epoch_head),
        });
    control.publish(same);
    const auto after_first = control.statistics().Announcements;
    control.publish(same);
    expect(after_first >= announcements
            && control.statistics().Announcements == after_first,
        "one combined bounded announcement is deduplicated across identical updates");
}

[[nodiscard]] auto pull_request_projection()
    -> application::github_pull_request_projection
{
    return {
        .Profile = core::host_profile_id{1U},
        .Host = core::git_host_id{2U},
        .Account = core::github_account_id{3U},
        .Snapshot = core::provider_snapshot_id{4U},
        .Repository = core::repository_id{5U},
        .PullRequest = core::pull_request_id{6U},
        .Owner = "better-pull-review",
        .RepositoryName = "desktop",
        .Number = core::pull_request_number{7U},
        .Title = "Epoch comparison render fixture",
        .Body =
            "The real loaded-content composition contains the reusable "
            "comparison control.",
        .AuthorLogin = "reviewer",
        .BaseRef = "main",
        .HeadRef = "feature/epochs",
        .BaseOid = std::string(40U, '1'),
        .HeadOid = std::string(40U, '2'),
        .FileCount = 3U,
        .CommitCount = 4U,
    };
}

auto test_real_loaded_window_composition_renders() -> void
{
    qt::github_window window{
        qt::github_view_config{},
        [](application::github_intent) {},
    };
    window.publish(application::github_presentation_update{
        .Transition =
            application::github_transition_projection{
                .Status = core::github_status_delta{
                    .Status = core::github_status::pull_request_ready,
                },
            },
        .PullRequest = pull_request_projection(),
    });
    const auto third = epoch_row(3, '1', '2');
    const auto second = epoch_row(2, '1', '3');
    window.epoch_control()->publish(loaded_catalog_update({third, second}));
    window.epoch_control()->publish(comparison_update(
        core::named_selection(core::named_comparison::whole_pr),
        {
            .Left = anchor(third, core::comparison_anchor_kind::epoch_base),
            .Right = anchor(third, core::comparison_anchor_kind::epoch_head),
        }));
    window.resize(1'100, 820);
    window.show();
    QApplication::processEvents();
    expect(window.epoch_control()->isVisible()
            && window.epoch_control()->left_selector()->isVisible()
            && window.epoch_control()->right_selector()->isVisible(),
        "the reusable control is reachable in the real loaded-content window");

    const QPixmap rendered = window.grab();
    const auto scale = window.devicePixelRatioF();
    expect(!rendered.isNull()
            && rendered.width()
                == static_cast<int>(std::lround(window.width() * scale))
            && rendered.height()
                == static_cast<int>(std::lround(window.height() * scale)),
        "the integrated window renders at the exact active device scale");
    const auto path = qEnvironmentVariable("BPR_QT_EPOCH_RENDER_PATH");
    if(!path.isEmpty())
    {
        expect(rendered.save(path, "PNG"),
            "the integrated epoch render is saved only in the build tree");
    }
}
}

auto main(int argc, char** argv) -> int
{
    QApplication application{argc, argv};
    test_model_applies_stable_incremental_rows();
    test_control_translates_exact_selection_and_preserves_projection();
    test_explicit_loading_empty_failure_and_availability_states();
    test_focus_appearance_accessibility_and_announcements();
    test_real_loaded_window_composition_renders();
    std::cout << "epoch-comparison-control-tests:pass\n";
    return EXIT_SUCCESS;
}
