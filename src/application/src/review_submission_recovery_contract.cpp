#include "bpr/application/review_submission_recovery_port.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <span>

namespace bpr::application {
	namespace {
		void append(std::string& aBytes, std::uint64_t aValue)
		{
			for (auto shift = 0U; shift < 64U; shift += 8U) {
				aBytes.push_back(static_cast<char>((aValue >> shift) & 255U));
			}
		}
		void append(std::string& aBytes, std::string_view aValue)
		{
			append(aBytes, aValue.size());
			aBytes.append(aValue);
		}
		void append(std::string& aBytes, const core::git_commit_oid& aOid)
		{
			append(aBytes, static_cast<std::uint64_t>(aOid.algorithm()));
			aBytes.append(reinterpret_cast<const char*>(aOid.bytes().data()), aOid.bytes().size());
		}
		void append(std::string& aBytes, const core::review_mutation_target& aTarget)
		{
			append(aBytes, aTarget.Host.value());
			append(aBytes, aTarget.Repository.value());
			append(aBytes, aTarget.PullRequest.value());
			append(aBytes, aTarget.ExpectedHead);
		}
		auto digest(std::string_view aBytes) -> std::array<std::uint8_t, 32> { return core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(aBytes.data()), aBytes.size() }); }
	}
	auto review_submission_authority_fingerprint(const review_submission_live_binding& aBinding) -> std::array<std::uint8_t, 32>
	{
		auto        bytes  = std::string{ "review-submission-authority-v1" };
		const auto& target = aBinding.Target;
		const auto& actor  = aBinding.ActingIdentity;
		append(bytes, target.Semantic);
		append(bytes, target.Hostname.value());
		append(bytes, static_cast<std::uint64_t>(target.ServerContract));
		append(bytes, target.Repository.Owner);
		append(bytes, target.Repository.Name);
		append(bytes, target.Number.value());
		append(bytes, target.RepositoryDatabaseId);
		append(bytes, target.RepositoryNodeId);
		append(bytes, target.PullRequestDatabaseId);
		append(bytes, target.PullRequestNodeId);
		append(bytes, target.BaseRepositoryDatabaseId);
		append(bytes, target.BaseOid);
		append(bytes, target.BaseRef);
		append(bytes, target.HeadRef);
		append(bytes, target.Open ? 1U : 0U);
		append(bytes, actor.Account.value());
		append(bytes, actor.DatabaseId);
		append(bytes, actor.NodeId);
		append(bytes, actor.Login);
		return digest(bytes);
	}
	auto review_submission_request_fingerprint(const review_submission_request& aRequest) -> std::array<std::uint8_t, 32>
	{
		auto        bytes    = std::string{ "review-submission-request-v1" };
		const auto& proposal = aRequest.Proposal;
		append(bytes, proposal.Draft.value());
		append(bytes, proposal.Attempt.value());
		append(bytes, proposal.Target);
		append(bytes, proposal.ActingAccount.value());
		append(bytes, proposal.Body);
		append(bytes, static_cast<std::uint64_t>(proposal.Disposition));
		append(bytes, aRequest.IdempotencyKey);
		append(bytes, aRequest.LiveBinding ? 1U : 0U);
		if (aRequest.LiveBinding) {
			const auto authority = review_submission_authority_fingerprint(*aRequest.LiveBinding);
			bytes.append(reinterpret_cast<const char*>(authority.data()), authority.size());
		}
		// [[BPR-1345]]. Line comments follow everything else, so a summary-only
		// request keeps the fingerprint it always had.
		if (!proposal.Comments.empty()) {
			bytes.append("comments");
			append(bytes, proposal.Comments.size());
			for (const auto& comment : proposal.Comments) {
				append(bytes, comment.Draft.value());
				append(bytes, comment.DraftRevision);
				append(bytes, comment.Path);
				append(bytes, static_cast<std::uint64_t>(comment.Side));
				append(bytes, std::uint64_t{ comment.Line });
				append(bytes, comment.StartLine ? std::uint64_t{ *comment.StartLine } + 1U : 0U);
				append(bytes, comment.Body);
			}
		}
		return digest(bytes);
	}
	auto same_review_submission_identity(const review_submission_live_binding& aLeft, const review_submission_live_binding& aRight) -> bool
	{
		if (!valid(aLeft) || !valid(aRight)) {
			return false;
		}
		auto left                         = aLeft;
		left.Target.Semantic.ExpectedHead = aRight.Target.Semantic.ExpectedHead;
		left.Target.BaseOid               = aRight.Target.BaseOid;
		left.Target.BaseRef               = aRight.Target.BaseRef;
		left.Target.HeadRef               = aRight.Target.HeadRef;
		left.Target.Open                  = aRight.Target.Open;
		return left == aRight;
	}
	auto valid(const review_submission_recovery_record& aRecord) -> bool
	{
		if (!valid(aRecord.Request) || !aRecord.Request.LiveBinding || aRecord.RequestFingerprint != review_submission_request_fingerprint(aRecord.Request) || aRecord.AuthorityFingerprint != review_submission_authority_fingerprint(*aRecord.Request.LiveBinding)) {
			return false;
		}
		if (aRecord.BaselineReviewIds) {
			const auto& ids = *aRecord.BaselineReviewIds;
			if (ids.size() > maximum_review_submission_baseline_reviews || !std::ranges::is_sorted(ids) || std::ranges::adjacent_find(ids) != ids.end() || !std::ranges::all_of(ids, [](auto aId) { return aId.value() != 0U; })) {
				return false;
			}
			if (aRecord.Transmission != review_mutation_recovery_transmission::prepared && aRecord.Transmission != review_mutation_recovery_transmission::possibly_transmitted) {
				return false;
			}
		} else if (aRecord.Transmission != review_mutation_recovery_transmission::baseline_incomplete) {
			return false;
		}
		if (aRecord.State == review_mutation_recovery_state::unresolved) {
			return !aRecord.Receipt;
		}
		return aRecord.State == review_mutation_recovery_state::succeeded && aRecord.Receipt && valid(*aRecord.Receipt, aRecord.Request) && aRecord.BaselineReviewIds && aRecord.Transmission == review_mutation_recovery_transmission::possibly_transmitted && !std::ranges::binary_search(*aRecord.BaselineReviewIds, aRecord.Receipt->ReviewDatabaseId);
	}
}
