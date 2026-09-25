#ifndef BPR_APP_STARTUP_DIAGNOSTIC_HPP
#define BPR_APP_STARTUP_DIAGNOSTIC_HPP

#include <exception>
#include <string>
#include <string_view>

namespace bpr::app::detail {
	inline constexpr std::size_t startup_diagnostic_limit = 2048U;

	/// \brief Captures at most 2 KiB of a developer exception for one terminal line.
	/// \details Reads at most the limit plus one byte before allocating the result.
	/// Truncation is explicit and ASCII controls become spaces. This is a resource
	/// and terminal-format bound, not a privacy classifier for arbitrary text.
	[[nodiscard]] inline auto bounded_startup_diagnostic(const char* const aText) -> std::string
	{
		if (aText == nullptr) {
			return "exception text unavailable";
		}
		auto length = std::size_t{ 0U };
		while (length < startup_diagnostic_limit && aText[length] != '\0') {
			++length;
		}
		auto text = std::string{ aText, length };
		for (auto& character : text) {
			const auto byte = static_cast<unsigned char>(character);
			if (byte < 0x20U || byte == 0x7fU) {
				character = ' ';
			}
		}
		if (length == startup_diagnostic_limit && aText[length] != '\0') {
			constexpr std::string_view suffix = " [truncated]";
			text.resize(startup_diagnostic_limit - suffix.size());
			text.append(suffix);
		}
		return text;
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
