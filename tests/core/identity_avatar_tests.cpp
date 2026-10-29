/// \file
/// \brief [[BPR-1583]]. Pins the one avatar rule every surface draws a person
/// with: the slot an identity keeps, the initials, and the tone table's
/// contrast against the four built-in themes' grounds.

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/identity_avatar.hpp"
#include "bpr/core/identity_avatar_picture.hpp"
#include "bpr/core/markdown_image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

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

	/// \brief One built-in theme's window and base grounds, as its profile
	/// assigns `application_surface` and `code_background`.
	struct theme_grounds
	{
		appearance_variant              Variant;
		std::array<appearance_color, 2> Grounds;
	};

	constexpr auto themes = std::array{
		theme_grounds{ appearance_variant::light, { srgb(0xF6F7F9U), srgb(0xFFFFFFU) } },
		theme_grounds{ appearance_variant::dark, { srgb(0x1C2026U), srgb(0x0D1117U) } },
		theme_grounds{ appearance_variant::high_contrast_light, { srgb(0xFFFFFFU), srgb(0xFFFFFFU) } },
		theme_grounds{ appearance_variant::high_contrast_dark, { srgb(0x000000U), srgb(0x000000U) } },
	};

	void test_an_identity_keeps_one_slot()
	{
		expect(identity_tone_slot("MikeMarcin") == identity_tone_slot("mikemarcin") && identity_tone_slot("MIKEMARCIN") == identity_tone_slot("mikemarcin"),
			   "a login is case-insensitive, so its case does not move its slot");
		expect(identity_tone_slot("").has_value() == false, "no identity has no slot");
		// FNV-1a 32 over the folded bytes, MurmurHash3's finalizer, modulo the
		// tone count: tbutler is slot 2 on every machine.
		auto hash = std::uint32_t{ 2'166'136'261U };
		for (const auto character : std::string_view{ "tbutler" }) {
			hash = (hash ^ static_cast<unsigned char>(character)) * 16'777'619U;
		}
		hash ^= hash >> 16U;
		hash *= 0x85EB'CA6BU;
		hash ^= hash >> 13U;
		hash *= 0xC2B2'AE35U;
		hash ^= hash >> 16U;
		expect(identity_tone_slot("tbutler") == std::optional<std::size_t>{ hash % identity_tone_count } && hash % identity_tone_count == 2U,
			   "the slot is FNV-1a of the folded login, finalised, modulo the tone count");
		auto seen = std::set<std::size_t>{ };
		for (const auto* const login : { "tbutler", "jiayin", "rcaloca", "mike", "MikeMarcin", "MikeMarcinTest", "reviewer-four", "octo-org", "dependabot[bot]", "a", "b", "c", "d", "e", "f", "g", "h" }) {
			seen.insert(identity_tone_slot(login).value_or(identity_tone_count));
		}
		expect(seen.size() >= 6U, "ordinary logins spread over the tones");
	}

	void test_the_initials_rule()
	{
		expect(identity_initials("tbutler") == "T", "one word is one initial");
		expect(identity_initials("reviewer-four") == "RF", "two words are two initials");
		expect(identity_initials("a-b-c") == "AB", "never more than two");
		expect(identity_initials("dependabot[bot]") == "D", "a bot's suffix is not a word");
		expect(identity_initials("github-actions[bot]") == "GA", "a bot's own words still count");
		expect(identity_initials("__x_y") == "XY", "leading separators are skipped");
		expect(identity_initials("9lives") == "9", "a digit is a character");
		expect(identity_initials("") == "?" && identity_initials("---") == "?", "no word is a question mark");
		expect(identity_initials("\xC3\xA9tienne") == "\xC3\xA9", "a character outside ASCII is kept whole");
	}

	/// \brief AC3: every tone clears every built-in ground at 3:1 and holds its
	/// initials at the theme's text minimum, which the table meets at 7:1.
	void test_every_tone_reads_in_all_four_themes()
	{
		for (const auto& theme : themes) {
			for (std::size_t slot = 0U; slot <= identity_tone_count; ++slot) {
				const auto chosen = slot < identity_tone_count ? std::optional<std::size_t>{ slot } : std::nullopt;
				const auto tone   = identity_tone_of(chosen, identity_ground_of(theme.Grounds.front()));
				for (const auto ground : theme.Grounds) {
					expect(contrast_ratio_hundredths(tone.Fill, ground) >= minimum_contrast_hundredths(contrast_kind::indicator, theme.Variant),
						   "a disc stands out from its ground at 3:1");
				}
				expect(contrast_ratio_hundredths(tone.Ink, tone.Fill) >= minimum_contrast_hundredths(contrast_kind::text, theme.Variant),
					   "the initials read at the theme's text minimum");
				expect(contrast_ratio_hundredths(tone.Ink, tone.Fill) >= high_contrast_text_minimum_hundredths, "the table holds 7:1 throughout");
				// Built-in grounds never need the custom-theme adjustment, so a
				// person is the same colour on a window and on a base.
				expect(identity_tone_for(slot < identity_tone_count ? std::string(1U, static_cast<char>('a' + slot)) : std::string{ }, theme.Grounds)
						   == identity_tone_of(identity_tone_slot(slot < identity_tone_count ? std::string(1U, static_cast<char>('a' + slot)) : std::string{ }), identity_ground_of(theme.Grounds.front())),
					   "a built-in theme takes the tabled tone unchanged");
			}
		}
		expect(identity_ground_of(srgb(0xF6F7F9U)) == identity_ground::light && identity_ground_of(srgb(0x1C2026U)) == identity_ground::dark,
			   "a light ground takes the deep half, a dark ground the light half");
	}

	/// \brief Negative control: [[BPR-1290]]'s pastel disc, HSL(hue, 150, 212)
	/// on white, does not stand out at 3:1, so the check above can fail.
	void test_the_old_pastel_disc_fails_the_check()
	{
		// HSL(120, 150/255, 212/255) as Qt resolves it: rgb(187, 237, 187).
		const auto pastel = srgb(0xBBEDBBU);
		expect(contrast_ratio_hundredths(pastel, srgb(0xFFFFFFU)) < indicator_contrast_minimum_hundredths, "negative control: the pastel disc is under 3:1 on white");
	}

	/// \brief A custom theme whose ground the table does not clear moves the
	/// fill away from it until it does, and the initials only gain.
	void test_a_custom_ground_moves_the_fill()
	{
		const auto grey   = std::array{ srgb(0x8A8A8AU) };
		const auto tabled = identity_tone_of(identity_tone_slot("tbutler"), identity_ground_of(grey.front()));
		const auto tone   = identity_tone_for("tbutler", grey);
		expect(contrast_ratio_hundredths(tone.Fill, grey.front()) >= indicator_contrast_minimum_hundredths, "a mid-grey ground is cleared");
		expect(contrast_ratio_hundredths(tone.Ink, tone.Fill) >= contrast_ratio_hundredths(tabled.Ink, tabled.Fill), "and the initials read no worse");
		expect(identity_tone_for("tbutler", { }) == identity_tone_of(identity_tone_slot("tbutler"), identity_ground::light), "no ground takes the light half");
	}

	/// \brief [[BPR-1807]]. A picture is asked for in a handful of sizes.
	void test_a_picture_size_covers_the_drawn_box()
	{
		expect(identity_avatar_picture_pixels(0U) == 32U && identity_avatar_picture_pixels(20U) == 32U, "a small face takes the smallest copy");
		expect(identity_avatar_picture_pixels(40U) == 48U && identity_avatar_picture_pixels(48U) == 48U, "a 20 px face at 2x takes 48");
		expect(identity_avatar_picture_pixels(72U) == 96U && identity_avatar_picture_pixels(120U) == 128U, "the next size up covers the box");
		expect(identity_avatar_picture_pixels(4096U) == 192U, "anything larger takes the largest");
	}

	/// \brief [[BPR-1807]]. Only a GitHub account's login is asked for.
	void test_only_a_login_is_admitted()
	{
		for (const auto* const login : { "octocat", "reviewer-four", "MikeMarcin", "dependabot[bot]", "a", "a-b-c1" }) {
			expect(identity_avatar_login(login), std::string{ "a login is admitted: " } + login);
		}
		for (const auto* const login : { "", "-lead", "has space", "slash/inside", "dot.inside", "../up", "x?y", "%2e", "[bot]", "name[bot]x",
										 "0123456789012345678901234567890123456789" }) {
			expect(!identity_avatar_login(login), std::string{ "a non-login is refused: " } + login);
		}
		expect(identity_avatar_folded_login("MikeMarcin") == "mikemarcin", "a login is kept case-folded");
	}

	/// \brief [[BPR-1807]]. GitHub.com asks the avatar host, by login, with no credential.
	void test_github_com_asks_the_avatar_host()
	{
		const auto policy = markdown_image_fetch_policy{ .Contract = markdown_image_host_contract::github_com, .Hostname = "github.com", .CredentialAvailable = true };
		const auto fetch  = admit_identity_avatar("MikeMarcin", 40U, policy);
		expect(fetch.has_value(), "a login on GitHub.com is admitted");
		expect(fetch->First.Address == "https://avatars.githubusercontent.com/mikemarcin?s=48", "the avatar host, by folded login, at the covering size");
		expect(!fetch->First.Credential && fetch->Policy.AvatarOrigins && fetch->Pixels == 48U && fetch->Login == "mikemarcin", "no credential, avatar origins only");
		const auto bot = admit_identity_avatar("dependabot[bot]", 20U, policy);
		expect(bot && bot->First.Address == "https://avatars.githubusercontent.com/dependabot%5Bbot%5D?s=32", "a bot's brackets are encoded");
		expect(admit_identity_avatar("../etc", 40U, policy).error() == markdown_image_state::blocked, "a path is refused");
	}

	/// \brief [[BPR-1807]]. GHES asks the host itself; redirects stay on its avatar origins.
	void test_ghes_stays_on_its_avatar_origins()
	{
		const auto policy = markdown_image_fetch_policy{ .Contract = markdown_image_host_contract::ghes, .Hostname = "GHE.Example.com", .CredentialAvailable = true };
		const auto fetch  = admit_identity_avatar("Octo", 64U, policy);
		expect(fetch && fetch->First.Address == "https://ghe.example.com/octo.png?size=64", "the host's own login picture");
		expect(fetch->First.Credential, "the credential rides the hop to the host itself");
		const auto onHost = admit_markdown_image_redirect(fetch->First, "/avatars/u/7?s=64", fetch->Policy);
		expect(onHost && onHost->Credential && onHost->Address == "https://ghe.example.com/avatars/u/7?s=64", "a redirect on the host keeps the credential");
		const auto subdomain = admit_markdown_image_redirect(fetch->First, "https://avatars.ghe.example.com/u/7?s=64", fetch->Policy);
		expect(subdomain && !subdomain->Credential, "the avatars subdomain is admitted without the credential");
		expect(admit_markdown_image_redirect(fetch->First, "https://media.ghe.example.com/u/7", fetch->Policy).error() == markdown_image_state::blocked,
			   "a body image origin is not an avatar origin");
		expect(admit_markdown_image_redirect(fetch->First, "https://elsewhere.example/u/7", fetch->Policy).error() == markdown_image_state::blocked,
			   "an avatar redirect never leaves the host");
		const auto com = admit_identity_avatar("octo", 64U, { .Contract = markdown_image_host_contract::github_com, .Hostname = "github.com" });
		expect(com && admit_markdown_image_redirect(com->First, "https://camo.githubusercontent.com/x", com->Policy).error() == markdown_image_state::blocked,
			   "on GitHub.com only the avatar host is an avatar origin");
		expect(admit_identity_avatar("octo", 64U, { .Contract = markdown_image_host_contract::ghes }).error() == markdown_image_state::blocked,
			   "a GHES profile with no host asks nothing");
	}

	/// \brief [[BPR-1807]]. A picture's cache identity is its own.
	void test_a_picture_has_its_own_cache_identity()
	{
		const auto key = identity_avatar_cache_key("GitHub.com", 7U, "MikeMarcin", 48U);
		expect(key == identity_avatar_cache_key("github.com", 7U, "mikemarcin", 48U), "host and login are folded");
		expect(key != identity_avatar_cache_key("github.com", 8U, "mikemarcin", 48U) && key != identity_avatar_cache_key("github.com", 7U, "mikemarcin", 64U)
				   && key != identity_avatar_cache_key("ghe.example.com", 7U, "mikemarcin", 48U),
			   "account, size and host each separate a picture");
		expect(key != markdown_image_cache_key("github.com", 7U, { .Kind = markdown_image_source_kind::absolute_web, .Written = "mikemarcin", .Canonical = "mikemarcin\n48" }),
			   "no body image shares a picture's key");
		expect(valid(identity_avatar_style::github) && valid(identity_avatar_style::generated) && !valid(static_cast<identity_avatar_style>(2)), "two styles");
	}
} // namespace

auto main() -> int
{
	test_an_identity_keeps_one_slot();
	test_the_initials_rule();
	test_every_tone_reads_in_all_four_themes();
	test_the_old_pastel_disc_fails_the_check();
	test_a_custom_ground_moves_the_fill();
	test_a_picture_size_covers_the_drawn_box();
	test_only_a_login_is_admitted();
	test_github_com_asks_the_avatar_host();
	test_ghes_stays_on_its_avatar_origins();
	test_a_picture_has_its_own_cache_identity();
	std::cout << "identity-avatar-core-tests:pass\n";
	return 0;
}
