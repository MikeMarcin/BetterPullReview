#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_VIEWS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_VIEWS_HPP

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_views.hpp"

#include <cstddef>
#include <optional>
#include <stop_token>

/// \file
/// \brief What the named comparison views and the revision history resolve
/// from, read from the cycle store (RFC-BPR-0057, BPR-1163).
/// \details The facts are the reviewer's latest explicit checkpoint and the
/// revision each frozen round was bound to, read from the round's verified
/// payload. The history reads each listed revision's manifest for the
/// checkout head it recorded, and the sender revisions an attached round
/// mapped onto this cycle. Nothing here reads the workspace, a response or a
/// delivery observation. Runs on the effect lane.
namespace bpr::application {
	/// \brief The latest checkpoint and every round's bound revision.
	/// \details A round whose payload names no local revision is left out:
	/// its "this round" view is then `unknown_round` rather than a guess.
	[[nodiscard]] auto load_view_facts(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop)
		-> cycle_store_result<core::cycle_view_facts>;

	inline constexpr std::size_t maximum_cycle_history_page = 64U;

	struct cycle_history_page
	{
		core::cycle_history History;
		/// Continue after this revision; absent on the last page.
		std::optional<core::cycle_source_revision_id> Next;
	};
	/// \brief One page of the history after `aAfter`, each capture described
	/// against the one before it (read for context when it is on an earlier page).
	[[nodiscard]] auto load_cycle_history(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject,
										  std::optional<core::cycle_source_revision_id> aAfter, std::size_t aLimit, std::stop_token aStop)
		-> cycle_store_result<cycle_history_page>;
}

#endif
