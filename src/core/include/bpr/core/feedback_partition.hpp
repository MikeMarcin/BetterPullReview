#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_PARTITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_PARTITION_HPP

#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/feedback_conversation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Conversation accounting for label counters: who is counted, in
/// which bucket, and how complete the count is (RFC-BPR-0049 R21-R26).
/// \details One logical conversation is one unit. Its bucket comes from its
/// root post's classification in the annotation set and nothing else. For a
/// selected list of dedicated labels, every included unit lands in exactly
/// one dedicated bucket or in the catch-all, so the dedicated counts plus the
/// catch-all always equal the included population. Resolution is a separate
/// fact with four states, and every count carries the coverage limits that
/// keep an incomplete or unknown population from reading as a clean zero.
///
/// Counting is two steps. A tally reads the population once per scope into
/// fixed label-by-state rows; a projection then answers any column choice and
/// resolution mode from the tally alone, without the units or their bodies.
namespace bpr::core {
	/// \brief A conversation's native thread-resolution state, as the provider
	/// reported it. These are four disjoint facts; none is a default.
	enum class feedback_resolution : std::uint8_t {
		/// The provider positively reports an unresolved native review thread.
		unresolved,
		/// The provider positively reports the thread resolved.
		resolved,
		/// The conversation could have a resolution, but none is established,
		/// such as an anonymous REST comment group.
		unknown,
		/// The surface has no native resolution: a top-level comment or a
		/// review summary.
		not_applicable,
	};
	inline constexpr std::size_t feedback_resolution_count = 4U;
	[[nodiscard]] auto           stable_name(feedback_resolution aResolution) noexcept -> std::string_view;

	/// \brief Which resolution states a count includes.
	enum class feedback_resolution_mode : std::uint8_t {
		/// Only positively unresolved units: the initial primary mode.
		unresolved,
		/// The whole admitted population, with its state breakdown kept.
		all,
	};
	inline constexpr auto default_feedback_resolution_mode = feedback_resolution_mode::unresolved;
	[[nodiscard]] auto    stable_name(feedback_resolution_mode aMode) noexcept -> std::string_view;

	/// \brief Whether a unit is shared review content.
	/// \details Only `published` units are counted. Private notes, unsent
	/// drafts and pending publication attempts keep their own surfaces.
	enum class feedback_unit_origin : std::uint8_t {
		published,
		private_note,
		draft,
		pending_publication,
	};
	[[nodiscard]] auto stable_name(feedback_unit_origin aOrigin) noexcept -> std::string_view;

	/// \brief How exactly a unit is attributed to a file.
	enum class feedback_anchor_status : std::uint8_t {
		/// Bound to one file by exact current or retained identity; an
		/// outdated anchor still counts for that file.
		file,
		/// Genuinely not file-bound: a top-level comment, a review summary or
		/// a native thread without a path. Counted only pull-request wide.
		unanchored,
		/// File-bound, but the exact file could not be established, such as a
		/// lost anchor or an uncertain rename. Counted only pull-request wide,
		/// and it leaves every file and folder count incomplete.
		unknown,
	};
	[[nodiscard]] auto stable_name(feedback_anchor_status aStatus) noexcept -> std::string_view;

	struct feedback_anchor
	{
		feedback_anchor_status Status = feedback_anchor_status::unanchored;
		/// Repository-relative, `/`-separated; set exactly when `file`.
		std::optional<std::string> Path                                             = std::nullopt;
		bool                       Outdated                                         = false;
		auto                       operator==(const feedback_anchor&) const -> bool = default;
	};

	/// \brief One observation of a conversation's non-textual facts from the
	/// accepted read model. `Id` is the conversation identity the annotation
	/// set uses.
	struct feedback_conversation_facts
	{
		std::string          Id;
		feedback_unit_origin Origin = feedback_unit_origin::published;
		/// The whole conversation is gone. A deleted root in a surviving
		/// thread is not this; it is a root status.
		bool                Deleted    = false;
		feedback_resolution Resolution = feedback_resolution::unknown;
		feedback_anchor     Anchor;
		auto                operator==(const feedback_conversation_facts&) const -> bool = default;
	};

	/// \brief One admitted, counted conversation.
	struct feedback_counter_unit
	{
		std::string Id;
		/// The root classification from the annotation set. `Label` is set
		/// exactly when `labelled`.
		feedback_root_status          Root       = feedback_root_status::root_missing;
		std::optional<feedback_label> Label      = std::nullopt;
		feedback_resolution           Resolution = feedback_resolution::unknown;
		feedback_anchor               Anchor;
	};

