#include "bpr/application/review_mutation_coordinator.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace bpr::application {
	namespace {
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

	auto valid(const review_mutation_target_binding& aBinding) noexcept -> bool
	{
		return aBinding.Semantic.Host.value() != 0U
			&& aBinding.Semantic.Repository.value() != 0U
			&& aBinding.Semantic.PullRequest.value() != 0U
			&& aBinding.Semantic.ExpectedHead.valid()
			&& core::valid(aBinding.Repository) && aBinding.Number.valid()
			&& aBinding.RepositoryDatabaseId != 0U
			&& bounded_text(aBinding.RepositoryNodeId, 256U)
			&& aBinding.PullRequestDatabaseId != 0U
			&& bounded_text(aBinding.PullRequestNodeId, 256U)
			&& aBinding.BaseRepositoryDatabaseId != 0U && aBinding.Open;
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

	review_mutation_coordinator::review_mutation_coordinator(
		review_mutation_port&               aProvider,
		const core::review_mutation_target& aDesignated,
		const core::github_account_id       aActingIdentity) noexcept
		: Provider{ &aProvider }
		, Designated{ aDesignated }
		, ActingIdentity{ aActingIdentity }
	{
	}

	review_mutation_coordinator::review_mutation_coordinator(
		review_mutation_port&           aProvider,
		review_mutation_target_binding  aDesignated,
		review_mutation_acting_identity aActingIdentity) noexcept
		: Provider{ &aProvider }
		, Designated{ aDesignated.Semantic }
		, ActingIdentity{ aActingIdentity.Account }
		, TargetBinding{ std::move(aDesignated) }
		, ActingIdentityBinding{ std::move(aActingIdentity) }
	{
	}

	auto review_mutation_coordinator::designated() const noexcept
		-> const core::review_mutation_target&
	{
		return Designated;
	}

	auto review_mutation_coordinator::capabilities() const
		-> review_provider_capabilities
	{
		return Provider->capabilities();
	}

	auto review_mutation_coordinator::statistics() const noexcept
		-> review_mutation_statistics
	{
		return Statistics;
	}

	auto review_mutation_coordinator::confirm_target(const bool aAcknowledged)
		-> std::expected<void, review_mutation_failure>
	{
		++Statistics.TargetObservations;
		const auto observed = Provider->observe_target();
		if (!observed) {
			return std::unexpected(observed.error());
		}

		switch (core::revalidate(Designated, *observed)) {
		case core::target_revalidation::accepted:
			return { };
		case core::target_revalidation::head_moved:
			// A moved head is the reviewer's decision to make, and only theirs.
			return aAcknowledged
				? std::expected<void, review_mutation_failure>{ }
				: std::unexpected(review_mutation_failure::stale_head);
		case core::target_revalidation::target_changed:
			// Not overridable at any level. The coordinator was designated for
			// one pull request and the server is now describing another.
			return std::unexpected(review_mutation_failure::undesignated_target);
		case core::target_revalidation::head_unknown:
			break;
		}
		return std::unexpected(review_mutation_failure::target_moved);
	}

	auto review_mutation_coordinator::publish(
		const core::review_publication_proposal& aProposal,
		const bool                               aAcknowledged)
		-> std::expected<review_mutation_receipt, review_mutation_failure>
	{
		return publish_bound(aProposal, std::nullopt, aAcknowledged);
	}

	auto review_mutation_coordinator::publish(
		const core::review_publication_proposal& aProposal,
		review_mutation_anchor_binding           aAnchor,
		const bool                               aAcknowledged)
		-> std::expected<review_mutation_receipt, review_mutation_failure>
	{
		return publish_bound(aProposal, std::move(aAnchor), aAcknowledged);
	}

	auto review_mutation_coordinator::publish_bound(
		const core::review_publication_proposal&      aProposal,
		std::optional<review_mutation_anchor_binding> aAnchor,
		const bool                                    aAcknowledged)
		-> std::expected<review_mutation_receipt, review_mutation_failure>
	{
		++Statistics.Attempts;
		const auto completeBinding = TargetBinding.has_value()
			&& ActingIdentityBinding.has_value() && aAnchor.has_value();
		if (completeBinding
			&& (!valid(*TargetBinding) || !valid(*ActingIdentityBinding)
				|| !valid(*aAnchor) || aAnchor->Semantic != aProposal.Anchor
				|| aAnchor->Commit != TargetBinding->Semantic.ExpectedHead)) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(review_mutation_failure::undesignated_target);
		}
		if (!completeBinding
			&& (TargetBinding.has_value() || ActingIdentityBinding.has_value()
				|| aAnchor.has_value())) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(review_mutation_failure::undesignated_target);
		}

		if (auto prepared = confirm_target(aAcknowledged); !prepared) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(prepared.error());
		}

		// Observed again immediately before submitting. The first check decided
		// whether publication was permitted; this one decides whether it is
		// still the same pull request. Between the two there is a window, and
		// this is what closes it.
		if (auto current = confirm_target(aAcknowledged); !current) {
			++Statistics.RefusedBeforeSubmit;
			return std::unexpected(current.error());
		}

		++Statistics.Submissions;
		auto liveBinding = std::optional<review_mutation_live_binding>{ };
		if (completeBinding) {
			liveBinding = review_mutation_live_binding{
				.Target         = *TargetBinding,
				.ActingIdentity = *ActingIdentityBinding,
				.Anchor         = std::move(*aAnchor),
			};
		}

		auto receipt = Provider->submit(review_mutation_request{
			.Proposal       = aProposal,
			.Target         = Designated,
			.ActingIdentity = ActingIdentity,
			.IdempotencyKey = idempotency_key_for(aProposal, Designated),
			.LiveBinding    = std::move(liveBinding),
		});
		if (!receipt) {
			return receipt;
		}
		if (receipt->Duplicate) {
			++Statistics.Duplicates;
		}
		return receipt;
	}
} // namespace bpr::application
