#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_TOUR_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_TOUR_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/profile_window.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bpr::core {
	using tour_draft_id                                       = strong_id<struct tour_draft_id_tag>;
	using tour_stop_id                                        = strong_id<struct tour_stop_id_tag>;
	inline constexpr std::size_t maximum_tour_stops           = 256U;
	inline constexpr std::size_t maximum_tour_field_bytes     = 64U * 1024U;
	inline constexpr std::size_t maximum_tour_bytes           = 4U * 1024U * 1024U;
	inline constexpr std::size_t maximum_tour_related_sources = 32U;
	inline constexpr std::size_t maximum_tour_path_bytes      = 4096U;

	/// \brief Durable owner; runtime window and request generations are separate.
	struct tour_owner
	{
		bpr_profile_id             Profile;
		git_host_id                Host;
		host_authority_fingerprint HostAuthority;
		github_account_id          Account;
		repository_id              Repository;
		pull_request_id            PullRequest;
		auto                       operator==(const tour_owner&) const -> bool = default;
	};

	/// \brief One contiguous interval in one exact source, never alignment rows.
	/// A historical or unchanged source remains representable without granting a write subject.
	struct tour_source_range
	{
		source_line_ref             First;
		source_line_ref             Last;
		git_commit_oid              Commit;
		std::optional<git_blob_oid> Blob;
		std::string                 Path;
		auto                        operator==(const tour_source_range&) const -> bool = default;
	};

	enum class tour_mapping : std::uint8_t { exact_original,
											 unchanged_material,
											 changed_material,
											 ambiguous,
											 missing,
											 unavailable };
	enum class tour_preparation : std::uint8_t { suggested,
												 editing,
												 ready,
												 needs_attention };

	/// \brief Bounded observed text retained only for explicit editing/recovery.
	struct tour_field_baseline
	{
		std::string                                        Raw;
		std::string                                        RemoteEntity;
		std::string                                        ObservedVersion;
		std::optional<std::pair<std::size_t, std::size_t>> SourceSpan;
		std::uint64_t                                      LocalGeneration                                      = 0;
		auto                                               operator==(const tour_field_baseline&) const -> bool = default;
	};

	/// \brief A local thought with immutable original source and a separate current mapping.
	struct tour_stop
	{
		tour_stop_id                       Id;
		std::string                        Title;
		std::string                        Body;
		std::optional<tour_source_range>   Original;
		std::optional<tour_source_range>   Mapped;
		tour_mapping                       Mapping = tour_mapping::unavailable;
		std::vector<tour_source_range>     Related;
		tour_preparation                   Preparation = tour_preparation::editing;
		std::optional<tour_field_baseline> Baseline;
		auto                               operator==(const tour_stop&) const -> bool = default;
	};

	/// \brief A private draft; vector order is conceptual route order, never file or title order.
	struct review_tour
	{
		tour_draft_id                      Id;
		tour_owner                         Owner;
		git_commit_oid                     Base;
		git_commit_oid                     Head;
		std::string                        Problem;
		std::string                        Solution;
		std::optional<tour_field_baseline> ProblemBaseline;
		std::optional<tour_field_baseline> SolutionBaseline;
		std::vector<tour_stop>             Stops;
		std::uint64_t                      Revision                                     = 1;
		std::uint64_t                      LastStopId                                   = 0;
		auto                               operator==(const review_tour&) const -> bool = default;
	};

	enum class tour_command_kind : std::uint8_t { problem,
												  solution,
												  insert,
												  edit_stop,
												  move,
												  duplicate,
												  split,
												  merge_next,
												  remove,
												  complete,
												  observe_head,
												  restore };
	/// \brief Exact revision command. Insert/split/duplicate allocate IDs from the draft high-water mark.
	struct tour_command
	{
		tour_draft_id              Draft;
		tour_owner                 Owner;
		std::uint64_t              ExpectedRevision = 0;
		tour_command_kind          Kind             = tour_command_kind::problem;
		tour_stop_id               Stop;
		std::string                Text;
		tour_stop                  Value;
		std::size_t                Position  = 0;
		std::size_t                SplitByte = 0;
		git_commit_oid             Head;
		std::optional<review_tour> Restore;
	};
	enum class tour_refusal : std::uint8_t { none,
											 identity,
											 conflict,
											 invalid_source,
											 invalid_command,
											 bounds,
											 incomplete,
											 exhausted };
	/// \brief Refusal preserves every byte of the original draft.
	struct tour_outcome
	{
		review_tour  Accepted;
		tour_refusal Refusal = tour_refusal::none;
		tour_stop_id Selected;
		bool         Changed = false;
	};

	[[nodiscard]] auto valid(const tour_owner& aOwner) noexcept -> bool;
	[[nodiscard]] auto valid(const tour_source_range& aRange) noexcept -> bool;
	[[nodiscard]] auto validate(const review_tour& aTour) noexcept -> tour_refusal;
	/// \brief Conservative encoded-size budget including identities, metadata and length prefixes.
	[[nodiscard]] auto retained_tour_bytes(const review_tour& aTour) noexcept -> std::size_t;
	[[nodiscard]] auto ready(const tour_stop& aStop) noexcept -> bool;
	/// \brief Applies local authoring only. No command produces publication or review receipts.
	[[nodiscard]] auto apply(const review_tour& aTour, const tour_command& aCommand) -> tour_outcome;
}
#endif
