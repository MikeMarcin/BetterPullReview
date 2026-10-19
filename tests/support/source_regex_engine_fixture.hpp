#ifndef BPR_TESTS_SUPPORT_SOURCE_REGEX_ENGINE_FIXTURE_HPP
#define BPR_TESTS_SUPPORT_SOURCE_REGEX_ENGINE_FIXTURE_HPP

#include "bpr/core/source_matcher.hpp"

namespace bpr::tests {
	/// \brief The product's RE2 source-search engine, shared by matcher tests.
	inline auto search_engine() -> const core::source_regex_engine&
	{
		return core::default_source_regex_engine();
	}
}

#endif
