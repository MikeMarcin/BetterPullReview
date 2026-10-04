#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_feedback.hpp"
#include "bpr/application/review_cycle_import.hpp"
#include "bpr/application/review_cycle_publishing.hpp"
#include "bpr/application/review_cycle_response.hpp"
#include "bpr/application/review_cycle_retrieval.hpp"
#include "bpr/application/review_cycle_round.hpp"
#include "bpr/application/review_cycle_share.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/review_cycle_verification.hpp"
#include "bpr/application/review_cycle_views.hpp"
#include "bpr/application/review_material_bridge.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

/// \file
/// \brief The local Review Cycles runtime (RFC-BPR-0053, RFC-BPR-0060).
/// \details Composed from a profile's cycle store and two executors, nothing
/// else: no GitHub account, provider, credential vault, agent adapter or
/// executable lookup is needed to open, create, capture or reopen a cycle.
namespace bpr::application {
	/// \brief What observing a chosen root found (BPR-1157).
	struct workspace_root_observation_result
	{
		std::optional<core::workspace_root_observation> Root;
		std::optional<workspace_failure>                Failure;
	};

	/// \brief Owns the cycle store on the decision lane.
	/// \details Store calls run one at a time on the effect lane in request
	/// order; each accepted call completes exactly once on the decision lane.
	/// Selecting another review subject cancels work issued for the previous
	/// selection, whose completions then report `cancelled`. Executors and
	/// completion targets must stay alive until `is_quiescent()`.
	///
	/// [[BPR-1184]]: with a capture executor, a workspace capture runs there
	/// instead, and while it runs the effect lane keeps serving reads and the
	/// tab record, so a reviewer can read and move between files. The first
	/// call that could move the cycle on waits for the capture, and every call
	/// queued after it waits too, so request order holds for everything that
	/// could observe the difference. Without one, a capture is an ordinary
	/// call on the effect lane.
	class review_cycle_runtime final
	{
	public:
		template <typename T>
		using completion = std::function<void(cycle_store_result<T>)>;

		review_cycle_runtime(std::unique_ptr<review_cycle_store> aStore, effect_executor& aEffects, decision_executor& aDecisions, effect_executor* aCaptures = nullptr);
		~review_cycle_runtime();
		review_cycle_runtime(const review_cycle_runtime&)                    = delete;
		auto operator=(const review_cycle_runtime&) -> review_cycle_runtime& = delete;
		review_cycle_runtime(review_cycle_runtime&&)                         = delete;
		auto operator=(review_cycle_runtime&&) -> review_cycle_runtime&      = delete;

		/// \brief Makes a subject current and supersedes work for the previous one.
		/// \returns The new selection generation.
		auto               select(core::review_subject aSubject) -> std::uint64_t;
		[[nodiscard]] auto selected() const noexcept -> const std::optional<core::review_subject>&;

		/// Each returns false, without calling the completion, when closing.
		auto open(completion<core::review_cycle_catalog> aDone) -> bool;
		auto create(cycle_operation_id aOperation, core::create_review_cycle aCommand, completion<cycle_write_receipt> aDone) -> bool;
		/// \brief Loads the selected cycle. A pull-request selection is refused:
		/// the cycle store never answers for another authority.
		auto load_selected(completion<std::optional<core::review_cycle_state>> aDone) -> bool;
		/// \brief Installs a manifest and records it as a capture of the selected cycle.
		auto capture_selected(cycle_operation_id aOperation, std::uint64_t aExpectedRevision, core::cycle_capture_observation aCapture,
							  std::vector<std::uint8_t> aManifest, completion<cycle_write_receipt> aDone) -> bool;

		/// \brief Captures the selected cycle's bound workspace through `aSource`
		/// on the effect lane: read, verify, install and record, or nothing.
		/// \details `aSource` must stay alive until the completion runs.
		auto capture_workspace(workspace_source_port& aSource, cycle_capture_request aRequest, completion<cycle_capture_outcome> aDone) -> bool;
		/// \brief [[BPR-1184]]. Stops the capture running on the capture lane,
		/// within the file it is reading; it completes `cancelled` and records
		/// nothing. A capture still waiting to start is not affected.
		void cancel_capture();
		/// Whether a capture is running on the capture lane.
		[[nodiscard]] auto capturing() const noexcept -> bool;

