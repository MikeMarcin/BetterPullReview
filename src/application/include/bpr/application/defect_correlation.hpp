#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEFECT_CORRELATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEFECT_CORRELATION_HPP

#include "bpr/core/defect_workflow.hpp"

#include <compare>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace bpr::application {
	/// \brief Ties one local defect to one ordinary GitHub thread.
	///
	/// \details The link is the whole of the relationship. The thread carries
	/// no BetterPullReview marker and does not know it has been classified —
	/// which is why a GitHub-only collaborator sees an ordinary review thread.
	struct defect_thread_link
	{
		core::defect_id Defect;
		std::string     ThreadId;

		auto operator<=>(const defect_thread_link&) const = default;
	};

	/// \brief What the host observed about one thread on the last refresh.
	struct defect_thread_observation
	{
		std::string ThreadId;
		bool        Resolved           = false;
		bool        AuthorRepliedSince = false;

		auto operator<=>(const defect_thread_observation&) const = default;
	};

	/// \brief Reports what correlation raised.
	struct defect_correlation_result
	{
		std::vector<core::defect_record>   Defects;
		std::vector<core::defect_evidence> Evidence;

		auto operator<=>(const defect_correlation_result&) const = default;
	};

	/// \brief Correlates ordinary GitHub facts with local defect records.
	///
	/// \details This is a **pure function with no provider**. It cannot issue a
	/// request, so "classifying an existing thread does not mutate GitHub"
	/// holds because there is nothing here that could. Publishing, replying,
	/// and resolving all go through RFC-BPR-0008's mutation coordinator, which
	/// a user reaches by a distinct action.
	///
	/// Everything it raises is evidence. No path through it reaches `verified`,
	/// which is the property the whole RFC turns on and which the tests assert
	/// exhaustively rather than by example.
	///
	/// \param aChangedAnchors Comparison digests whose material moved in the
	/// advanced epoch. A defect anchored to one of them has had code change
	/// beneath it — a suggestion that it may have been addressed, and nothing
	/// more.
	[[nodiscard]] auto correlate(
		std::span<const core::defect_record>          aDefects,
		std::span<const defect_thread_link>           aLinks,
		std::span<const defect_thread_observation>    aThreads,
		std::span<const core::file_comparison_digest> aChangedAnchors)
		-> defect_correlation_result;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEFECT_CORRELATION_HPP
