#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_COMPONENTS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_COMPONENTS_HPP

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/// \file
/// \brief The diff engine's evaluation seam ([[RFC-BPR-0082]], [[BPR-1465]]).
///
/// \details A candidate algorithm supplies the primary line matches, a
/// post-pass over them, the pair scores of a replacement or the matched
/// scalars of a similar pair. Everything else stays the engine's: decoding,
/// line splitting and normalization, pairing policy, move detection,
/// identities, geometry, budgets and validation. The engine validates every
/// supplied value before it assembles anything and refuses a comparison whose
/// supplied values fail, so a candidate never publishes an invalid alignment.
///
/// The existing `compute_file_diff` and `compute_material_diff` overloads do
/// not consult this seam. Whether it ships beyond the evaluation is decided by
/// adoption, not by this header.
namespace bpr::core {
	/// \brief One matched line, or one matched scalar of a line pair: an index
	/// on each side.
	struct diff_match
	{
		std::uint32_t Left  = 0U;
		std::uint32_t Right = 0U;

		auto operator<=>(const diff_match&) const = default;
	};

	/// \brief Counts a component's work in the engine's steps.
	///
	/// \details `tick` polls the caller's stop predicate as the engine's own
	/// loops do. When the caller asked to stop it does not return: it unwinds
	/// the component by an exception the engine catches, so a component must
	/// hold its state in objects that clean up after themselves. A component
	/// call is one restart unit: a stopped computation calls it again when
	/// resumed.
	class diff_component_meter
	{
	public:
		diff_component_meter()                                               = default;
		diff_component_meter(const diff_component_meter&)                    = delete;
		diff_component_meter(diff_component_meter&&)                         = delete;
		auto operator=(const diff_component_meter&) -> diff_component_meter& = delete;
		auto operator=(diff_component_meter&&) -> diff_component_meter&      = delete;

		virtual void tick(std::uint64_t aSteps) = 0;

	protected:
		~diff_component_meter() = default;
	};

	/// \brief The two sides a primary component matches.
	struct diff_primary_request
	{
		/// The decoded sides; a side the comparison does not have is empty.
		const decoded_text_document* Left  = nullptr;
		const decoded_text_document* Right = nullptr;
		/// One class per line: two lines share a class exactly when their
		/// normalized forms, under `Options`, are equal.
		std::span<const std::uint32_t> LeftClasses;
		std::span<const std::uint32_t> RightClasses;
		diff_options                   Options;
	};

	/// \brief The lines of one replacement a pairing component scores.
	struct diff_pairing_request
	{
		/// Each line's normalized scalars, line-ending marker included.
		std::span<const std::span<const std::uint32_t>> Left;
		std::span<const std::span<const std::uint32_t>> Right;
		/// Scores below this are never used, so their values need not be exact.
		std::uint16_t ThresholdPermille = 0U;
	};

	/// \brief Which members of a `diff_components` the engine calls.
	struct diff_component_parts
	{
		bool Primary   = false;
		bool Slide     = false;
		bool Pairing   = false;
		bool Intraline = false;

		auto operator<=>(const diff_component_parts&) const = default;
	};

	/// \brief Candidate algorithms for parts of one computation.
	///
	/// \details The engine calls only the members `parts` names. Each member
	/// returns `std::nullopt` to decline, which keeps the engine's own
	/// algorithm for that part. The engine calls a component from the thread
	/// computing the diff; one object shared by several computations must be
	/// safe to call from several threads.
	class diff_components
	{
	public:
		diff_components()                                          = default;
		diff_components(const diff_components&)                    = default;
		diff_components(diff_components&&)                         = default;
		auto operator=(const diff_components&) -> diff_components& = default;
		auto operator=(diff_components&&) -> diff_components&      = default;
		virtual ~diff_components()                                 = default;

		/// \brief The members this object supplies; none by default.
		[[nodiscard]] virtual auto parts() const noexcept -> diff_component_parts;

		/// \brief Monotonic matches over the whole comparison, in place of the
		/// engine's primary matcher. Each must join two lines of one class,
		/// and both indices must strictly increase.
		[[nodiscard]] virtual auto primary_matches(const diff_primary_request& aRequest, diff_component_meter& aMeter)
			-> std::optional<std::vector<diff_match>>;

		/// \brief A post-pass over the sorted primary matches, such as hunk
		/// sliding, under the same rules as `primary_matches`.
		[[nodiscard]] virtual auto slide_matches(const diff_primary_request& aRequest,
												 std::span<const diff_match> aMatches,
												 diff_component_meter&       aMeter)
			-> std::optional<std::vector<diff_match>>;

		/// \brief The similarity of every left and right line of one
		/// replacement, row by row, in per mille of `maximum_similarity_threshold_permille`.
		/// \details A pair of equal lines scores the maximum and no other pair
		/// may. The engine charges its matrix budget as its own scoring would,
		/// so a budget limit leaves the same replacements unpaired.
		[[nodiscard]] virtual auto pair_scores(const diff_pairing_request& aRequest, diff_component_meter& aMeter)
			-> std::optional<std::vector<std::uint16_t>>;

		/// \brief The matched scalars of one similar pair, by scalar index,
		/// both indices strictly increasing. The unmatched scalars between
		/// them become the pair's intraline edits.
		[[nodiscard]] virtual auto intraline_matches(std::span<const std::uint32_t> aLeft,
													 std::span<const std::uint32_t> aRight,
													 diff_component_meter&          aMeter)
			-> std::optional<std::vector<diff_match>>;
	};

	/// \brief Primary matches observed outside the core, such as the hunks of
	/// an installed Git, checked by the same rules as
	/// `diff_components::primary_matches`.
	struct diff_primary_observation
	{
		std::vector<diff_match> Matches;
	};

	/// \brief `compute_file_diff` with candidate components.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput, diff_components& aComponents)
		-> diff_compute_outcome;
	/// \brief One stoppable slice of `compute_file_diff` with candidate
	/// components. A paused state resumes only with the same components.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput, const diff_compute_control& aControl,
										 diff_components& aComponents, paused_diff_compute aResume = { })
		-> file_diff_compute_slice;
	/// \brief `compute_file_diff` with observed primary matches.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput, const diff_primary_observation& aObservation)
		-> diff_compute_outcome;

	/// \brief `compute_material_diff` with candidate components.
	[[nodiscard]] auto compute_material_diff(const material_diff_input& aInput, diff_components& aComponents)
		-> material_diff_outcome;
	/// \brief One stoppable slice of `compute_material_diff` with candidate
	/// components.
	[[nodiscard]] auto compute_material_diff(const material_diff_input& aInput, const diff_compute_control& aControl,
											 diff_components& aComponents, paused_diff_compute aResume = { })
		-> material_diff_compute_slice;
	/// \brief `compute_material_diff` with observed primary matches.
	[[nodiscard]] auto compute_material_diff(const material_diff_input&      aInput,
											 const diff_primary_observation& aObservation) -> material_diff_outcome;

#if defined(BPR_DIFF_ENGINE_EVALUATION)
	namespace detail {
		/// \brief The components an evaluation build gives the ordinary entry
		/// points, or null for the engine's own algorithms.
		/// \details Declared only when the evaluation option compiles the core;
		/// the evaluation target defines it. No shipping build has it.
		[[nodiscard]] auto evaluation_components() noexcept -> diff_components*;
	} // namespace detail
#endif
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_COMPONENTS_HPP