		/// \brief One bounded page of a resolved pair of the selected cycle,
		/// from retained material only. Superseded by a new selection.
		auto compare_selected(core::cycle_comparison_pair aPair, std::optional<std::string> aAfter, std::size_t aLimit, bool aChangedOnly,
							  completion<cycle_comparison_page_observation> aDone) -> bool;
		/// \brief One file of a resolved pair (or full source) of the selected cycle.
		auto compare_file_selected(core::cycle_comparison_pair aPair, std::string aPath, core::diff_options aOptions, completion<cycle_file_observation> aDone) -> bool;

		/// \brief Records the reviewer's explicit checkpoint on the selected
		/// cycle (BPR-1163). `Allocated` is its ordinal.
		auto mark_reviewed_selected(cycle_operation_id aOperation, core::cycle_source_revision_id aRevision, completion<cycle_write_receipt> aDone) -> bool;
		/// \brief What the selected cycle's named views resolve from.
		auto view_facts_selected(completion<core::cycle_view_facts> aDone) -> bool;
		/// \brief One page of the selected cycle's revision history.
		auto history_selected(std::optional<core::cycle_source_revision_id> aAfter, std::size_t aLimit, completion<cycle_history_page> aDone) -> bool;

		/// \brief Reads a chosen response file with `aRead` on the effect lane
		/// and appends it to round `aRound` of the selected cycle (BPR-1164).
		/// Output arriving after the selection changed completes `cancelled`
		/// and appends nothing.
		auto import_response_selected(cycle_operation_id aOperation, core::cycle_round_id aRound, cycle_response_file_reader aRead, completion<cycle_response_result> aDone) -> bool;
		/// \brief Appends a reply the reviewer entered for round `aRound`.
		auto attach_reply_selected(cycle_operation_id aOperation, core::cycle_round_id aRound, core::cycle_manual_reply aReply, completion<cycle_response_result> aDone) -> bool;
		/// \brief Every statement appended to the selected cycle, with its content.
		auto responses_selected(completion<cycle_response_entries> aDone) -> bool;

		/// \brief Records the reviewer's decision about a note of the selected
		/// cycle against the checked revision's retained material (BPR-1165).
		auto decide_selected(cycle_operation_id aOperation, cycle_decision_request aRequest, completion<cycle_decision_result> aDone) -> bool;
		/// \brief Every note of the selected cycle reconciled against `aAgainst`.
		auto reconcile_selected(core::cycle_source_revision_id aAgainst, completion<std::vector<core::cycle_item_reconciliation>> aDone) -> bool;
		/// \brief Saves the next round's draft, planned from the latest frozen round.
		auto prepare_next_round_selected(cycle_operation_id aBegin, cycle_operation_id aSave, completion<cycle_next_round_result> aDone) -> bool;

		/// \brief Starts a note on the selected cycle after verifying each
		/// target against its revision's retained bytes.
		auto begin_feedback_selected(cycle_operation_id aOperation, std::uint64_t aExpectedRevision, core::begin_cycle_feedback aCommand, completion<cycle_write_receipt> aDone) -> bool;
		/// \brief Changes one note of the selected cycle; an added reference is
		/// verified first.
		auto change_feedback_selected(cycle_operation_id aOperation, core::cycle_feedback_item_ref aItem, std::uint64_t aExpectedVersion, core::cycle_feedback_change aChange,
									  completion<cycle_write_receipt> aDone) -> bool;
		auto feedback_selected(std::optional<core::cycle_feedback_item_id> aAfter, completion<cycle_feedback_page> aDone) -> bool;
		auto feedback_revisions_selected(core::cycle_feedback_item_ref aItem, std::optional<core::private_review_turn_revision_id> aAfter, completion<cycle_feedback_revision_page> aDone) -> bool;
		/// \brief Where a note's target stands in one revision of the selected cycle.
		auto observe_feedback_selected(core::cycle_feedback_target aTarget, core::cycle_source_revision_id aAgainst, completion<cycle_correspondence_observation> aDone) -> bool;

		/// \brief Adds an exact suggestion to a note of the selected cycle after
		/// verifying its selection against the retained bytes.
		auto add_suggestion_selected(cycle_operation_id aOperation, core::cycle_feedback_item_ref aItem, std::uint64_t aExpectedVersion, core::suggest_change aChange,
									 completion<cycle_write_receipt> aDone) -> bool;
		/// \brief Revises, revalidates or records a suggestion as already
		/// applied; a candidate is verified and an application confirmed from
		/// retained bytes first.
		auto change_suggestion_selected(cycle_operation_id aOperation, core::cycle_suggestion_ref aSuggestion, std::uint64_t aExpectedRevision, core::cycle_suggestion_change aChange,
										completion<cycle_write_receipt> aDone) -> bool;
		auto suggestions_selected(core::cycle_feedback_item_ref aItem, completion<std::vector<core::cycle_suggestion>> aDone) -> bool;
		auto preview_suggestion_selected(core::cycle_suggestion aSuggestion, completion<cycle_suggestion_preview_observation> aDone) -> bool;
		auto preflight_suggestions_selected(std::vector<core::cycle_suggestion> aSuggestions, std::optional<core::cycle_source_revision_id> aAgainst,
											completion<cycle_suggestion_preflight_observation> aDone) -> bool;

