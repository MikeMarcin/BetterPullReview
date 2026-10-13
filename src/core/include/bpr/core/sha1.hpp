#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA1_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA1_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

/// \file
/// \brief SHA-1 with collision detection, for Git object identities
/// (RFC-BPR-0059, RFC-BPR-0080).
/// \details Git repositories in the SHA-1 object format name every object by
/// this digest, and the product checks fetched and published bytes against
/// those names, so it is a security-relevant digest. It is computed with
/// sha1collisiondetection configured as Git configures it, the SHA-1 Git and
/// GitHub use: an ordinary input gets the plain SHA-1 digest, and an input the
/// library detects as part of a SHA-1 collision attack (SHAttered,
/// SHA-mbles) is refused and gets no digest at all, never a substituted one.
/// Every SHA-1 Git identity in the product is computed here ([[BPR-1450]]).
namespace bpr::core {
	using sha1_digest = std::array<std::uint8_t, 20>;

	/// \brief Why no SHA-1 digest was given.
	enum class sha1_refusal : std::uint8_t {
		/// Collision detection found the input to be part of a SHA-1
		/// collision attack.
		collision_detected,
	};
	[[nodiscard]] auto name(sha1_refusal aRefusal) noexcept -> std::string_view;

	using sha1_result = std::expected<sha1_digest, sha1_refusal>;

	/// \brief SHA-1 over bytes delivered in pieces, with collision detection.
	class sha1_stream
	{
	public:
		sha1_stream() noexcept;
		void update(std::span<const std::uint8_t> aBytes) noexcept;
		/// \brief The digest of everything delivered, or the refusal when
		/// collision detection fired. Call once.
		[[nodiscard]] auto finish() noexcept -> sha1_result;

		/// The library's context, held in place so a stream never allocates.
		static constexpr std::size_t context_bytes = 2400U;

	private:
		alignas(std::uint64_t) std::array<unsigned char, context_bytes> Context{ };
	};

	/// \brief The SHA-1 of one whole input, or the refusal when collision
	/// detection fired.
	[[nodiscard]] auto sha1(std::span<const std::uint8_t> aBytes) noexcept -> sha1_result;
}

#endif
