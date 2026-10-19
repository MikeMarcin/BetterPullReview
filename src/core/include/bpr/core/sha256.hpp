#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// \file
/// \brief Deterministic SHA-256 through CommonCrypto on macOS, CNG on
/// Windows, and pinned HACL* SHA-2 elsewhere (RFC-BPR-0080 r4). Every SHA-256 the core computes, Git
/// identities in the SHA-256 object format included, goes through here.
namespace bpr::core {
	/// \brief Computes the sha256 result.
	[[nodiscard]] auto sha256(std::span<const std::uint8_t> aBytes) noexcept
		-> std::array<std::uint8_t, 32>;

	/// \brief SHA-256 over bytes delivered in pieces, so a large input is
	/// hashed chunk by chunk without being held whole.
	/// \details Copies retain independent prefixes. Updates allocate no
	/// first-party memory. CommonCrypto and HACL allocate nothing; CNG may
	/// allocate at construction/copy and releases its handle at destruction.
	/// Failure to start a native stream selects HACL. A native failure after
	/// starting (including duplication) terminates: noexcept never hides an
	/// incorrect digest. Finish once; do not update or copy a finished stream.
	class sha256_stream
	{
	public:
		sha256_stream() noexcept;
		sha256_stream(const sha256_stream& aOther) noexcept;
		sha256_stream(sha256_stream&& aOther) noexcept;
		auto operator=(const sha256_stream& aOther) noexcept -> sha256_stream&;
		auto operator=(sha256_stream&& aOther) noexcept -> sha256_stream&;
		~sha256_stream() noexcept;
		void update(std::span<const std::uint8_t> aBytes) noexcept;
		/// \brief The digest of everything delivered. Call once.
		[[nodiscard]] auto finish() noexcept -> std::array<std::uint8_t, 32>;

	private:
		[[nodiscard]] auto start_platform() noexcept -> bool;
		void               copy_platform(const sha256_stream& aOther) noexcept;
		void               destroy_platform() noexcept;
		void               update_platform(std::span<const std::uint8_t> aBytes) noexcept;
		[[nodiscard]] auto finish_platform() noexcept -> std::array<std::uint8_t, 32>;
		void               swap(sha256_stream& aOther) noexcept;

		std::array<std::uint32_t, 8> State{ };
		std::array<std::uint8_t, 64> Block{ };
		std::size_t                  BlockSize = 0U;
		std::uint64_t                ByteCount = 0U;
		// Native types stay private to their implementation. CommonCrypto's
		// trivially copyable context travels by memcpy, never type punning.
		std::array<std::uint8_t, 128> PlatformState{ };
		void*                         PlatformHandle = nullptr;
		bool                          PlatformActive = false;
	};
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_SHA256_HPP