		/// \brief Starts a round draft on the selected cycle (BPR-1158).
		/// `Allocated` is the draft.
		auto begin_round_selected(cycle_operation_id aOperation, std::uint64_t aExpectedRevision, completion<cycle_write_receipt> aDone) -> bool;
		/// \brief Stores a selection as the draft's next revision.
		auto save_round_selected(cycle_operation_id aOperation, core::cycle_round_draft_ref aDraft, std::uint64_t aExpectedRevision, core::cycle_round_selection aSelection,
								 completion<cycle_write_receipt> aDone) -> bool;
		auto load_round_selected(core::cycle_round_draft_ref aDraft, completion<cycle_round_draft_content> aDone) -> bool;
		/// \brief Freezes a draft revision of the selected cycle into one
		/// immutable round, or says what in the selection must change.
		auto freeze_round_selected(cycle_round_freeze_request aRequest, completion<cycle_round_freeze_result> aDone) -> bool;
		/// \brief One frozen round of the selected cycle, verified against its digest.
		auto read_round_selected(core::cycle_round_id aRound, completion<cycle_frozen_round> aDone) -> bool;
		auto rounds_selected(std::optional<core::cycle_round_id> aAfter, completion<cycle_round_page> aDone) -> bool;

		/// \brief Compose feedback (BPR-1161): the selected cycle's latest
		/// round draft, brought to its notes' current wording, or its first.
		auto open_share_selected(cycle_operation_id aBegin, cycle_operation_id aSave, completion<cycle_share_draft> aDone) -> bool;
		/// \brief One edit to the draft Compose feedback shows.
		auto edit_share_selected(cycle_operation_id aSave, core::cycle_round_draft_ref aDraft, std::uint64_t aExpectedRevision, core::cycle_compose_edit aEdit,
								 completion<cycle_share_draft> aDone) -> bool;
		/// \brief One note's code for the share sheet, cut to `aRows` rows
		/// with `aContext` lines either side.
		auto note_context_selected(core::cycle_feedback_item_id aItem, std::size_t aContext, std::size_t aRows, completion<review_cycle_note_context> aDone) -> bool;
		/// \brief Decides, freezes and renders a share whose bytes the host
		/// hands on itself (Copy to Clipboard, Use in external agent).
		auto prepare_share_selected(cycle_share_request aRequest, completion<cycle_share_output> aDone) -> bool;
		/// \brief Shares to the chosen file, used only on the effect lane, and
		/// records the save.
		auto share_to_file_selected(cycle_share_request aRequest, std::shared_ptr<cycle_export_destination> aDestination, std::string aName,
									std::optional<core::cycle_material_digest> aConfirmed, cycle_operation_id aRecord, completion<cycle_file_share> aDone) -> bool;
		/// \brief Decides and renders one frozen round of the selected cycle as
		/// it was frozen (BPR-1167); the host hands the bytes on itself.
		auto export_round_selected(cycle_round_export_request aRequest, completion<cycle_share_output> aDone) -> bool;
		/// \brief Exports one frozen round to the chosen file, used only on the
		/// effect lane, and records the save.
		auto export_round_to_file_selected(cycle_round_export_request aRequest, std::shared_ptr<cycle_export_destination> aDestination, std::string aName,
										   std::optional<core::cycle_material_digest> aConfirmed, cycle_operation_id aRecord, completion<cycle_file_share> aDone) -> bool;
		/// \brief Records one delivery observation of a frozen round of the selected cycle.
		auto record_delivery_selected(cycle_operation_id aOperation, core::cycle_delivery_observation aObservation, completion<cycle_write_receipt> aDone) -> bool;
		/// \brief Every delivery observation of the selected cycle, in order.
		auto deliveries_selected(completion<std::vector<core::cycle_delivery_record>> aDone) -> bool;

