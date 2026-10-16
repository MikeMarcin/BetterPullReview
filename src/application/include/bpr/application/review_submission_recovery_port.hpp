#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_RECOVERY_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_RECOVERY_PORT_HPP

#include "bpr/application/review_mutation_recovery_port.hpp"
#include "bpr/application/review_submission_contract.hpp"
#include "bpr/application/staged_review_publication.hpp"
#include "bpr/core/review_submission_unknown.hpp"

namespace bpr::application {
	inline constexpr std::size_t maximum_review_submission_baseline_reviews = 100U;

	/// \brief Freezes the exact anchor-free request and complete pre-send review identity set.
	struct review_submission_recovery_record
	{
		review_submission_request                      Request;
		std::array<std::uint8_t, 32>                   RequestFingerprint{ };
		std::array<std::uint8_t, 32>                   AuthorityFingerprint{ };
		std::optional<std::vector<review_database_id>> BaselineReviewIds;
		review_mutation_recovery_transmission          Transmission = review_mutation_recovery_transmission::baseline_incomplete;
		review_mutation_recovery_state                 State        = review_mutation_recovery_state::unresolved;
		std::optional<review_submission_receipt>       Receipt;
		/// [[BPR-1540]]. Why the last reconciliation of this unresolved
		/// attempt left it unknown; none once it is resolved.
		std::optional<core::review_submission_unknown_diagnosis> LastUnknown;
		auto                                                     operator<=>(const review_submission_recovery_record&) const = default;
	};
	struct review_submission_recovery_admission
	{
		review_submission_recovery_record Record;
		bool                              Inserted = false;
	};

	/// \brief Validates exact fingerprints, transmission state, baseline and correlated receipt.
	[[nodiscard]] auto valid(const review_submission_recovery_record& aRecord) -> bool;

	/// \brief Digests every semantic request and live-authority field, including draft and attempt.
	[[nodiscard]] auto review_submission_request_fingerprint(const review_submission_request& aRequest) -> std::array<std::uint8_t, 32>;

	/// \brief Digests the immutable live target and authenticated actor binding.
	[[nodiscard]] auto review_submission_authority_fingerprint(const review_submission_live_binding& aBinding) -> std::array<std::uint8_t, 32>;

	/// \brief Compares stable PR and actor identity without rebinding mutable revision context.
	[[nodiscard]] auto same_review_submission_identity(const review_submission_live_binding& aLeft, const review_submission_live_binding& aRight) -> bool;

	/// \brief Transactional durable admission, transmission witness and exact resolution.
	class review_submission_recovery_port
	{
	public:
		review_submission_recovery_port()          = default;
		virtual ~review_submission_recovery_port() = default;

		review_submission_recovery_port(const review_submission_recovery_port&)                    = delete;
		auto operator=(const review_submission_recovery_port&) -> review_submission_recovery_port& = delete;
		review_submission_recovery_port(review_submission_recovery_port&&)                         = delete;
		auto operator=(review_submission_recovery_port&&) -> review_submission_recovery_port&      = delete;

		[[nodiscard]] virtual auto load_staged(const std::string& aKey) -> std::expected<std::optional<staged_publication_record>, review_mutation_recovery_failure>
		{
			static_cast<void>(aKey);
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
		[[nodiscard]] virtual auto advance_staged(const std::optional<staged_publication_record>& aBefore, const staged_publication_record& aAfter) -> std::expected<staged_publication_record, review_mutation_recovery_failure>
		{
			static_cast<void>(aBefore);
			static_cast<void>(aAfter);
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
		[[nodiscard]] virtual auto pending_staged() -> std::expected<std::vector<staged_publication_record>, review_mutation_recovery_failure>
		{
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
		/// \brief Atomically reads all unresolved records in this scope, bounded by store capacity.
		[[nodiscard]] virtual auto pending() -> std::expected<std::vector<review_submission_recovery_record>, review_mutation_recovery_failure> = 0;

		[[nodiscard]] virtual auto load(const std::string& aKey) -> std::expected<std::optional<review_submission_recovery_record>, review_mutation_recovery_failure> = 0;

		[[nodiscard]] virtual auto admit(const review_submission_recovery_record& aRecord) -> std::expected<review_submission_recovery_admission, review_mutation_recovery_failure> = 0;

		[[nodiscard]] virtual auto mark_possibly_transmitted(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<review_submission_recovery_record, review_mutation_recovery_failure> = 0;

		[[nodiscard]] virtual auto resolve(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const review_submission_receipt& aReceipt) -> std::expected<review_submission_recovery_record, review_mutation_recovery_failure> = 0;
		/// \brief [[BPR-1540]]. Records, with the unresolved attempt admitted
		/// with this exact fingerprint, why its outcome is still unknown,
		/// replacing the reason recorded before. Counts and enums only.
		[[nodiscard]] virtual auto note_unknown(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const core::review_submission_unknown_diagnosis& aDiagnosis)
			-> std::expected<void, review_mutation_recovery_failure>
		{
			static_cast<void>(aKey);
			static_cast<void>(aFingerprint);
			static_cast<void>(aDiagnosis);
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
		/// \brief [[BPR-1345]]. Removes the unresolved attempt admitted with
		/// this exact fingerprint once it is definitely not published: never
		/// sent, or refused by GitHub. A resolved attempt is never removed; a
		/// missing one is already gone.
		[[nodiscard]] virtual auto withdraw(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<void, review_mutation_recovery_failure>
		{
			static_cast<void>(aKey);
			static_cast<void>(aFingerprint);
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
	};
}

#endif
