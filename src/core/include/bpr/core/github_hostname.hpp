#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_HOSTNAME_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_HOSTNAME_HPP

#include <cstddef>
#include <string_view>

namespace bpr::core {
	inline constexpr std::size_t maximum_github_hostname_bytes       = 253U;
	inline constexpr std::size_t maximum_github_hostname_label_bytes = 63U;

	/// \brief Computes the valid github canonical hostname result.
	[[nodiscard]] constexpr auto valid_github_canonical_hostname(
		const std::string_view aHostname) noexcept -> bool
	{
		if (
			aHostname.empty() || aHostname.size() > maximum_github_hostname_bytes
			|| aHostname.front() == '.' || aHostname.back() == '.') {
			return false;
		}

		bool        sawDot     = false;
		std::size_t labelBegin = 0U;
		while (labelBegin < aHostname.size()) {
			const auto labelEnd = aHostname.find('.', labelBegin);
			const auto end      = labelEnd == std::string_view::npos ? aHostname.size() : labelEnd;
			const auto label    = aHostname.substr(labelBegin, end - labelBegin);
			if (
				label.empty() || label.size() > maximum_github_hostname_label_bytes
				|| label.front() == '-' || label.back() == '-'
				|| label.starts_with("xn--")) {
				return false;
			}

			bool labelHasLetter = false;
			for (const auto character : label) {
				if (character >= 'a' && character <= 'z') {
					labelHasLetter = true;
					continue;
				}
				if (
					(character < '0' || character > '9')
					&& character != '-') {
					return false;
				}
			}

			if (end == aHostname.size()) {
				return sawDot && labelHasLetter;
			}
			sawDot     = true;
			labelBegin = end + 1U;
		}
		return false;
	}

}
#endif
