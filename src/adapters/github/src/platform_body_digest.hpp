#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_PLATFORM_BODY_DIGEST_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_PLATFORM_BODY_DIGEST_HPP

#include "bpr/application/body_digest_port.hpp"

#include <span>
#include <string_view>

/// \file
/// \brief [[BPR-1447]]. The body digest port on platform crypto: CommonCrypto
/// on macOS. Windows and Linux have no measured backend yet (BPR-1448 and
/// BPR-1449 add CNG and libcrypto), so there the port is the core's SHA-256,
/// still run off the window thread.
namespace bpr::adapters::github {
	class platform_body_digest final : public application::body_digest_port
	{
	public:
		void digest(std::span<const std::string_view> aBodies, std::span<application::body_digest> aDigests) const override;
	};

	/// \brief Which backend `platform_body_digest` uses on this build:
	/// `CommonCrypto` or `core`.
	[[nodiscard]] auto platform_body_digest_backend() noexcept -> std::string_view;
}

#endif
