#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_IMAGE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_IMAGE_HPP

#include "bpr/core/markdown_navigation.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \brief RFC-BPR-0084. What an image in a rendered body is, whether and how it
/// may be fetched, and how large it shows.
///
/// \details Pure policy: every input is text or a number the caller observed,
/// and nothing here fetches, decodes, caches or draws. The reader supplies
/// untrusted source text; GitHub's own rendering supplies the address it
/// serves each image from; the host performs what these functions admit.
namespace bpr::core {
	/// \brief Bounds of RFC-BPR-0084 "Bounds".
	inline constexpr std::size_t   markdown_image_references_per_body   = 32U;
	inline constexpr std::size_t   markdown_image_bodies_per_resolution = 25U;
	inline constexpr std::size_t   markdown_image_body_html_bytes       = 1024U * 1024U;
	inline constexpr std::size_t   markdown_image_served_tags_per_body  = 256U;
	inline constexpr std::size_t   markdown_image_fetch_concurrency     = 4U;
	inline constexpr std::size_t   markdown_image_window_queue          = 128U;
	inline constexpr std::size_t   markdown_image_encoded_bytes         = 10U * 1024U * 1024U;
	inline constexpr std::uint32_t markdown_image_redirects             = 3U;
	inline constexpr std::uint32_t markdown_image_fetch_deadline_ms     = 20'000U;
	inline constexpr std::uint64_t markdown_image_frame_pixels          = 40'000'000U;
	inline constexpr std::uint32_t markdown_image_side_pixels           = 16'384U;
	inline constexpr std::size_t   markdown_image_animation_frames      = 400U;
	inline constexpr std::size_t   markdown_image_decoded_bytes         = 256U * 1024U * 1024U;
	inline constexpr std::size_t   markdown_image_memory_cache_bytes    = 256U * 1024U * 1024U;
	inline constexpr std::size_t   markdown_image_disk_cache_bytes      = 128U * 1024U * 1024U;
	inline constexpr std::uint32_t markdown_image_disk_cache_days       = 7U;
	inline constexpr std::size_t   markdown_image_source_bytes          = 4096U;

	/// \brief The one state an image is in (RFC-BPR-0084 "What renders").
	enum class markdown_image_state : std::uint8_t {
		loading,
		loaded,
		failed,
		too_large,
		blocked,
		unsupported,
		over_limit,
	};

	/// \brief A `width` or `height` as written: pixels, or a percentage of the reader's width.
	struct markdown_image_length
	{
		enum class unit : std::uint8_t {
			pixels,
			percent,
		};
		unit          Unit  = unit::pixels;
		std::uint32_t Value = 0U;

		auto operator==(const markdown_image_length&) const -> bool = default;
	};

	/// \brief The author's size hints; absent when not written or not understood.
	struct markdown_image_hints
	{
		std::optional<markdown_image_length> Width;
		std::optional<markdown_image_length> Height;

		auto operator==(const markdown_image_hints&) const -> bool = default;
	};

	/// \brief Reads one `width`/`height` attribute value: `40`, `40px` or `50%`.
	/// \details Zero, negative, fractional-only, oversized or otherwise unknown
	/// text gives no hint. Pixels are capped at the largest side; percentages at 100.
	[[nodiscard]] auto parse_markdown_image_length(std::string_view aText) -> std::optional<markdown_image_length>;

	/// \brief One `<img>`, linked or chosen from a `<picture>`, found in prose text.
	struct html_image_span
	{
		/// UTF-8 byte offsets of the whole recognized markup in the scanned text.
		std::size_t          Begin = 0U;
		std::size_t          End   = 0U;
		std::string          Source;
		std::string          Alt;
		markdown_image_hints Hints;
		/// The `href` of an `<a>` wrapping exactly this image.
		std::optional<std::string> Link;
		/// A `<picture>`'s `prefers-color-scheme` sources.
		std::optional<std::string> DarkSource;
		std::optional<std::string> LightSource;

