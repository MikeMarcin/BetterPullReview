#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QTimeZone>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xa5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(
		const char        aDigit,
		const std::size_t aLength = 40U) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(std::string(aLength, aDigit));
		expect(parsed.has_value(), "fixture commit OID parses");
		return *parsed;
	}

	[[nodiscard]] auto catalog(const std::int64_t aRevision = 7)
		-> core::epoch_catalog_identity
	{
		return {
			.Incarnation = identity<core::epoch_catalog_incarnation_id>(2U),
			.Revision    = core::epoch_catalog_revision{ aRevision },
		};
	}

	[[nodiscard]] auto authority(const std::uint8_t aSeed = 1U)
		-> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 11U + aSeed },
			.ProfileRevision = core::github_profile_revision{ 21U + aSeed },
			.Host            = core::git_host_id{ 31U + aSeed },
			.HostAuthority   = identity<core::host_authority_fingerprint>(2U + aSeed),
			.Account         = core::github_account_id{ 41U + aSeed },
			.Viewer          = core::github_viewer_actor_id{ 51U + aSeed },
			.Repository      = core::repository_id{ 61U + aSeed },
			.Cache           = identity<core::repository_cache_scope_id>(3U + aSeed),
			.PullRequest     = core::pull_request_id{ 71U + aSeed },
			.Number          = core::pull_request_number{ 81U + aSeed },
			.Scope           = identity<core::pull_request_authority_scope_id>(1U),
		};
	}

	[[nodiscard]] auto epoch_id(const std::int64_t aOrdinal)
		-> core::review_epoch_id
	{
		return {
			.Authority   = identity<core::pull_request_authority_scope_id>(1U),
			.Incarnation = catalog().Incarnation,
			.Ordinal     = core::epoch_ordinal{ aOrdinal },
		};
	}

	[[nodiscard]] auto epoch_row(
		const std::int64_t              aOrdinal,
		const char                      aBase,
		const char                      aHead,
		const core::epoch_primary_cause aPrimary = core::epoch_primary_cause::author_upload)
		-> application::epoch_row_projection
	{
		return {
			.Id       = epoch_id(aOrdinal),
			.Snapshot = core::provider_snapshot_id{
				static_cast<std::uint64_t>(aOrdinal) },
			.IdentityGeneration    = core::github_identity_generation{ 3U },
			.PullRequestGeneration = core::request_generation{ 4U },
			.BaseBranch            = identity<core::base_branch_id>(5U),
			.Base                  = commit(aBase),
			.Head                  = commit(aHead),
			.Cause                 = {
				.Origin   = core::epoch_origin::live_observation,
				.Base     = core::epoch_base_transition::unchanged,
				.Head     = aPrimary == core::epoch_primary_cause::force_push_rebase ? core::epoch_head_transition::non_fast_forward : core::epoch_head_transition::descendant,
				.Material = core::epoch_material_relation::different,
				.Primary  = aPrimary,
			},
			.ObservedAt       = core::observed_utc_milliseconds{ 1'700'000'000'000LL + aOrdinal },
			.BaseAvailability = core::anchor_availability::available,
			.HeadAvailability = core::anchor_availability::available,
		};
	}

	[[nodiscard]] auto anchor(
		const application::epoch_row_projection& aRow,
		const core::comparison_anchor_kind       aKind,
		const core::anchor_availability          aAvailability = core::anchor_availability::available) -> core::comparison_anchor
	{
		return {
			.Identity = {
				.Authority   = aRow.Id.Authority,
				.Incarnation = aRow.Id.Incarnation,
				.Kind        = aKind,
				.Epoch       = aRow.Id.Ordinal,
			},
			.Commit       = aKind == core::comparison_anchor_kind::epoch_base ? aRow.Base : aRow.Head,
			.Availability = aAvailability,
		};
	}

	[[nodiscard]] auto loaded_catalog_update(
		std::vector<application::epoch_row_projection>   aRows,
		const std::optional<core::epoch_history_cursor>& aContinuation = std::nullopt,
		const core::epoch_history_navigation             aNavigation   = core::epoch_history_navigation::newest)
		-> application::epoch_presentation_update
	{
		const auto currentCatalog = catalog();
		return {
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = currentCatalog,
				.HistoryPage = application::epoch_history_page_projection{
					.Phase        = core::epoch_history_page_phase::loaded,
					.Catalog      = currentCatalog,
					.Navigation   = aNavigation,
					.Limit        = core::maximum_epoch_history_page_size,
					.Rows         = std::move(aRows),
					.Continuation = aContinuation,
				},
			},
		};
	}

	[[nodiscard]] auto enabled_commands() -> core::epoch_command_delta
	{
		return {
			.Commands = {
				core::epoch_command_projection{
					.Id             = core::epoch_command_id::refresh_epochs,
					.Enabled        = true,
					.DisabledReason = core::epoch_reason_code::none,
				},
				core::epoch_command_projection{
					.Id             = core::epoch_command_id::select_comparison,
					.Enabled        = true,
					.DisabledReason = core::epoch_reason_code::none,
				},
				core::epoch_command_projection{
					.Id             = core::epoch_command_id::prune_selected_epochs,
					.Enabled        = true,
					.DisabledReason = core::epoch_reason_code::none,
				},
				core::epoch_command_projection{
					.Id             = core::epoch_command_id::delete_repository_cache,
					.Enabled        = true,
					.DisabledReason = core::epoch_reason_code::none,
				},
				core::epoch_command_projection{
					.Id             = core::epoch_command_id::cancel_epoch_refresh,
					.Enabled        = true,
					.DisabledReason = core::epoch_reason_code::none,
				},
				core::epoch_command_projection{
					.Id             = core::epoch_command_id::cancel_epoch_maintenance,
					.Enabled        = true,
					.DisabledReason = core::epoch_reason_code::none,
				},
			},
		};
	}

	[[nodiscard]] auto comparison_update(
		const core::comparison_selection& aSelection,
		const core::comparison_pair&      aPair,
		const bool                        aAvailable = true,
		const core::epoch_reason_code     aReason    = core::epoch_reason_code::none)
		-> application::epoch_presentation_update
	{
		return {
			.Transition = application::epoch_transition_projection{
				.Changed = core::epoch_changed_domain::selection
					| core::epoch_changed_domain::comparison
					| core::epoch_changed_domain::authority
					| core::epoch_changed_domain::status
					| core::epoch_changed_domain::commands,
				.Authority = application::epoch_authority_projection{
					.Authority  = authority(),
					.Generation = core::epoch_generation{ 8U },
				},
				.Selection = core::comparison_selection_delta{
					.Selection = aSelection,
				},
				.Comparison = application::epoch_comparison_projection{
					.Selection        = aSelection,
					.Pair             = aPair,
					.Generation       = core::comparison_generation{ 9U },
					.Revision         = core::comparison_revision{ 3 },
					.AvailableForDiff = aAvailable,
					.Reason           = aReason,
				},
				.Status = core::epoch_status_delta{
					.Status = core::epoch_status::ready,
				},
				.Commands = enabled_commands(),
			},
		};
	}

	struct confirmation_observation
	{
		QString Text;
		QString AccessibleName;
		bool    CancelWasDefault = false;
	};

	void respond_to_confirmation(const QString&            aObjectName,
								 const bool                aConfirm,
								 confirmation_observation& aObservation,
								 std::function<void()>     aBeforeResponse = { })
	{
		QTimer::singleShot(0,
						   qApp,
						   [aObjectName,
							aConfirm,
							&aObservation,
							beforeResponse = std::move(aBeforeResponse)] {
							   auto* const dialog = qobject_cast<QMessageBox*>(
								   QApplication::activeModalWidget());
							   expect(dialog != nullptr && dialog->objectName() == aObjectName,
									  "the expected Qt-owned confirmation dialog is active");
							   aObservation.Text           = dialog->text();
							   aObservation.AccessibleName = dialog->accessibleName();
							   auto* const cancel          = dialog->findChild<QPushButton*>(
								   aObjectName + QStringLiteral("CancelButton"));
							   auto* const confirmed = dialog->findChild<QPushButton*>(
								   aObjectName + QStringLiteral("ConfirmButton"));
							   aObservation.CancelWasDefault = cancel != nullptr
								   && dialog->defaultButton() == cancel;
							   expect(cancel != nullptr && confirmed != nullptr,
									  "confirmation exposes stable cancel and destructive buttons");
							   if (beforeResponse) {
								   beforeResponse();
							   }
							   (aConfirm ? confirmed : cancel)->click();
						   });
	}

	auto test_model_applies_stable_incremental_rows() -> void
	{
		qt::epoch_history_model model;
		std::size_t             inserted = 0U;
		std::size_t             removed  = 0U;
		std::size_t             moved    = 0U;
		std::size_t             changed  = 0U;
		QObject::connect(&model,
						 &QAbstractItemModel::rowsInserted,
						 [&inserted](const QModelIndex&, const int aFirst, const int aLast) {
							 inserted += static_cast<std::size_t>(aLast - aFirst + 1);
						 });
		QObject::connect(&model,
						 &QAbstractItemModel::rowsRemoved,
						 [&removed](const QModelIndex&, const int aFirst, const int aLast) {
							 removed += static_cast<std::size_t>(aLast - aFirst + 1);
						 });
		QObject::connect(&model,
						 &QAbstractItemModel::rowsMoved,
						 [&moved](const QModelIndex&,
								  const int aFirst,
								  const int aLast,
								  const QModelIndex&,
								  const int) {
							 moved += static_cast<std::size_t>(aLast - aFirst + 1);
						 });
		QObject::connect(&model,
						 &QAbstractItemModel::dataChanged,
						 [&changed](const QModelIndex& aFirst, const QModelIndex& aLast) {
							 changed += static_cast<std::size_t>(aLast.row() - aFirst.row() + 1);
						 });

		const auto third  = epoch_row(3, '1', '2');
		const auto second = epoch_row(2, '1', '3',
									  core::epoch_primary_cause::force_push_rebase);
		model.apply(*loaded_catalog_update({ third, second }).Catalog);
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

		const QPersistentModelIndex stableSecondHead{
			model.index_for_anchor(anchor(second,
										  core::comparison_anchor_kind::epoch_head)
									   .Identity)
		};
		const auto fourth = epoch_row(4, '1', '4');
		model.apply(application::epoch_catalog_projection{
			.Catalog = catalog(8),
			.Changes = {
				application::epoch_row_change_projection{
					.Kind  = application::epoch_row_change_kind::insert,
					.Index = 0U,
					.Id    = fourth.Id,
					.Row   = fourth,
				},
			},
		});
		expect(stableSecondHead.isValid() && stableSecondHead.row() == 5
				   && model.statistics().ModelResets == 0U,
			   "an insertion preserves exact persistent anchor identity without reset");

		model.apply(application::epoch_catalog_projection{
			.Catalog = catalog(9),
			.Changes = {
				application::epoch_row_change_projection{
					.Kind  = application::epoch_row_change_kind::insert,
					.Index = 0U,
					.Id    = second.Id,
					.Row   = second,
				},
			},
		});
		expect(moved == 2U && stableSecondHead.isValid()
				   && stableSecondHead.row() == 1,
			   "an existing stable epoch moves by an exact two-anchor move signal");

		model.apply(application::epoch_catalog_projection{
			.Catalog                   = catalog(10),
			.AvailabilityChangedEpochs = { second.Id },
			.AvailabilityChangedTo     = core::anchor_availability::pruned,
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
			.Changes = {
				application::epoch_row_change_projection{
					.Kind = application::epoch_row_change_kind::remove,
					.Id   = third.Id,
				},
			},
		});
		expect(removed == 2U && model.index_for_anchor(anchor(third, core::comparison_anchor_kind::epoch_head).Identity).isValid() == false,
			   "removal emits an exact two-anchor removal and removes only its ID");

		model.apply(application::epoch_catalog_projection{
			.Catalog              = catalog(12),
			.AllEpochAvailability = core::anchor_availability::cache_deleted,
		});
		expect(model.data(model.index(0, 0),
						  qt::epoch_history_model::availability_role)
						   .toInt()
					   == static_cast<int>(core::anchor_availability::cache_deleted)
				   && model.statistics().ModelResets == 0U,
			   "bulk cache deletion updates resident rows without a model reset");
	}

	auto test_model_replaces_more_than_one_thousand_bounded_pages() -> void
	{
		qt::epoch_history_model                        model;
		std::vector<application::epoch_row_projection> newest;
		newest.reserve(core::maximum_epoch_history_page_size);
		for (std::int64_t ordinal = 1'100; ordinal >= 1'001; --ordinal) {
			newest.push_back(epoch_row(ordinal, '1', '2'));
		}
		model.apply(*loaded_catalog_update(std::move(newest)).Catalog);
		expect(model.epoch_count() == core::maximum_epoch_history_page_size
				   && model.rowCount()
					   == static_cast<int>(
						   core::maximum_epoch_history_page_size * 2U)
				   && model.statistics().ModelResets == 0U,
			   "the newest page retains exactly the fixed 100-epoch/200-anchor "
			   "working set without reset");

		const auto                                     retained = epoch_row(1'001U, '1', '2');
		const QPersistentModelIndex                    retainedHead{ model.index_for_anchor(
			anchor(retained, core::comparison_anchor_kind::epoch_head).Identity) };
		std::vector<application::epoch_row_projection> overlapping;
		overlapping.reserve(core::maximum_epoch_history_page_size);
		for (std::int64_t ordinal = 1'001; ordinal >= 902; --ordinal) {
			overlapping.push_back(epoch_row(ordinal, '1', '2'));
		}
		model.apply(*loaded_catalog_update(std::move(overlapping),
										   std::nullopt,
										   core::epoch_history_navigation::older)
						 .Catalog);
		expect(retainedHead.isValid() && retainedHead.row() == 1
				   && model.epoch_count() == core::maximum_epoch_history_page_size
				   && model.statistics().ModelResets == 0U,
			   "page replacement preserves an overlapping stable anchor through "
			   "incremental removal and movement");

		for (std::int64_t ordinal = 1'001; ordinal != 0; --ordinal) {
			const auto before = model.statistics();
			model.apply(*loaded_catalog_update(
							 { epoch_row(ordinal, '1', '2') },
							 std::nullopt,
							 core::epoch_history_navigation::older)
							 .Catalog);
			const auto after = model.statistics();
			expect(model.epoch_count() == 1U && model.rowCount() == 2
					   && model.data(model.index(0, 0),
									 qt::epoch_history_model::epoch_ordinal_role)
							   .toLongLong()
						   == ordinal
					   && after.ModelResets == 0U
					   && after.RowsRemoved - before.RowsRemoved <= 200U
					   && after.RowsInserted - before.RowsInserted <= 2U
					   && after.RowsMoved - before.RowsMoved <= 2U
					   && after.RowsChanged - before.RowsChanged <= 2U,
				   "each of more than one thousand older pages performs bounded "
				   "resident work without accumulating history");
		}
		expect(!retainedHead.isValid() && model.epoch_count() == 1U
				   && model.data(model.index(0, 0),
								 qt::epoch_history_model::epoch_ordinal_role)
						   .toULongLong()
					   == 1U,
			   "incremental paging reaches ordinal one and legally invalidates an "
			   "anchor only after it leaves the resident page");

		std::vector<application::epoch_row_projection> exactBound;
		exactBound.reserve(core::maximum_epoch_history_page_size);
		for (std::int64_t ordinal = 200; ordinal >= 101; --ordinal) {
			exactBound.push_back(epoch_row(ordinal, '1', '2'));
		}
		model.apply(*loaded_catalog_update(std::move(exactBound)).Catalog);
		const auto latest = epoch_row(300U, '1', '2');
		model.apply(application::epoch_catalog_projection{
			.Catalog = catalog(8),
			.Changes = {
				application::epoch_row_change_projection{
					.Kind  = application::epoch_row_change_kind::insert,
					.Index = 0U,
					.Id    = latest.Id,
					.Row   = latest,
				},
			},
		});
		expect(model.epoch_count() == core::maximum_epoch_history_page_size
				   && model.index_for_anchor(
							   anchor(latest,
									  core::comparison_anchor_kind::epoch_base)
								   .Identity)
						   .row()
					   == 0
				   && model.statistics().ModelResets == 0U,
			   "an incremental newest-row projection evicts the resident tail and "
			   "cannot exceed the fixed working-set cap");
	}

	auto test_control_translates_exact_selection_and_preserves_projection() -> void
	{
		std::vector<application::epoch_intent> intents;
		qt::epoch_comparison_control           control{
			[&intents](application::epoch_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		// [[BPR-0676]]. Before anything is compared the line says so, rather
		// than leaving a reviewer to read two marks and guess.
		expect(control.line_summary() != nullptr
				   && control.line_summary()->text() == QStringLiteral("no pair yet"),
			   "the line says when nothing is being compared yet");
		const auto third        = epoch_row(3, '1', '2');
		const auto second       = epoch_row(2, '1', '3');
		const auto continuation = core::epoch_history_cursor{
			.Authority   = third.Id.Authority,
			.Incarnation = third.Id.Incarnation,
			.Revision    = catalog().Revision,
			.Before      = core::epoch_ordinal{ 2 },
		};
		control.publish(loaded_catalog_update({ third, second }, continuation));
		const auto wholePair = core::comparison_pair{
			.Left  = anchor(third, core::comparison_anchor_kind::epoch_base),
			.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
		};
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr), wholePair));

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
				   && !control.load_history_action()->shortcut().isEmpty()
				   && !control.return_to_newest_action()->shortcut().isEmpty(),
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
				   && page->Navigation == core::epoch_history_navigation::older
				   && page->Cursor == continuation
				   && page->Limit == core::maximum_epoch_history_page_size,
			   "history action dispatches the exact bounded continuation");

		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = catalog(8),
				.HistoryPage = application::epoch_history_page_projection{
					.Phase      = core::epoch_history_page_phase::invalidated,
					.Catalog    = catalog(),
					.Navigation = core::epoch_history_navigation::newest,
					.Reason     = core::epoch_reason_code::catalog_revision_mismatch,
				},
			},
		});
		expect(control.history_model()->epoch_count() == 2U
				   && !control.load_history_action()->isEnabled()
				   && control.return_to_newest_action()->isEnabled()
				   && !control.left_selector()->isEnabled()
				   && !control.right_selector()->isEnabled(),
			   "a catalog revision retains the stale page read-only until an exact "
			   "newest restart");
		control.return_to_newest_action()->trigger();
		page = std::get_if<core::request_epoch_history_page_intent>(
			&intents.back());
		expect(page != nullptr && page->Catalog == catalog(8)
				   && page->Navigation == core::epoch_history_navigation::newest
				   && !page->Cursor,
			   "a catalog revision change discards the stale cursor and reloads "
			   "from the exact new revision");
		const auto newestContinuation = core::epoch_history_cursor{
			.Authority   = third.Id.Authority,
			.Incarnation = third.Id.Incarnation,
			.Revision    = catalog(8).Revision,
			.Before      = core::epoch_ordinal{ 2 },
		};
		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = catalog(8),
				.HistoryPage = application::epoch_history_page_projection{
					.Phase        = core::epoch_history_page_phase::loaded,
					.Catalog      = catalog(8),
					.Navigation   = core::epoch_history_navigation::newest,
					.Limit        = core::maximum_epoch_history_page_size,
					.Rows         = { third, second },
					.Continuation = newestContinuation,
				},
			},
		});
		expect(control.history_model()->epoch_count() == 2U
				   && control.left_selector()->isEnabled()
				   && control.right_selector()->isEnabled()
				   && control.load_history_action()->isEnabled()
				   && !control.return_to_newest_action()->isEnabled(),
			   "a successful newest page incrementally replaces stale content and "
			   "restores selection plus exact older navigation");
		control.refresh_action()->trigger();
		expect(std::holds_alternative<core::refresh_epochs_intent>(intents.back()),
			   "refresh action emits only the semantic refresh intent");
		control.cancel_action()->trigger();
		expect(std::holds_alternative<core::cancel_epoch_refresh_intent>(
				   intents.back()),
			   "cancel action emits only the semantic cancellation intent");

		const auto reverse = core::comparison_pair{
			.Left  = anchor(third, core::comparison_anchor_kind::epoch_head),
			.Right = anchor(second, core::comparison_anchor_kind::epoch_head),
		};
		control.publish(comparison_update(core::explicit_selection(
											  reverse.Left.Identity, reverse.Right.Identity),
										  reverse));
		expect(control.left_selector()->currentIndex() == 1
				   && control.right_selector()->currentIndex() == 3
				   && !control.whole_pr_action()->isChecked()
				   && control.comparison_text().contains(
					   QStringLiteral("Exact epoch pair")),
			   "reverse exact pairs remain visible without alias remapping");
		// [[BPR-0676]]. The owner could not tell whether the line was working.
		// It says so on the line now, in three words, with the long sentence
		// from the details region as its tooltip.
		expect(control.line_summary() != nullptr
				   && control.line_summary()->text() == QStringLiteral("pair ready")
				   && control.line_summary()->toolTip() == control.comparison_text(),
			   "the line says a readable pair is compared, and carries the detail");
		const auto identical = core::comparison_pair{
			.Left  = reverse.Right,
			.Right = reverse.Right,
		};
		control.publish(comparison_update(core::explicit_selection(
											  identical.Left.Identity, identical.Right.Identity),
										  identical));
		expect(control.left_oid_editor()->text()
					   == control.right_oid_editor()->text()
				   && control.left_selector()->currentIndex()
					   == control.right_selector()->currentIndex(),
			   "identical exact pairs remain representable");

		const auto unavailable = core::comparison_pair{
			.Left  = anchor(second,
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
		for (const auto& [reason, expected] : std::array<std::pair<core::epoch_reason_code, QString>, 4U>{ {
				 { core::epoch_reason_code::git_tool_unsupported,
				   QStringLiteral("Configure Git 2.49 or later with exact revision cloning, then retry.") },
				 { core::epoch_reason_code::git_tool_probe_failed,
				   QStringLiteral("The configured Git could not be checked. Check its executable and retry.") },
				 { core::epoch_reason_code::gh_tool_unsupported,
				   QStringLiteral("Configure GitHub CLI 2.96 or later with its api, repo clone and git credential commands, then retry.") },
				 { core::epoch_reason_code::gh_tool_probe_failed,
				   QStringLiteral("The configured GitHub CLI could not be checked. Check its executable and retry.") },
			 } }) {
			control.publish(comparison_update(core::named_selection(
												  core::named_comparison::previous_upload_to_latest),
											  unavailable, false, reason));
			expect(control.comparison_text().contains(expected)
					   && control.line_summary()->text() == QStringLiteral("pair not readable"),
				   "a Git or GitHub CLI refusal presents its own fixed actionable reason without raw subprocess text");
		}
		// [[BPR-0676]]. A pair that cannot be read says so on the line, not
		// only inside the details a reviewer has to open.
		expect(control.line_summary()->text() == QStringLiteral("pair not readable"),
			   "the line says when the pair it names cannot be read");

		const auto reviewAnchor = core::comparison_anchor{
			.Identity = {
				.Authority   = third.Id.Authority,
				.Incarnation = third.Id.Incarnation,
				.Kind        = core::comparison_anchor_kind::review_commit,
				.Review      = identity<core::review_association_id>(19U),
			},
			.Commit       = commit('4'),
			.Availability = core::anchor_availability::available,
		};
		control.publish(comparison_update(core::named_selection(
											  core::named_comparison::my_last_reviewed_to_latest),
										  {
											  .Left  = reviewAnchor,
											  .Right = reverse.Left,
										  }));
		expect(control.last_reviewed_action()->isChecked()
				   && control.left_selector()->currentIndex() == -1
				   && control.left_oid_editor()->text() == QString(40, QLatin1Char('4')),
			   "a distinct review anchor stays selected and copyable without being "
			   "invented as an epoch row");

		auto sha256Left             = reviewAnchor;
		sha256Left.Identity.Review  = identity<core::review_association_id>(20U);
		sha256Left.Commit           = commit('a', 64U);
		auto sha256Right            = reviewAnchor;
		sha256Right.Identity.Review = identity<core::review_association_id>(21U);
		sha256Right.Commit          = commit('b', 64U);
		control.publish(comparison_update(core::explicit_selection(
											  sha256Left.Identity, sha256Right.Identity),
										  { .Left = sha256Left, .Right = sha256Right }));
		expect(control.left_oid_editor()->text().size() == 64
				   && control.right_oid_editor()->text().size() == 64
				   && control.left_oid_editor()->text() == QString(64, QLatin1Char('a')),
			   "SHA-256 comparisons preserve every exact OID character");

		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr), wholePair));
		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = catalog(8),
				.HistoryPage = application::epoch_history_page_projection{
					.Phase      = core::epoch_history_page_phase::loaded,
					.Catalog    = catalog(8),
					.Navigation = core::epoch_history_navigation::older,
					.Limit      = core::maximum_epoch_history_page_size,
					.Rows       = { second },
				},
			},
		});
		expect(control.left_selector()->currentIndex() == -1
				   && control.right_selector()->currentIndex() == -1
				   && control.left_oid_editor()->text() == QString(40, QLatin1Char('1'))
				   && control.right_oid_editor()->text() == QString(40, QLatin1Char('2'))
				   && control.left_selector()->accessibleDescription().contains(
					   QStringLiteral("outside the loaded history window"))
				   && control.right_selector()->accessibleDescription().contains(
					   QStringLiteral("outside the loaded history window"))
				   && control.return_to_newest_action()->isEnabled(),
			   "an exact selected pair remains fully copyable and accessibly "
			   "identified after both anchors leave the one-page working set");
	}

	/// \brief RFC-BPR-0028's epoch line over the same projections the
	/// selectors read.
	/// BPR-0725. On a cache hit the host publishes the catalogue identity and
	/// then the core's newest page; the control holds no rows of its own and
	/// must not ask for them. Until the page lands the line stays empty, which
	/// is exactly when it paints "No updates loaded."; the page then draws one
	/// node per update with the ready pair's handles.
	auto test_epoch_line_waits_for_the_newest_page_on_a_cached_head() -> void
	{
		std::vector<application::epoch_intent> sent;
		qt::epoch_comparison_control           control{
			[&sent](application::epoch_intent aIntent) {
				sent.push_back(std::move(aIntent));
			},
		};
		control.resize(700, 80);
		control.show();
		QApplication::processEvents();
		auto* const line = control.line();

		const auto latest  = epoch_row(2, '1', '3');
		const auto earlier = epoch_row(1, '1', '2');
		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{ .Catalog = catalog() },
		});
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr),
			{
				.Left  = anchor(earlier, core::comparison_anchor_kind::epoch_base),
				.Right = anchor(latest, core::comparison_anchor_kind::epoch_head),
			}));
		QApplication::processEvents();
		expect(line != nullptr && line->update_count() == 0U
				   && control.line_summary()->text() == QStringLiteral("pair ready")
				   && std::ranges::none_of(
					   sent,
					   [](const application::epoch_intent& aIntent) {
						   return std::holds_alternative<
							   core::request_epoch_history_page_intent>(aIntent);
					   }),
			   "a catalogue identity and a ready pair leave the line empty, and "
			   "so reading \"No updates loaded.\", without Qt requesting a page");

		control.publish(loaded_catalog_update({ latest, earlier }));
		QApplication::processEvents();
		expect(line->update_count() == 2U
				   && line->left_stop()
					   == qt::epoch_line_stop{
						   .Update = 0U,
						   .Kind   = core::comparison_anchor_kind::epoch_base }
				   && line->right_stop() == qt::epoch_line_stop{ .Update = 1U, .Kind = core::comparison_anchor_kind::epoch_head },
			   "the newest page fed after the ready pair draws every update with "
			   "the pair's handles");
	}

	// Presentation evidence only: host ingestion and cross-catalogue identity
	// acceptance belong to BPR-0743's separate production-path checks.
	[[nodiscard]] auto pull_request_projection() -> application::github_pull_request_projection;

	auto test_occurrence_evidence_and_unavailable_gestures() -> void
	{
		std::vector<application::epoch_intent> intents;
		qt::workbench_window                   window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			{ },
			[](application::github_intent) { },
		};
		window.publish(application::github_presentation_update{
			.PullRequest = pull_request_projection(),
		});
		auto& control = *window.epoch_selectors();
		control.set_dispatch([&intents](application::epoch_intent aIntent) {
			intents.push_back(std::move(aIntent));
		});
		window.resize(1100, 820);
		window.show();
		const auto                     current = epoch_row(12, '1', '2');
		auto                           update  = loaded_catalog_update({ current });
		core::epoch_occurrence_history history;
		history.Ambiguous        = true;
		history.UnresolvedLegacy = true;
		history.Occurrences      = {
			{ .Id                    = identity<core::epoch_occurrence_id>(8),
			  .Evidence              = { .Kind = core::epoch_occurrence_kind::force_push_event, .EventId = 100, .Head = commit('2') },
			  .DerivedComparisonBase = commit('3'),
			  .DateSource            = core::epoch_occurrence_date_source::github_event,
			  .DateUtcMilliseconds   = 1'700'000'000'000,
			  .FirstSeen             = core::observed_utc_milliseconds{ 1'800'000'000'000 } },
			{ .Id                  = identity<core::epoch_occurrence_id>(9),
			  .Evidence            = { .Kind = core::epoch_occurrence_kind::observed_head, .Head = commit('4') },
			  .DateSource          = core::epoch_occurrence_date_source::commit_date,
			  .DateUtcMilliseconds = 1'700'000'000'000,
			  .FirstSeen           = core::observed_utc_milliseconds{ 1'800'000'000'000 } },
		};
		update.Catalog->OccurrenceHistory = history;
		update.Catalog->CurrentEpoch      = current;
		control.publish(update);
		control.resize(1100, 100);
		control.show();
		QApplication::processEvents();
		auto* line = control.line();
		expect(line->update_count() == 3 && line->label_text(0).startsWith(QStringLiteral("Event time"))
				   && line->label_text(1).startsWith(QStringLiteral("Commit date"))
				   && line->label_text(2).contains(QStringLiteral("Provisional")),
			   "event and commit dates stay distinct and unmatched current snapshot is provisional");
		expect(line->node_tooltip(0).contains(QStringLiteral("Historical base unknown"))
				   && line->node_tooltip(0).contains(QStringLiteral("Derived comparison base 3333"))
				   && line->node_tooltip(0).contains(QStringLiteral("Comparison unavailable"))
				   && line->node_tooltip(0).contains(QStringLiteral("Local first-seen"))
				   && line->node_tooltip(1).contains(QStringLiteral("Derived comparison base unavailable"))
				   && line->history_text().contains(QStringLiteral("ambiguous"))
				   && line->history_text().contains(QStringLiteral("unresolved"))
				   && line->history_text().contains(QStringLiteral("not push order")),
			   "unknown historical base, derived comparison material and history gaps remain explicit");
		// The default small-review projection must keep unavailable occurrences inert.
		line->set_selection_offered(true);
		const auto unavailableCount  = intents.size();
		const auto unavailableLeft   = line->left_stop();
		const auto unavailableRight  = line->right_stop();
		const auto unavailableStatus = control.status_text();
		auto       unavailablePress  = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ line->node_centre(0) },
													QPointF{ line->mapToGlobal(line->node_centre(0)) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(line, &unavailablePress);
		expect(intents.size() == unavailableCount && line->left_stop() == unavailableLeft
				   && line->right_stop() == unavailableRight && control.status_text() == unavailableStatus,
			   "small-review unavailable occurrence click preserves handles and status without dispatch");
		update.Catalog->AllowOccurrenceImport = true;
		const auto firstTooltip               = line->node_tooltip(0);
		const auto secondTooltip              = line->node_tooltip(1);
		std::ranges::reverse(history.Occurrences);
		std::ranges::sort(history.Occurrences, core::epoch_occurrence_less);
		update.Catalog->OccurrenceHistory = history;
		control.publish(update);
		expect(line->node_tooltip(0) == firstTooltip && line->node_tooltip(1) == secondTooltip
				   && line->node_centre(0).x() < line->node_centre(1).x()
				   && line->accessibleDescription().contains(QStringLiteral("not push order"))
				   && !window.grab().isNull(),
			   "the real workbench preserves core portable-key order for equal mixed-source dates and exposes the no-push-order qualification");
		line->set_selection_offered(true);
		const auto before = intents.size();
		auto       press  = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ line->node_centre(0) },
										 QPointF{ line->mapToGlobal(line->node_centre(0)) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(line, &press);
		expect(intents.size() == before + 1 && std::holds_alternative<core::select_epoch_occurrence_intent>(intents.back()), "an unimported force-push dispatches a typed occurrence choice");
		const auto selectedOccurrence = std::get<core::select_epoch_occurrence_intent>(intents.back());
		expect(selectedOccurrence.Occurrence == history.Occurrences.front().Id && selectedOccurrence.Side == core::comparison_side::left
				   && selectedOccurrence.ExpectedCatalog == *control.history_model()->catalog(),
			   "occurrence gesture carries the exact ID, side and projected catalog");
		auto rightPress = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ line->node_centre(0) }, QPointF{ line->mapToGlobal(line->node_centre(0)) }, Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier };
		QApplication::sendEvent(line, &rightPress);
		expect(std::get<core::select_epoch_occurrence_intent>(intents.back()).Side == core::comparison_side::right, "modified click imports onto the right side");
		const auto afterImport   = intents.size();
		auto       observedPress = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ line->node_centre(1) }, QPointF{ line->mapToGlobal(line->node_centre(1)) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(line, &observedPress);
		line->set_selection_offered(false);
		QApplication::sendEvent(line, &press);
		expect(intents.size() == afterImport, "unlinked observed-head evidence and disabled input do not import");
		line->set_selection_offered(true);
		// [[BPR-1130]]. No caption row: the ambiguous history is a mark at the
		// line's start, and the statement is the tooltip and description.
		expect(line->height() == qt::epoch_line::height_pixels && line->history_marked()
				   && line->statement() == line->history_text() && line->accessibleDescription() == line->statement(),
			   "the line is the nodes and dates alone, marked, with its statement as tooltip and description");
		history.Occurrences.front().LocalEpoch = current.Id;
		history.Occurrences.front().DerivedComparisonBase.reset();
		history.Occurrences.back().DateSource = core::epoch_occurrence_date_source::unavailable;
		update.Catalog->OccurrenceHistory     = history;
		control.publish(update);
		// [[BPR-1340]]. An update with neither an event time nor a commit date
		// is labelled by when it was first seen here, never "Date unavailable".
		const auto firstSeen = QLocale{ }.toString(QDateTime::fromMSecsSinceEpoch(1'800'000'000'000, QTimeZone::UTC).toLocalTime().date(), QStringLiteral("MMM d"));
		expect(line->update_count() == 2 && line->label_text(1) == QStringLiteral("First seen %1").arg(firstSeen)
				   && !line->node_tooltip(1).contains(QStringLiteral("Commit date"))
				   && line->node_tooltip(1).contains(QStringLiteral("Push and commit dates unavailable")),
			   "exact local association removes only the provisional duplicate and an undated update says when it was first seen");
		expect(line->node_tooltip(0).contains(QStringLiteral("Historical base unknown"))
				   && line->node_tooltip(0).contains(QStringLiteral("Locally observed base 1111")),
			   "a live association exposes the observed base without claiming historical provenance");
		std::optional<core::comparison_anchor_identity> chosen;
		line->set_dispatch([&chosen](core::comparison_side, core::comparison_anchor_identity aAnchor) { chosen = aAnchor; });
		line->set_selection_offered(true);
		line->set_selection(anchor(current, core::comparison_anchor_kind::epoch_head).Identity, std::nullopt);
		auto left = QKeyEvent{ QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier };
		QApplication::sendEvent(line, &left);
		expect(chosen == anchor(current, core::comparison_anchor_kind::epoch_base).Identity,
			   "the matched live occurrence can step to its observed base without a derived or historical base");
		const auto next                       = epoch_row(13, '3', '4');
		history.Occurrences.back().LocalEpoch = next.Id;
		update                                = loaded_catalog_update({ next, current });
		update.Catalog->OccurrenceHistory     = history;
		update.Catalog->CurrentEpoch          = next;
		control.publish(update);
		expect(line->update_count() == 2
				   && line->node_tooltip(0).contains(QStringLiteral("Locally observed base 1111"))
				   && line->node_tooltip(0).contains(QStringLiteral("Historical base unknown"))
				   && line->node_tooltip(1).contains(QStringLiteral("Locally observed base 3333")),
			   "advancing the live epoch preserves each occurrence's distinct locally observed base");
		chosen.reset();
		line->set_selection_offered(true);
		line->set_selection(anchor(current, core::comparison_anchor_kind::epoch_head).Identity, std::nullopt);
		QApplication::sendEvent(line, &left);
		expect(chosen == anchor(current, core::comparison_anchor_kind::epoch_base).Identity,
			   "the earlier live update retains its base stop after a newer live update arrives");
		auto imported                                     = epoch_row(11, '3', '2', core::epoch_primary_cause::explicit_historical_import);
		imported.Cause.Origin                             = core::epoch_origin::explicit_historical_import;
		imported.Cause.Base                               = core::epoch_base_transition::not_applicable;
		imported.Cause.Head                               = core::epoch_head_transition::not_applicable;
		history.Occurrences.front().LocalEpoch            = imported.Id;
		history.Occurrences.front().DerivedComparisonBase = imported.Base;
		update                                            = loaded_catalog_update({ imported, current });
		update.Catalog->OccurrenceHistory                 = history;
		update.Catalog->CurrentEpoch                      = current;
		control.publish(update);
		expect(line->node_tooltip(0).contains(QStringLiteral("Historical base unknown"))
				   && line->node_tooltip(0).contains(QStringLiteral("Derived comparison base 3333"))
				   && !line->node_tooltip(0).contains(QStringLiteral("Locally observed base")),
			   "an imported epoch never labels its derived comparison anchor as a local observation");
	}

	/// \brief [[BPR-1130]]. RFC-BPR-0052 AC-04, as RFC-BPR-0029 requires: the
	/// line carries no permanent caption. Its statement is its tooltip and
	/// accessible description, and a mark at its start says when history is
	/// incomplete or an occurrence could not be matched; complete history
	/// carries no mark.
	auto test_the_line_marks_incomplete_history_without_a_caption() -> void
	{
		qt::epoch_comparison_control control;
		control.resize(900, 60);
		control.show();
		QApplication::processEvents();
		auto* const line = control.line();
		expect(line->height() == qt::epoch_line::height_pixels && qt::epoch_line::height_pixels == 34
				   && !line->history_marked() && line->statement() == line->empty_status(),
			   "the line is one 34-pixel row, unmarked while it knows no updates");

		const auto earlier  = epoch_row(11, '1', '3');
		const auto current  = epoch_row(12, '1', '2');
		auto       update   = loaded_catalog_update({ current, earlier });
		auto       history  = core::epoch_occurrence_history{ };
		history.Incomplete  = false;
		history.Occurrences = {
			{ .Id                  = identity<core::epoch_occurrence_id>(7),
			  .Evidence            = { .Kind = core::epoch_occurrence_kind::observed_head, .Head = earlier.Head },
			  .LocalEpoch          = earlier.Id,
			  .DateSource          = core::epoch_occurrence_date_source::commit_date,
			  .DateUtcMilliseconds = 1'600'000'000'000,
			  .FirstSeen           = core::observed_utc_milliseconds{ 1'800'000'000'000 } },
			{ .Id                  = identity<core::epoch_occurrence_id>(8),
			  .Evidence            = { .Kind = core::epoch_occurrence_kind::observed_head, .Head = current.Head },
			  .LocalEpoch          = current.Id,
			  .DateSource          = core::epoch_occurrence_date_source::commit_date,
			  .DateUtcMilliseconds = 1'700'000'000'000,
			  .FirstSeen           = core::observed_utc_milliseconds{ 1'800'000'000'000 } },
		};
		update.Catalog->OccurrenceHistory = history;
		update.Catalog->CurrentEpoch      = current;
		control.publish(update);
		QApplication::processEvents();
		const auto unmarkedNode = line->node_centre(0U).x();
		expect(!line->history_marked() && line->statement() == QStringLiteral("Known updates · display order is not push order")
				   && line->accessibleDescription() == line->statement() && line->toolTip() == line->statement(),
			   "complete history carries no mark, and its statement is the tooltip and the description");
		const auto away = QPoint{ line->width() - 3, qt::epoch_line::height_pixels - 2 };
		auto       help = QHelpEvent{ QEvent::ToolTip, away, line->mapToGlobal(away) };
		QApplication::sendEvent(line, &help);
		// [[BPR-1354]]. Directly below the line, left edges aligned, not at
		// the pointer.
		const auto anchoredBelow = [](const QWidget& aTip, const QRect& aAnchor) {
			const auto area = QGuiApplication::screenAt(aAnchor.center()) != nullptr ? QGuiApplication::screenAt(aAnchor.center())->availableGeometry() : aTip.screen()->availableGeometry();
			return aTip.geometry() == qt::anchored_tooltip_geometry(aAnchor, aTip.size(), area)
				&& (aTip.geometry().top() == aAnchor.bottom() + 1 + qt::anchored_tooltip_gap || aTip.geometry().bottom() + 1 + qt::anchored_tooltip_gap == aAnchor.top());
		};
		const auto* statementTip = qobject_cast<const QLabel*>(qt::anchored_tooltip());
		expect(statementTip != nullptr && statementTip->text() == line->statement() && !QToolTip::isVisible()
				   && anchoredBelow(*statementTip, QRect{ line->mapToGlobal(QPoint{ 0, 0 }), line->size() }),
			   "away from a node the tooltip is the history statement, directly below the line");
		qt::hide_anchored_tooltip();
		// A node is painted, with no widget of its own: its tooltip sits below
		// the node and its label, the band the pointer finds it in.
		const auto node     = line->node_centre(0U);
		auto       nodeHelp = QHelpEvent{ QEvent::ToolTip, node, line->mapToGlobal(node) };
		QApplication::sendEvent(line, &nodeHelp);
		const auto* nodeTip    = qobject_cast<const QLabel*>(qt::anchored_tooltip());
		const auto  nodeAnchor = QRect{ line->mapToGlobal(QPoint{ node.x() - 7, 0 }), QSize{ 15, 30 } };
		expect(nodeTip != nullptr && !nodeTip->text().isEmpty() && nodeTip->text() != line->statement() && !QToolTip::isVisible() && anchoredBelow(*nodeTip, nodeAnchor),
			   "a node's tooltip names its update, directly below the node and its label");
		qt::hide_anchored_tooltip();

		const auto markInk = [line] {
			// The exclamation's stroke, inside the ring before the track.
			const auto raster = line->grab().toImage();
			const auto scale  = raster.devicePixelRatio();
			// The row's own surface, clear of the focus ring at its edge and of
			// the one node, which sits at its middle.
			const auto ground = raster.pixelColor(qRound(line->width() * scale * 0.75), qRound(3 * scale));
			auto       ink    = 0;
			for (auto y = qRound(4 * scale); y <= qRound(6 * scale); ++y) {
				for (auto x = qRound(18 * scale); x <= qRound(19 * scale); ++x) {
					ink += raster.pixelColor(x, y) != ground ? 1 : 0;
				}
			}
			return ink > 0;
		};
		expect(!markInk(), "no mark is painted for complete history");

		for (const auto& [incomplete, ambiguous, unresolved, label] : std::array{
				 std::tuple{ true, false, false, "incomplete" },
				 std::tuple{ false, true, false, "ambiguous" },
				 std::tuple{ false, false, true, "unresolved" },
			 }) {
			history.Incomplete                = incomplete;
			history.Ambiguous                 = ambiguous;
			history.UnresolvedLegacy          = unresolved;
			update.Catalog->OccurrenceHistory = history;
			control.publish(update);
			QApplication::processEvents();
			expect(line->history_marked() && markInk() && line->node_centre(0U).x() == unmarkedNode + qt::epoch_line::history_mark_size + 6
					   && line->statement().contains(QStringLiteral("not push order")) && line->toolTip() == line->statement(),
				   std::string{ label } + " history is marked before the track, which starts after the mark");
		}

		// Local records alone, with no occurrence history, are incomplete too.
		qt::epoch_comparison_control local;
		local.publish(loaded_catalog_update({ current, earlier }));
		expect(local.line()->history_marked()
				   && local.line()->statement() == QStringLiteral("Incomplete history — local records only"),
			   "local records alone are marked incomplete");
	}

	/// \brief [[BPR-1129]]. The review bar takes the "Comparing" button; the
	/// control keeps it current wherever it is placed.
	auto test_the_comparing_button_moves_to_the_review_bar() -> void
	{
		qt::epoch_comparison_control control{ };
		control.resize(700, 80);
		control.show();
		QApplication::processEvents();
		auto* const name = control.findChild<QLabel*>(QStringLiteral("epochLineName"));
		expect(name != nullptr && control.isAncestorOf(control.comparing_button()),
			   "before it is released the button and the line's name are the control's");
		auto  bar      = QWidget{ };
		auto* released = control.release_comparing_button();
		expect(released == control.comparing_button() && released != nullptr && released->parentWidget() == nullptr
				   && control.release_comparing_button() == nullptr,
			   "the button is released once, to no parent, for the caller to place");
		released->setParent(&bar);
		QApplication::processEvents();
		expect(control.findChild<QLabel*>(QStringLiteral("epochLineName")) == nullptr && control.line()->isVisible(),
			   "the line keeps its row and gives its Updates name to the bar");
		control.publish(loaded_catalog_update({ epoch_row(2, '3', '4'), epoch_row(1, '1', '2') }));
		QApplication::processEvents();
		expect(released->text().startsWith(QStringLiteral("Comparing")) && released->menu() != nullptr
				   && released->menu()->actions().contains(control.whole_pr_action()),
			   "the control keeps the released button's text and menu current");
	}

	/// \brief [[BPR-1340]]. The owner could drag neither handle of the whole-PR
	/// pair. It puts the left handle on the newest update's base and the right
	/// on its head, seven pixels apart, and a press on the left handle took
	/// the right one. Each handle is now the nearer one to a press, and after
	/// a step the line keeps the keyboard while the comparison resolves.
	auto test_epoch_line_handles_on_one_node_drag_apart_and_keep_the_keyboard() -> void
	{
		std::vector<application::epoch_intent> intents;
		qt::epoch_comparison_control           control{
			[&intents](application::epoch_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		control.resize(700, 80);
		control.show();
		QApplication::processEvents();
		auto* const line   = control.line();
		const auto  third  = epoch_row(3, '1', '4');
		const auto  second = epoch_row(2, '1', '3');
		const auto  first  = epoch_row(1, '1', '2');
		control.publish(loaded_catalog_update({ third, second, first }));
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr),
			{
				.Left  = anchor(third, core::comparison_anchor_kind::epoch_base),
				.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
			}));
		QApplication::processEvents();
		expect(line->selection_offered() && line->left_stop() == qt::epoch_line_stop{ .Update = 2U, .Kind = core::comparison_anchor_kind::epoch_base }
				   && line->right_stop() == qt::epoch_line_stop{ .Update = 2U, .Kind = core::comparison_anchor_kind::epoch_head },
			   "the whole-PR pair puts both handles on the newest update");
		const auto drag = [line](const QPoint& aFrom, const QPoint& aTo) {
			auto press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aFrom }, QPointF{ line->mapToGlobal(aFrom) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(line, &press);
			auto move = QMouseEvent{ QEvent::MouseMove, QPointF{ aTo }, QPointF{ line->mapToGlobal(aTo) }, Qt::NoButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(line, &move);
			auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ aTo }, QPointF{ line->mapToGlobal(aTo) }, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(line, &release);
		};
		const auto chosen = [&intents]() -> const core::select_comparison_anchor_intent* {
			return intents.size() == 1U ? std::get_if<core::select_comparison_anchor_intent>(&intents.back()) : nullptr;
		};
		intents.clear();
		drag(line->stop_position(*line->left_stop()), line->node_centre(0));
		expect(chosen() != nullptr && chosen()->Side == core::comparison_side::left
				   && chosen()->Anchor == anchor(first, core::comparison_anchor_kind::epoch_head).Identity,
			   "a drag that starts on the left handle moves the left handle, not the right one beside it");
		intents.clear();
		drag(line->stop_position(*line->right_stop()), line->node_centre(1));
		expect(chosen() != nullptr && chosen()->Side == core::comparison_side::right
				   && chosen()->Anchor == anchor(second, core::comparison_anchor_kind::epoch_head).Identity,
			   "a drag that starts on the right handle moves the right handle");
		const auto between = (line->stop_position(*line->left_stop()) + line->stop_position(*line->right_stop())) / 2;
		const auto nearer  = line->stop_position(*line->left_stop()).x() + 2;
		intents.clear();
		drag(QPoint{ nearer, between.y() }, line->node_centre(0));
		expect(chosen() != nullptr && chosen()->Side == core::comparison_side::left,
			   "a press nearer the left handle takes the left handle");

		// The keyboard alternative: a step starts a comparison, the readiness
		// line stands in for the line while it resolves, and the line has the
		// keyboard again when it returns, so the next step is its too.
		auto* const before = new QPushButton{ QStringLiteral("Before"), &control };
		before->setFocusPolicy(Qt::StrongFocus);
		before->show();
		line->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		expect(control.window()->focusWidget() == line, "the line has the keyboard before the step");
		control.set_readiness(QStringLiteral("Pull request loaded. Preparing file comparisons…"), true);
		QApplication::processEvents();
		expect(line->isHidden() && control.window()->focusWidget() != line,
			   "while the comparison resolves the line is hidden and Qt moves the keyboard on");
		control.set_readiness(QString{ }, true);
		QApplication::processEvents();
		expect(!line->isHidden() && control.window()->focusWidget() == line,
			   "the line has the keyboard back when it returns");
		control.set_readiness(QStringLiteral("Pull request loaded. Preparing file comparisons…"), true);
		auto* const elsewhere = new QLineEdit{ &control };
		elsewhere->show();
		elsewhere->setFocus(Qt::OtherFocusReason);
		control.set_readiness(QString{ }, true);
		QApplication::processEvents();
		expect(control.window()->focusWidget() == elsewhere,
			   "focus the reviewer moved elsewhere meanwhile stays where they put it");
		delete elsewhere;
		delete before;
	}

	/// \brief [[BPR-1553]]. The owner (2026-10-16): "It used to have a left
	/// and right head you could move to choose which versions you were
	/// comparing but I cannot find those at all anymore." On a review read
	/// with selected objects (every review within the read limits since
	/// 0bfcbaa9a) an update GitHub recorded is chosen by importing it, and the
	/// occurrence history grants that. The next catalogue projection, which
	/// does not carry the grant, took it back, so the GitHub-recorded updates
	/// were not stops and both handles sat on the one update seen here, at
	/// the line's end. In each built-in theme both handles are painted, each
	/// drags onto a GitHub-recorded update, the keys step there too, and the
	/// imported pair moves the handles and the span between them.
	auto test_github_recorded_updates_stay_reachable_by_both_handles() -> void
	{
		const auto modes = std::array{
			std::pair{ "light", core::appearance_mode::light },
			std::pair{ "dark", core::appearance_mode::dark },
			std::pair{ "high-contrast-light", core::appearance_mode::high_contrast_light },
			std::pair{ "high-contrast-dark", core::appearance_mode::high_contrast_dark },
		};
		for (const auto& [name, mode] : modes) {
			const auto                             theme = std::string{ name };
			std::vector<application::epoch_intent> intents;
			qt::epoch_comparison_control           control{
				[&intents](application::epoch_intent aIntent) {
					intents.push_back(std::move(aIntent));
				},
			};
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(control);
			control.resize(700, 80);
			control.show();
			QApplication::processEvents();
			auto* const line = control.line();

			// Two force pushes GitHub recorded, neither imported, then the
			// update this install observed, which is the current one.
			const auto                     current = epoch_row(12, '1', '2');
			core::epoch_occurrence_history history;
			history.Incomplete  = true;
			history.Occurrences = {
				{ .Id                  = identity<core::epoch_occurrence_id>(8),
				  .Evidence            = { .Kind = core::epoch_occurrence_kind::force_push_event, .EventId = 100, .Head = commit('3') },
				  .DateSource          = core::epoch_occurrence_date_source::github_event,
				  .DateUtcMilliseconds = 1'700'000'000'000,
				  .FirstSeen           = core::observed_utc_milliseconds{ 1'800'000'000'000 } },
				{ .Id                  = identity<core::epoch_occurrence_id>(9),
				  .Evidence            = { .Kind = core::epoch_occurrence_kind::force_push_event, .EventId = 101, .Head = commit('4') },
				  .DateSource          = core::epoch_occurrence_date_source::github_event,
				  .DateUtcMilliseconds = 1'700'000'100'000,
				  .FirstSeen           = core::observed_utc_milliseconds{ 1'800'000'000'000 } },
				{ .Id                  = identity<core::epoch_occurrence_id>(10),
				  .Evidence            = { .Kind = core::epoch_occurrence_kind::observed_head, .Head = current.Head },
				  .LocalEpoch          = current.Id,
				  .HistoricalBase      = current.Base,
				  .DateSource          = core::epoch_occurrence_date_source::commit_date,
				  .DateUtcMilliseconds = 1'700'000'200'000,
				  .FirstSeen           = core::observed_utc_milliseconds{ 1'800'000'000'000 } },
			};
			auto granted                           = loaded_catalog_update({ current });
			granted.Catalog->OccurrenceHistory     = history;
			granted.Catalog->CurrentEpoch          = current;
			granted.Catalog->AllowOccurrenceImport = true;
			control.publish(granted);
			control.publish(comparison_update(
				core::named_selection(core::named_comparison::whole_pr),
				{
					.Left  = anchor(current, core::comparison_anchor_kind::epoch_base),
					.Right = anchor(current, core::comparison_anchor_kind::epoch_head),
				}));
			// The host's next catalogue projection: the accepted epoch's page,
			// which says nothing about occurrences or their import.
			control.publish(loaded_catalog_update({ current }));
			QApplication::processEvents();
			expect(line->isVisible() && line->selection_offered() && line->update_count() == 3U
					   && line->left_stop() == qt::epoch_line_stop{ .Update = 2U, .Kind = core::comparison_anchor_kind::epoch_base }
					   && line->right_stop() == qt::epoch_line_stop{ .Update = 2U, .Kind = core::comparison_anchor_kind::epoch_head },
				   theme + ": the whole-PR pair puts both handles on the update seen here");

			// Both handles are painted in the theme's accent, away from the
			// node and the track: the left one's left side, the right one's
			// right side, on the line's top rows.
			const auto image  = line->grab().toImage();
			const auto ratio  = image.devicePixelRatio();
			const auto accent = line->palette().color(QPalette::Active, QPalette::Highlight);
			const auto ground = line->palette().color(QPalette::Active, QPalette::Window);
			const auto inked  = [&](const double aX) {
				auto best = std::numeric_limits<int>::max();
				// Half-pixel steps over the handle's top rows and its side.
				for (auto row = 4; row <= 10; ++row) {
					for (auto column = -3; column <= 3; ++column) {
						const auto x     = aX + (0.5 * column);
						const auto y     = 0.5 * row;
						const auto pixel = image.pixelColor(static_cast<int>(std::lround(x * ratio)), static_cast<int>(std::lround(y * ratio)));
						best             = std::min(best, std::abs(pixel.red() - accent.red()) + std::abs(pixel.green() - accent.green()) + std::abs(pixel.blue() - accent.blue()));
					}
				}
				return best;
			};
			const auto separation = std::abs(accent.red() - ground.red()) + std::abs(accent.green() - ground.green()) + std::abs(accent.blue() - ground.blue());
			const auto leftEdge   = static_cast<double>(line->stop_position(*line->left_stop()).x()) - 3.5;
			const auto rightEdge  = static_cast<double>(line->stop_position(*line->right_stop()).x()) + 3.5;
			const auto emptyEdge  = static_cast<double>(line->node_centre(0U).x() + line->node_centre(1U).x()) / 2.0;
			expect(separation >= 60 && inked(leftEdge) * 3 < separation && inked(rightEdge) * 3 < separation && inked(emptyEdge) * 3 >= separation * 2,
				   theme + ": both handles are painted in the accent, which stands off the line's ground");

			const auto drag = [line](const QPoint& aFrom, const QPoint& aTo) {
				auto press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aFrom }, QPointF{ line->mapToGlobal(aFrom) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(line, &press);
				auto move = QMouseEvent{ QEvent::MouseMove, QPointF{ aTo }, QPointF{ line->mapToGlobal(aTo) }, Qt::NoButton, Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(line, &move);
				auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ aTo }, QPointF{ line->mapToGlobal(aTo) }, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
				QApplication::sendEvent(line, &release);
			};
			const auto imported = [&intents](const core::comparison_side aSide, const core::epoch_occurrence_id aOccurrence) {
				const auto* const chosen = intents.size() == 1U ? std::get_if<core::select_epoch_occurrence_intent>(&intents.back()) : nullptr;
				return chosen != nullptr && chosen->Side == aSide && chosen->Occurrence == aOccurrence && chosen->ExpectedCatalog == catalog();
			};
			intents.clear();
			drag(line->stop_position(*line->right_stop()), line->node_centre(1U));
			expect(imported(core::comparison_side::right, history.Occurrences[1].Id),
				   theme + ": the right handle drags onto the newer GitHub-recorded update, which is imported to the right");
			intents.clear();
			drag(line->stop_position(*line->left_stop()), line->node_centre(0U));
			expect(imported(core::comparison_side::left, history.Occurrences[0].Id),
				   theme + ": the left handle drags onto the older GitHub-recorded update, which is imported to the left");
			line->setFocus(Qt::OtherFocusReason);
			intents.clear();
			auto shiftLeft = QKeyEvent{ QEvent::KeyPress, Qt::Key_Left, Qt::ShiftModifier };
			QApplication::sendEvent(line, &shiftLeft);
			expect(imported(core::comparison_side::right, history.Occurrences[1].Id),
				   theme + ": Shift+Left steps the right handle onto the GitHub-recorded update before it");

			// The import lands: the older update is an epoch of this install,
			// and the core's pair puts the handles on it and the current one.
			auto earlier                          = epoch_row(11, '5', '3', core::epoch_primary_cause::explicit_historical_import);
			earlier.Cause.Origin                  = core::epoch_origin::explicit_historical_import;
			earlier.Cause.Base                    = core::epoch_base_transition::not_applicable;
			earlier.Cause.Head                    = core::epoch_head_transition::not_applicable;
			history.Occurrences[0].LocalEpoch     = earlier.Id;
			auto landed                           = loaded_catalog_update({ current, earlier });
			landed.Catalog->OccurrenceHistory     = history;
			landed.Catalog->CurrentEpoch          = current;
			landed.Catalog->AllowOccurrenceImport = true;
			control.publish(landed);
			control.publish(comparison_update(
				core::explicit_selection(anchor(earlier, core::comparison_anchor_kind::epoch_head).Identity,
										 anchor(current, core::comparison_anchor_kind::epoch_head).Identity),
				{
					.Left  = anchor(earlier, core::comparison_anchor_kind::epoch_head),
					.Right = anchor(current, core::comparison_anchor_kind::epoch_head),
				}));
			QApplication::processEvents();
			const auto spanned = line->grab().toImage();
			const auto middle  = QPoint{ line->node_centre(1U).x() + 12, line->node_centre(1U).y() };
			const auto bar     = spanned.pixelColor(static_cast<int>(std::lround(middle.x() * ratio)), static_cast<int>(std::lround(middle.y() * ratio)));
			expect(line->left_stop() == qt::epoch_line_stop{ .Update = 0U, .Kind = core::comparison_anchor_kind::epoch_head }
					   && line->right_stop() == qt::epoch_line_stop{ .Update = 2U, .Kind = core::comparison_anchor_kind::epoch_head }
					   && (std::abs(bar.red() - accent.red()) + std::abs(bar.green() - accent.green()) + std::abs(bar.blue() - accent.blue())) * 3 < separation,
				   theme + ": the imported pair moves the handles apart, with the accent span between them");
		}
	}

	auto test_epoch_line_selects_updates_by_click_drag_and_keys() -> void
	{
		std::vector<application::epoch_intent> intents;
		qt::epoch_comparison_control           control{
			[&intents](application::epoch_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		control.resize(700, 80);
		control.show();
		QApplication::processEvents();
		auto* const line = control.line();
		expect(line != nullptr && line->isVisible() && !control.details_visible()
				   && control.details() != nullptr && !control.details()->isVisible()
				   && line->height() == qt::epoch_line::height_pixels
				   && line->update_count() == 0U,
			   "the control shows the epoch line and hides the exact selectors "
			   "until they are asked for");

		// Three updates, newest first as the pages deliver them.
		const auto third  = epoch_row(3, '1', '4');
		const auto second = epoch_row(2, '1', '3');
		const auto first  = epoch_row(1, '1', '2');
		control.publish(loaded_catalog_update({ third, second, first }));
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr),
			{
				.Left  = anchor(first, core::comparison_anchor_kind::epoch_base),
				.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
			}));
		QApplication::processEvents();
		expect(line->update_count() == 3U
				   && line->left_stop()
					   == qt::epoch_line_stop{
						   .Update = 0U,
						   .Kind   = core::comparison_anchor_kind::epoch_base }
				   && line->right_stop() == qt::epoch_line_stop{ .Update = 2U, .Kind = core::comparison_anchor_kind::epoch_head } && line->node_centre(0).x() < line->node_centre(1).x() && line->node_centre(1).x() < line->node_centre(2).x() && line->stop_position(*line->left_stop()).x() < line->node_centre(0).x(),
			   "the line reads oldest to newest with the whole-PR handles on the "
			   "oldest base and the newest head");
		expect(line->label_text(2).startsWith(QStringLiteral("First seen"))
				   && !line->label_text(2).contains(QStringLiteral("latest"))
				   && line->label_shown(0) && line->label_shown(1)
				   && line->label_shown(2)
				   && line->node_tooltip(1).contains(QStringLiteral("Update 2 of 3"))
				   && line->node_tooltip(1).contains(QStringLiteral("upload")),
			   "legacy labels identify local first-seen without claiming a latest update");
		// [[BPR-1129]]. The button names the comparison and the dates of the
		// two compared updates, the bare dates the line's labels carry.
		const auto dates = line->selection_dates();
		expect(dates.has_value() && line->label_text(0).endsWith(dates->first)
				   && line->label_text(2).endsWith(dates->second),
			   "the compared updates' dates are the bare dates of their labels");
		expect(control.comparing_button() != nullptr
				   && control.comparing_button()->text()
					   == QStringLiteral("Comparing whole PR, %1 → %2").arg(dates->first, dates->second)
				   && control.comparing_button()->menu() != nullptr
				   && control.comparing_button()->menu()->actions().contains(
					   control.last_reviewed_action()),
			   "the comparing menu names the alias in force with its dates and "
			   "carries the named comparisons");
		control.set_comparing_dates_shown(false);
		expect(control.comparing_button()->text() == QStringLiteral("Comparing whole PR"),
			   "where the review bar is narrow the button drops the dates and keeps the name");
		control.set_comparing_dates_shown(true);
		expect(line->selection_offered(),
			   "with the select command offered the line takes gestures");

		// Click a node: compare from it, its head on the left.
		const auto clickAt = [line](const QPoint& aPoint, const Qt::KeyboardModifiers aModifiers) {
			auto press = QMouseEvent{
				QEvent::MouseButtonPress,
				QPointF{ aPoint },
				QPointF{ line->mapToGlobal(aPoint) },
				Qt::LeftButton,
				Qt::LeftButton,
				aModifiers,
			};
			QApplication::sendEvent(line, &press);
			auto release = QMouseEvent{
				QEvent::MouseButtonRelease,
				QPointF{ aPoint },
				QPointF{ line->mapToGlobal(aPoint) },
				Qt::LeftButton,
				Qt::NoButton,
				aModifiers,
			};
			QApplication::sendEvent(line, &release);
		};
		intents.clear();
		clickAt(line->node_centre(1), Qt::NoModifier);
		const auto* fromHere = intents.size() == 1U
			? std::get_if<core::select_comparison_anchor_intent>(&intents.back())
			: nullptr;
		expect(fromHere != nullptr && fromHere->Side == core::comparison_side::left
				   && fromHere->Anchor
					   == anchor(second, core::comparison_anchor_kind::epoch_head).Identity
				   && fromHere->ExpectedCatalogRevision == catalog().Revision,
			   "clicking a node asks to compare from that update's head, once, "
			   "bound to the catalog revision");
		expect(line->left_stop()->Update == 0U,
			   "the line shows what was published, not what was asked");

		// Cmd-click: compare to it.
		intents.clear();
		clickAt(line->node_centre(1), Qt::ControlModifier);
		const auto* toHere = intents.size() == 1U
			? std::get_if<core::select_comparison_anchor_intent>(&intents.back())
			: nullptr;
		expect(toHere != nullptr && toHere->Side == core::comparison_side::right
				   && toHere->Anchor
					   == anchor(second, core::comparison_anchor_kind::epoch_head).Identity,
			   "Cmd-clicking a node asks to compare to that update's head");

		// Keyboard: Right steps the left handle from the oldest base to its
		// head; Shift+Left steps the right handle back one update.
		intents.clear();
		line->setFocus();
		auto right = QKeyEvent{ QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier };
		QApplication::sendEvent(line, &right);
		const auto* stepped = intents.size() == 1U
			? std::get_if<core::select_comparison_anchor_intent>(&intents.back())
			: nullptr;
		expect(stepped != nullptr && stepped->Side == core::comparison_side::left
				   && stepped->Anchor
					   == anchor(first, core::comparison_anchor_kind::epoch_head).Identity,
			   "Right steps the left handle one stop, from the oldest base to "
			   "its head");
		intents.clear();
		auto shiftLeft = QKeyEvent{ QEvent::KeyPress, Qt::Key_Left, Qt::ShiftModifier };
		QApplication::sendEvent(line, &shiftLeft);
		const auto* steppedRight = intents.size() == 1U
			? std::get_if<core::select_comparison_anchor_intent>(&intents.back())
			: nullptr;
		expect(steppedRight != nullptr
				   && steppedRight->Side == core::comparison_side::right
				   && steppedRight->Anchor
					   == anchor(second, core::comparison_anchor_kind::epoch_head).Identity,
			   "Shift+Left steps the right handle back one update");
		intents.clear();
		auto left = QKeyEvent{ QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier };
		QApplication::sendEvent(line, &left);
		expect(intents.empty(),
			   "the left handle at its first stop has nowhere further left to go");

		// Drag the right handle onto the middle node.
		intents.clear();
		const auto rightHandle = line->stop_position(*line->right_stop());
		auto       press       = QMouseEvent{
			QEvent::MouseButtonPress,
			QPointF{ rightHandle },
			QPointF{ line->mapToGlobal(rightHandle) },
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(line, &press);
		const auto target = line->node_centre(1);
		auto       move   = QMouseEvent{
			QEvent::MouseMove,
			QPointF{ target },
			QPointF{ line->mapToGlobal(target) },
			Qt::NoButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(line, &move);
		auto release = QMouseEvent{
			QEvent::MouseButtonRelease,
			QPointF{ target },
			QPointF{ line->mapToGlobal(target) },
			Qt::LeftButton,
			Qt::NoButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(line, &release);
		const auto* dragged = intents.size() == 1U
			? std::get_if<core::select_comparison_anchor_intent>(&intents.back())
			: nullptr;
		expect(dragged != nullptr && dragged->Side == core::comparison_side::right
				   && dragged->Anchor
					   == anchor(second, core::comparison_anchor_kind::epoch_head).Identity,
			   "dragging the right handle onto a node asks for that update's head "
			   "on the right, once, on release");

		// The published pair moves the handles; my last review gets its tick.
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::my_last_reviewed_to_latest),
			{
				.Left  = anchor(second, core::comparison_anchor_kind::epoch_head),
				.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
			}));
		control.publish(application::epoch_presentation_update{
			.Transition = application::epoch_transition_projection{
				.Changed   = core::epoch_changed_domain::authority,
				.Authority = application::epoch_authority_projection{
					.Authority = authority(),
					.Revision  = application::epoch_revision_projection{
						.Base                = commit('1'),
						.Head                = commit('4'),
						.CurrentViewerReview = core::current_viewer_review_candidate{
							.Commit = commit('3') },
					},
					.Generation = core::epoch_generation{ 9U },
				} } });
		QApplication::processEvents();
		expect(line->left_stop()->Update == 1U && line->right_stop()->Update == 2U
				   && line->last_review_update() == std::optional<std::size_t>{ 1U }
				   && line->node_tooltip(1).contains(QStringLiteral("My last review"))
				   && control.comparing_button()->text().startsWith(QStringLiteral("Comparing since my last review, "))
				   && control.comparing_button()->text().contains(QStringLiteral(" → ")),
			   "the line draws the published pair, marks my last review on the "
			   "update whose head I reviewed, and the menu names the alias");

		// Dense: forty updates in 700 pixels keep the meaningful labels only.
		auto dense = std::vector<application::epoch_row_projection>{ };
		for (std::int64_t ordinal = 40; ordinal >= 1; --ordinal) {
			dense.push_back(epoch_row(ordinal, '1', '2'));
		}
		control.publish(loaded_catalog_update(dense));
		QApplication::processEvents();
		auto shown = std::size_t{ 0U };
		for (std::size_t index = 0U; index < line->update_count(); ++index) {
			shown += line->label_shown(index) ? 1U : 0U;
		}
		expect(line->update_count() == 40U && line->label_shown(0)
				   && line->label_shown(39) && shown < 40U && shown >= 2U,
			   "a dense history keeps the end labels and thins the rest");

		control.set_details_visible(true);
		QApplication::processEvents();
		expect(control.details_visible() && control.left_selector()->isVisible()
				   && line->isVisible(),
			   "the details region brings the exact selectors back beneath the "
			   "line without hiding the line");
		control.hide();
	}

	auto test_confirmed_public_maintenance_actions() -> void
	{
		std::vector<application::epoch_intent> intents;
		qt::epoch_comparison_control           control{
			[&intents](application::epoch_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		const auto third  = epoch_row(3, '1', '2');
		const auto second = epoch_row(2, '1', '3');
		control.publish(loaded_catalog_update({ third, second }));
		const auto wholePair = core::comparison_pair{
			.Left  = anchor(third, core::comparison_anchor_kind::epoch_base),
			.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
		};
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr), wholePair));

		expect(control.prune_action()->isEnabled()
				   && control.delete_cache_action()->isEnabled()
				   && control.cancel_maintenance_action()->isEnabled()
				   && !control.prune_action()->shortcut().isEmpty()
				   && !control.delete_cache_action()->shortcut().isEmpty(),
			   "core command projections enable keyboard-addressable maintenance "
			   "actions");

		control.set_maintenance_scope(authority(), QStringLiteral("github.example / owner/repo #17 — reviewer"));

		auto       observation  = confirmation_observation{ };
		const auto beforeCancel = intents.size();
		respond_to_confirmation(
			QStringLiteral("pruneSelectedEpochsConfirmationDialog"),
			false,
			observation);
		control.prune_action()->trigger();
		expect(intents.size() == beforeCancel && observation.CancelWasDefault
				   && observation.Text.contains(QStringLiteral("github.example / owner/repo #17 — reviewer"))
				   && observation.Text.contains(QStringLiteral("retention refs"))
				   && observation.Text.contains(QStringLiteral("unrecoverable"))
				   && observation.Text.contains(QStringLiteral("tombstones"))
				   && observation.Text.contains(QStringLiteral("GitHub"))
				   && observation.Text.contains(QStringLiteral("worktree"))
				   && !observation.AccessibleName.isEmpty(),
			   "prune confirmation defaults to Cancel, states consequences, and "
			   "dispatches nothing when canceled");

		auto otherAuthority    = authority();
		otherAuthority.Account = decltype(otherAuthority.Account){ 99U };
		control.set_maintenance_scope(otherAuthority, QStringLiteral("stale scope label"));
		observation = { };
		respond_to_confirmation(QStringLiteral("pruneSelectedEpochsConfirmationDialog"), false, observation);
		control.prune_action()->trigger();
		expect(intents.size() == beforeCancel && !observation.Text.contains(QStringLiteral("stale scope label")),
			   "a label bound to another authority never describes this maintenance target");

		observation = { };
		respond_to_confirmation(
			QStringLiteral("pruneSelectedEpochsConfirmationDialog"),
			true,
			observation);
		control.prune_action()->trigger();
		const auto* one = std::get_if<core::prune_review_epochs_intent>(&intents.back());
		expect(one != nullptr && one->Catalog == catalog()
				   && one->Epochs == std::vector{ third.Id },
			   "base and head selections from one epoch deduplicate into one exact "
			   "confirmed prune target");

		const auto twoPair = core::comparison_pair{
			.Left  = anchor(third, core::comparison_anchor_kind::epoch_head),
			.Right = anchor(second, core::comparison_anchor_kind::epoch_head),
		};
		control.publish(comparison_update(core::explicit_selection(
											  twoPair.Left.Identity, twoPair.Right.Identity),
										  twoPair));
		observation = { };
		respond_to_confirmation(
			QStringLiteral("pruneSelectedEpochsConfirmationDialog"),
			true,
			observation);
		control.prune_action()->trigger();
		const auto* two = std::get_if<core::prune_review_epochs_intent>(&intents.back());
		expect(two != nullptr && two->Catalog == catalog()
				   && two->Epochs == std::vector{ second.Id, third.Id },
			   "two selected epochs become one sorted exact confirmed batch");

		const auto review = core::comparison_anchor{
			.Identity = {
				.Authority   = third.Id.Authority,
				.Incarnation = third.Id.Incarnation,
				.Kind        = core::comparison_anchor_kind::review_commit,
				.Review      = identity<core::review_association_id>(19U),
			},
			.Commit       = commit('4'),
			.Availability = core::anchor_availability::available,
		};
		const auto mixedPair = core::comparison_pair{
			.Left  = review,
			.Right = anchor(second, core::comparison_anchor_kind::epoch_head),
		};
		control.publish(comparison_update(core::explicit_selection(
											  mixedPair.Left.Identity, mixedPair.Right.Identity),
										  mixedPair));
		observation = { };
		respond_to_confirmation(
			QStringLiteral("pruneSelectedEpochsConfirmationDialog"),
			true,
			observation);
		control.prune_action()->trigger();
		const auto* mixed = std::get_if<core::prune_review_epochs_intent>(&intents.back());
		expect(mixed != nullptr && mixed->Epochs == std::vector{ second.Id },
			   "a review anchor is never invented as an epoch prune target");

		auto reviewOnly                              = comparison_update(core::explicit_selection(
																			 review.Identity, review.Identity),
																		 { .Left = review, .Right = review });
		reviewOnly.Transition->Commands->Commands[2] = core::epoch_command_projection{
			.Id             = core::epoch_command_id::prune_selected_epochs,
			.Enabled        = false,
			.DisabledReason = core::epoch_reason_code::invalid_maintenance_target,
		};
		control.publish(reviewOnly);
		expect(!control.prune_action()->isEnabled()
				   && control.delete_cache_action()->isEnabled(),
			   "core disables pruning for a review-only pair without disabling "
			   "whole-cache administration");

		expect(control.prune_action()->toolTip()
					   == QStringLiteral("The maintenance target is invalid.")
				   && control.prune_action()->statusTip()
					   == control.prune_action()->toolTip(),
			   "a disabled prune command states its exact core reason in the "
			   "assistive-technology surfaces instead of the enabled description");

		{
			const auto dispatched = intents.size();
			control.prune_action()->trigger();
			expect(intents.size() == dispatched
					   && QApplication::activeModalWidget() == nullptr,
				   "triggering the disabled prune command opens no confirmation "
				   "and dispatches no maintenance intent");
		}

		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr), wholePair));
		observation = { };
		respond_to_confirmation(
			QStringLiteral("pruneSelectedEpochsConfirmationDialog"),
			true,
			observation,
			[&control] {
				control.publish(application::epoch_presentation_update{
					.Catalog = application::epoch_catalog_projection{
						.Catalog = catalog(8),
					},
				});
			});
		control.prune_action()->trigger();
		const auto* stale = std::get_if<core::prune_review_epochs_intent>(&intents.back());
		expect(stale != nullptr && stale->Catalog == catalog()
				   && stale->Epochs == std::vector{ third.Id }
				   && control.history_model()->catalog() == catalog(8),
			   "confirmation captures the complete old target so a catalog change "
			   "is rejected by the core instead of silently retargeted");

		control.publish(loaded_catalog_update({ third, second }));
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr), wholePair));
		observation                   = { };
		const auto beforeDeleteCancel = intents.size();
		respond_to_confirmation(
			QStringLiteral("deleteRepositoryCacheConfirmationDialog"),
			false,
			observation);
		control.delete_cache_action()->trigger();
		expect(intents.size() == beforeDeleteCancel,
			   "canceling whole-cache deletion dispatches no intent");
		expect(observation.CancelWasDefault,
			   "whole-cache deletion defaults to Cancel");
		expect(observation.Text.contains(QStringLiteral("every retained"))
				   && observation.Text.contains(QStringLiteral("repository bytes"))
				   && observation.Text.contains(QStringLiteral("Cache-deleted"))
				   && observation.Text.contains(QStringLiteral("GitHub"))
				   && observation.Text.contains(QStringLiteral("worktree")),
			   "whole-cache confirmation defaults to Cancel and states its exact "
			   "repository-wide local consequences");

		observation = { };
		respond_to_confirmation(
			QStringLiteral("deleteRepositoryCacheConfirmationDialog"),
			true,
			observation);
		control.delete_cache_action()->trigger();
		const auto* deleted = std::get_if<core::delete_repository_cache_intent>(&intents.back());
		expect(deleted != nullptr && deleted->Catalog == catalog()
				   && deleted->Cache == authority().Cache,
			   "confirmed cache deletion carries the exact captured catalog and "
			   "repository-cache scope");

		control.cancel_maintenance_action()->trigger();
		expect(std::holds_alternative<core::cancel_epoch_maintenance_intent>(
				   intents.back()),
			   "maintenance cancellation emits only its typed semantic intent");

		auto* const maintenanceButton = control.findChild<QToolButton*>(
			QStringLiteral("localHistoryMaintenanceButton"));
		expect(maintenanceButton != nullptr
				   && maintenanceButton->menu() != nullptr
				   && maintenanceButton->menu()->objectName()
					   == QStringLiteral("localHistoryMaintenanceMenu")
				   && maintenanceButton->menu()->actions().contains(
					   control.prune_action())
				   && maintenanceButton->menu()->actions().contains(
					   control.delete_cache_action())
				   && maintenanceButton->menu()->actions().contains(
					   control.cancel_maintenance_action())
				   && !maintenanceButton->accessibleName().isEmpty()
				   && !maintenanceButton->accessibleDescription().isEmpty(),
			   "one secondary public maintenance menu exposes stable action, object, "
			   "and accessibility contracts");

		// The menu indicator has to fit inside the button that owns it. It is
		// drawn by the style at the trailing edge, and if the size hint covers
		// only the text the chevron lands past the frame - reported at the
		// first attended review of BPR-0194, on a real display, as an
		// indicator drawn outside its own widget rectangle.
		//
		// Asserted against the style's own metric rather than a fixed number,
		// because the indicator's width is the style's to decide and a
		// constant here would pass on the one platform it was measured on.
		{
			auto option = QStyleOptionToolButton{ };
			option.initFrom(maintenanceButton);
			option.text          = maintenanceButton->text();
			option.features      = QStyleOptionToolButton::HasMenu;
			const auto indicator = maintenanceButton->style()->pixelMetric(
				QStyle::PM_MenuButtonIndicator, &option, maintenanceButton);
			const auto textWidth = maintenanceButton->fontMetrics()
									   .horizontalAdvance(maintenanceButton->text());
			expect(
				indicator > 0,
				"the style reserves a real width for a menu indicator, so this "
				"comparison can fail");
			expect(
				maintenanceButton->sizeHint().width() >= textWidth + indicator,
				"the maintenance button asks for room for its label and its "
				"menu indicator, not just its label");
			// The minimum carries the indicator too — a layout under pressure
			// squeezes towards it — and carries it exactly once. Asserting
			// equality with the size hint catches both the omission and the
			// double correction; a >= bound catches only the first, and let a
			// draft through that widened the minimum twice.
			expect(
				maintenanceButton->minimumSizeHint().width()
					>= textWidth + indicator,
				"even squeezed to its minimum the button keeps room for the "
				"indicator rather than letting it escape the frame");
			expect(
				maintenanceButton->minimumSizeHint().width()
					== maintenanceButton->sizeHint().width(),
				"the minimum applies the indicator correction once, not once "
				"per hint");
			// Exactly one indicator wider than the same button without a menu,
			// measured rather than bounded by a guess. A tolerance here passed
			// a first draft that added the indicator twice — the button was
			// then wide enough for the assertion and sixteen pixels wider than
			// it asked to be.
			auto plain = QToolButton{ };
			plain.setToolButtonStyle(maintenanceButton->toolButtonStyle());
			plain.setText(maintenanceButton->text());
			plain.setFont(maintenanceButton->font());
			plain.setStyle(maintenanceButton->style());
			expect(
				maintenanceButton->sizeHint().width()
					== plain.sizeHint().width() + indicator,
				"the button is exactly one menu indicator wider than the same "
				"button without a menu");
		}
	}

	auto test_explicit_loading_empty_failure_and_availability_states() -> void
	{
		qt::epoch_comparison_control control;
		control.publish(application::epoch_presentation_update{
			.Transition = application::epoch_transition_projection{
				.Status = core::epoch_status_delta{
					.Status = core::epoch_status::loading_catalog,
				},
			},
		});
		expect(control.status_text().contains(QStringLiteral("Loading")),
			   "catalog loading has an explicit visible state");
		expect(control.line()->empty_status() == control.status_text()
				   && control.line()->accessibleDescription() == control.status_text()
				   && control.line()->toolTip() == control.status_text(),
			   "the compact empty updates line paints and announces the loading state");

		auto empty       = loaded_catalog_update({ });
		empty.Transition = application::epoch_transition_projection{
			.Status = core::epoch_status_delta{ .Status = core::epoch_status::ready },
		};
		control.publish(empty);
		expect(control.status_text().contains(QStringLiteral("No review epochs")),
			   "an exhausted empty catalog has an explicit visible state");
		control.publish(application::epoch_presentation_update{
			.Transition = application::epoch_transition_projection{
				.Status = core::epoch_status_delta{ .Status = core::epoch_status::failed },
			},
		});
		expect(control.line()->empty_status().contains(QStringLiteral("could not be updated"))
				   && !control.line()->empty_status().contains(QStringLiteral("No review epochs")),
			   "a failed comparison setup is not presented as an empty completed history");

		auto neverObserved             = epoch_row(3, '1', '2');
		neverObserved.HeadAvailability = core::anchor_availability::never_observed;
		control.publish(loaded_catalog_update({ neverObserved }));
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::previous_upload_to_latest),
			{
				.Left  = anchor(neverObserved,
								core::comparison_anchor_kind::epoch_head,
								core::anchor_availability::never_observed),
				.Right = anchor(neverObserved,
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
			.Transition = application::epoch_transition_projection{
				.Status = core::epoch_status_delta{
					.Status = core::epoch_status::preparing,
				},
			},
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = catalog(),
				.HistoryPage = application::epoch_history_page_projection{
					.Phase   = core::epoch_history_page_phase::loading,
					.Catalog = catalog(),
				},
			},
		});
		expect(control.status_text().contains(QStringLiteral("retained")),
			   "refresh and page loading retain the accepted history visibly");

		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = catalog(),
				.HistoryPage = application::epoch_history_page_projection{
					.Phase   = core::epoch_history_page_phase::failed,
					.Catalog = catalog(),
					.Reason  = core::epoch_reason_code::quota_exceeded,
				},
			},
		});
		expect(control.status_text().contains(QStringLiteral("could not be loaded"))
				   && control.status_text().contains(QStringLiteral("limit"))
				   && control.history_model()->epoch_count() == 1U,
			   "a failed page retains rows and presents the typed reason");

		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = catalog(),
				.HistoryPage = application::epoch_history_page_projection{
					.Phase   = core::epoch_history_page_phase::cancelled,
					.Catalog = catalog(),
					.Reason  = core::epoch_reason_code::cancelled,
				},
			},
		});
		expect(control.status_text().contains(QStringLiteral("canceled"))
				   && control.history_model()->epoch_count() == 1U,
			   "a canceled page retains rows and has an explicit visible state");

		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{
				.Catalog              = catalog(8),
				.AllEpochAvailability = core::anchor_availability::cache_deleted,
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
		const auto third  = epoch_row(3, '1', '2');
		const auto second = epoch_row(2, '1', '3');
		control.publish(loaded_catalog_update({ third, second }));
		control.publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr),
			{
				.Left  = anchor(third, core::comparison_anchor_kind::epoch_base),
				.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
			}));
		QApplication::processEvents();

		control.left_selector()->setFocus(Qt::OtherFocusReason);
		const QPersistentModelIndex stable{
			control.history_model()->index_for_anchor(
				anchor(second, core::comparison_anchor_kind::epoch_head).Identity)
		};
		const auto resetsBefore = control.history_model()->statistics().ModelResets;
		const auto fourth       = epoch_row(4, '1', '4');
		control.publish(application::epoch_presentation_update{
			.Catalog = application::epoch_catalog_projection{
				.Catalog = catalog(8),
				.Changes = {
					application::epoch_row_change_projection{
						.Kind  = application::epoch_row_change_kind::insert,
						.Index = 0U,
						.Id    = fourth.Id,
						.Row   = fourth,
					},
				},
			},
		});
		expect(QApplication::focusWidget() == control.left_selector()
				   && stable.isValid() && stable.row() == 5
				   && control.history_model()->statistics().ModelResets
					   == resetsBefore,
			   "routine projection preserves focus, identity, and zero-reset model state");

		auto* const leftAccessible = QAccessible::queryAccessibleInterface(control.left_selector());
		auto* const oidAccessible  = QAccessible::queryAccessibleInterface(control.left_oid_editor());
		expect(leftAccessible != nullptr
				   && !leftAccessible->text(QAccessible::Name).isEmpty()
				   && leftAccessible->state().focusable && oidAccessible != nullptr
				   && !oidAccessible->text(QAccessible::Name).isEmpty()
				   && oidAccessible->state().readOnly,
			   "selectors and exact OIDs expose named focusable/read-only accessibility");

		const auto appearanceBefore   = control.statistics().AppearanceUpdates;
		const auto resetsAtAppearance = control.history_model()->statistics().ModelResets;
		auto       changedFont        = control.font();
		changedFont.setPointSizeF(changedFont.pointSizeF() + 1.0);
		control.setFont(changedFont);
		QApplication::processEvents();
		expect(control.statistics().AppearanceUpdates > appearanceBefore
				   && control.history_model()->statistics().ModelResets
					   == resetsAtAppearance,
			   "system appearance changes refresh presentation roles without a reset");

		const auto announcements = control.statistics().Announcements;
		const auto same          = comparison_update(
			core::named_selection(core::named_comparison::whole_pr),
			{
				.Left  = anchor(third, core::comparison_anchor_kind::epoch_base),
				.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
			});
		control.publish(same);
		const auto afterFirst = control.statistics().Announcements;
		control.publish(same);
		expect(afterFirst >= announcements
				   && control.statistics().Announcements == afterFirst,
			   "one combined bounded announcement is deduplicated across identical updates");
	}

	[[nodiscard]] auto pull_request_projection()
		-> application::github_pull_request_projection
	{
		return {
			.Profile        = core::host_profile_id{ 1U },
			.Host           = core::git_host_id{ 2U },
			.Account        = core::github_account_id{ 3U },
			.Snapshot       = core::provider_snapshot_id{ 4U },
			.Repository     = core::repository_id{ 5U },
			.PullRequest    = core::pull_request_id{ 6U },
			.Owner          = "better-pull-review",
			.RepositoryName = "desktop",
			.Number         = core::pull_request_number{ 7U },
			.Title          = "Epoch comparison render fixture",
			.Body           = "The real loaded-content composition contains the reusable "
							  "comparison control.",
			.AuthorLogin    = "reviewer",
			.BaseRef        = "main",
			.HeadRef        = "feature/epochs",
			.BaseOid        = std::string(40U, '1'),
			.HeadOid        = std::string(40U, '2'),
			.FileCount      = 3U,
			.CommitCount    = 4U,
		};
	}

	auto test_real_loaded_window_composition_renders() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			{ },
			[](application::github_intent) { },
		};
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_ready,
				},
			},
			.PullRequest = pull_request_projection(),
		});
		std::vector<application::epoch_row_projection> page;
		page.reserve(core::maximum_epoch_history_page_size);
		for (std::int64_t ordinal = 1'100; ordinal >= 1'001; --ordinal) {
			page.push_back(epoch_row(ordinal, '1', '2'));
		}
		const auto renderTop = page.front();
		window.epoch_selectors()->publish(
			loaded_catalog_update(std::move(page)));
		window.epoch_selectors()->publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr),
			{
				.Left = anchor(
					renderTop, core::comparison_anchor_kind::epoch_base),
				.Right = anchor(
					renderTop, core::comparison_anchor_kind::epoch_head),
			}));
		window.resize(1'100, 820);
		window.show();
		// [[BPR-1130]]. The Updates line open, as the reviewer left it.
		window.publish(application::workbench_projection{
			.UpdatesLine = core::workbench_updates_line_delta{ .State = core::workbench_updates_line::open } });
		// [[BPR-0646]]. The exact selectors are review details, shown on
		// request; the open epoch line is the chrome.
		window.epoch_selectors()->set_details_visible(true);
		QApplication::processEvents();
		expect(window.epoch_selectors()->isVisible()
				   && window.epoch_selectors()->line()->isVisible()
				   && window.epoch_selectors()->left_selector()->isVisible()
				   && window.epoch_selectors()->right_selector()->isVisible()
				   && window.epoch_selectors()->history_model()->epoch_count()
					   == core::maximum_epoch_history_page_size
				   && window.epoch_selectors()->history_model()->rowCount()
					   == static_cast<int>(
						   core::maximum_epoch_history_page_size * 2U)
				   && window.epoch_selectors()->left_selector()->currentIndex() == 0
				   && window.epoch_selectors()->right_selector()->currentIndex() == 1,
			   "the exact 100-epoch/200-anchor working set is reachable in the real "
			   "loaded-content window");

		const QPixmap rendered = window.grab();
		const auto    scale    = window.devicePixelRatioF();
		expect(!rendered.isNull()
				   && rendered.width()
					   == static_cast<int>(std::lround(window.width() * scale))
				   && rendered.height()
					   == static_cast<int>(std::lround(window.height() * scale)),
			   "the integrated window renders at the exact active device scale");
		const auto path = qEnvironmentVariable("BPR_QT_EPOCH_RENDER_PATH");
		if (!path.isEmpty()) {
			expect(rendered.save(path, "PNG"),
				   "the integrated epoch render is saved only in the build tree");
		}
	}
}

