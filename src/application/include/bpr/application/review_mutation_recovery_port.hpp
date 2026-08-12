#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_RECOVERY_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_RECOVERY_PORT_HPP

#include "bpr/application/review_mutation_port.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t maximum_review_mutation_recovery_records    = 1'024U;
	inline constexpr std::size_t maximum_review_mutation_recovery_path_bytes = 4'096U;
	inline constexpr std::size_t maximum_review_mutation_baseline_comments   = 100U;

	enum class review_mutation_recovery_failure : std::uint8_t {
		unavailable,
		conflict,
		integrity_failure,
		capacity,
	};

	enum class review_mutation_recovery_state : std::uint8_t {
		unresolved,
		succeeded,
	};

	enum class review_mutation_recovery_transmission : std::uint8_t {
		baseline_incomplete,
		prepared,
		possibly_transmitted,
	};

	/// \brief One exact write whose transmission outcome must survive restart.
	struct review_mutation_recovery_record
	{
		std::string                    IdempotencyKey;
		std::array<std::uint8_t, 32>   RequestFingerprint{ };
		std::array<std::uint8_t, 32>   AuthorityFingerprint{ };
		std::string                    Body;
		review_mutation_anchor_binding Anchor;
		/// Null means a migrated attempt that predates durable baseline capture.
		std::optional<std::vector<std::string>> BaselineExactCommentIds;
		review_mutation_recovery_transmission   Transmission
			= review_mutation_recovery_transmission::baseline_incomplete;
		review_mutation_recovery_state State
			= review_mutation_recovery_state::unresolved;
		std::optional<review_mutation_receipt> Receipt;

		auto operator<=>(const review_mutation_recovery_record&) const = default;
	};

	[[nodiscard]] auto valid(
		const review_mutation_recovery_record& aRecord) noexcept -> bool;

	struct review_mutation_recovery_admission
	{
		review_mutation_recovery_record Record;
		bool                            Inserted = false;
	};

	/// \brief Durable admission and resolution for one exact mutation attempt.
	class review_mutation_recovery_port
	{
	public:
		review_mutation_recovery_port()          = default;
		virtual ~review_mutation_recovery_port() = default;

		review_mutation_recovery_port(const review_mutation_recovery_port&) = delete;
		auto operator=(const review_mutation_recovery_port&)
			-> review_mutation_recovery_port&                          = delete;
		review_mutation_recovery_port(review_mutation_recovery_port&&) = delete;
		auto operator=(review_mutation_recovery_port&&)
			-> review_mutation_recovery_port& = delete;

		[[nodiscard]] virtual auto load(const std::string& aIdempotencyKey)
			-> std::expected<
				std::optional<review_mutation_recovery_record>,
				review_mutation_recovery_failure> = 0;

		/// \brief Inserts unresolved durably or returns the existing exact row.
		[[nodiscard]] virtual auto admit(
			const review_mutation_recovery_record& aRecord)
			-> std::expected<
				review_mutation_recovery_admission,
				review_mutation_recovery_failure> = 0;

		/// \brief Durably witnesses that the transport created the POST operation.
		[[nodiscard]] virtual auto mark_possibly_transmitted(
			const std::string&                  aIdempotencyKey,
			const std::array<std::uint8_t, 32>& aRequestFingerprint)
			-> std::expected<
				review_mutation_recovery_record,
				review_mutation_recovery_failure> = 0;

		/// \brief Atomically records the exact server-confirmed receipt.
		[[nodiscard]] virtual auto resolve(
			const std::string&                  aIdempotencyKey,
			const std::array<std::uint8_t, 32>& aRequestFingerprint,
			const review_mutation_receipt&      aReceipt)
			-> std::expected<
				review_mutation_recovery_record,
				review_mutation_recovery_failure> = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_RECOVERY_PORT_HPP
