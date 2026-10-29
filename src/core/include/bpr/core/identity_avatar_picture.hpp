#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_IDENTITY_AVATAR_PICTURE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_IDENTITY_AVATAR_PICTURE_HPP

#include "bpr/core/markdown_image.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

/// \brief [[BPR-1807]]. A person's own GitHub or GHES picture in place of the
/// generated initials: which reviewers want it, which address serves it, and
/// how large a copy is asked for.
///
/// \details Pure policy, as `markdown_image.hpp` is: the input is a login the
/// presentation drew and a size it measured, and nothing here fetches, decodes
/// or draws. The host performs what `admit_identity_avatar` admits through the
/// profile's image lane; every hop is held to the host's avatar origins.
namespace bpr::core {
	/// \brief What a person's avatar shows (Settings > Appearance > People).
	enum class identity_avatar_style : std::uint8_t {
		/// The person's picture from their GitHub or GHES host, the initials
		/// while it loads and whenever it cannot be had.
		github,
		/// [[BPR-1583]]'s coloured initials, never a picture and never a request.
		generated,
	};

	inline constexpr std::size_t identity_avatar_style_count = 2U;

	[[nodiscard]] constexpr auto valid(const identity_avatar_style aStyle) noexcept -> bool
	{
		return static_cast<std::size_t>(aStyle) < identity_avatar_style_count;
	}

	/// \brief The square sizes, in device pixels, a picture is asked for in.
	/// \details A drawn avatar takes the smallest that covers its device size,
	/// so the faces on one screen share a handful of copies; anything larger
	/// takes the largest and is scaled down.
	inline constexpr std::array<std::uint32_t, 6> identity_avatar_picture_sizes{ 32U, 48U, 64U, 96U, 128U, 192U };

	/// \brief Bounds of one picture fetch: its encoded bytes and total time.
	inline constexpr std::size_t   identity_avatar_encoded_bytes     = 1024U * 1024U;
	inline constexpr std::uint32_t identity_avatar_fetch_deadline_ms = 10'000U;
	/// \brief At most this many pictures wait at once in one window; later ones
	/// are `over_limit` and keep their initials.
	inline constexpr std::size_t identity_avatar_window_queue = 64U;

	/// \brief One drawn avatar's request: the login under it and the device
	/// pixels its box covers. Untrusted presentation text.
	struct identity_avatar_intent
	{
		std::string   Login;
		std::uint32_t DevicePixels = 0U;

		auto operator<=>(const identity_avatar_intent&) const = default;
	};

	/// \brief The picture size an avatar of `aDevicePixels` asks for.
	[[nodiscard]] auto identity_avatar_picture_pixels(std::uint32_t aDevicePixels) noexcept -> std::uint32_t;

	/// \brief Whether `aLogin` names a GitHub account: 1 to 39 ASCII letters,
	/// digits and hyphens, not starting with a hyphen, optionally followed by
	/// `[bot]` for an app's bot account.
	[[nodiscard]] auto identity_avatar_login(std::string_view aLogin) noexcept -> bool;

	/// \brief The login a picture is kept under: ASCII case-folded, as GitHub
	/// logins are case-insensitive.
	[[nodiscard]] auto identity_avatar_folded_login(std::string_view aLogin) -> std::string;

	/// \brief One admitted picture fetch.
	struct identity_avatar_fetch
	{
		/// The folded login and the picture size asked for.
		std::string   Login;
		std::uint32_t Pixels = 0U;
		/// The first hop, and the policy every hop is judged by: the profile's
		/// with `AvatarOrigins` set.
		markdown_image_hop          First;
		markdown_image_fetch_policy Policy;
	};

	/// \brief Admits one avatar picture on the profile `aPolicy` describes.
	///
	/// \details GitHub.com: `https://avatars.githubusercontent.com/<login>?s=<n>`,
	/// never a credential. GHES: `https://<host>/<login>.png?size=<n>`, which
	/// the host redirects to its avatar origin; the credential rides only a hop
	/// to the host itself, as for a body image. Every redirect is judged by the
	/// same policy, so no hop leaves the host's avatar origins. A login that is
	/// not a GitHub account's is `blocked`.
	[[nodiscard]] auto admit_identity_avatar(std::string_view aLogin, std::uint32_t aDevicePixels, markdown_image_fetch_policy aPolicy)
		-> std::expected<identity_avatar_fetch, markdown_image_state>;

	/// \brief The cache identity of one picture: host, account (zero for none),
	/// folded login and size; a body image's key can never equal it.
	[[nodiscard]] auto identity_avatar_cache_key(std::string_view aHostname, std::uint64_t aAccount, std::string_view aLogin,
												 std::uint32_t aPixels) -> std::string;
} // namespace bpr::core

#endif
