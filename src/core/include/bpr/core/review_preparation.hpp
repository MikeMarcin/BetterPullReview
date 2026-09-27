#ifndef BPR_CORE_REVIEW_PREPARATION_HPP
#define BPR_CORE_REVIEW_PREPARATION_HPP
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace bpr::core {
	enum class writing_hook : std::uint8_t { problem,
											 solution,
											 outline,
											 comment };
	inline constexpr std::size_t writing_hook_count                = 4;
	inline constexpr std::size_t maximum_writing_instruction_bytes = 8192;
	/// \brief Profile-owned text overrides; empty selects the shipped version.
	struct writing_hook_settings
	{
		std::array<std::string, writing_hook_count> Overrides;
		std::uint64_t                               Revision                                        = 1;
		auto                                        operator<=>(const writing_hook_settings&) const = default;
	};
	[[nodiscard]] auto valid_writing_utf8(std::string_view aText) noexcept -> bool;
	[[nodiscard]] auto valid(const writing_hook_settings& aSettings) noexcept -> bool;
}
#endif
