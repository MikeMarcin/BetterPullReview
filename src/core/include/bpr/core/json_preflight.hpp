#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_JSON_PREFLIGHT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_JSON_PREFLIGHT_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bpr::core {
	/// \brief The outcome of one bounded structural JSON check.
	enum class json_preflight_result : std::uint8_t {
		valid,
		malformed,
		duplicate_key,
		structural_limit,
	};

	/// \brief The most object members any caller may admit; keys are compared
	/// within one fixed stack buffer of this size.
	inline constexpr std::size_t maximum_json_preflight_object_members = 256U;

	/// \brief Ceilings for one document, chosen per response class.
	struct json_preflight_limits
	{
		std::size_t Depth         = 32U;
		std::size_t ObjectMembers = 64U;
		std::size_t ArrayElements = 100U;
		std::size_t Values        = 50'000U;
		std::size_t StringBytes   = 256U * 1024U;
	};

	/// \brief The result and how many values the document holds.
	struct json_preflight_report
	{
		json_preflight_result Result = json_preflight_result::malformed;
		std::size_t           Values = 0U;
	};

	/// \brief One bounded structural check of a JSON document before a decoder
	/// such as glaze reads its exact fields ([[BPR-1433]]). glaze decodes but
	/// does not refuse duplicate keys, so this check stays: exactly one value,
	/// well-formed strings with no control character, numbers in JSON grammar
	/// whose magnitude is below 10^308 (so none is infinite), no duplicate or
	/// escaped object key, and the given ceilings. A string beyond
	/// `StringBytes` or any other ceiling is a structural limit, not malformed.
	/// It proves nothing about meaning. `ObjectMembers` above
	/// `maximum_json_preflight_object_members` is treated as that maximum.
	[[nodiscard]] auto json_preflight(std::string_view aInput, const json_preflight_limits& aLimits) noexcept -> json_preflight_report;
}

#endif
