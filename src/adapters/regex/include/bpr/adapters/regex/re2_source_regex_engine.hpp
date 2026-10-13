#ifndef BPR_SRC_ADAPTERS_REGEX_INCLUDE_BPR_ADAPTERS_REGEX_RE2_SOURCE_REGEX_ENGINE_HPP
#define BPR_SRC_ADAPTERS_REGEX_INCLUDE_BPR_ADAPTERS_REGEX_RE2_SOURCE_REGEX_ENGINE_HPP

#include "bpr/core/source_matcher.hpp"

namespace bpr::adapters::regex {

	/// \brief Source search's regex engine: Google RE2, linear time, no backtracking.
	/// \details [[BPR-1431]]. Replaces a hand-written engine. Patterns use RE2
	/// syntax in UTF-8 mode with leftmost-first semantics and no captures; each
	/// program's memory, including its DFA caches, is capped by the requested
	/// maximum. Compiled programs are immutable and safe to share across threads.
	class re2_source_regex_engine final : public core::source_regex_engine
	{
	public:
		[[nodiscard]] auto compile(std::string_view aPattern, core::source_regex_syntax aSyntax) const -> core::source_regex_compile override;
	};
}

#endif