	/// \brief How much of the provider population the observation covers.
	enum class feedback_collection : std::uint8_t {
		complete,
		/// Some pages, bodies or threads were not collected: paging stopped,
		/// a rate limit hit, a body is missing.
		partial,
		/// A retained earlier snapshot, not a fresh observation.
		stale,
		/// Nothing could be read, such as for lack of permission.
		unavailable,
	};
	[[nodiscard]] auto stable_name(feedback_collection aCollection) noexcept -> std::string_view;

	/// \brief Observed units left out of the shared population, by reason.
	struct feedback_population_exclusions
	{
		std::size_t Private     = 0U;
		std::size_t Unpublished = 0U;
		std::size_t Deleted     = 0U;
		/// Identical repeated observations of a unit already counted.
		std::size_t Collapsed                                                       = 0U;
		auto        operator==(const feedback_population_exclusions&) const -> bool = default;
	};

	/// \brief The shared population of one review scope at one accepted
	/// annotation epoch. `Units` is ordered by identity, each once.
	struct feedback_population
	{
		cached_review_key                  Scope;
		std::uint64_t                      Epoch      = 0U;
		feedback_collection                Collection = feedback_collection::complete;
		std::vector<feedback_counter_unit> Units;
		feedback_population_exclusions     Excluded;
	};

	enum class feedback_population_refusal : std::uint8_t {
		none,
		/// The annotation set has no valid scope.
		invalid_scope,
		/// An empty conversation identity.
		invalid_identity,
		/// A `file` anchor without a clean relative path, or a path on
		/// another anchor status.
		invalid_anchor,
		/// One conversation observed twice with different facts.
		contradictory_duplicate,
	};
	[[nodiscard]] auto stable_name(feedback_population_refusal aRefusal) noexcept -> std::string_view;

	struct feedback_population_result
	{
		feedback_population_refusal Refusal = feedback_population_refusal::none;
		feedback_population         Population;
	};

	/// \brief Admit the shared population from the read model's facts and
	/// the annotation set's root classification.
	/// \details Identical repeated facts collapse; differing ones refuse the
	/// whole population. Private, unpublished and deleted conversations are
	/// left out and tallied. A conversation the set does not classify, because
	/// none of its posts was observed, is `root_missing`. The population takes
	/// the set's scope and epoch, so a count names the exact accepted state
	/// it was made from.
	[[nodiscard]] auto build_feedback_population(const feedback_annotation_set& aAnnotations, std::span<const feedback_conversation_facts> aFacts,
												 feedback_collection aCollection) -> feedback_population_result;

	enum class feedback_counter_scope_kind : std::uint8_t {
		pull_request,
		file,
		/// Files at or below `Path`; an empty path is every file.
		folder,
		/// [[BPR-1089]]. Files directly in `Path`, not in a folder below it;
		/// an empty path is the files at the repository root. The Overview's
		/// flat folder row lists exactly these files.
		directory,
		/// [[BPR-1089]]. Units with no exact file: unanchored threads, pull
		/// request comments, review summaries and units whose file is unknown.
		/// The Overview's "Not on a file" row. `Path` is empty.
		unattributed,
	};

	/// \brief Which units one count covers.
	struct feedback_counter_scope
	{
		feedback_counter_scope_kind Kind = feedback_counter_scope_kind::pull_request;
		std::string                 Path;
		auto                        operator==(const feedback_counter_scope&) const -> bool = default;
	};

	/// \brief Whether a unit belongs to a scope. Only exact file attribution
	/// places a unit in a file or folder; the pull request holds every unit.
	[[nodiscard]] auto in_feedback_counter_scope(const feedback_counter_unit& aUnit, const feedback_counter_scope& aScope) -> bool;

	/// \brief Units counted per resolution state.
	using feedback_state_counts = std::array<std::size_t, feedback_resolution_count>;