		auto operator==(const html_image_span&) const -> bool = default;
	};

	/// \brief Finds GitHub's admitted image markup in one run of prose text.
	/// \details Recognizes `<img>`, `<a href>` wrapping exactly one image, and
	/// `<picture>` whose `<source>` elements select only by
	/// `prefers-color-scheme`. Reads only `src`, `alt`, `width`, `height`,
	/// `href`, `media` and the first `srcset` candidate; decodes character
	/// references in attribute values. Anything else is not consumed and stays
	/// text. Bounded: at most `markdown_image_references_per_body` spans and
	/// 8 KiB of markup per tag. This is a tag recognizer, not an HTML parser.
	[[nodiscard]] auto scan_html_images(std::string_view aText) -> std::vector<html_image_span>;

	/// \brief One image `src` or `srcset` candidate GitHub's rendered HTML serves.
	struct markdown_served_image
	{
		/// The address GitHub serves, character references decoded.
		std::string Source;
		/// GitHub's `data-canonical-src`: the address as written, for a proxied image.
		std::string Canonical;

		auto operator==(const markdown_served_image&) const -> bool = default;
	};

	/// \brief Reads every served image of one body's rendered HTML.
	/// \details Takes `<img src>` and `<source srcset>` with their
	/// `data-canonical-src`. Refuses input over `markdown_image_body_html_bytes`
	/// or with more than `markdown_image_served_tags_per_body` image tags.
	[[nodiscard]] auto served_markdown_images(std::string_view aHtml) -> std::optional<std::vector<markdown_served_image>>;

	enum class markdown_image_source_kind : std::uint8_t {
		/// An absolute `http` or `https` address.
		absolute_web,
		/// The host's own `user-attachments/assets/<uuid>`; Canonical is the UUID.
		attachment,
		/// A path GitHub resolves against the repository.
		repository_relative,
	};

	/// \brief An admitted image reference. Canonical is the cache identity of its content.
	struct markdown_image_source
	{
		markdown_image_source_kind Kind = markdown_image_source_kind::absolute_web;
		std::string                Written;
		std::string                Canonical;

		auto operator==(const markdown_image_source&) const -> bool = default;
	};

	/// \brief Admits one untrusted source as written in a body on `aHostname`.
	/// \details `data:`, `file:`, any scheme but `http`/`https`, network-path,
	/// parent-escaping, control-bearing and oversized references are blocked.
	[[nodiscard]] auto admit_markdown_image_source(std::string_view aSource, std::string_view aHostname)
		-> std::expected<markdown_image_source, markdown_image_state>;

	/// \brief The served address GitHub's rendering gives this reference, if any.
	/// \details A camo-proxied image matches its `data-canonical-src`, an
	/// attachment its UUID, and anything else its exact address. A reference
	/// GitHub does not serve is blocked: GitHub does not show it either.
	[[nodiscard]] auto match_served_markdown_image(const markdown_image_source&           aSource,
												   std::span<const markdown_served_image> aServed)
		-> std::optional<std::string>;

	/// \brief The source with its cache identity settled by its served address.
	/// \details A repository-relative path names different content in every
	/// repository and revision, so its identity is the served address without
	/// its query. Other kinds are returned unchanged.
	[[nodiscard]] auto resolved_markdown_image_source(markdown_image_source aSource, std::string_view aServed) -> markdown_image_source;

	/// \brief Which server contract's image origins apply.
	enum class markdown_image_host_contract : std::uint8_t {
		github_com,
		ghes,
	};

	/// \brief The accepted profile's facts an image fetch is judged against.
	struct markdown_image_fetch_policy
	{
		markdown_image_host_contract Contract = markdown_image_host_contract::github_com;
		/// The profile's canonical hostname: `github.com` or the GHES host.
		std::string Hostname;
		/// Whether the transport performing the fetch holds the profile's credential.
		bool CredentialAvailable = false;
	};

