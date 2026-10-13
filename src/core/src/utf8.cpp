#include "bpr/core/utf8.hpp"

#include <utf8proc.h>

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
}
