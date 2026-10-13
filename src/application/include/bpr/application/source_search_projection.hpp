#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_SEARCH_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_SEARCH_PROJECTION_HPP

#include "bpr/application/source_corpus_port.hpp"
#include "bpr/core/source_matcher.hpp"
#include <memory>
#include <string>

namespace bpr::application {
	/// \brief Content-free execution measurements for structural regression checks.
	struct source_search_statistics
	{
		std::uint64_t Compilations            = 0U;
		std::uint64_t ReadCalls               = 0U;
		std::uint64_t CacheHits               = 0U;
		std::uint64_t AcquiredBytes           = 0U;
		std::uint64_t Scans                   = 0U;
		std::uint64_t AcquisitionMicroseconds = 0U;
		std::uint64_t MatchingMicroseconds    = 0U;
		std::uint64_t PeakOwnedBound          = 0U;
	};
	/// \brief One exact source occurrence; locations retain both proven sides.
	struct source_search_match
	{
		core::file_identity       File;
		core::source_match_record Match;
	};
	/// \brief Honest per-side qualification; absent sides are not skipped material.
	struct source_search_skipped_side
	{
		core::file_identity File;
		core::diff_side     Side   = core::diff_side::left;
		diff_blob_status    Reason = diff_blob_status::failure;
	};
	/// \brief Runtime-only search presentation, with explicit empty/partial/error coverage.
	struct source_search_projection
	{
		std::optional<core::source_search_scope>                       Scope;
		std::string                                                    Query;
		std::string                                                    Include;
		std::string                                                    Exclude;
		core::source_search_options                                    Options;
		core::source_search_input                                      Input  = core::source_search_input::empty;
		core::source_search_status                                     Status = core::source_search_status::idle;
		core::source_search_summary                                    Summary;
		std::uint64_t                                                  Generation = 0U;
		source_search_statistics                                       Statistics;
		std::shared_ptr<const std::vector<source_search_match>>        Matches;
		std::optional<std::uint32_t>                                   CurrentMatch;
		std::shared_ptr<const source_corpus>                           Corpus;
		std::shared_ptr<const std::vector<source_search_skipped_side>> Skipped;
		bool                                                           RetryAvailable = false;
		core::source_search_reason                                     Reason         = core::source_search_reason::none;
		bool                                                           Open           = false;
		bool                                                           Wrapped        = false;
		bool                                                           NoResults      = false;
	};

	class source_search_projection_port
	{
	public:
		virtual ~source_search_projection_port()                          = default;
		virtual void publish(const source_search_projection& aProjection) = 0;
	};

}
#endif