	/// \brief One scope's units, read once into fixed rows: one per catalog
	/// label and three catch-all rows, each split by resolution state.
	/// \details `Plain` and `Custom` are known catch-all roots. `Unclassified`
	/// is a root whose label is not known (malformed, over budget, missing,
	/// deleted or ambiguous); it is never folded into `Plain`.
	struct feedback_counter_tally
	{
		bool                                                    Valid = false;
		cached_review_key                                       Scope;
		std::uint64_t                                           Epoch      = 0U;
		feedback_collection                                     Collection = feedback_collection::complete;
		feedback_counter_scope                                  Where;
		std::array<feedback_state_counts, feedback_label_count> Labelled{ };
		feedback_state_counts                                   Plain{ };
		feedback_state_counts                                   Custom{ };
		feedback_state_counts                                   Unclassified{ };
		/// Units in this scope with no exact file. Only the pull request and
		/// the unattributed scope have them; it is the remainder file rows
		/// cannot show.
		feedback_state_counts Unattributed{ };
		/// The population holds a unit whose file is unknown, so a file,
		/// folder or directory count may be missing it.
		bool AttributionUnknown = false;
	};

	/// \brief Read a population once for one scope. An invalid scope path
	/// (leading, trailing or doubled `/`, or an empty file path) gives a tally
	/// that is not `Valid`.
	[[nodiscard]] auto tally_feedback_counters(const feedback_population& aPopulation, const feedback_counter_scope& aScope) -> feedback_counter_tally;

	/// \brief A reason a count is not an exact, complete answer.
	enum class feedback_count_limit : std::uint8_t {
		collection_partial,
		collection_stale,
		collection_unavailable,
		/// Some root in the scope has no known label, so a label's count may
		/// be missing units.
		classification_partial,
		/// In unresolved mode, some unit's resolution is unknown, so the
		/// unresolved counts may be missing units.
		resolution_unknown,
		/// For a file or folder, some unit's file is unknown.
		attribution_unknown,
	};
	[[nodiscard]] auto stable_name(feedback_count_limit aLimit) noexcept -> std::string_view;

	struct feedback_dedicated_count
	{
		feedback_label Label;
		std::size_t    Count = 0U;
		/// [[BPR-1089]]. The label's units in the scope by resolution state, in
		/// every mode, so a count shows its breakdown.
		feedback_state_counts States{ };
		auto                  operator==(const feedback_dedicated_count&) const -> bool = default;
	};

	/// \brief What the catch-all holds, by reason.
	struct feedback_catchall_parts
	{
		/// Catalog labels without a dedicated column.
		std::size_t Unselected = 0U;
		std::size_t Plain      = 0U;
		std::size_t Custom     = 0U;
		/// Roots whose label is not known.
		std::size_t Unclassified                                             = 0U;
		auto        operator==(const feedback_catchall_parts&) const -> bool = default;
	};

