#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_JSON_PREFLIGHT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_JSON_PREFLIGHT_HPP

#include "bpr/core/json_preflight.hpp"

#include <cstddef>
#include <limits>
#include <string_view>

namespace bpr::adapters::github::detail {
	/// \brief The mutation and publication contracts' ceilings for the shared
	/// core structural check ([[BPR-1433]]); objects admit 128 members.
	struct json_preflight_limits
	{
		std::size_t Depth    = 24U;
		std::size_t Elements = 128U;
		std::size_t Values   = 4'096U;
	};

	/// \brief Whether a response passes the shared core structural check.
	class json_preflight final
	{
	public:
		explicit json_preflight(const std::string_view aInput, const json_preflight_limits aLimits = { }) noexcept
			: Input{ aInput }
			, Limits{ aLimits }
		{
		}

		[[nodiscard]] auto run() const noexcept -> bool
		{
			return core::json_preflight(Input, { .Depth = Limits.Depth, .ObjectMembers = 128U, .ArrayElements = Limits.Elements, .Values = Limits.Values, .StringBytes = std::numeric_limits<std::size_t>::max() }).Result
				== core::json_preflight_result::valid;
		}

	private:
		std::string_view      Input;
		json_preflight_limits Limits;
	};
}

#endif
