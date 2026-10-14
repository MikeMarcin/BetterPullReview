#include "hash_arms.hpp"

#include "bpr/core/sha1.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" {
#include <internal/Hacl_Hash_SHA2.h>
#include <sha1.h>
}

#if defined(__APPLE__)
	#include <CommonCrypto/CommonDigest.h>
#endif

#if defined(BPR_HASH_EVALUATION_QT)
	#include <QByteArrayView>
	#include <QCryptographicHash>
#endif

namespace bpr::hash_evaluation {
	namespace {
		template <std::size_t TSize>
		auto to_digest(const std::array<std::uint8_t, TSize>& aBytes) -> digest
		{
			digest result;
			std::ranges::copy(aBytes, result.Bytes.begin());
			result.Size = TSize;
			return result;
		}

		/// The current core SHA-1, the SHA-1 baseline: sha1collisiondetection
		/// since [[BPR-1450]]. A refused input has no digest (an empty one
		/// here) and reports the detection.
		class core_sha1 final : public hash_arm
		{
		public:
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return "core"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return hash_algorithm::sha1; }
			void               reset() override
			{
				Stream   = core::sha1_stream{ };
				Detected = false;
			}
			void               update(std::span<const std::uint8_t> aBytes) override { Stream.update(aBytes); }
			[[nodiscard]] auto finish() -> digest override
			{
				const auto result = Stream.finish();
				Detected          = !result.has_value();
				return result ? to_digest(*result) : digest{ };
			}
			[[nodiscard]] auto detected() const noexcept -> bool override { return Detected; }

		private:
			core::sha1_stream Stream;
			bool              Detected = false;
		};

		/// The product's SHA-256, bpr/core/sha256.hpp. Since [[BPR-1451]] it is
		/// HACL*'s SHA-2 behind the core wrapper, so this arm measures the
		/// product path and the "hacl" arm below the library called directly;
		/// the two must agree everywhere, and their gap is the wrapper's cost.
		class core_sha256 final : public hash_arm
		{
		public:
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return "core"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return hash_algorithm::sha256; }
			void               reset() override { Stream = core::sha256_stream{ }; }
			void               update(std::span<const std::uint8_t> aBytes) override { Stream.update(aBytes); }
			[[nodiscard]] auto finish() -> digest override { return to_digest(Stream.finish()); }

