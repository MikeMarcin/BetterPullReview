#include "bpr/core/review_cycle_material.hpp"

#include <algorithm>
#include <string_view>

namespace bpr::core {
	namespace {
		/// A pointer's first line is `version <scheme>//git-lfs.github.com/spec/v1`;
		/// only its fixed parts are compared, so no locator is held here.
		constexpr auto lfs_version_prefix = std::string_view{ "version " };
		constexpr auto lfs_version_suffix = std::string_view{ "//git-lfs.github.com/spec/v1" };

		auto contains(const std::span<const std::uint8_t> aBytes, const std::string_view aNeedle) -> bool
		{
			const auto found = std::ranges::search(aBytes, aNeedle, [](const std::uint8_t aByte, const char aCharacter) {
				return aByte == static_cast<std::uint8_t>(aCharacter);
			});
			return !found.empty();
		}

		auto starts_with(const std::span<const std::uint8_t> aBytes, const std::string_view aPrefix) -> bool
		{
			return aBytes.size() >= aPrefix.size()
				&& std::ranges::equal(aBytes.first(aPrefix.size()), aPrefix, [](const std::uint8_t aByte, const char aCharacter) {
					   return aByte == static_cast<std::uint8_t>(aCharacter);
				   });
		}

		/// Length of the well-formed UTF-8 sequence at `aOffset`, or zero.
		auto sequence_length(const std::span<const std::uint8_t> aText, const std::size_t aOffset) noexcept -> std::size_t
		{
			const auto lead = aText[aOffset];
			if (lead < 0x80U) {
				return 1;
			}
			auto length = std::size_t{ 0 };
			auto low    = std::uint8_t{ 0x80U };
			auto high   = std::uint8_t{ 0xBFU };
			if (lead >= 0xC2U && lead <= 0xDFU) {
				length = 2;
			} else if (lead >= 0xE0U && lead <= 0xEFU) {
				length = 3;
				low    = static_cast<std::uint8_t>(lead == 0xE0U ? 0xA0U : 0x80U);
				high   = static_cast<std::uint8_t>(lead == 0xEDU ? 0x9FU : 0xBFU);
			} else if (lead >= 0xF0U && lead <= 0xF4U) {
				length = 4;
				low    = static_cast<std::uint8_t>(lead == 0xF0U ? 0x90U : 0x80U);
				high   = static_cast<std::uint8_t>(lead == 0xF4U ? 0x8FU : 0xBFU);
			} else {
				return 0;
			}
			if (aOffset + length > aText.size() || aText[aOffset + 1] < low || aText[aOffset + 1] > high) {
				return 0;
			}
			for (std::size_t index = 2; index < length; ++index) {
				if ((aText[aOffset + index] & 0xC0U) != 0x80U) {
					return 0;
				}
			}
			return length;
		}
	}

	void material_classifier::add(const std::span<const std::uint8_t> aBytes) noexcept
	{
		for (const auto byte : aBytes) {
			add_byte(byte);
		}
	}

	void material_classifier::add_byte(const std::uint8_t aValue) noexcept
	{
		if (Size < Head.size()) {
			Head.at(static_cast<std::size_t>(Size)) = aValue;
		}
		if (Size < pointer_limit) {
			Prefix.at(static_cast<std::size_t>(Size)) = aValue;
		}
		++Size;
		Last = aValue;
		Nul  = Nul || aValue == 0U;

		if (PreviousCr) {
			SawCrlf = SawCrlf || aValue == '\n';
			SawCr   = SawCr || aValue != '\n';
		} else if (aValue == '\n') {
			SawLf = true;
		}
		PreviousCr = aValue == '\r';

		if (InvalidUtf8) {
			return;
		}
		if (Pending > 0) {
			if (aValue < NextLow || aValue > NextHigh) {
				InvalidUtf8 = true;
				return;
			}
			--Pending;
			NextLow  = 0x80U;
			NextHigh = 0xBFU;
			return;
		}
		if (aValue < 0x80U) {
			return;
		}
		if (aValue >= 0xC2U && aValue <= 0xDFU) {
			Pending = 1;
		} else if (aValue >= 0xE0U && aValue <= 0xEFU) {
			Pending  = 2;
			NextLow  = static_cast<std::uint8_t>(aValue == 0xE0U ? 0xA0U : 0x80U);
			NextHigh = static_cast<std::uint8_t>(aValue == 0xEDU ? 0x9FU : 0xBFU);
		} else if (aValue >= 0xF0U && aValue <= 0xF4U) {
			Pending  = 3;
			NextLow  = static_cast<std::uint8_t>(aValue == 0xF0U ? 0x90U : 0x80U);
			NextHigh = static_cast<std::uint8_t>(aValue == 0xF4U ? 0x8FU : 0xBFU);
		} else {
			InvalidUtf8 = true;
		}
	}