namespace {
	/// Sends the epoch line a context-menu request at aPoint and runs aStep
	/// once, from the event loop, once the menu shows. It does not matter
	/// whether the menu waits in a nested loop of its own, so the same course
	/// runs against the stack `exec()` form [[BPR-1034]] replaced and fails
	/// there. The line must not be used after aStep, which may destroy it.
	auto with_open_epoch_line_menu(qt::epoch_line& aLine, const QPoint aPoint, const std::function<void(QMenu&)>& aStep) -> void
	{
		auto context  = QObject{ };
		auto ran      = false;
		auto attempts = 0;
		auto poll     = std::function<void()>{ };
		poll          = [&] {
			if (auto* const popup = qobject_cast<QMenu*>(QApplication::activePopupWidget()); popup != nullptr) {
				ran = true;
				aStep(*popup);
			} else if (++attempts < 250) {
				QTimer::singleShot(20, &context, poll);
			}
		};
		QTimer::singleShot(0, &context, poll);
		auto request = QContextMenuEvent{ QContextMenuEvent::Mouse, aPoint, aLine.mapToGlobal(aPoint) };
		QApplication::sendEvent(&aLine, &request);
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!ran && attempts < 250 && elapsed.elapsed() < 10'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		expect(ran, "a context-menu request on an update opens the epoch line's menu");
	}