		private:
			core::sha256_stream Stream;
		};

		/// The RFC's control arm: the former core SHA-1's compression function,
		/// hashing whole blocks straight from the input as BPR-1358 made the
		/// core SHA-256 do. It exists only in this evaluation, and since
		/// [[BPR-1450]] it is also the plain-SHA-1 control the collision run
		/// compares against.
		class first_party_block_sha1 final : public hash_arm
		{
		public:
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return "first-party-block"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return hash_algorithm::sha1; }
			void               reset() override
			{
				State     = initial_state;
				BlockSize = 0U;
				ByteCount = 0U;
			}
			void update(std::span<const std::uint8_t> aBytes) override
			{
				ByteCount += aBytes.size();
				auto rest = aBytes;
				if (BlockSize != 0U) {
					const auto take = std::min(Block.size() - BlockSize, rest.size());
					std::copy_n(rest.begin(), take, Block.begin() + static_cast<std::ptrdiff_t>(BlockSize));
					BlockSize += take;
					rest = rest.subspan(take);
					if (BlockSize != Block.size()) {
						return;
					}
					transform(Block);
					BlockSize = 0U;
				}
				while (rest.size() >= Block.size()) {
					transform(rest.first<64>());
					rest = rest.subspan(Block.size());
				}
				std::ranges::copy(rest, Block.begin());
				BlockSize = rest.size();
			}
			[[nodiscard]] auto finish() -> digest override
			{
				const auto                   bitCount = ByteCount * 8U;
				std::array<std::uint8_t, 72> padding{ };
				padding[0]          = 0x80U;
				const auto padBytes = BlockSize < 56U ? 56U - BlockSize : 120U - BlockSize;
				for (std::size_t shift = 0U; shift < 8U; ++shift) {
					padding[padBytes + shift] = static_cast<std::uint8_t>(bitCount >> ((7U - shift) * 8U));
				}
				const auto byteCount = ByteCount;
				update(std::span{ padding }.first(padBytes + 8U));
				ByteCount = byteCount;
				std::array<std::uint8_t, 20> result{ };
				for (std::size_t index = 0U; index < State.size(); ++index) {
					result[index * 4U]      = static_cast<std::uint8_t>(State[index] >> 24U);
					result[index * 4U + 1U] = static_cast<std::uint8_t>(State[index] >> 16U);
					result[index * 4U + 2U] = static_cast<std::uint8_t>(State[index] >> 8U);
					result[index * 4U + 3U] = static_cast<std::uint8_t>(State[index]);
				}
				return to_digest(result);
			}

		private:
			static constexpr std::array<std::uint32_t, 5> initial_state{ 0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U };

			/// The former core transform (src/core/src/sha1.cpp, deleted by
			/// [[BPR-1450]]), reading the block it is given instead of the
			/// buffer.
			void transform(std::span<const std::uint8_t, 64> aBlock) noexcept
			{
				std::array<std::uint32_t, 80> words{ };
				for (std::size_t index = 0U; index < 16U; ++index) {
					words[index] = (static_cast<std::uint32_t>(aBlock[index * 4U]) << 24U) | (static_cast<std::uint32_t>(aBlock[index * 4U + 1U]) << 16U)
						| (static_cast<std::uint32_t>(aBlock[index * 4U + 2U]) << 8U) | static_cast<std::uint32_t>(aBlock[index * 4U + 3U]);
				}
				for (std::size_t index = 16U; index < words.size(); ++index) {
					words[index] = std::rotl(words[index - 3U] ^ words[index - 8U] ^ words[index - 14U] ^ words[index - 16U], 1);
				}
				auto a = State[0];
				auto b = State[1];
				auto c = State[2];
				auto d = State[3];
				auto e = State[4];
				for (std::size_t index = 0U; index < words.size(); ++index) {
					std::uint32_t mix      = 0U;
					std::uint32_t constant = 0U;
					if (index < 20U) {
						mix      = (b & c) | (~b & d);
						constant = 0x5A827999U;
					} else if (index < 40U) {
						mix      = b ^ c ^ d;
						constant = 0x6ED9EBA1U;
					} else if (index < 60U) {
						mix      = (b & c) | (b & d) | (c & d);
						constant = 0x8F1BBCDCU;
					} else {
						mix      = b ^ c ^ d;
						constant = 0xCA62C1D6U;
					}
					const auto next = std::rotl(a, 5) + mix + e + constant + words[index];
					e               = d;
					d               = c;
					c               = std::rotl(b, 30);
					b               = a;
					a               = next;
				}
				State[0] += a;
				State[1] += b;
				State[2] += c;
				State[3] += d;
				State[4] += e;
			}

			std::array<std::uint32_t, 5> State = initial_state;
			std::array<std::uint8_t, 64> Block{ };
			std::size_t                  BlockSize = 0U;
			std::uint64_t                ByteCount = 0U;
		};

		/// sha1collisiondetection, configured as Git configures it: the plain
		/// SHA-1 digest, with detection reported beside it.
		class sha1dc final : public hash_arm
		{
		public:
			explicit sha1dc(bool aDetectReducedRound)
				: DetectReducedRound(aDetectReducedRound)
			{
				reset();
			}
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return DetectReducedRound ? "sha1dc-reduced-round" : "sha1dc"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return hash_algorithm::sha1; }
			void               reset() override
			{
				SHA1DCInit(&Context);
				SHA1DCSetSafeHash(&Context, 0);
				SHA1DCSetDetectReducedRoundCollision(&Context, DetectReducedRound ? 1 : 0);
				Detected = false;
			}
			void update(std::span<const std::uint8_t> aBytes) override
			{
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the C API takes char.
				SHA1DCUpdate(&Context, reinterpret_cast<const char*>(aBytes.data()), aBytes.size());
			}
			[[nodiscard]] auto finish() -> digest override
			{
				std::array<std::uint8_t, 20> result{ };
				Detected = SHA1DCFinal(result.data(), &Context) != 0;
				return to_digest(result);
			}
			[[nodiscard]] auto detected() const noexcept -> bool override { return Detected; }

		private:
			SHA1_CTX Context{ };
			bool     DetectReducedRound = false;
			bool     Detected           = false;
		};

		/// HACL*'s generated SHA-256 called directly through its block-level
		/// calls, so a stream allocates nothing. The core wrapper
		/// ([[BPR-1451]]) has the same shape; this arm is its library-only
		/// reference.
		class hacl_sha256 final : public hash_arm
		{
		public:
			hacl_sha256() { reset(); }
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return "hacl"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return hash_algorithm::sha256; }
			void               reset() override
			{
				Hacl_Hash_SHA2_sha256_init(State.data());
				BlockSize = 0U;
				ByteCount = 0U;
			}
			void update(std::span<const std::uint8_t> aBytes) override
			{
				ByteCount += aBytes.size();
				auto rest = aBytes;
				if (BlockSize != 0U) {
					const auto take = std::min(Block.size() - BlockSize, rest.size());
					std::copy_n(rest.begin(), take, Block.begin() + static_cast<std::ptrdiff_t>(BlockSize));
					BlockSize += take;
					rest = rest.subspan(take);
					if (BlockSize != Block.size()) {
						return;
					}
					Hacl_Hash_SHA2_sha256_update_nblocks(64U, Block.data(), State.data());
					BlockSize = 0U;
				}
				// The C length is 32 bits; hand it whole blocks a gibibyte at a time.
				constexpr std::size_t most = std::size_t{ 1 } << 30U;
				while (rest.size() >= Block.size()) {
					const auto whole = std::min(rest.size() - rest.size() % Block.size(), most);
					// HACL* reads the input through a non-const pointer and never writes it.
					Hacl_Hash_SHA2_sha256_update_nblocks(static_cast<std::uint32_t>(whole), const_cast<std::uint8_t*>(rest.data()), State.data());
					rest = rest.subspan(whole);
				}
				std::ranges::copy(rest, Block.begin());
				BlockSize = rest.size();
			}
			[[nodiscard]] auto finish() -> digest override
			{
				Hacl_Hash_SHA2_sha256_update_last(ByteCount, static_cast<std::uint32_t>(BlockSize), Block.data(), State.data());
				std::array<std::uint8_t, 32> result{ };
				Hacl_Hash_SHA2_sha256_finish(State.data(), result.data());
				return to_digest(result);
			}

		private:
			std::array<std::uint32_t, 8> State{ };
			std::array<std::uint8_t, 64> Block{ };
			std::size_t                  BlockSize = 0U;
			std::uint64_t                ByteCount = 0U;
		};

