#include "bpr/core/review_preparation.hpp"

#include <algorithm>
#include <limits>
namespace bpr::core {
	auto valid_writing_utf8(std::string_view aText) noexcept -> bool
	{
		for (std::size_t i = 0; i < aText.size();) {
			const auto lead = static_cast<unsigned char>(aText[i++]);
			if (lead == 0) {
				return false;
			}
			if (lead < 0x80) {
				continue;
			}
			const auto count = lead >= 0xc2 && lead <= 0xdf ? 1U : lead >= 0xe0 && lead <= 0xef ? 2U
				: lead >= 0xf0 && lead <= 0xf4                                                  ? 3U
																								: 4U;
			if (count == 4 || count > aText.size() - i) {
				return false;
			}
			std::uint32_t point = lead & (0x7fU >> count);
			for (unsigned j = 0; j < count; ++j) {
				const auto byte = static_cast<unsigned char>(aText[i++]);
				if ((byte & 0xc0) != 0x80) {
					return false;
				}
				point = (point << 6) | (byte & 0x3f);
			}
			if (point < (count == 1 ? 0x80U : count == 2 ? 0x800U
														 : 0x10000U)
				|| point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) {
				return false;
			}
		}
		return true;
	}
	auto valid(const writing_hook_settings& aSettings) noexcept -> bool
	{
		return aSettings.Revision > 0 && aSettings.Revision < static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
			&& std::ranges::all_of(aSettings.Overrides, [](const auto& aText) { return aText.size() <= maximum_writing_instruction_bytes && valid_writing_utf8(aText); });
	}
}
