#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CPP_IDENTIFIER_CONTEXT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CPP_IDENTIFIER_CONTEXT_HPP

#include "bpr/core/syntax_classification.hpp"

#include <cstddef>
#include <stop_token>
#include <string_view>

namespace bpr::core {
	/// \brief Refines C++ identifiers using only grammar-protected complete-blob facts.
	/// \details Temporary state is bounded by admitted source bytes. Cancellation or
	/// span overflow publishes no partial result; no source, theme or repository is read.
	/// The caller admits a complete UTF-8 blob whose offsets fit uint32_t and
	/// grammar spans contained within its lines/bytes before calling this refinement.
	[[nodiscard]] auto refine_cpp_identifiers(
		std::string_view aText, token_span_list& aSpans,
		std::size_t aMaximumSpans, std::stop_token aStop)
		-> syntax_classification_status;
}

#endif
