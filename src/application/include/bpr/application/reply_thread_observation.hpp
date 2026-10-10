#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REPLY_THREAD_OBSERVATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REPLY_THREAD_OBSERVATION_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/core/reply_draft_standing.hpp"

namespace bpr::application {
	/// \brief [[BPR-1374]]. The accepted snapshot's review threads, as the
	/// core reads them to decide where an unsent reply stands.
	/// \details Effect-free: it reads the snapshot it is given and nothing
	/// else. The observation is complete only when the snapshot read the
	/// GraphQL thread collection; a signed-out or failed read leaves it
	/// incomplete, so no reply is ever called orphaned by a read that could
	/// not see its thread.
	[[nodiscard]] auto observe_reply_threads(const github_pull_request_snapshot& aSnapshot) -> core::reply_thread_observation;
}

#endif
