#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_IMAGE_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_IMAGE_PORTS_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/core/markdown_image.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// \brief RFC-BPR-0084. The capabilities the host uses to show body images:
/// GitHub's rendering of a body (which address serves each image), one bounded
/// image fetch, and a restricted decoder.
namespace bpr::application {
	/// \brief Asks GitHub how it renders the bodies of the accepted pull request.
	/// \details Nodes are the GraphQL identities of the bodies: the pull
	/// request, reviews, review comments and issue comments. At most
	/// `core::markdown_image_bodies_per_resolution`.
	struct markdown_image_resolution_request
	{
		core::pull_request_request_context          Context;
		github_profile_authority                    ProfileAuthority;
		github_viewer_record                        Viewer;
		github_account_transport                    AccountTransport;
		std::optional<core::application_session_id> AuthenticationSession;
		std::vector<std::string>                    Nodes;
		std::uint64_t                               Sequence = 0U;
		/// The accepted repository and pull request GraphQL identities the
		/// answer must name.
		std::string RepositoryNode;
		std::string PullRequestNode;

		auto operator<=>(const markdown_image_resolution_request&) const = default;
	};

	/// \brief One body's served images, in GitHub's rendered order.
	struct markdown_image_resolved_body
	{
		std::string                              Node;
		std::vector<core::markdown_served_image> Served;

		auto operator==(const markdown_image_resolved_body&) const -> bool = default;
	};

	struct markdown_image_resolution_success
	{
		markdown_image_resolution_request         Request;
		std::vector<markdown_image_resolved_body> Bodies;
		github_rate_limit_state                   RateLimits;
	};

	struct markdown_image_resolution_error
	{
		markdown_image_resolution_request Request;
		github_provider_error             Error;
	};

	struct markdown_image_resolution_cancelled
	{
		markdown_image_resolution_request Request;
	};

	using markdown_image_resolution_completion = std::variant<markdown_image_resolution_success,
															  markdown_image_resolution_error,
															  markdown_image_resolution_cancelled>;

	/// \brief Implemented by the GitHub provider through the profile's own account transport.
	class markdown_image_resolution_port
	{
	public:
		virtual ~markdown_image_resolution_port() = default;
		/// \brief Delivers exactly one completion, on any thread.
		virtual void resolve(markdown_image_resolution_request                         aRequest,
							 std::function<void(markdown_image_resolution_completion)> aCompletion)
			= 0;
		virtual void cancel_resolution(const markdown_image_resolution_request& aRequest) = 0;
	};

	/// \brief One image fetch: an admitted first hop and the policy every redirect is judged by.
	/// \details The transport follows redirects only through
	/// `core::admit_markdown_image_redirect`, sends the credential only on a hop
	/// that policy marks, and enforces the byte and total-time bounds.
	struct markdown_image_fetch_request
	{
		core::markdown_image_hop          First;
		core::markdown_image_fetch_policy Policy;
		std::size_t                       MaximumBytes = core::markdown_image_encoded_bytes;
		std::uint32_t                     DeadlineMs   = core::markdown_image_fetch_deadline_ms;
		/// The accepted profile and, for a credentialed GHES hop, the exact
		/// account lease whose credential may ride it.
		github_profile_authority                    ProfileAuthority;
		github_account_transport                    AccountTransport;
		std::optional<core::application_session_id> AuthenticationSession;
		core::github_identity_generation            IdentityGeneration;
		core::github_account_id                     Account;
	};

	/// \brief The bytes of a 200 response whose content type is an image.
	struct markdown_image_fetched
	{
		std::string Bytes;
		std::string ContentType;
	};

	/// \brief Bytes, or the state the fetch ends the image in: `failed`,
	/// `too_large`, `blocked` (a refused hop) or `unsupported` (not an image).
	using markdown_image_fetch_result = std::expected<markdown_image_fetched, core::markdown_image_state>;

	/// \brief Implemented by the adapter that owns HTTPS and, for GHES, the profile's credential.
	class markdown_image_fetch_port
	{
	public:
		virtual ~markdown_image_fetch_port() = default;
		/// \brief Delivers exactly one result, on any thread, also after `aStop`.
		virtual void fetch(markdown_image_fetch_request                     aRequest,
						   std::stop_token                                  aStop,
						   std::function<void(markdown_image_fetch_result)> aCompletion)
			= 0;
	};

	/// \brief Decodes PNG, JPEG and GIF, recognized by signature, within the
	/// pixel, side, frame and decoded-byte bounds. Called off the GUI and
	/// decision lanes. Anything else is `unsupported`; an over-bound still
	/// image is `too_large`; an over-bound animation keeps its first frame.
	class markdown_image_decoder_port
	{
	public:
		virtual ~markdown_image_decoder_port() = default;
		[[nodiscard]] virtual auto decode(std::string_view aBytes) const
			-> std::expected<markdown_decoded_image, core::markdown_image_state>
			= 0;
	};
} // namespace bpr::application

#endif
