#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_feedback.hpp"
#include "bpr/application/review_cycle_round.hpp"
#include "bpr/application/review_cycle_store.hpp"
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
	class review_cycle_runtime final
	{
	public:
		template <typename T>
		using completion = std::function<void(cycle_store_result<T>)>;

		review_cycle_runtime(std::unique_ptr<review_cycle_store> aStore, effect_executor& aEffects, decision_executor& aDecisions);
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

		/// \brief One bounded page of a resolved pair of the selected cycle,
		/// from retained material only. Superseded by a new selection.
		auto compare_selected(core::cycle_comparison_pair aPair, std::optional<std::string> aAfter, std::size_t aLimit, bool aChangedOnly,
							  completion<cycle_comparison_page_observation> aDone) -> bool;
		/// \brief One file of a resolved pair (or full source) of the selected cycle.
		auto compare_file_selected(core::cycle_comparison_pair aPair, std::string aPath, core::diff_options aOptions, completion<cycle_file_observation> aDone) -> bool;

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
