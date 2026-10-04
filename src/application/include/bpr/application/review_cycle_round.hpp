#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_ROUND_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_ROUND_HPP

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_round.hpp"

#include <cstdint>
#include <optional>
#include <stop_token>

/// \file
/// \brief Round drafts and frozen rounds of a local Review Cycle
/// (RFC-BPR-0056, BPR-1158).
/// \details A draft stores the reviewer's selection; freezing reads the
/// cycle's current records and retained bytes for exactly what the selection
/// names, assembles the one canonical payload in the core, installs it and
/// records the round in one store transaction. An interrupted freeze leaves
/// at most an unreferenced payload object and no round; repeating it freezes
/// once, and repeating a completed freeze returns the same round. Nothing is
/// sent, copied or written outside the profile's cycle store. Runs on the
/// effect lane.
namespace bpr::application {
	/// \brief Stores a selection as the draft's next revision.
	[[nodiscard]] auto save_round_selection(review_cycle_store& aStore, const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject,
											const core::cycle_round_draft_ref& aDraft, std::uint64_t aExpectedRevision, const core::cycle_round_selection& aSelection,
											std::stop_token aStop) -> cycle_store_result<cycle_write_receipt>;

	struct cycle_round_draft_content
	{
		core::cycle_round_draft     Draft;
		core::cycle_round_selection Selection;
	};
	/// \brief Reads a draft and its selection. A body that is not a
	/// selection is reported corrupt.
	[[nodiscard]] auto load_round_selection(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft, std::stop_token aStop)
		-> cycle_store_result<cycle_round_draft_content>;

	struct cycle_round_freeze_request
	{
		cycle_operation_id          Operation;
		core::cycle_round_draft_ref Draft;
		std::uint64_t               ExpectedDraftRevision = 0;
		/// What the draft's editor reports; a freeze needs it flushed.
		core::cycle_draft_editor_state Editor;
	};
	/// \brief A frozen round and its verified content.
	struct cycle_frozen_round
	{
		stored_cycle_round        Round;
		core::cycle_decoded_round Content;
		/// The draft revision was already frozen with this content.
		bool Reused = false;
		/// This call returned an earlier call's result.
		bool Replayed = false;
	};
	/// \brief The freeze decision: a round, or the core's refusal naming what
	/// in the selection must change first. Store failures are errors.
	struct cycle_round_freeze_result
	{
		std::optional<cycle_frozen_round>        Frozen;
		std::optional<core::cycle_round_refusal> Refusal;
	};
	/// \brief The round a draft's stored revision would freeze as, assembled
	/// from the same records as freezing but installed nowhere and recorded
	/// nowhere (BPR-1161): exactly what sharing it would send.
	struct cycle_round_preview
	{
		core::cycle_round_draft     Draft;
		core::cycle_round_selection Selection;
		/// The identity it takes: this revision's round, or the next one.
		core::cycle_round_id Round;
		/// The canonical content, unless the selection cannot be frozen.
		std::optional<core::cycle_decoded_round> Content;
		std::optional<core::cycle_round_refusal> Refusal;
		/// This draft revision's round, when it is already frozen.
		std::optional<stored_cycle_round> Frozen;
		/// The latest round frozen from this draft, from any revision.
		std::optional<stored_cycle_round> Prior;
	};
	[[nodiscard]] auto preview_cycle_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft,
										   std::stop_token aStop) -> cycle_store_result<cycle_round_preview>;

	/// \brief Freezes the draft revision into an immutable round.
	/// \details `aHost` must serve the same store and authority.
	[[nodiscard]] auto freeze_cycle_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject,
										  const cycle_round_freeze_request& aRequest, std::stop_token aStop) -> cycle_store_result<cycle_round_freeze_result>;
	/// \brief [[BPR-1274]]. Freezes from `aPreview`, the draft's preview this
	/// same effect-lane call already assembled: the round is assembled,
	/// encoded and digested once, not again.
	[[nodiscard]] auto freeze_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_round_freeze_request& aRequest, cycle_round_preview aPreview,
										  std::stop_token aStop) -> cycle_store_result<cycle_round_freeze_result>;

	/// \brief Reads a frozen round and verifies its stored payload against
	/// the recorded digest. A payload that differs is reported corrupt.
	[[nodiscard]] auto read_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, core::cycle_round_id aRound, std::stop_token aStop)
		-> cycle_store_result<cycle_frozen_round>;
}

#endif
