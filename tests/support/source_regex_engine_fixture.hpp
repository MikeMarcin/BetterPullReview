#ifndef BPR_TESTS_SUPPORT_SOURCE_REGEX_ENGINE_FIXTURE_HPP
#define BPR_TESTS_SUPPORT_SOURCE_REGEX_ENGINE_FIXTURE_HPP

#include "bpr/adapters/regex/re2_source_regex_engine.hpp"

namespace bpr::tests {
	/// \brief The product's RE2 source-search engine, shared by matcher tests.
	inline auto search_engine() -> const core::source_regex_engine&
	{
		static const auto engine = adapters::regex::re2_source_regex_engine{ };
		return engine;
	}
}

#endif
