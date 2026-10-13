#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/application/review_mutation_recovery_port.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto lowercase_hexadecimal(
			const std::string_view aValue) noexcept -> bool
		{
			return aValue.size() == 64U
				&& std::ranges::all_of(aValue, [](const unsigned char aCharacter) {
					   return (aCharacter >= '0' && aCharacter <= '9')
						   || (aCharacter >= 'a' && aCharacter <= 'f');
				   });
		}

		[[nodiscard]] auto bounded_text(
			const std::string_view aText,
			const std::size_t      aMaximum) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= aMaximum
				&& std::ranges::all_of(aText, [](const unsigned char aCharacter) {
					   return aCharacter >= 0x20U && aCharacter != 0x7FU;
				   });
		}

		[[nodiscard]] auto valid_server_comment_id(
			const std::string_view aValue) noexcept -> bool
		{
			return bounded_text(aValue, 256U);
		}

		void append(std::string& aText, const std::uint64_t aValue)
		{
			aText += std::to_string(aValue);
			aText += '\x1f';
		}

		void append(std::string& aText, const std::array<std::uint8_t, 32>& aBytes)
		{
			for (const auto byte : aBytes) {
				aText += static_cast<char>(byte);
			}
			aText += '\x1f';
		}
	} // namespace

	auto valid(const review_mutation_recovery_record& aRecord) noexcept -> bool
	{
		const auto receiptMatchesState
			= aRecord.State == review_mutation_recovery_state::succeeded
			? aRecord.Receipt.has_value()
				&& valid_server_comment_id(aRecord.Receipt->ServerCommentId)
			: !aRecord.Receipt.has_value();
		const auto baselineValid = !aRecord.BaselineExactCommentIds.has_value()
			|| (aRecord.BaselineExactCommentIds->size()
					<= maximum_review_mutation_baseline_comments
				&& std::ranges::all_of(
					*aRecord.BaselineExactCommentIds,
					valid_server_comment_id)
				&& std::ranges::is_sorted(*aRecord.BaselineExactCommentIds)
				&& std::ranges::adjacent_find(*aRecord.BaselineExactCommentIds)
					== aRecord.BaselineExactCommentIds->end());
		const auto transmissionMatchesBaseline
			= aRecord.BaselineExactCommentIds.has_value()
			? aRecord.Transmission
					== review_mutation_recovery_transmission::prepared
				|| aRecord.Transmission
					== review_mutation_recovery_transmission::possibly_transmitted
			: aRecord.Transmission
				== review_mutation_recovery_transmission::baseline_incomplete;
		return lowercase_hexadecimal(aRecord.IdempotencyKey)
			&& !aRecord.Body.empty() && aRecord.Body.size() <= 64U * 1024U
			// [[BPR-1212]]. Either side: `valid(anchor)` holds it to one, and a
			// new comment on the base side is GitHub's `side: LEFT`.
			&& valid(aRecord.Anchor)
			&& aRecord.Anchor.Path.size()
			<= maximum_review_mutation_recovery_path_bytes
			&& receiptMatchesState && baselineValid && transmissionMatchesBaseline;
	}

	auto valid(const review_mutation_target_binding& aBinding) noexcept -> bool
	{
		return aBinding.Semantic.Host.value() != 0U
			&& aBinding.Semantic.Repository.value() != 0U
			&& aBinding.Semantic.PullRequest.value() != 0U
			&& aBinding.Semantic.ExpectedHead.valid()
			&& valid_github_authority_hostname(
				   aBinding.ServerContract, aBinding.Hostname)
			&& core::valid(aBinding.Repository) && aBinding.Number.valid()
			&& aBinding.RepositoryDatabaseId != 0U
			&& bounded_text(aBinding.RepositoryNodeId, 256U)
			&& aBinding.PullRequestDatabaseId != 0U
			&& bounded_text(aBinding.PullRequestNodeId, 256U)
			&& aBinding.BaseRepositoryDatabaseId != 0U
			&& aBinding.BaseOid.valid()
			&& aBinding.BaseOid.algorithm()
			== aBinding.Semantic.ExpectedHead.algorithm()
			&& bounded_text(aBinding.BaseRef, 256U)
			&& bounded_text(aBinding.HeadRef, 256U) && aBinding.Open;
	}

	auto valid(const review_mutation_acting_identity& aIdentity) noexcept -> bool
	{
		return aIdentity.Account.value() != 0U && aIdentity.DatabaseId != 0U
			&& bounded_text(aIdentity.NodeId, 256U)
			&& bounded_text(aIdentity.Login, core::maximum_repository_owner_length);
	}

	auto valid(const review_mutation_anchor_binding& aAnchor) noexcept -> bool
	{
		return aAnchor.Semantic.Comparison.valid()
			&& (aAnchor.Semantic.Side == core::diff_side::left
				|| aAnchor.Semantic.Side == core::diff_side::right)
			&& aAnchor.Commit.valid() && bounded_text(aAnchor.Path, 4U * 1024U)
			&& aAnchor.Path.front() != '/';
	}

	auto valid(const review_mutation_live_binding& aBinding) noexcept -> bool
	{
		return valid(aBinding.Target) && valid(aBinding.ActingIdentity)
			&& valid(aBinding.Anchor)
			&& aBinding.Anchor.Commit == aBinding.Target.Semantic.ExpectedHead;
	}

	auto name(const review_mutation_failure aFailure) noexcept -> std::string_view
	{
		switch (aFailure) {
		case review_mutation_failure::credential_unavailable:
			return "credential_unavailable";
		case review_mutation_failure::unavailable:
			return "unavailable";
		case review_mutation_failure::refused_by_server:
			return "refused_by_server";
		case review_mutation_failure::undesignated_target:
			return "undesignated_target";
		case review_mutation_failure::target_moved:
			return "target_moved";
		case review_mutation_failure::stale_head:
			return "stale_head";
		case review_mutation_failure::unsupported:
			return "unsupported";
		case review_mutation_failure::cancelled:
			return "cancelled";
		case review_mutation_failure::target_unavailable:
			return "target_unavailable";
		case review_mutation_failure::incompatible_body:
			return "incompatible_body";
		case review_mutation_failure::malformed_response:
			return "malformed_response";
		}
		return "unavailable";
	}

	auto idempotency_key_for(
		const core::review_publication_proposal& aProposal,
		const core::review_mutation_target&      aTarget) -> std::string
	{
		auto material = std::string{ };
		// [[BPR-1212]]. A range and one line at its last line are different
		// publications. The range's own prefix leads, so a single line's key
		// is the same as it always was.
		if (aProposal.StartLine) {
			material = "bpr-review-comment-range-v1";
			append(material, *aProposal.StartLine);
		}
		if (aProposal.Durable) {
			material += "bpr-durable-review-publication-v1";
			append(material, aProposal.Durable->Incarnation);
			append(material, aProposal.Durable->DraftRevision);
		}
		append(material, aProposal.Draft.value());
		append(material, aTarget.Host.value());
		append(material, aTarget.Repository.value());
		append(material, aTarget.PullRequest.value());
		append(material, aTarget.ExpectedHead.bytes());
		append(material, aProposal.Anchor.Comparison.bytes());
		append(material, static_cast<std::uint64_t>(aProposal.Anchor.Side));
		append(material, aProposal.Anchor.Line);
		append(material, static_cast<std::uint64_t>(aProposal.Disposition));
		// [[BPR-0657]]. A reply and a new root at the same line with the same
		// body are different publications.
		append(material, static_cast<std::uint64_t>(aProposal.ReplyToThread.has_value() ? 1U : 0U));
		if (aProposal.ReplyToThread.has_value()) {
			append(material, static_cast<std::uint64_t>(aProposal.ReplyToThread->size()));
			material += *aProposal.ReplyToThread;
		}
		material += aProposal.Body;

		const auto digest = core::sha256(std::span{
			reinterpret_cast<const std::uint8_t*>(material.data()),
			material.size() });

		constexpr auto nibbles = std::string_view{ "0123456789abcdef" };
		auto           key     = std::string{ };
		key.reserve(digest.size() * 2U);
		for (const auto byte : digest) {
			key += nibbles[byte >> 4U];
			key += nibbles[byte & 0x0FU];
		}
		return key;
	}
} // namespace bpr::application
