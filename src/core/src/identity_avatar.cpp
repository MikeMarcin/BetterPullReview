#include "bpr/core/identity_avatar.hpp"

#include "bpr/core/appearance_contrast.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto srgb(const std::uint32_t aValue) noexcept -> appearance_color
		{
			constexpr auto mask = std::uint32_t{ 0xFFU };
			return appearance_color{
				static_cast<std::uint8_t>((aValue >> 16U) & mask),
				static_cast<std::uint8_t>((aValue >> 8U) & mask),
				static_cast<std::uint8_t>(aValue & mask),
				255U,
			};
		}

		/// \brief One slot's two halves.
		struct tone_pair
		{
			identity_tone Light;
			identity_tone Dark;
		};

		constexpr auto white = srgb(0xFFFFFFU);

		/// [[BPR-1583]]. Ten OKLCH hues (22, 52, 82, 140, 178, 215, 252, 282,
		/// 312 and 345 degrees). On a light ground each is the lightest fill
		/// that keeps white initials at 7.2:1; on a dark ground the darkest
		/// that keeps the hue's own near-black initials at 7.2:1.
		constexpr auto tones = std::array<tone_pair, identity_tone_count>{ {
			{ { srgb(0xA32730U), white }, { srgb(0xEC918EU), srgb(0x331414U) } },
			{ { srgb(0x8D4201U), white }, { srgb(0xE59A6BU), srgb(0x311806U) } },
			{ { srgb(0x715101U), white }, { srgb(0xCFA656U), srgb(0x2A1D01U) } },
			{ { srgb(0x1C6501U), white }, { srgb(0x84BB78U), srgb(0x11250DU) } },
			{ { srgb(0x006354U), white }, { srgb(0x4CC0AAU), srgb(0x00261FU) } },
			{ { srgb(0x056070U), white }, { srgb(0x44BBD5U), srgb(0x00242CU) } },
			{ { srgb(0x0258A0U), white }, { srgb(0x77B0EFU), srgb(0x0C2036U) } },
			{ { srgb(0x5148B0U), white }, { srgb(0xA2A5F1U), srgb(0x1C1C36U) } },
			{ { srgb(0x7A3A9CU), white }, { srgb(0xC69AE1U), srgb(0x271830U) } },
			{ { srgb(0x972C71U), white }, { srgb(0xE293C0U), srgb(0x301525U) } },
		} };

		/// The face of no one: a signed-out account, a hidden actor.
		constexpr auto neutral = tone_pair{ { srgb(0x50565EU), white }, { srgb(0xA9B1BAU), srgb(0x161A1FU) } };

		constexpr auto fnv_offset = std::uint32_t{ 2'166'136'261U };
		constexpr auto fnv_prime  = std::uint32_t{ 16'777'619U };

		constexpr auto adjust_steps = 16U;

		/// \brief MurmurHash3's 32-bit finalizer: every input bit reaches every
		/// output bit, so logins that differ in one letter land far apart.
		[[nodiscard]] constexpr auto finalised(std::uint32_t aHash) noexcept -> std::uint32_t
		{
			aHash ^= aHash >> 16U;
			aHash *= 0x85EB'CA6BU;
			aHash ^= aHash >> 13U;
			aHash *= 0xC2B2'AE35U;
			aHash ^= aHash >> 16U;
			return aHash;
		}

		[[nodiscard]] constexpr auto ascii_lower(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		[[nodiscard]] constexpr auto ascii_upper(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'a' && aCharacter <= 'z' ? static_cast<char>(aCharacter - 'a' + 'A') : aCharacter;
		}

		/// \brief Whether a byte separates words: ASCII that is not a letter or digit.
		[[nodiscard]] constexpr auto separates(const char aCharacter) noexcept -> bool
		{
			const auto byte = static_cast<unsigned char>(aCharacter);
			if (byte >= 0x80U) {
				return false;
			}
			const auto lower = ascii_lower(aCharacter);
			return !((lower >= 'a' && lower <= 'z') || (aCharacter >= '0' && aCharacter <= '9'));
		}

		/// \brief Whether a byte continues a UTF-8 sequence.
		[[nodiscard]] constexpr auto continues(const char aCharacter) noexcept -> bool
		{
			return (static_cast<unsigned char>(aCharacter) & 0xC0U) == 0x80U;
		}

		/// \brief The name without a trailing `[bot]`, any case.
		[[nodiscard]] auto without_bot(const std::string_view aName) noexcept -> std::string_view
		{
			constexpr auto bot = std::string_view{ "[bot]" };
			if (aName.size() <= bot.size()) {
				return aName;
			}
			const auto tail = aName.substr(aName.size() - bot.size());
			const auto same = std::ranges::equal(tail, bot, [](const char aLeft, const char aRight) { return ascii_lower(aLeft) == aRight; });
			return same ? aName.substr(0U, aName.size() - bot.size()) : aName;
		}

		/// \brief One channel a step of `adjust_steps` of the way to a target.
		[[nodiscard]] constexpr auto towards(const std::uint8_t aFrom, const std::uint8_t aTo, const std::uint32_t aStep) noexcept -> std::uint8_t
		{
			const auto from = static_cast<std::int32_t>(aFrom);
			const auto to   = static_cast<std::int32_t>(aTo);
			const auto step = static_cast<std::int32_t>(aStep);
			const auto half = static_cast<std::int32_t>(adjust_steps / 2U);
			const auto move = (to - from) * step;
			return static_cast<std::uint8_t>(from + (move >= 0 ? (move + half) : (move - half)) / static_cast<std::int32_t>(adjust_steps));
		}

		[[nodiscard]] auto clears(const appearance_color aFill, const std::span<const appearance_color> aGrounds) noexcept -> bool
		{
			return std::ranges::all_of(aGrounds, [aFill](const appearance_color aGround) {
				return contrast_ratio_hundredths(aFill, aGround) >= indicator_contrast_minimum_hundredths;
			});
		}
	} // namespace

	auto identity_tone_slot(const std::string_view aIdentity) noexcept -> std::optional<std::size_t>
	{
		if (aIdentity.empty()) {
			return std::nullopt;
		}
		auto hash = fnv_offset;
		for (const auto character : aIdentity) {
			hash = (hash ^ static_cast<unsigned char>(ascii_lower(character))) * fnv_prime;
		}
		return static_cast<std::size_t>(finalised(hash) % identity_tone_count);
	}

	auto identity_tone_of(const std::optional<std::size_t> aSlot, const identity_ground aGround) noexcept -> identity_tone
	{
		const auto& pair = aSlot.has_value() && *aSlot < identity_tone_count ? tones.at(*aSlot) : neutral;
		return aGround == identity_ground::light ? pair.Light : pair.Dark;
	}

	auto identity_ground_of(const appearance_color aGround) noexcept -> identity_ground
	{
		return contrast_ratio_hundredths(aGround, white) >= contrast_ratio_hundredths(aGround, srgb(0x000000U))
			? identity_ground::dark
			: identity_ground::light;
	}

	auto identity_tone_for(const std::string_view aIdentity, const std::span<const appearance_color> aGrounds) noexcept -> identity_tone
	{
		const auto ground = aGrounds.empty() ? identity_ground::light : identity_ground_of(aGrounds.front());
		const auto tabled = identity_tone_of(identity_tone_slot(aIdentity), ground);
		const auto target = ground == identity_ground::light ? srgb(0x000000U) : white;
		auto       tone   = tabled;
		for (auto step = 1U; step <= adjust_steps && !clears(tone.Fill, aGrounds); ++step) {
			tone.Fill = appearance_color{
				towards(tabled.Fill.Red, target.Red, step),
				towards(tabled.Fill.Green, target.Green, step),
				towards(tabled.Fill.Blue, target.Blue, step),
				255U,
			};
		}
		return tone;
	}

	auto identity_initials(const std::string_view aName) -> std::string
	{
		const auto name     = without_bot(aName);
		auto       initials = std::string{ };
		auto       words    = 0U;
		auto       inWord   = false;
		for (std::size_t index = 0U; index < name.size() && words < 2U; ++index) {
			const auto character = name[index];
			if (separates(character)) {
				inWord = false;
				continue;
			}
			if (inWord || continues(character)) {
				continue;
			}
			inWord = true;
			++words;
			initials.push_back(ascii_upper(character));
			for (auto next = index + 1U; next < name.size() && continues(name[next]); ++next) {
				initials.push_back(name[next]);
			}
		}
		return initials.empty() ? std::string{ "?" } : initials;
	}
} // namespace bpr::core
