#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GH_CLI_AVATAR_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GH_CLI_AVATAR_HPP

#include "bpr/core/markdown_image.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \brief [[BPR-1846]]. The GitHub CLI avatar helper's admission and wire
/// format.
///
/// \details A GHES profile signed in through `gh` holds no credential in the
/// application, so its people's pictures cannot be asked of the server's
/// avatars API ([[BPR-1845]]) from the application. The shipped helper
/// executable asks `gh auth token` for the profile's host itself, fetches a
/// batch of avatars API addresses with that token, and writes back only the
/// image bytes, their content type and an HTTP status. The token stays in the
/// helper: it is never in the helper's arguments, environment or output.
///
/// Pure policy and framing: what the helper may fetch and how a batch and its
/// answers are written. The helper and the application both judge by these
/// functions, so the two ends of the pipe cannot disagree.
namespace bpr::core {
	/// \brief The first line of a request batch and of its answer.
	inline constexpr std::string_view gh_cli_avatar_protocol = "bpr-gh-avatar/1";
	/// \brief At most this many addresses in one helper launch.
	inline constexpr std::size_t gh_cli_avatar_batch_limit = 16U;
	/// \brief The longest address line a batch admits.
	inline constexpr std::size_t gh_cli_avatar_address_bytes = 512U;
	/// \brief The longest content type an answer carries.
	inline constexpr std::size_t gh_cli_avatar_content_type_bytes = 127U;
	/// \brief The bound on `gh auth token`: its run and the token's length.
	inline constexpr std::uint32_t gh_cli_avatar_token_deadline_ms = 10'000U;
	inline constexpr std::size_t   gh_cli_avatar_token_bytes       = 1024U;
	/// \brief The bound on one whole helper launch: the token, then every fetch.
	inline constexpr std::uint32_t gh_cli_avatar_launch_deadline_ms = 30'000U;

	/// \brief How a helper launch ended as a whole.
	enum class gh_cli_avatar_batch_state : std::uint8_t {
		/// Every address has its answer, in order.
		answered,
		/// `gh` could not be run, crashed, or gave no usable answer.
		cli_unavailable,
		/// `gh` ran and has no token for the host and account: not signed in.
		cli_signed_out,
		/// The batch or the helper's arguments were refused before any fetch.
		refused,
	};

	/// \brief One address's answer.
	/// \details `State` is `loaded` for a 200 response, whose bytes are then
	/// non-empty; otherwise `failed`, `too_large`, `blocked` or `unsupported`
	/// with no bytes. `HttpStatus` is the last response's status, zero when
	/// none arrived.
	struct gh_cli_avatar_answer
	{
		markdown_image_state State      = markdown_image_state::failed;
		std::uint16_t        HttpStatus = 0U;
		std::string          ContentType;
		std::string          Bytes;

		auto operator==(const gh_cli_avatar_answer&) const -> bool = default;
	};

	struct gh_cli_avatar_batch
	{
		gh_cli_avatar_batch_state         State = gh_cli_avatar_batch_state::refused;
		std::vector<gh_cli_avatar_answer> Answers;

		auto operator==(const gh_cli_avatar_batch&) const -> bool = default;
	};

	/// \brief Whether \p aHostname is a lower-case DNS name the helper may be
	/// pinned to: 1 to 253 bytes of `a-z`, `0-9`, `-` and `.`, with no empty label.
	[[nodiscard]] auto gh_cli_avatar_hostname(std::string_view aHostname) noexcept -> bool;

	/// \brief The policy every helper hop is judged by: GHES on \p aHostname,
	/// with the credential (the helper holds it) and only the avatar origins.
	[[nodiscard]] auto gh_cli_avatar_policy(std::string_view aHostname) -> markdown_image_fetch_policy;

	/// \brief Admits one address the helper is asked for.
	/// \details Only `identity_avatar_api_prefix(<host>)` followed by
	/// `<login>?s=<n>` on the exact pinned host, a GitHub login as `admit_identity_avatar`
	/// writes it and one of `identity_avatar_picture_sizes`; anything else,
	/// another host included, is `blocked`. The admitted first hop carries the
	/// credential; redirects are judged by `admit_markdown_image_redirect`
	/// under `gh_cli_avatar_policy`.
	[[nodiscard]] auto admit_gh_cli_avatar_address(std::string_view aAddress, std::string_view aHostname)
		-> std::expected<markdown_image_hop, markdown_image_state>;

	/// \brief Writes a request batch: the protocol line, then one address per line.
	/// \returns Nothing when the batch is empty, too long, or holds an address
	/// that is not printable ASCII within `gh_cli_avatar_address_bytes`.
	[[nodiscard]] auto encode_gh_cli_avatar_requests(std::span<const std::string> aAddresses) -> std::optional<std::string>;

	/// \brief Reads a request batch exactly as `encode_gh_cli_avatar_requests` writes it.
	[[nodiscard]] auto decode_gh_cli_avatar_requests(std::string_view aText) -> std::optional<std::vector<std::string>>;

	/// \brief Writes a batch's answers: `<protocol> <batch> <count>`, then for
	/// each answer `<state> <status> <content type or -> <length>` and its bytes.
	/// \returns Nothing for an answer outside the bounds `decode` admits.
	[[nodiscard]] auto encode_gh_cli_avatar_batch(const gh_cli_avatar_batch& aBatch) -> std::optional<std::string>;

	/// \brief Reads a helper's whole output.
	/// \details Refuses anything `encode_gh_cli_avatar_batch` would not write,
	/// trailing bytes included, and an answered batch whose count is not
	/// \p aExpected. A batch that is not `answered` has no answers.
	[[nodiscard]] auto decode_gh_cli_avatar_batch(std::string_view aText, std::size_t aExpected) -> std::optional<gh_cli_avatar_batch>;
} // namespace bpr::core

#endif