#if defined(__APPLE__)
		// CommonCrypto marks SHA-1 deprecated as a security digest; here it is a reference.
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		/// CommonCrypto, hardware-accelerated where the CPU has SHA instructions.
		class common_crypto_sha1 final : public hash_arm
		{
		public:
			common_crypto_sha1() { reset(); }
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return "commoncrypto"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return hash_algorithm::sha1; }
			void               reset() override { CC_SHA1_Init(&Context); }
			void               update(std::span<const std::uint8_t> aBytes) override
			{
				constexpr std::size_t most = std::size_t{ 1 } << 30U;
				for (auto rest = aBytes; !rest.empty();) {
					const auto take = std::min(rest.size(), most);
					CC_SHA1_Update(&Context, rest.data(), static_cast<CC_LONG>(take));
					rest = rest.subspan(take);
				}
			}
			[[nodiscard]] auto finish() -> digest override
			{
				std::array<std::uint8_t, 20> result{ };
				CC_SHA1_Final(result.data(), &Context);
				return to_digest(result);
			}

		private:
			CC_SHA1_CTX Context{ };
		};
	#pragma clang diagnostic pop

		class common_crypto_sha256 final : public hash_arm
		{
		public:
			common_crypto_sha256() { reset(); }
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return "commoncrypto"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return hash_algorithm::sha256; }
			void               reset() override { CC_SHA256_Init(&Context); }
			void               update(std::span<const std::uint8_t> aBytes) override
			{
				constexpr std::size_t most = std::size_t{ 1 } << 30U;
				for (auto rest = aBytes; !rest.empty();) {
					const auto take = std::min(rest.size(), most);
					CC_SHA256_Update(&Context, rest.data(), static_cast<CC_LONG>(take));
					rest = rest.subspan(take);
				}
			}
			[[nodiscard]] auto finish() -> digest override
			{
				std::array<std::uint8_t, 32> result{ };
				CC_SHA256_Final(result.data(), &Context);
				return to_digest(result);
			}

		private:
			CC_SHA256_CTX Context{ };
		};
