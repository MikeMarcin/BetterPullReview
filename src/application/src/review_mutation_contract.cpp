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
			? aRecord.Receipt.has_value() && !aRecord.Receipt->ServerCommentId.empty()
				&& aRecord.Receipt->ServerCommentId.size() <= 256U
			: !aRecord.Receipt.has_value();
		return lowercase_hexadecimal(aRecord.IdempotencyKey)
			&& !aRecord.Body.empty() && aRecord.Body.size() <= 64U * 1024U
			&& valid(aRecord.Anchor)
			&& aRecord.Anchor.Semantic.Side == core::diff_side::right
			&& aRecord.Anchor.Path.size()
			<= maximum_review_mutation_recovery_path_bytes
			&& receiptMatchesState;
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
		}
		return "unavailable";
	}

	auto idempotency_key_for(
		const core::review_publication_proposal& aProposal,
		const core::review_mutation_target&      aTarget) -> std::string
	{
		auto material = std::string{ };
		append(material, aProposal.Draft.value());
		append(material, aTarget.Host.value());
		append(material, aTarget.Repository.value());
		append(material, aTarget.PullRequest.value());
		append(material, aTarget.ExpectedHead.bytes());
		append(material, aProposal.Anchor.Comparison.bytes());
		append(material, static_cast<std::uint64_t>(aProposal.Anchor.Side));
		append(material, aProposal.Anchor.Line);
		append(material, static_cast<std::uint64_t>(aProposal.Disposition));
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
