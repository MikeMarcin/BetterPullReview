#include "bpr/application/review_submission_contract.hpp"

#include <algorithm>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto bounded_identity(const std::string_view aValue,
											const std::size_t      aMaximum) noexcept -> bool
		{
			return !aValue.empty() && aValue.size() <= aMaximum
				&& std::ranges::all_of(aValue, [](const unsigned char aByte) {
					   return aByte >= 0x21U && aByte <= 0x7EU;
				   });
		}
	} // namespace

	auto valid(const review_submission_live_binding& aBinding) noexcept -> bool
	{
		return valid(aBinding.Target) && valid(aBinding.ActingIdentity);
	}

	auto valid(const review_submission_request& aRequest) noexcept -> bool
	{
		return core::valid(aRequest.Proposal)
			&& bounded_identity(aRequest.IdempotencyKey, maximum_review_submission_idempotency_key_bytes)
			&& (!aRequest.LiveBinding
				|| (valid(*aRequest.LiveBinding)
					&& aRequest.LiveBinding->Target.Semantic == aRequest.Proposal.Target
					&& aRequest.LiveBinding->ActingIdentity.Account == aRequest.Proposal.ActingAccount));
	}

	auto valid(const review_submission_receipt& aReceipt) noexcept -> bool
	{
		return aReceipt.ReviewDatabaseId.value() != 0U
			&& bounded_identity(aReceipt.ReviewNodeId, maximum_review_submission_node_id_bytes)
			&& core::valid(aReceipt.Target) && aReceipt.ActingAccount.value() != 0U
			&& core::valid(aReceipt.Disposition);
	}

	auto valid(const review_submission_receipt& aReceipt,
			   const review_submission_request& aRequest) noexcept -> bool
	{
		return valid(aReceipt) && valid(aRequest)
			&& aReceipt.Target == aRequest.Proposal.Target
			&& aReceipt.ActingAccount == aRequest.Proposal.ActingAccount
			&& aReceipt.Disposition == aRequest.Proposal.Disposition;
	}
} // namespace bpr::application