#endif

#if defined(BPR_HASH_EVALUATION_QT)
		/// QCryptographicHash in the pinned Qt, the product's other hash.
		class qt_hash final : public hash_arm
		{
		public:
			explicit qt_hash(hash_algorithm aAlgorithm)
				: Algorithm(aAlgorithm)
				, Hash(aAlgorithm == hash_algorithm::sha1 ? QCryptographicHash::Sha1 : QCryptographicHash::Sha256)
			{
			}
			[[nodiscard]] auto name() const noexcept -> std::string_view override { return "qt"; }
			[[nodiscard]] auto algorithm() const noexcept -> hash_algorithm override { return Algorithm; }
			void               reset() override { Hash.reset(); }
			void               update(std::span<const std::uint8_t> aBytes) override
			{
				Hash.addData(QByteArrayView{ aBytes.data(), static_cast<qsizetype>(aBytes.size()) });
			}
			[[nodiscard]] auto finish() -> digest override
			{
				const auto bytes = Hash.resultView();
				digest     result;
				result.Size = static_cast<std::size_t>(bytes.size());
				std::ranges::copy(bytes, result.Bytes.begin());
				return result;
			}

		private:
			hash_algorithm     Algorithm;
			QCryptographicHash Hash;
		};
#endif
	}

	auto hex(const digest& aDigest) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		std::string                result;
		result.reserve(aDigest.Size * 2U);
		for (const auto byte : aDigest.view()) {
			result.push_back(digits[byte >> 4U]);
			result.push_back(digits[byte & 0x0FU]);
		}
		return result;
	}

	auto digest_size(hash_algorithm aAlgorithm) noexcept -> std::size_t
	{
		return aAlgorithm == hash_algorithm::sha1 ? 20U : 32U;
	}

	auto hash_arm::hash(std::span<const std::uint8_t> aBytes) -> digest
	{
		reset();
		update(aBytes);
		return finish();
	}

	auto make_sha1dc(bool aDetectReducedRound) -> std::unique_ptr<hash_arm>
	{
		return std::make_unique<sha1dc>(aDetectReducedRound);
	}

	auto make_plain_sha1() -> std::unique_ptr<hash_arm>
	{
		return std::make_unique<first_party_block_sha1>();
	}

	auto make_platform_reference(hash_algorithm aAlgorithm) -> std::unique_ptr<hash_arm>
	{
#if defined(__APPLE__)
		if (aAlgorithm == hash_algorithm::sha1) {
			return std::make_unique<common_crypto_sha1>();
		}
		return std::make_unique<common_crypto_sha256>();
#else
		static_cast<void>(aAlgorithm);
		return nullptr;
#endif
	}

	auto make_arms(hash_algorithm aAlgorithm) -> std::vector<std::unique_ptr<hash_arm>>
	{
		std::vector<std::unique_ptr<hash_arm>> arms;
		if (aAlgorithm == hash_algorithm::sha1) {
			arms.push_back(std::make_unique<core_sha1>());
			arms.push_back(std::make_unique<first_party_block_sha1>());
			arms.push_back(make_sha1dc(false));
		} else {
			arms.push_back(std::make_unique<core_sha256>());
			arms.push_back(std::make_unique<hacl_sha256>());
		}
		if (auto platform = make_platform_reference(aAlgorithm)) {
			arms.push_back(std::move(platform));
		}
#if defined(BPR_HASH_EVALUATION_QT)
		arms.push_back(std::make_unique<qt_hash>(aAlgorithm));
#endif
		return arms;
	}
}
