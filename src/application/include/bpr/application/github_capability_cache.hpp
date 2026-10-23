#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CAPABILITY_CACHE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CAPABILITY_CACHE_HPP

#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/github_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <variant>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t github_capability_fact_count             = 4U;
	inline constexpr std::size_t github_schema_capability_fact_count      = 10U;
	inline constexpr std::size_t maximum_github_capability_cache_capacity = 64U;
	inline constexpr std::uint64_t
		maximum_github_capability_freshness_window = 900U;

	inline constexpr std::array<
		core::github_capability,
		github_capability_fact_count>
		canonical_github_capabilities{
			core::github_capability::review_threads,
			core::github_capability::viewed_file_mutations,
			core::github_capability::resolve_thread,
			core::github_capability::unresolve_thread,
		};

	enum class github_capability_evidence_source : std::uint8_t {
		uninitialized,
		recorded_contract,
		observed_contract,
	};

	enum class github_schema_capability : std::uint8_t {
		review_threads_read,
		viewed_file_state_read,
		mark_file_as_viewed,
		unmark_file_as_viewed,
		resolve_permission,
		resolve_review_thread,
		unresolve_permission,
		unresolve_review_thread,
		/// [[BPR-1746]]. `PullRequestReviewThread.viewerCanReply: Boolean!`.
		reply_permission,
		/// [[BPR-1746]]. `PullRequest.locked: Boolean!`.
		conversation_lock,
	};

	inline constexpr std::array<
		github_schema_capability,
		github_schema_capability_fact_count>
		canonical_github_schema_capabilities{
			github_schema_capability::review_threads_read,
			github_schema_capability::viewed_file_state_read,
			github_schema_capability::mark_file_as_viewed,
			github_schema_capability::unmark_file_as_viewed,
			github_schema_capability::resolve_permission,
			github_schema_capability::resolve_review_thread,
			github_schema_capability::unresolve_permission,
			github_schema_capability::unresolve_review_thread,
			github_schema_capability::reply_permission,
			github_schema_capability::conversation_lock,
		};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		github_capability_evidence_source aSource) noexcept -> bool
	{
		return aSource == github_capability_evidence_source::recorded_contract
			|| aSource
			== github_capability_evidence_source::observed_contract;
	}

	/// \brief Describes the github capability fact record.
	struct github_capability_fact
	{
		core::github_capability           Capability = core::github_capability::review_threads;
		core::github_capability_state     State      = core::github_capability_state::unknown;
		github_capability_evidence_source Source     = github_capability_evidence_source::uninitialized;

		auto operator<=>(const github_capability_fact&) const = default;
	};

	/// \brief Describes the github schema capability fact record.
	struct github_schema_capability_fact
	{
		github_schema_capability          Capability = github_schema_capability::review_threads_read;
		core::github_capability_state     State      = core::github_capability_state::unknown;
		github_capability_evidence_source Source     = github_capability_evidence_source::uninitialized;

		auto operator<=>(const github_schema_capability_fact&) const = default;
	};

	enum class github_capability_unavailable_reason : std::uint8_t {
		none,
		authentication,
		permission,
		network,
		api_version,
		rate_limit,
		certificate,
		redirect,
		cancellation,
		timeout,
		malformed_response,
		response_limit,
		transport_unavailable,
		incomplete_evidence,
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		github_capability_unavailable_reason aReason) noexcept -> bool
	{
		switch (aReason) {
		case github_capability_unavailable_reason::authentication:
		case github_capability_unavailable_reason::permission:
		case github_capability_unavailable_reason::network:
		case github_capability_unavailable_reason::api_version:
		case github_capability_unavailable_reason::rate_limit:
		case github_capability_unavailable_reason::certificate:
		case github_capability_unavailable_reason::redirect:
		case github_capability_unavailable_reason::cancellation:
		case github_capability_unavailable_reason::timeout:
		case github_capability_unavailable_reason::malformed_response:
		case github_capability_unavailable_reason::response_limit:
		case github_capability_unavailable_reason::transport_unavailable:
		case github_capability_unavailable_reason::incomplete_evidence:
			return true;
		case github_capability_unavailable_reason::none:
			return false;
		}
		return false;
	}

	/// \brief Describes the github capability authority key record.
	struct github_capability_authority_key
	{
		core::host_profile_id          Profile;
		github_profile_revision        ProfileRevision;
		core::git_host_id              Host;
		core::github_account_id        Account;
		github_server_contract         ServerContract = github_server_contract::github_com_2022_11_28;
		github_probe_contract_revision ProbeContractRevision;

		auto operator<=>(const github_capability_authority_key&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_capability_authority_key& aAuthority) noexcept -> bool
	{
		return aAuthority.Profile.value() != 0U
			&& aAuthority.ProfileRevision.valid()
			&& aAuthority.Host.value() != 0U
			&& aAuthority.Account.value() != 0U
			&& valid(aAuthority.ServerContract)
			&& aAuthority.ProbeContractRevision.valid();
	}

	/// \brief Describes the github capability probe observation record.
	struct github_capability_probe_observation
	{
		github_capability_authority_key                                  Authority;
		github_freshness_epoch                                           ObservedAt;
		std::array<github_capability_fact, github_capability_fact_count> Facts;
		std::array<
			github_schema_capability_fact,
			github_schema_capability_fact_count>
			SchemaFacts;

		auto operator<=>(const github_capability_probe_observation&) const = default;
	};

	/// \brief Describes the github capability probe unavailable record.
	struct github_capability_probe_unavailable
	{
		github_capability_authority_key      Authority;
		github_freshness_epoch               ObservedAt;
		core::github_capabilities            Capabilities;
		github_capability_unavailable_reason Reason = github_capability_unavailable_reason::none;

		auto operator<=>(const github_capability_probe_unavailable&) const = default;
	};

	using github_capability_probe_result = std::variant<
		github_capability_probe_observation,
		github_capability_probe_unavailable>;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_capability_probe_observation& aObservation) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const github_capability_probe_unavailable& aUnavailable) noexcept -> bool;
	/// \brief Computes the projected capabilities result.
	[[nodiscard]] auto projected_capabilities(
		const github_capability_probe_observation& aObservation) noexcept
		-> core::github_capabilities;
	/// \brief Computes the projected read shape result.
	[[nodiscard]] auto projected_read_shape(
		const github_capability_probe_observation& aObservation) noexcept
		-> github_read_shape;
	/// \brief Computes the all unknown result.
	[[nodiscard]] auto all_unknown(
		const core::github_capabilities& aCapabilities) noexcept -> bool;

	/// \brief Describes the github capability cache policy record.
	struct github_capability_cache_policy
	{
		std::size_t               Capacity              = 16U;
		std::uint64_t             FreshnessWindowEpochs = 300U;
		core::capability_revision FirstRevision{ 1U };

		auto operator<=>(const github_capability_cache_policy&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_capability_cache_policy& aPolicy) noexcept -> bool
	{
		return aPolicy.Capacity != 0U
			&& aPolicy.Capacity <= maximum_github_capability_cache_capacity
			&& aPolicy.FreshnessWindowEpochs != 0U
			&& aPolicy.FreshnessWindowEpochs
			<= maximum_github_capability_freshness_window
			&& aPolicy.FirstRevision.value() != 0U;
	}

	/// \brief Describes the github capability cache entry record.
	struct github_capability_cache_entry
	{
		github_capability_authority_key                                  Authority;
		std::array<github_capability_fact, github_capability_fact_count> Facts;
		std::array<
			github_schema_capability_fact,
			github_schema_capability_fact_count>
								  SchemaFacts;
		core::github_capabilities Capabilities;
		github_read_shape         ReadShape;
		core::capability_revision Revision;
		github_freshness_epoch    ObservedAt;
		github_freshness_epoch    ExpiresBefore;

		auto operator<=>(const github_capability_cache_entry&) const = default;
	};

	enum class github_capability_cache_error : std::uint8_t {
		none,
		invalid_cache,
		invalid_policy,
		allocation_failure,
		invalid_authority,
		invalid_probe,
		probe_unavailable,
		invalid_epoch,
		epoch_regression,
		epoch_overflow,
		revision_overflow,
		capacity_exhausted,
		stale_stage,
	};

	enum class github_capability_cache_lookup_status : std::uint8_t {
		hit,
		miss,
		expired,
		rejected,
	};

	/// \brief Describes the github capability cache lookup result record.
	struct github_capability_cache_lookup_result
	{
		github_capability_cache_lookup_status        Status = github_capability_cache_lookup_status::rejected;
		github_capability_cache_error                Error  = github_capability_cache_error::none;
		std::optional<github_capability_cache_entry> Entry;

		[[nodiscard]] auto hit() const noexcept -> bool
		{
			return Status == github_capability_cache_lookup_status::hit
				&& Error == github_capability_cache_error::none
				&& Entry.has_value();
		}
	};

	enum class github_capability_cache_store_disposition : std::uint8_t {
		inserted,
		replaced,
		reused_expired,
		rejected,
	};

	/// \brief Provides the github capability cache stage contract.
	class github_capability_cache_stage final
	{
	public:
		[[nodiscard]] auto entry() const noexcept
			-> const github_capability_cache_entry&
		{
			return Entry;
		}

		[[nodiscard]] auto disposition() const noexcept
			-> github_capability_cache_store_disposition
		{
			return Disposition;
		}

	private:
		friend class github_capability_cache;

		github_capability_cache_stage(
			github_capability_cache_entry             aEntry,
			github_capability_cache_store_disposition aDisposition,
			std::uint64_t                             aMutationGeneration) noexcept
			: Entry{ std::move(aEntry) }
			, Disposition{ aDisposition }
			, MutationGeneration{ aMutationGeneration }
		{
		}

		github_capability_cache_entry             Entry;
		github_capability_cache_store_disposition Disposition        = github_capability_cache_store_disposition::rejected;
		std::uint64_t                             MutationGeneration = 0U;
	};

	/// \brief Describes the github capability cache stage result record.
	struct github_capability_cache_stage_result
	{
		github_capability_cache_error                Error = github_capability_cache_error::none;
		std::optional<github_capability_cache_stage> Stage;

		[[nodiscard]] auto staged() const noexcept -> bool
		{
			return Error == github_capability_cache_error::none
				&& Stage.has_value();
		}
	};

	/// \brief Describes the github capability cache store result record.
	struct github_capability_cache_store_result
	{
		github_capability_cache_store_disposition    Disposition = github_capability_cache_store_disposition::rejected;
		github_capability_cache_error                Error       = github_capability_cache_error::none;
		std::optional<github_capability_cache_entry> Entry;

		[[nodiscard]] auto stored() const noexcept -> bool
		{
			if (
				Error != github_capability_cache_error::none
				|| !Entry.has_value()) {
				return false;
			}
			switch (Disposition) {
			case github_capability_cache_store_disposition::inserted:
			case github_capability_cache_store_disposition::replaced:
			case github_capability_cache_store_disposition::reused_expired:
				return true;
			case github_capability_cache_store_disposition::rejected:
				return false;
			}
			return false;
		}
	};

	/// \brief Describes the github capability cache invalidation result record.
	struct github_capability_cache_invalidation_result
	{
		github_capability_cache_error Error   = github_capability_cache_error::none;
		std::size_t                   Removed = 0U;

		[[nodiscard]] auto accepted() const noexcept -> bool
		{
			return Error == github_capability_cache_error::none;
		}
	};

	/// \brief Provides the github capability cache contract.
	class github_capability_cache final
	{
	public:
		[[nodiscard]] static auto create(
			github_capability_cache_policy aPolicy) noexcept
			-> std::expected<
				github_capability_cache,
				github_capability_cache_error>;

		github_capability_cache(const github_capability_cache&) = delete;
		auto operator=(const github_capability_cache&)
			-> github_capability_cache& = delete;
		github_capability_cache(github_capability_cache&& aOther) noexcept;
		auto operator=(github_capability_cache&&)
			-> github_capability_cache& = delete;

		[[nodiscard]] auto lookup(
			const github_capability_authority_key& aAuthority,
			github_freshness_epoch                 aNow) noexcept
			-> github_capability_cache_lookup_result;
		[[nodiscard]] auto store(
			const github_capability_probe_result& aResult) noexcept
			-> github_capability_cache_store_result;
		[[nodiscard]] auto stage(
			const github_capability_probe_result& aResult) noexcept
			-> github_capability_cache_stage_result;
		[[nodiscard]] auto commit(
			const github_capability_cache_stage& aStage) noexcept
			-> github_capability_cache_store_result;

		[[nodiscard]] auto invalidate_exact(
			const github_capability_authority_key& aAuthority) noexcept
			-> github_capability_cache_invalidation_result;
		[[nodiscard]] auto invalidate_profile(
			core::host_profile_id aProfile) noexcept
			-> github_capability_cache_invalidation_result;
		[[nodiscard]] auto invalidate_identity(
			core::host_profile_id   aProfile,
			core::git_host_id       aHost,
			core::github_account_id aAccount) noexcept
			-> github_capability_cache_invalidation_result;
		[[nodiscard]] auto clear() noexcept -> std::size_t;

		[[nodiscard]] auto size() const noexcept -> std::size_t;
		[[nodiscard]] auto capacity() const noexcept -> std::size_t;
		[[nodiscard]] auto last_epoch() const noexcept
			-> github_freshness_epoch;

	private:
		explicit github_capability_cache(
			github_capability_cache_policy aPolicy) noexcept;

		github_capability_cache_policy             Policy;
		std::vector<github_capability_cache_entry> Entries;
		github_freshness_epoch                     LastEpoch;
		std::uint64_t                              NextRevision       = 1U;
		std::uint64_t                              MutationGeneration = 1U;
		bool                                       RevisionExhausted  = false;
		bool                                       Usable             = true;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_CAPABILITY_CACHE_HPP
