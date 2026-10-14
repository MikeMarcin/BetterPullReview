#ifndef BPR_TESTS_EVALUATION_DIFF_ENGINE_CANDIDATES_CANDIDATES_HPP
#define BPR_TESTS_EVALUATION_DIFF_ENGINE_CANDIDATES_CANDIDATES_HPP

// [[RFC-BPR-0082]] [[BPR-1466]]. The evaluation's candidate components, by
// name. Each wraps one candidate algorithm behind the core's evaluation seam;
// the engine keeps everything else.

#include "bpr/core/diff_compute_components.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::evaluation {
	/// \brief Which part of the engine a candidate replaces.
	enum class component_kind : std::uint8_t {
		baseline,
		primary,
		slide,
		pairing,
		intraline,
	};

	/// \brief How a candidate would ship: the RFC's integration shapes.
	enum class shape_kind : std::uint8_t {
		baseline,
		process,
		linked,
		core,
	};

	/// \brief One candidate: its name, the part it replaces and its shape.
	struct candidate_description
	{
		std::string    Name;
		component_kind Component = component_kind::baseline;
		shape_kind     Shape     = shape_kind::baseline;
		/// The library or program and version it wraps.
		std::string Source;
	};

	/// \brief Counters every candidate keeps, for the report.
	struct candidate_counters
	{
		std::atomic<std::uint64_t> Calls{ 0U };
		std::atomic<std::uint64_t> Declines{ 0U };
		std::atomic<std::uint64_t> Failures{ 0U };
	};

	/// \brief A candidate's components and counters.
	class candidate_components : public core::diff_components
	{
	public:
		[[nodiscard]] auto counters() noexcept -> candidate_counters&
		{
			return Counters;
		}

	private:
		candidate_counters Counters;
	};

	/// \brief Every candidate the evaluation measures, baseline first.
	[[nodiscard]] auto candidate_descriptions() -> const std::vector<candidate_description>&;

	/// \brief The description of one candidate, if it exists.
	[[nodiscard]] auto find_candidate(std::string_view aName) -> std::optional<candidate_description>;

	/// \brief New components for a candidate; null for the baseline or an
	/// unknown name.
	[[nodiscard]] auto make_candidate(std::string_view aName) -> std::unique_ptr<candidate_components>;

	/// \brief The Git programs the process shape can run.
	struct git_program
	{
		std::string Version;
		std::string Path;
	};

	[[nodiscard]] auto git_programs() -> std::vector<git_program>;

	/// \brief The Git diff algorithms the process and linked shapes run.
	enum class git_algorithm : std::uint8_t {
		myers,
		minimal,
		patience,
		histogram,
	};

	[[nodiscard]] auto name(git_algorithm aAlgorithm) noexcept -> std::string_view;

	/// \brief One hunk of a zero-context diff, zero-based: a change replaces
	/// left lines [OldBegin, OldBegin + OldCount) with right lines
	/// [NewBegin, NewBegin + NewCount).
	struct line_hunk
	{
		std::uint32_t OldBegin = 0U;
		std::uint32_t OldCount = 0U;
		std::uint32_t NewBegin = 0U;
		std::uint32_t NewCount = 0U;

		auto operator<=>(const line_hunk&) const = default;
	};

	/// \brief The matched lines between and around zero-context hunks, or
	/// nothing when the hunks do not tile both sides.
	[[nodiscard]] auto matches_from_hunks(std::span<const line_hunk> aHunks, std::uint32_t aLeftLines, std::uint32_t aRightLines)
		-> std::optional<std::vector<core::diff_match>>;

	/// \brief The hunks of a `git diff -U0` output: each `@@ -a,b +c,d @@`
	/// header, converted to zero-based ranges.
	[[nodiscard]] auto parse_zero_context_hunks(std::string_view aOutput) -> std::optional<std::vector<line_hunk>>;

	/// \brief The `git diff` options of one process run: the algorithm, the
	/// indent heuristic and the whitespace flags mapped from the options.
	[[nodiscard]] auto git_diff_arguments(git_algorithm aAlgorithm, bool aIndentHeuristic, const core::diff_options& aOptions)
		-> std::vector<std::string>;

	/// \brief Runs `git diff --no-index` over two temporary files, outside any
	/// repository and with no user or system configuration. Returns the
	/// standard output, or nothing when Git fails.
	[[nodiscard]] auto run_git_no_index(const std::string& aGit, std::span<const std::uint8_t> aLeft, std::span<const std::uint8_t> aRight,
										const std::vector<std::string>& aArguments) -> std::optional<std::string>;

	/// \brief The linked shape: Git 2.56.0's xdiff, a shared library, over the
	/// two sides; under any option other than exact comparison each line is
	/// first normalized as the engine normalizes it.
	[[nodiscard]] auto xdiff_hunks(const core::decoded_text_document* aLeft, const core::decoded_text_document* aRight, git_algorithm aAlgorithm,
								   bool aIndentHeuristic, const core::diff_options& aOptions) -> std::optional<std::vector<line_hunk>>;

	/// \brief O(NP) (dtl) matches over two class sequences, minimal.
	[[nodiscard]] auto onp_matches(std::span<const std::uint32_t> aLeft, std::span<const std::uint32_t> aRight, core::diff_component_meter* aMeter)
		-> std::vector<core::diff_match>;

	/// \brief The first-party histogram matcher over two class sequences.
	[[nodiscard]] auto histogram_matches(std::span<const std::uint32_t> aLeft, std::span<const std::uint32_t> aRight, core::diff_component_meter* aMeter)
		-> std::vector<core::diff_match>;

	/// \brief The first-party sliding post-pass.
	[[nodiscard]] auto slide_matches(const core::diff_primary_request& aRequest, std::span<const core::diff_match> aMatches, core::diff_component_meter* aMeter)
		-> std::vector<core::diff_match>;

	/// \brief rapidfuzz-cpp Indel scores of one replacement, row by row.
	[[nodiscard]] auto rapidfuzz_scores(const core::diff_pairing_request& aRequest, core::diff_component_meter* aMeter) -> std::vector<std::uint16_t>;

	/// \brief diff-match-patch with semantic cleanup over two scalar lines.
	[[nodiscard]] auto dmp_matches(std::span<const std::uint32_t> aLeft, std::span<const std::uint32_t> aRight) -> std::vector<core::diff_match>;
} // namespace bpr::evaluation

#endif // BPR_TESTS_EVALUATION_DIFF_ENGINE_CANDIDATES_CANDIDATES_HPP
