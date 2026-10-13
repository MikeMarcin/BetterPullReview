#ifndef BPR_TESTS_HASH_EVALUATION_HASH_ARMS_HPP
#define BPR_TESTS_HASH_EVALUATION_HASH_ARMS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1446]]. The implementations RFC-BPR-0080 compares, behind one
/// streaming interface: the current core hashes (the baseline), the
/// candidates, and the platform and Qt references.
namespace bpr::hash_evaluation {
	enum class hash_algorithm {
		sha1,
		sha256,
	};

	/// \brief A SHA-1 or SHA-256 digest.
	struct digest
	{
		std::array<std::uint8_t, 32> Bytes{ };
		std::size_t                  Size = 0U;

		[[nodiscard]] auto view() const noexcept -> std::span<const std::uint8_t> { return { Bytes.data(), Size }; }
		auto               operator==(const digest&) const -> bool = default;
	};

	[[nodiscard]] auto hex(const digest& aDigest) -> std::string;
	[[nodiscard]] auto digest_size(hash_algorithm aAlgorithm) noexcept -> std::size_t;

	/// \brief One implementation of one algorithm, used as a stream.
	/// \details `reset` starts a message, `update` may be called any number of
	/// times, and `finish` ends it. `detected` reports, after `finish`, whether
	/// the implementation found the message to be part of a SHA-1 collision
	/// attack; only sha1collisiondetection can say so.
	class hash_arm
	{
	public:
		hash_arm()                                   = default;
		hash_arm(const hash_arm&)                    = delete;
		hash_arm(hash_arm&&)                         = delete;
		auto operator=(const hash_arm&) -> hash_arm& = delete;
		auto operator=(hash_arm&&) -> hash_arm&      = delete;
		virtual ~hash_arm()                          = default;

		[[nodiscard]] virtual auto name() const noexcept -> std::string_view    = 0;
		[[nodiscard]] virtual auto algorithm() const noexcept -> hash_algorithm = 0;
		virtual void               reset()                                      = 0;
		virtual void               update(std::span<const std::uint8_t> aBytes) = 0;
		[[nodiscard]] virtual auto finish() -> digest                           = 0;
		[[nodiscard]] virtual auto detected() const noexcept -> bool { return false; }

		/// \brief The whole message in one update.
		[[nodiscard]] auto hash(std::span<const std::uint8_t> aBytes) -> digest;
	};

	/// \brief Every arm of \p aAlgorithm this build has. The first is always the
	/// current core implementation, the baseline every other arm is compared to.
	[[nodiscard]] auto make_arms(hash_algorithm aAlgorithm) -> std::vector<std::unique_ptr<hash_arm>>;

	/// \brief sha1collisiondetection as Git configures it, optionally also
	/// detecting reduced-round collisions (its own test vector needs that).
	[[nodiscard]] auto make_sha1dc(bool aDetectReducedRound) -> std::unique_ptr<hash_arm>;

	/// \brief A platform reference of \p aAlgorithm (CommonCrypto on macOS), if
	/// this build has one.
	[[nodiscard]] auto make_platform_reference(hash_algorithm aAlgorithm) -> std::unique_ptr<hash_arm>;
}

#endif
