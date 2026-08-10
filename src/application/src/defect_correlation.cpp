#include "bpr/application/defect_correlation.hpp"

#include <algorithm>
#include <unordered_map>

namespace bpr::application {
	namespace {
		/// \brief Raises one piece of evidence and folds it into the record.
		///
		/// \details The fold goes through `core::observe`, so the ceiling on
		/// what evidence can do is the core's and not this file's. Deciding
		/// here what a reply means would put the policy in the host, where the
		/// next person to add an evidence kind would have to rediscover it.
		void raise(
			core::defect_record&                aDefect,
			std::vector<core::defect_evidence>& aRaised,
			const core::defect_evidence_kind    aKind)
		{
			const auto evidence = core::defect_evidence{
				.Defect = aDefect.Id,
				.Kind   = aKind,
			};
			const auto outcome = core::observe(aDefect, evidence);
			if (!outcome.Changed) {
				return;
			}
			aDefect = outcome.Defect;
			aRaised.push_back(evidence);
		}
	} // namespace

	auto correlate(
		const std::span<const core::defect_record>       aDefects,
		const std::span<const defect_thread_link>        aLinks,
		const std::span<const defect_thread_observation> aThreads,
		const std::span<const core::source_line_ref>     aChangedAnchors)
		-> defect_correlation_result
	{
		auto threadFor = std::unordered_map<std::uint64_t, std::string>{ };
		for (const auto& link : aLinks) {
			threadFor.emplace(link.Defect.value(), link.ThreadId);
		}

		auto observationFor
			= std::unordered_map<std::string, const defect_thread_observation*>{ };
		for (const auto& thread : aThreads) {
			observationFor.emplace(thread.ThreadId, &thread);
		}

		auto result = defect_correlation_result{ };
		result.Defects.reserve(aDefects.size());

		for (const auto& original : aDefects) {
			auto defect = original;

			// Code moving beneath a defect is the weakest of the three: nobody
			// said anything, something merely changed nearby.
			const auto moved = std::ranges::find(aChangedAnchors, defect.Anchor)
				!= aChangedAnchors.end();
			if (moved) {
				raise(
					defect,
					result.Evidence,
					core::defect_evidence_kind::nearby_code_change);
			}

			const auto linked = threadFor.find(defect.Id.value());
			if (linked != threadFor.end()) {
				const auto observed = observationFor.find(linked->second);
				if (observed != observationFor.end()) {
					if (observed->second->AuthorRepliedSince) {
						raise(
							defect,
							result.Evidence,
							core::defect_evidence_kind::author_reply);
					}
					if (observed->second->Resolved) {
						raise(
							defect,
							result.Evidence,
							core::defect_evidence_kind::
								thread_resolved_externally);
					}
				}
			}

			result.Defects.push_back(defect);
		}

		return result;
	}
} // namespace bpr::application