		/// \brief Exposes a frozen round of the selected cycle to a harness
		/// ([[BPR-1169]]): the response-only grant, or reading only.
		auto grant_round_selected(cycle_operation_id aOperation, core::cycle_round_id aRound, bool aReadOnly, core::cycle_disclosure_policy aImposed, completion<cycle_grant_result> aDone)
			-> bool;
		/// \brief Withdraws the round's grant; it stays stored as revoked.
		auto revoke_round_selected(cycle_operation_id aOperation, core::cycle_round_id aRound, completion<cycle_grant_result> aDone) -> bool;
		/// \brief Every grant of the selected cycle, by round.
		auto grants_selected(completion<std::vector<core::cycle_round_grant>> aDone) -> bool;
		/// \brief Reads a frozen round of the selected cycle for a consumer
		/// holding its stored grant, and only then.
		auto read_granted_round_selected(core::cycle_round_id aRound, core::cycle_disclosure_policy aImposed, completion<cycle_granted_round> aDone) -> bool;
		/// \brief Reads a response with `aRead` and appends it to a round of the
		/// selected cycle for a consumer whose grant allows it.
		auto append_granted_response_selected(cycle_operation_id aOperation, core::cycle_round_id aRound, cycle_response_file_reader aRead, completion<cycle_granted_response> aDone) -> bool;
		/// \brief The profile's stored disclosure policy, whatever is selected.
		auto disclosure_policy(completion<core::cycle_disclosure_policy> aDone) -> bool;
		/// \brief Sets one channel's permission for one classification.
		auto set_channel_permission(cycle_operation_id aOperation, core::cycle_data_classification aClassification, core::cycle_disclosure_channel aChannel,
									core::cycle_channel_permission aPermission, completion<cycle_write_receipt> aDone) -> bool;

		/// \brief One Publish as Pull Request step (BPR-1177) on the effect lane,
		/// given the store and the selected cycle as it is now; superseded by
		/// a new selection like every other call.
		using publication_work = std::function<cycle_store_result<review_cycle_publication_step>(review_cycle_store&, const core::review_cycle_state&, std::stop_token)>;
		auto publication_selected(publication_work aWork, completion<review_cycle_publication_step> aDone) -> bool;

		/// \brief Opens a portable bundle into this profile's quarantine,
		/// whatever is selected (BPR-1160). Needs no account, provider or agent.
		auto import_bundle(cycle_operation_id aOperation, std::vector<std::uint8_t> aBundle, completion<cycle_import_result> aDone) -> bool;
		auto imports(std::optional<core::cycle_import_id> aAfter, completion<cycle_import_page> aDone) -> bool;
		/// \brief One quarantined round, verified against its digest.
		auto read_import(core::cycle_import_id aImport, completion<cycle_imported_round> aDone) -> bool;
		/// \brief Associates a quarantined round with the selected cycle from
		/// explicit mappings only (BPR-1162).
		auto attach_import_selected(cycle_import_attach_request aRequest, completion<cycle_import_attach_result> aDone) -> bool;
		/// \brief Forks an associated round into the selected cycle's notes.
		auto fork_import_selected(cycle_operation_id aOperation, core::cycle_import_id aImport, completion<cycle_import_fork_result> aDone) -> bool;

		/// \brief Observes a deliberately chosen root on the effect lane:
		/// identity only, nothing captured. `aSource` must stay alive until the
		/// completion runs.
		auto observe_workspace(workspace_source_port& aSource, std::string aLocation, completion<workspace_root_observation_result> aDone) -> bool;
		/// \brief One page of this profile's cycles, whatever is selected.
		auto list(std::optional<core::review_cycle_id> aAfter, completion<cycle_page> aDone) -> bool;
		/// \brief One cycle's state, whatever is selected (for recents).
		auto load(core::local_cycle_subject aSubject, completion<std::optional<core::review_cycle_state>> aDone) -> bool;
		/// \brief Applies one change to the selected cycle (rename, scope, bind).
		auto apply_selected(cycle_operation_id aOperation, std::uint64_t aExpectedRevision, core::review_cycle_change aChange, completion<cycle_write_receipt> aDone) -> bool;
		auto revisions_selected(std::optional<core::cycle_source_revision_id> aAfter, completion<cycle_revision_page> aDone) -> bool;
		auto save_tabs_selected(core::cycle_tabs_record aRecord, completion<void> aDone) -> bool;
		auto load_tabs_selected(completion<std::optional<core::cycle_tabs_record>> aDone) -> bool;

		/// \brief Stops admitting work and cancels outstanding calls.
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}

#endif
