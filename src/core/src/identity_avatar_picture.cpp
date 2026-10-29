#include "bpr/core/identity_avatar_picture.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr std::size_t      maximum_login_bytes = 39U;
		constexpr std::string_view bot_suffix          = "[bot]";

		[[nodiscard]] constexpr auto login_character(const char aChar) noexcept -> bool
		{
			return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z') || (aChar >= '0' && aChar <= '9') || aChar == '-';
		}

		[[nodiscard]] auto ascii_lower(std::string aText) -> std::string
		{
			std::ranges::transform(aText, aText.begin(), [](const char aChar) {
				return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar + ('a' - 'A')) : aChar;
			});
			return aText;
		}

		/// The login as an address path segment: `[bot]` percent-encoded, the
		/// rest already within the unreserved characters.
		[[nodiscard]] auto path_segment(const std::string_view aLogin) -> std::string
		{
			if (aLogin.ends_with(bot_suffix)) {
				return std::string{ aLogin.substr(0, aLogin.size() - bot_suffix.size()) } + "%5Bbot%5D";
			}
			return std::string{ aLogin };
		}
	} // namespace

	auto identity_avatar_picture_pixels(const std::uint32_t aDevicePixels) noexcept -> std::uint32_t
	{
		const auto found = std::ranges::find_if(identity_avatar_picture_sizes, [aDevicePixels](const std::uint32_t aSize) { return aSize >= aDevicePixels; });
		return found == identity_avatar_picture_sizes.end() ? identity_avatar_picture_sizes.back() : *found;
	}

	auto identity_avatar_login(std::string_view aLogin) noexcept -> bool
	{
		if (aLogin.ends_with(bot_suffix)) {
			aLogin.remove_suffix(bot_suffix.size());
		}
		return !aLogin.empty() && aLogin.size() <= maximum_login_bytes && aLogin.front() != '-' && std::ranges::all_of(aLogin, login_character);
	}

	auto identity_avatar_folded_login(const std::string_view aLogin) -> std::string
	{
		return ascii_lower(std::string{ aLogin });
	}

	auto admit_identity_avatar(const std::string_view aLogin, const std::uint32_t aDevicePixels, markdown_image_fetch_policy aPolicy)
		-> std::expected<identity_avatar_fetch, markdown_image_state>
	{
		if (!identity_avatar_login(aLogin)) {
			return std::unexpected{ markdown_image_state::blocked };
		}
		aPolicy.AvatarOrigins = true;
		const auto login      = identity_avatar_folded_login(aLogin);
		const auto pixels     = identity_avatar_picture_pixels(aDevicePixels);
		const auto size       = std::to_string(pixels);
		auto       address    = std::string{ };
		if (aPolicy.Contract == markdown_image_host_contract::github_com) {
			address = "https://avatars.githubusercontent.com/" + path_segment(login) + "?s=" + size;
		} else {
			if (aPolicy.Hostname.empty()) {
				return std::unexpected{ markdown_image_state::blocked };
			}
			address = "https://" + ascii_lower(aPolicy.Hostname) + "/" + path_segment(login) + ".png?size=" + size;
		}
		auto first = admit_markdown_image_fetch(address, aPolicy);
		if (!first) {
			return std::unexpected{ first.error() };
		}
		return identity_avatar_fetch{ .Login = login, .Pixels = pixels, .First = std::move(*first), .Policy = std::move(aPolicy) };
	}

	auto identity_avatar_cache_key(const std::string_view aHostname, const std::uint64_t aAccount, const std::string_view aLogin,
								   const std::uint32_t aPixels) -> std::string
	{
		auto key = ascii_lower(std::string{ aHostname });
		key += '\n';
		key += std::to_string(aAccount);
		key += "\navatar\n";
		key += identity_avatar_folded_login(aLogin);
		key += '\n';
		key += std::to_string(aPixels);
		return key;
	}
} // namespace bpr::core
