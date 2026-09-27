#ifndef BPR_REVIEW_PUBLICATION_CAPABILITY_CACHE_HPP
#define BPR_REVIEW_PUBLICATION_CAPABILITY_CACHE_HPP

#include "bpr/application/review_mutation_port.hpp"

#include <array>
#include <map>
#include <optional>

namespace bpr::application {
	enum class review_publication_operation : std::uint8_t { create_off_hunk_line };
	/// \brief Complete authority for one operation; unrelated read facts use their own cache.
	struct review_publication_capability_key
	{
		core::host_profile_id           Profile;
		github_profile_revision         ProfileRevision;
		core::git_host_id               Host;
		github_canonical_hostname       Hostname;
		github_server_contract          ServerContract = github_server_contract::github_com_2022_11_28;
		review_mutation_acting_identity Actor;
		std::uint64_t                   RepositoryDatabaseId = 0U;
		std::string                     RepositoryNodeId;
		review_publication_operation    Operation           = review_publication_operation::create_off_hunk_line;
		std::uint32_t                   ApiRevision         = 20221128U;
		std::uint32_t                   ClassifierRevision  = 1U;
		std::uint64_t                   FreshnessGeneration = 0U;
		// Observations prove this exact commit/path/line, never repository-wide support.
		std::array<std::uint8_t, 32U> AnchorDigest{ };
		auto                          operator<=>(const review_publication_capability_key&) const = default;
	};
	enum class review_publication_evidence_source : std::uint8_t {
		recorded_contract,
		observed_native_success,
		observed_definitive_unsupported
	};
	/// \brief Typed evidence emitted only after the provider verifies its exact contract.
	struct review_publication_capability_evidence
	{
		core::off_hunk_creation_capability State  = core::off_hunk_creation_capability::unknown;
		review_publication_evidence_source Source = review_publication_evidence_source::recorded_contract;
		std::array<std::uint8_t, 32U>      ContractDigest{ };
		std::uint64_t                      ObservedAt                                                       = 0U;
		std::uint64_t                      ExpiresAt                                                        = 0U;
		auto                               operator<=>(const review_publication_capability_evidence&) const = default;
	};
	/// \brief Bounded session-owned write evidence. Monotonic seconds are explicit inputs.
	/// No caller-populated instance is injected into a production host or provider.
	class review_publication_capability_cache final
	{
	public:
		static constexpr std::size_t   maximum_entries          = 128U;
		static constexpr std::uint64_t maximum_lifetime_seconds = 300U;
		[[nodiscard]] auto             lookup(const review_publication_capability_key& aKey, std::uint64_t aNow) const -> std::optional<review_publication_capability_evidence>;
		[[nodiscard]] auto             record(const review_publication_capability_key& aKey, review_publication_capability_evidence aEvidence, std::uint64_t aNow) -> bool;

	private:
		std::map<review_publication_capability_key, review_publication_capability_evidence> Entries;
	};
}
#endif
