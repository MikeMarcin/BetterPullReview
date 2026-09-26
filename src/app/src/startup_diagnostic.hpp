#ifndef BPR_APP_STARTUP_DIAGNOSTIC_HPP
#define BPR_APP_STARTUP_DIAGNOSTIC_HPP

#include <exception>
#include <string>
#include <string_view>

namespace bpr::app::detail {
	/// \brief Retains a known composition cause; arbitrary exception text is omitted.
	[[nodiscard]] inline auto bounded_startup_diagnostic(const char* const aText) -> std::string
	{
		if (aText == nullptr) {
			return "exception text unavailable";
		}
		constexpr std::string_view known = "Application data or installed Git is unavailable";
		auto                       size  = std::size_t{ 0U };
		while (size <= known.size() && aText[size] != '\0') {
			++size;
		}
		if (std::string_view{ aText, size } == known) {
			return std::string{ known };
		}
		return "[omitted:unclassified-exception]";
	}

	/// \brief Names a caught developer exception without an unbounded what() copy.
	[[nodiscard]] inline auto startup_failure_cause_text(const std::exception_ptr& aCause) -> std::string
	{
		if (!aCause) {
			return { };
		}
		try {
			std::rethrow_exception(aCause);
		} catch (const std::exception& error) {
			return bounded_startup_diagnostic(error.what());
		} catch (...) {
			return "an exception that is not a std::exception";
		}
	}
} // namespace bpr::app::detail

#endif // BPR_APP_STARTUP_DIAGNOSTIC_HPP