	/// \brief One admitted request: its exact address and whether the credential rides it.
	struct markdown_image_hop
	{
		std::string   Address;
		bool          Credential = false;
		std::uint32_t Hop        = 0U;

		auto operator==(const markdown_image_hop&) const -> bool = default;
	};

	/// \brief Admits the first request for a served address.
	/// \details GitHub.com: HTTPS to GitHub's own image hosts only, never a
	/// credential. GHES: HTTPS anywhere; the credential only to the exact host
	/// and its `media.` host.
	[[nodiscard]] auto admit_markdown_image_fetch(std::string_view aServed, const markdown_image_fetch_policy& aPolicy)
		-> std::expected<markdown_image_hop, markdown_image_state>;

	/// \brief Admits one redirect from `aPrevious` to `aLocation`.
	/// \details The target is judged afresh; a credential rides it only if it
	/// rode `aPrevious` and the target is still a credential origin. At most
	/// `markdown_image_redirects` hops; a relative location must be root-relative.
	[[nodiscard]] auto admit_markdown_image_redirect(const markdown_image_hop& aPrevious, std::string_view aLocation,
													 const markdown_image_fetch_policy& aPolicy)
		-> std::expected<markdown_image_hop, markdown_image_state>;

	/// \brief A size in logical pixels.
	struct markdown_image_size
	{
		std::uint32_t Width  = 0U;
		std::uint32_t Height = 0U;

		auto operator==(const markdown_image_size&) const -> bool = default;
	};

	/// \brief The displayed size: hints over the natural size, aspect kept, no wider than available.
	[[nodiscard]] auto markdown_image_display_size(markdown_image_size aNatural, const markdown_image_hints& aHints,
												   std::uint32_t aAvailableWidth) -> markdown_image_size;

	/// \brief The box reserved while loading, when both dimensions are known from hints.
	[[nodiscard]] auto markdown_image_reserved_size(const markdown_image_hints& aHints, std::uint32_t aAvailableWidth)
		-> std::optional<markdown_image_size>;

	/// \brief One reference the reader found, in document order.
	struct markdown_image_reference
	{
		std::uint32_t Ordinal = 0U;
		std::string   Source;

		auto operator==(const markdown_image_reference&) const -> bool = default;
	};

	/// \brief The reader's request for one body's images. Empty Body is the description.
	struct markdown_image_intent
	{
		markdown_context                      Context;
		std::string                           Body;
		std::vector<markdown_image_reference> References;
	};

	enum class markdown_image_refusal : std::uint8_t {
		stale,
		unavailable,
	};

	/// \brief The host's current facts for an image request.
	struct markdown_image_observation
	{
		std::optional<markdown_context> Context;
		bool                            BodyAvailable = false;
		/// Whether this window can resolve and fetch images at all.
		bool             Available = false;
		std::string_view Hostname;
	};

	/// \brief One reference's admission: its source, or the state it ends in now.
	struct markdown_image_admission
	{
		std::uint32_t                                              Ordinal = 0U;
		std::expected<markdown_image_source, markdown_image_state> Source;
	};

	/// \brief Admits a body's references against the current accepted context.
	/// \details A changed window, session, identity, snapshot or publication
	/// revision, or an absent body, is stale. Ordinals beyond the per-body
	/// bound and duplicates are `over_limit`; at most that many are admitted.
	[[nodiscard]] auto admit_markdown_images(const markdown_image_intent& aIntent, const markdown_image_observation& aObservation)
		-> std::expected<std::vector<markdown_image_admission>, markdown_image_refusal>;

	/// \brief The cache identity: host, account (zero for none) and canonical source.
	/// \details Never a served address: GitHub's signed addresses change.
	[[nodiscard]] auto markdown_image_cache_key(std::string_view aHostname, std::uint64_t aAccount,
												const markdown_image_source& aSource) -> std::string;
} // namespace bpr::core

#endif
