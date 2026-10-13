#include "bpr/core/utf8.hpp"

#include <utf8proc.h>

#include <algorithm>
#include <vector>

namespace bpr::core::utf8 {
	auto decode(const std::string_view aText, std::size_t& aOffset) noexcept -> std::optional<char32_t>
	{
		if (aOffset >= aText.size()) {
			return std::nullopt;
		}
		auto       value    = utf8proc_int32_t{ -1 };
		const auto consumed = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(aText.data() + aOffset), static_cast<utf8proc_ssize_t>(aText.size() - aOffset), &value);
		if (consumed <= 0 || value < 0) {
			return std::nullopt;
		}
		aOffset += static_cast<std::size_t>(consumed);
		return static_cast<char32_t>(value);
	}

	auto valid(const std::string_view aText) noexcept -> bool
	{
		for (auto offset = std::size_t{ 0 }; offset < aText.size();) {
			if (!decode(aText, offset)) {
				return false;
			}
		}
		return true;
	}

	auto append(std::string& aOut, const char32_t aValue) -> bool
	{
		if (!utf8proc_codepoint_valid(static_cast<utf8proc_int32_t>(aValue))) {
			return false;
		}
		utf8proc_uint8_t bytes[4] = { };
		const auto       length   = utf8proc_encode_char(static_cast<utf8proc_int32_t>(aValue), bytes);
		aOut.append(reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(length));
		return true;
	}

	auto decompose(const std::string_view aText) -> std::optional<std::u32string>
	{
		if (aText.empty()) {
			return std::u32string{ };
		}
		const auto* const bytes   = reinterpret_cast<const utf8proc_uint8_t*>(aText.data());
		const auto        length  = static_cast<utf8proc_ssize_t>(aText.size());
		constexpr auto    options = static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_DECOMPOSE);
		auto              needed  = utf8proc_decompose(bytes, length, nullptr, 0, options);
		if (needed < 0) {
			return std::nullopt;
		}
		auto points = std::vector<utf8proc_int32_t>(static_cast<std::size_t>(needed));
		needed      = utf8proc_decompose(bytes, length, points.data(), needed, options);
		if (needed < 0 || static_cast<std::size_t>(needed) != points.size()) {
			return std::nullopt;
		}
		auto result = std::u32string{ };
		result.reserve(points.size());
		for (const auto point : points) {
			result.push_back(static_cast<char32_t>(point));
		}
		return result;
	}

	auto simple_fold(const char32_t aValue) noexcept -> char32_t
	{
		const auto value = static_cast<utf8proc_int32_t>(aValue);
		return static_cast<char32_t>(utf8proc_tolower(utf8proc_toupper(value)));
	}

	auto grapheme_end(const std::string_view aText, const std::size_t aOffset) noexcept -> std::size_t
	{
		if (aOffset >= aText.size()) {
			return aText.size();
		}
		// Two ASCII bytes always break, except CR LF.
		const auto lead = static_cast<unsigned char>(aText[aOffset]);
		if (lead < 0x80U && (aOffset + 1U == aText.size() || (static_cast<unsigned char>(aText[aOffset + 1U]) < 0x80U && !(lead == '\r' && aText[aOffset + 1U] == '\n')))) {
			return aOffset + 1U;
		}
		auto offset   = aOffset;
		auto previous = decode(aText, offset);
		if (!previous) {
			return aOffset + 1U;
		}
		// The state is reset at a boundary, which is where every call starts.
		auto state = utf8proc_int32_t{ 0 };
		while (offset < aText.size()) {
			auto       next  = offset;
			const auto value = decode(aText, next);
			if (!value || utf8proc_grapheme_break_stateful(static_cast<utf8proc_int32_t>(*previous), static_cast<utf8proc_int32_t>(*value), &state)) {
				break;
			}
			previous = value;
			offset   = next;
		}
		return offset;
	}

	auto grapheme_width(const std::string_view aCluster) noexcept -> std::uint32_t
	{
		constexpr auto presentation = char32_t{ 0xFE0F };
		const auto     regional     = [](const char32_t aValue) { return aValue >= 0x1F1E6 && aValue <= 0x1F1FF; };
		auto           width        = 0;
		auto           count        = std::size_t{ 0 };
		auto           first        = char32_t{ 0 };
		for (auto offset = std::size_t{ 0 }; offset < aCluster.size();) {
			const auto value = decode(aCluster, offset);
			if (!value) {
				break;
			}
			first = count == 0U ? *value : first;
			++count;
			width = std::max(width, utf8proc_charwidth(static_cast<utf8proc_int32_t>(*value)));
			if (*value == presentation || (count == 2U && regional(first) && regional(*value))) {
				width = std::max(width, 2);
			}
		}
		return static_cast<std::uint32_t>(std::clamp(width, 1, 2));
	}

	auto classify_word(const char32_t aValue) noexcept -> word_class
	{
		if (aValue == U' ' || aValue == U'\t') {
			return word_class::space;
		}
		switch (utf8proc_category(static_cast<utf8proc_int32_t>(aValue))) {
		case UTF8PROC_CATEGORY_LU:
		case UTF8PROC_CATEGORY_LL:
		case UTF8PROC_CATEGORY_LT:
		case UTF8PROC_CATEGORY_LM:
		case UTF8PROC_CATEGORY_LO:
		case UTF8PROC_CATEGORY_MN:
		case UTF8PROC_CATEGORY_MC:
		case UTF8PROC_CATEGORY_ME:
		case UTF8PROC_CATEGORY_ND:
		case UTF8PROC_CATEGORY_NL:
		case UTF8PROC_CATEGORY_NO:
		case UTF8PROC_CATEGORY_PC: return word_class::word;
		case UTF8PROC_CATEGORY_ZS: return word_class::space;
		default:                   return word_class::other;
		}
	}
}
