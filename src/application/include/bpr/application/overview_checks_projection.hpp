#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OVERVIEW_CHECKS_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OVERVIEW_CHECKS_PROJECTION_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace bpr::application {
	struct overview_check_summary
	{
		std::size_t Passed  = 0U;
		std::size_t Failed  = 0U;
		std::size_t Pending = 0U;
		std::size_t Skipped = 0U;
		std::size_t Other   = 0U;

		auto operator<=>(const overview_check_summary&) const = default;

		[[nodiscard]] auto total() const noexcept -> std::size_t
		{
			return Passed + Failed + Pending + Skipped + Other;
		}
	};

	/// \brief A bounded, literal result row, in failure/waiting/settled order.
	struct overview_check_row
	{
		std::string Identity, Name, Producer, Kind, Outcome, Description, DetailsUrl;
		std::string Requirement                                  = "Requirement unknown";
		bool        Failed                                       = false;
		bool        Pending                                      = false;
		bool        LinkAvailable                                = false;
		auto        operator<=>(const overview_check_row&) const = default;
	};

	/// Read-only check results, independently of the remaining PR status.
	struct overview_checks_projection
	{
		overview_check_summary          Checks;
		std::vector<overview_check_row> CheckRows;
		bool                            ChecksComplete = true;
		std::string                     ChecksNote;
		auto                            operator<=>(const overview_checks_projection&) const = default;
	};
}
#endif
