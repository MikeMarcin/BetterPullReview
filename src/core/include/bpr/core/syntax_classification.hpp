#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SYNTAX_CLASSIFICATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SYNTAX_CLASSIFICATION_HPP

#include "bpr/core/appearance_model.hpp"

#include <cstdint>
#include <vector>

namespace bpr::core {
	/// \brief Describes one classified token span.
	///
	/// \details `Line` is the zero-based line of the complete blob, `Column` the
	/// zero-based byte offset within that line, and `Length` the span length in
	/// bytes. Offsets are byte offsets because the host never decodes the blob.
	/// `Role` is a core role, so a span names a semantic category and never a
	/// colour.
	struct token_span
	{
		std::uint32_t   Line   = 0U;
		std::uint32_t   Column = 0U;
		std::uint32_t   Length = 0U;
		appearance_role Role   = appearance_role::syntax_normal;

		auto operator<=>(const token_span&) const = default;
	};

	using token_span_list = std::vector<token_span>;
	enum class syntax_classification_status : std::uint8_t {
		available,
		cancelled,
		unavailable,
		limit_exceeded,
	};

} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_SYNTAX_CLASSIFICATION_HPP
