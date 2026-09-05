#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_CONTRACT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_CONTRACT_HPP

#include "bpr/application/review_mutation_port.hpp"
#include "bpr/core/review_submission.hpp"

#include <cstddef>

namespace bpr::application {
	using review_database_id = core::strong_id<struct review_database_id_tag>;

	inline constexpr std::size_t maximum_review_submission_idempotency_key_bytes = 256U;
	inline constexpr std::size_t maximum_review_submission_node_id_bytes         = 256U;

	/// \brief Binds a whole review to a complete live target and authenticated actor.
	/// \details These are observations, not execution permission. Later host work
	/// must re-observe the exact target and actor before any effect.
	struct review_submission_live_binding
	{
		review_mutation_target_binding  Target;
		review_mutation_acting_identity ActingIdentity;

		auto operator<=>(const review_submission_live_binding&) const = default;
	};

	/// \brief Carries an anchor-free core proposal and bounded retry identity.
	/// \details LiveBinding is optional for deterministic fixtures. A live host
	/// must require it and preserve one idempotency key through reconciliation;
	/// validation alone does not authorize a remote write or a second POST.
	struct review_submission_request
	{
		core::review_submission_proposal              Proposal;
		std::string                                   IdempotencyKey;
		std::optional<review_submission_live_binding> LiveBinding;

		auto operator<=>(const review_submission_request&) const = default;
	};

	/// \brief Names an ordinary submitted review, distinctly from a comment receipt.
	struct review_submission_receipt
	{
		review_database_id           ReviewDatabaseId;
		std::string                  ReviewNodeId;
		core::review_mutation_target Target;
		core::github_account_id      ActingAccount;
		core::review_disposition     Disposition = core::review_disposition::comment;

		auto operator<=>(const review_submission_receipt&) const = default;
	};

	/// \brief Checks a complete target and actor binding without source-line data.
	[[nodiscard]] auto valid(const review_submission_live_binding& aBinding) noexcept -> bool;
	/// \brief Checks proposal, bounded printable ASCII key and exact live authority.
	[[nodiscard]] auto valid(const review_submission_request& aRequest) noexcept -> bool;
	/// \brief Checks positive review ID, bounded printable ASCII node ID and semantics.
	[[nodiscard]] auto valid(const review_submission_receipt& aReceipt) noexcept -> bool;
	/// \brief Checks that a valid receipt matches the exact request authority/disposition.
	[[nodiscard]] auto valid(const review_submission_receipt& aReceipt,
							 const review_submission_request& aRequest) noexcept -> bool;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SUBMISSION_CONTRACT_HPP
