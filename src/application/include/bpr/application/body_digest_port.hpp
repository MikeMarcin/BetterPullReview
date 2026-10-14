#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_BODY_DIGEST_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_BODY_DIGEST_PORT_HPP

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

/// \file
/// \brief [[BPR-1447]]. SHA-256 of many bodies at once, computed by the host
/// off the window thread (RFC-BPR-0080, "Platform crypto behind a narrow host
/// port").
/// \details The feedback classifier keys each annotation by the SHA-256 of the
/// body it read. Hashing ten thousand bodies on the window thread is the cost
/// this port moves: the read path digests the bodies on the provider's
/// completion thread and the digests travel with the snapshot. The digest is
/// the same SHA-256 the core computes, byte for byte, so nothing persisted or
/// compared changes; an absent digest is computed by the core as before.
namespace bpr::application {
	using body_digest = std::array<std::uint8_t, 32>;

	/// \brief Digests bodies with SHA-256.
	/// \details Called from worker threads, possibly several at once, so an
	/// implementation keeps no mutable state. \p aDigests has one element for
	/// each of \p aBodies, written in the same order. An implementation that
	/// cannot digest throws; the caller then supplies no digests and the core
	/// hashes as it would without the port.
	class body_digest_port
	{
	public:
		virtual ~body_digest_port()                                                                           = default;
		virtual void digest(std::span<const std::string_view> aBodies, std::span<body_digest> aDigests) const = 0;

	protected:
		body_digest_port()                                               = default;
		body_digest_port(const body_digest_port&)                        = default;
		body_digest_port(body_digest_port&&)                             = default;
		auto operator=(const body_digest_port&) -> body_digest_port&     = default;
		auto operator=(body_digest_port&&) noexcept -> body_digest_port& = default;
	};

	/// \brief The core's own SHA-256 behind the port: the backend on platforms
	/// with no measured platform crypto, still run off the window thread.
	class core_body_digest final : public body_digest_port
	{
	public:
		void digest(std::span<const std::string_view> aBodies, std::span<body_digest> aDigests) const override;
	};
}

#endif