	// [[BPR-1034]]. The epoch line's context menu and its window's lifetime,
	// as [[BPR-1028]] settled for the Windows strip menu. Anything that
	// destroys the window while the menu is open (an accessible Quit or close,
	// a profile shutdown, an update's quit) has to take the menu with it and
	// leave no frame of the line waiting for the menu. With the old stack menu
	// run by `exec()`, the destruction below deleted a stack object.
	auto test_epoch_line_menu_goes_with_its_window() -> void
	{
		auto window  = std::make_unique<QWidget>();
		auto control = QPointer<qt::epoch_comparison_control>{ new qt::epoch_comparison_control{ [](application::epoch_intent) { }, window.get() } };
		window->resize(700, 80);
		control->resize(700, 80);
		window->show();
		QApplication::processEvents();
		const auto third  = epoch_row(3, '1', '4');
		const auto second = epoch_row(2, '1', '3');
		const auto first  = epoch_row(1, '1', '2');
		control->publish(loaded_catalog_update({ third, second, first }));
		control->publish(comparison_update(
			core::named_selection(core::named_comparison::whole_pr),
			{
				.Left  = anchor(first, core::comparison_anchor_kind::epoch_base),
				.Right = anchor(third, core::comparison_anchor_kind::epoch_head),
			}));
		QApplication::processEvents();
		auto line = QPointer<qt::epoch_line>{ control->line() };
		expect(!line.isNull() && line->update_count() == 3U, "the line shows the three updates");

		auto closed = QPointer<QMenu>{ };
		auto names  = QStringList{ };
		with_open_epoch_line_menu(*line, line->node_centre(1), [&](QMenu& aMenu) {
			closed = &aMenu;
			for (const auto* const action : aMenu.actions()) {
				if (!action->isSeparator()) {
					names.push_back(action->objectName());
				}
			}
			expect(aMenu.objectName() == QStringLiteral("epochLineMenu") && aMenu.parent() == line.data(),
				   "the epoch line's menu is the line's own child");
			aMenu.close();
		});
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		expect(names
				   == QStringList{ QStringLiteral("compareFromUpdateAction"), QStringLiteral("compareToUpdateAction"),
								   QStringLiteral("showUpdateAloneAction"), QStringLiteral("copyUpdateHeadAction") },
			   "the menu offers Compare from here, Compare to here, Show this update alone and Copy head commit");
		expect(closed.isNull() && line->findChildren<QMenu*>(QStringLiteral("epochLineMenu")).isEmpty(),
			   "a closed epoch-line menu deletes itself, so opening it again leaves nothing behind");

		auto open = QPointer<QMenu>{ };
		with_open_epoch_line_menu(*line, line->node_centre(1), [&](QMenu& aMenu) {
			open = &aMenu;
			window.reset();
		});
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		expect(window == nullptr && control.isNull() && line.isNull(), "the window, its control and its line were destroyed with the menu open");
		expect(open.isNull(), "the open epoch-line menu went with its window");
		expect(QApplication::activePopupWidget() == nullptr, "no popup is left behind");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	{
		bpr::qt::epoch_comparison_control control;
		const auto                        readiness = QStringLiteral("Comparisons unavailable while the accepted review history is loading from retained data");
		control.set_readiness(readiness, false);
		auto* button = control.comparing_button();
		expect(button->text().startsWith(QStringLiteral("Comparisons")) && button->text().endsWith(QChar{ 0x2026 })
				   && button->toolTip().contains(readiness) && button->accessibleDescription() == readiness
				   && button->maximumWidth() == 320,
			   "folded readiness keeps its beginning, elides its end and exposes complete words");
		control.set_readiness(readiness, true);
		expect(button->maximumWidth() == QWIDGETSIZE_MAX, "opening the track releases the folded summary bound");
	}
	test_model_applies_stable_incremental_rows();
	test_model_replaces_more_than_one_thousand_bounded_pages();
	test_control_translates_exact_selection_and_preserves_projection();
	test_confirmed_public_maintenance_actions();
	test_explicit_loading_empty_failure_and_availability_states();
	test_focus_appearance_accessibility_and_announcements();
	test_real_loaded_window_composition_renders();
	test_occurrence_evidence_and_unavailable_gestures();
	test_epoch_line_selects_updates_by_click_drag_and_keys();
	test_epoch_line_handles_on_one_node_drag_apart_and_keep_the_keyboard();
	test_github_recorded_updates_stay_reachable_by_both_handles();
	test_epoch_line_waits_for_the_newest_page_on_a_cached_head();
	test_epoch_line_menu_goes_with_its_window();
	test_the_comparing_button_moves_to_the_review_bar();
	test_the_line_marks_incomplete_history_without_a_caption();
	std::cout << "epoch-comparison-control-tests:pass\n";
	return EXIT_SUCCESS;
}
