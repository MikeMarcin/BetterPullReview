#include "bpr/application/review_publication_capability_cache.hpp"

#include <algorithm>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto valid_key(const review_publication_capability_key& aKey) -> bool
		{
			return aKey.Profile.value() != 0U && aKey.ProfileRevision.valid() && aKey.Host.value() != 0U && aKey.Hostname.valid()
				&& valid(aKey.Actor) && aKey.RepositoryDatabaseId != 0U && !aKey.RepositoryNodeId.empty()
				&& aKey.RepositoryNodeId.size() <= 256U
				&& std::ranges::all_of(aKey.RepositoryNodeId, [](const unsigned char aByte) { return aByte >= 0x21U && aByte <= 0x7eU; })
				&& std::ranges::any_of(aKey.AnchorDigest, [](const auto aByte) { return aByte != 0U; })
				&& aKey.Operation <= review_publication_operation::create_tour_conversation_comment
				&& aKey.ApiRevision == 20221128U && aKey.ClassifierRevision != 0U && aKey.FreshnessGeneration != 0U
				&& (aKey.ServerContract == github_server_contract::github_com_2022_11_28 || aKey.ServerContract == github_server_contract::ghes_3_18_2022_11_28);
		}
		[[nodiscard]] auto current(const review_publication_capability_evidence& aEvidence, const std::uint64_t aNow) -> bool
		{
			return aNow >= aEvidence.ObservedAt && aNow < aEvidence.ExpiresAt;
		}
	}
	auto review_publication_capability_cache::lookup(const review_publication_capability_key& aKey, const std::uint64_t aNow) const -> std::optional<review_publication_capability_evidence>
	{
		if (!valid_key(aKey)) {
			return std::nullopt;
		}
		const auto found = Entries.find(aKey);
		return found != Entries.end() && current(found->second, aNow) ? std::optional{ found->second } : std::nullopt;
	}
	auto review_publication_capability_cache::record(const review_publication_capability_key& aKey, review_publication_capability_evidence aEvidence, const std::uint64_t aNow) -> bool
	{
		using state  = core::off_hunk_creation_capability;
		using source = review_publication_evidence_source;
		if (!valid_key(aKey) || !current(aEvidence, aNow)
			|| aEvidence.ExpiresAt - aEvidence.ObservedAt > maximum_lifetime_seconds
			|| (aEvidence.State != state::supported && aEvidence.State != state::unsupported)
			|| aEvidence.Source > source::observed_definitive_unsupported
			|| (aEvidence.Source == source::observed_native_success && aEvidence.State != state::supported)
			|| (aEvidence.Source == source::observed_definitive_unsupported && aEvidence.State != state::unsupported)
			|| std::ranges::all_of(aEvidence.ContractDigest, [](const auto aByte) { return aByte == 0U; })) {
			return false;
		}
		std::erase_if(Entries, [aNow](const auto& aEntry) { return !current(aEntry.second, aNow); });
		if (!Entries.contains(aKey) && Entries.size() >= maximum_entries) {
			return false;
		}
		Entries.insert_or_assign(aKey, std::move(aEvidence));
		return true;
	}
}