	auto material_classifier::finish() const noexcept -> material_profile
	{
		auto profile = material_profile{ .Size = Size };
		if (Size == 0) {
			return profile;
		}
		const auto head    = std::span<const std::uint8_t>{ Head }.first(static_cast<std::size_t>(std::min<std::uint64_t>(Size, Head.size())));
		const auto utf8Bom = starts_with(head, "\xEF\xBB\xBF");
		const auto wideBom = starts_with(head, "\xFF\xFE") || starts_with(head, "\xFE\xFF") || (head.size() == 4 && head[0] == 0 && head[1] == 0 && head[2] == 0xFEU && head[3] == 0xFFU);
		// A wide byte-order mark explains its NUL bytes: unsupported, not binary.
		if (!wideBom && Nul) {
			profile.Kind = material_text_kind::binary;
		} else if (wideBom || InvalidUtf8 || Pending > 0) {
			profile.Kind = material_text_kind::unsupported_encoding;
		} else {
			profile.Kind = utf8Bom ? material_text_kind::utf8_bom : material_text_kind::utf8;
		}

		const auto trailingCr = PreviousCr;
		const auto kinds      = (SawLf ? 1 : 0) + (SawCrlf ? 1 : 0) + ((SawCr || trailingCr) ? 1 : 0);
		if (kinds > 1) {
			profile.LineEndings = material_line_endings::mixed;
		} else if (SawLf) {
			profile.LineEndings = material_line_endings::lf;
		} else if (SawCrlf) {
			profile.LineEndings = material_line_endings::crlf;
		} else if (SawCr || trailingCr) {
			profile.LineEndings = material_line_endings::cr;
		}
		profile.FinalNewline = Last == '\n' || Last == '\r';

		if (Size < pointer_limit) {
			const auto prefix  = std::span<const std::uint8_t>{ Prefix }.first(static_cast<std::size_t>(Size));
			const auto newline = std::ranges::find(prefix, std::uint8_t{ '\n' });
			const auto first   = prefix.first(static_cast<std::size_t>(newline - prefix.begin()));
			const auto version = starts_with(first, lfs_version_prefix) && first.size() > lfs_version_suffix.size()
				&& std::ranges::equal(first.last(lfs_version_suffix.size()), lfs_version_suffix, [](const std::uint8_t aByte, const char aCharacter) {
									 return aByte == static_cast<std::uint8_t>(aCharacter);
								 });
			profile.LfsPointer = newline != prefix.end() && version && contains(prefix, "\noid sha256:") && contains(prefix, "\nsize ");
		}
		return profile;
	}

	auto classify_material(const std::span<const std::uint8_t> aBytes) noexcept -> material_profile
	{
		auto classifier = material_classifier{ };
		classifier.add(aBytes);
		return classifier.finish();
	}

	auto utf8_boundary(const std::span<const std::uint8_t> aText, const std::uint64_t aOffset) noexcept -> bool
	{
		if (aOffset > aText.size()) {
			return false;
		}
		return aOffset == aText.size() || (aText[static_cast<std::size_t>(aOffset)] & 0xC0U) != 0x80U;
	}

	auto utf8_column(const std::span<const std::uint8_t> aLine, const std::uint64_t aOffset) noexcept -> std::optional<std::uint64_t>
	{
		auto column = std::uint64_t{ 0 };
		for (std::size_t position = 0; position <= aLine.size();) {
			if (position == aOffset) {
				return column;
			}
			if (position == aLine.size() || position > aOffset) {
				return std::nullopt;
			}
			const auto length = sequence_length(aLine, position);
			if (length == 0) {
				return std::nullopt;
			}
			position += length;
			++column;
		}
		return std::nullopt;
	}

	auto utf8_offset(const std::span<const std::uint8_t> aLine, const std::uint64_t aColumn) noexcept -> std::optional<std::uint64_t>
	{
		auto column = std::uint64_t{ 0 };
		for (std::size_t position = 0;;) {
			if (column == aColumn) {
				return position;
			}
			if (position == aLine.size()) {
				return std::nullopt;
			}
			const auto length = sequence_length(aLine, position);
			if (length == 0) {
				return std::nullopt;
			}
			position += length;
			++column;
		}
	}
}
