// [[RFC-BPR-0082]] [[BPR-1466]]. The intraline candidates' diff-match-patch
// piece: the standard-library port of Google's diff-match-patch (leutloff,
// ba3bbe3), with semantic cleanup, over a pair's Unicode scalars. Its
// deadline is disabled (Diff_Timeout 0), but the port still reads clock() in
// its bisection loop; the report records that a core placement needs it
// removed. It has no stop hook: a stop is honoured before and after a pair.

#include "candidates/candidates.hpp"

#include "diff_match_patch.h"

#include <cstdint>
#include <cwctype>
#include <string>
#include <vector>

/// diff-match-patch's character traits for Unicode scalars held in char32_t,
/// so the port behaves the same where wchar_t is 16 bits.
template <>
struct diff_match_patch_traits<char32_t> : diff_match_patch_utf32_direct<char32_t>
{
	static auto is_alnum(const char32_t aValue) -> bool
	{
		return std::iswalnum(static_cast<std::wint_t>(aValue)) != 0;
	}

	static auto is_digit(const char32_t aValue) -> bool
	{
		return aValue >= U'0' && aValue <= U'9';
	}

	static auto is_space(const char32_t aValue) -> bool
	{
		return std::iswspace(static_cast<std::wint_t>(aValue)) != 0;
	}

	static auto to_int(const char32_t* aText) -> int
	{
		auto value = 0;
		for (; *aText >= U'0' && *aText <= U'9'; ++aText) {
			value = value * 10 + static_cast<int>(*aText - U'0');
		}
		return value;
	}

	static auto from_wchar(const wchar_t aValue) -> char32_t
	{
		return static_cast<char32_t>(aValue);
	}

	static auto to_wchar(const char32_t aValue) -> wchar_t
	{
		return static_cast<wchar_t>(aValue);
	}

	static auto cs(const wchar_t* aText) -> std::u32string
	{
		auto result = std::u32string{ };
		for (; *aText != L'\0'; ++aText) {
			result.push_back(static_cast<char32_t>(*aText));
		}
		return result;
	}

	static const char32_t eol = U'\n';
	static const char32_t tab = U'\t';
};

namespace bpr::evaluation {
	auto dmp_matches(const std::span<const std::uint32_t> aLeft, const std::span<const std::uint32_t> aRight) -> std::vector<core::diff_match>
	{
		using engine     = diff_match_patch<std::u32string>;
		auto dmp         = engine{ };
		dmp.Diff_Timeout = 0.0F;
		const auto left  = std::u32string{ aLeft.begin(), aLeft.end() };
		const auto right = std::u32string{ aRight.begin(), aRight.end() };
		auto       diffs = dmp.diff_main(left, right, false);
		engine::diff_cleanupSemantic(diffs);
		auto result     = std::vector<core::diff_match>{ };
		auto leftIndex  = std::uint32_t{ 0U };
		auto rightIndex = std::uint32_t{ 0U };
		for (const auto& diff : diffs) {
			const auto length = static_cast<std::uint32_t>(diff.text.size());
			switch (diff.operation) {
			case engine::EQUAL:
				for (std::uint32_t offset = 0U; offset < length; ++offset) {
					result.push_back({ .Left = leftIndex + offset, .Right = rightIndex + offset });
				}
				leftIndex += length;
				rightIndex += length;
				break;
			case engine::DELETE:
				leftIndex += length;
				break;
			case engine::INSERT:
				rightIndex += length;
				break;
			}
		}
		return result;
	}
} // namespace bpr::evaluation
