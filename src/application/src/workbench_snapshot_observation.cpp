#include "bpr/application/workbench_snapshot_observation.hpp"

#include "bpr/core/diff_model.hpp"

#include <algorithm>
#include <utility>

namespace bpr::application {
	namespace {
		/// \brief Builds one answer that repeats the request exactly.
		///
		/// \details Every return path goes through this. `Context` and
		/// `Authority` must repeat the request or the host refuses the terminal
		/// as foreign, and a failure path that forgot to copy them would be a
		/// misrouted answer rather than a reported failure.
		[[nodiscard]] auto answer(
			const workbench_observation_request& aRequest,
			const workbench_observation_status   aStatus)
			-> workbench_file_observation
		{
			return workbench_file_observation{
				.Context    = aRequest.Context,
				.Authority  = aRequest.Authority,
				.Status     = aStatus,
				.Actionable = { },
			};
		}
	} // namespace

	snapshot_workbench_observation::snapshot_workbench_observation(
		file_supplier aFiles)
		: Files{ std::move(aFiles) }
	{
	}

	auto snapshot_workbench_observation::assemble(
		const workbench_observation_request& aRequest,
		std::stop_token                      aStop) -> workbench_file_observation
	{
		if (aStop.stop_requested()) {
			return answer(aRequest, workbench_observation_status::cancelled);
		}
		if (!Files) {
			return answer(aRequest, workbench_observation_status::unavailable);
		}

		const auto files = Files(aRequest.Authority);
		if (!files) {
			return answer(aRequest, workbench_observation_status::unavailable);
		}

		// Reported rather than truncated, and with no partial order attached.
		// A truncated order would hand the core false neighbour facts: the last
		// file it could see would look like the last file there is, and
		// "next change" would stop early without saying so.
		if (files->size() > aRequest.MaximumFiles) {
			return answer(aRequest, workbench_observation_status::limit_exceeded);
		}

		auto result = answer(aRequest, workbench_observation_status::available);
		result.Actionable.reserve(files->size());
		for (const auto& record : *files) {
			if (aStop.stop_requested()) {
				return answer(aRequest, workbench_observation_status::cancelled);
			}

			// Derived from the head path. A rename therefore renames the file's
			// identity too, which is the decision recorded on
			// `core::file_identity_of` and not an oversight here.
			const auto identity = core::file_identity_of(record.Path);
			if (!identity.valid()) {
				// A path this build cannot name makes the whole answer
				// unavailable rather than a shorter list. Silently omitting the
				// record would hide a file from the reviewer entirely, and the
				// read model already bounds path length, so reaching this means
				// the snapshot is malformed rather than merely large.
				return answer(
					aRequest, workbench_observation_status::unavailable);
			}

			// The port owns uniqueness within the order; the host resolves the
			// first matching entry. A snapshot repeating a path would otherwise
			// put one file in the order twice and make navigation revisit it.
			if (std::ranges::find(result.Actionable, identity)
				!= result.Actionable.end()) {
				continue;
			}
			result.Actionable.push_back(identity);
		}

		// Presentation order is the snapshot's own order, unfiltered.
		//
		// `Actionable` is named for a filter that does not exist yet:
		// actionability is decided from receipts, and RFC-BPR-0007's review
		// ledger host is not composed. Until it is, every file in the review is
		// reported. This is stated here and in the RFC-BPR-0005 ledger rather
		// than left for a reader to infer from an unfiltered list, because an
		// "actionable" set that quietly means "all files" is exactly the kind
		// of half-truth this pack keeps finding.
		return result;
	}
} // namespace bpr::application