	enum class feedback_projection_refusal : std::uint8_t {
		none,
		/// The tally is not valid.
		invalid_tally,
		/// A label selected twice.
		duplicate_column,
	};
	[[nodiscard]] auto stable_name(feedback_projection_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The counts for one column choice and resolution mode.
	/// \details `Dedicated` follows the selected order. The included units
	/// are those whose state the mode includes; their dedicated counts plus
	/// `Catchall` equal `Included`. `States` and `ScopeTotal` cover the whole
	/// scope in every mode, so units a mode leaves out stay visible. `Exact`
	/// holds only when `Limits` is empty.
	struct feedback_counter_projection
	{
		feedback_projection_refusal           Refusal = feedback_projection_refusal::none;
		cached_review_key                     Scope;
		std::uint64_t                         Epoch = 0U;
		feedback_counter_scope                Where;
		feedback_resolution_mode              Mode = default_feedback_resolution_mode;
		std::vector<feedback_dedicated_count> Dedicated;
		std::size_t                           Catchall = 0U;
		/// [[BPR-1089]]. The catch-all's units by resolution state, in every
		/// mode.
		feedback_state_counts   CatchallStates{ };
		feedback_catchall_parts CatchallParts;
		std::size_t             Included = 0U;
		/// Included units with no exact file.
		std::size_t           Unattributed = 0U;
		std::size_t           ScopeTotal   = 0U;
		feedback_state_counts States{ };
		/// Units in the scope, of any state, whose root label is not known.
		std::size_t ScopeUnclassified = 0U;
		/// [[BPR-1089]]. Units in the scope, in every mode, that the Open
		/// count leaves out or cannot classify: resolution unknown or not
		/// applicable, or root label not known. Each unit once.
		std::size_t                       Uncertain = 0U;
		std::vector<feedback_count_limit> Limits;
		bool                              Exact = false;
	};

	/// \brief Partition a tally for one ordered column choice and mode.
	[[nodiscard]] auto project_feedback_counters(const feedback_counter_tally& aTally, std::span<const feedback_label> aColumns,
												 feedback_resolution_mode aMode) -> feedback_counter_projection;

	/// \brief [[BPR-1089]]. One tally from the tallies of disjoint scopes that
	/// together make \p aWhere, such as the files of a directory.
	/// \details The rows are added. The result is valid when \p aWhere is a
	/// valid scope and every part is valid and names the same pull request,
	/// epoch and coverage; at least one part is needed to name them. A file
	/// whose units are unknown limits the result if it limits any part. The
	/// caller vouches that the parts are disjoint and cover \p aWhere; the
	/// core cannot see the units behind a tally.
	[[nodiscard]] auto merge_feedback_counter_tallies(std::span<const feedback_counter_tally> aParts, const feedback_counter_scope& aWhere)
		-> feedback_counter_tally;

	/// \brief [[BPR-1089]]. Which of a projection's numbers is meant: the
	/// one a chip shows, a drill-down lists and a sort orders by.
	enum class feedback_count_kind : std::uint8_t {
		/// Every unit the mode includes.
		included,
		/// One dedicated label's units the mode includes.
		dedicated,
		/// The catch-all's units the mode includes.
		catchall,
		/// The `Uncertain` units, in every mode.
		uncertain,
	};
	[[nodiscard]] auto stable_name(feedback_count_kind aKind) noexcept -> std::string_view;

	/// \brief One number of a projection. `Label` is set exactly when
	/// `dedicated`.
	struct feedback_count_target
	{
		feedback_count_kind           Kind                                                   = feedback_count_kind::included;
		std::optional<feedback_label> Label                                                  = std::nullopt;
		auto                          operator==(const feedback_count_target&) const -> bool = default;
	};

	/// \brief The number a projection gives for one target; zero for a label
	/// the projection has no column for.
	[[nodiscard]] auto feedback_count_of(const feedback_counter_projection& aProjection, const feedback_count_target& aTarget) noexcept -> std::size_t;

	enum class feedback_selection_refusal : std::uint8_t {
		none,
		/// The scope path is not valid for its kind.
		invalid_scope,
		/// A label selected twice.
		duplicate_column,
		/// A dedicated target without a label, or with one that has no column.
		unselected_label,
	};
	[[nodiscard]] auto stable_name(feedback_selection_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The conversations one number counts.
	struct feedback_unit_selection
	{
		feedback_selection_refusal Refusal = feedback_selection_refusal::none;
		cached_review_key          Scope;
		std::uint64_t              Epoch = 0U;
		/// Conversation identities in identity order, each once.
		std::vector<std::string> Ids;
	};

	/// \brief [[BPR-1089]]. The exact conversations behind one number: a
	/// drill-down lists what the count counted (RFC-BPR-0049 R27).
	/// \details It reads the population with the scope, column choice and
	/// mode the projection was made with, and the same rules the tally and
	/// projection apply, so its size is `feedback_count_of` for the projection
	/// of `tally_feedback_counters(aPopulation, aWhere)` with these columns and
	/// this mode, and the population's epoch names the accepted state.
	[[nodiscard]] auto select_feedback_units(const feedback_population& aPopulation, const feedback_counter_scope& aWhere, std::span<const feedback_label> aColumns,
											 feedback_resolution_mode aMode, const feedback_count_target& aTarget) -> feedback_unit_selection;

	/// \brief [[BPR-1089]]. How a scope sorts by one number: the number, and
	/// whether it is qualified (the projection is not exact).
	struct feedback_sort_key
	{
		std::size_t Count                                              = 0U;
		bool        Qualified                                          = false;
		auto        operator==(const feedback_sort_key&) const -> bool = default;
	};
	[[nodiscard]] auto feedback_sort_key_of(const feedback_counter_projection& aProjection, const feedback_count_target& aTarget) noexcept -> feedback_sort_key;
	/// \brief Whether \p aLeft sorts before \p aRight: a larger number first;
	/// at an equal number, a qualified one first, since its true value may be
	/// larger. Numbers, never rendered text, decide; equal keys are left to
	/// the caller's own order.
	[[nodiscard]] auto feedback_sorts_before(const feedback_sort_key& aLeft, const feedback_sort_key& aRight) noexcept -> bool;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_PARTITION_HPP
