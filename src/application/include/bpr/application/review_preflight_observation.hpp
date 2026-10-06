#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_PREFLIGHT_OBSERVATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_PREFLIGHT_OBSERVATION_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/core/review_preflight.hpp"

#include <map>
#include <span>
#include <string>

namespace bpr::application {
	/// \brief Where one conversation sits and what its root says, for the
	/// preflight's list. Copied text; no navigation or publication authority.
	struct review_preflight_reference
	{
		/// "src/a.cpp:12", "src/a.cpp" or "Pull request".
		std::string Where;
		/// The root's subject line as written, or its first line.
		std::string Subject;
		auto        operator==(const review_preflight_reference&) const -> bool = default;
	};

	/// \brief [[BPR-1085]]. The published feedback of one accepted snapshot,
	/// read once for the review preflight.
	/// \details Effect-free: it reads the snapshot it is given and nothing
	/// else. Conversations take the identities the window's conversation
	/// projection gives the same records, so a preflight item reveals exactly
	/// the row Discuss shows. GraphQL review threads are read when available,
	/// otherwise the REST inline comment groups (whose resolution is unknown),
	/// like the Overview. Issue comments and review summaries have no native
	/// resolution. A published tour stop is read through its mapped narrative
	/// only; a file-level comment's user text is not mapped here, so its
	/// label is unclassified rather than guessed. `Admitted` is false when the
	/// population refused the observation; the preflight is then unavailable,
	/// never empty.
	struct review_preflight_observation
	{
		core::provider_snapshot_id Snapshot;
		core::github_account_id    Account;
		/// The pull request head the snapshot read.
		std::string Head;
		/// The viewer's exact provider identity: the submitting reviewer.
		std::string                                       Submitter;
		bool                                              Admitted = false;
		core::feedback_annotation_set                     Annotations;
		core::feedback_population                         Population;
		std::vector<core::review_preflight_authorship>    Authors;
		std::map<std::string, review_preflight_reference> References;
	};

	[[nodiscard]] auto observe_review_preflight(const core::cached_review_key& aScope, const github_pull_request_snapshot& aSnapshot)
		-> review_preflight_observation;

	/// \brief The preflight of an admitted observation for the selected drafts.
	/// An observation that was not admitted gives none.
	[[nodiscard]] auto project_review_preflight(const review_preflight_observation&           aObservation,
												std::span<const core::review_preflight_draft> aDrafts) -> std::optional<core::review_preflight>;
}
#endif
