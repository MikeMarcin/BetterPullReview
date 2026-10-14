// [[BPR-1481]] Intraline ranges come from dtl's common scalars of each similar
// pair (RFC-BPR-0082). They are counted through the engine's step meter, not
// charged to the matrix budget, which pairing then has to itself; a pair too
// large for dtl keeps the budgeted table, and a stop inside dtl abandons the
// computation like any other.

#include "bpr/core/diff_compute.hpp"
#include "product_diff.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;
	using bpr::tests::product_file_diff;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto product_matching() -> bpr::application::diff_line_matching&
	{
		static auto matching = bpr::application::diff_line_matching{ *bpr::tests::product_line_matcher() };
		return matching;
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const diff_options& aOptions) -> file_comparison_key
	{
		const auto scope       = opaque<pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
		const auto authority   = pull_request_authority{
			.Profile         = host_profile_id{ 10U },
			.ProfileRevision = github_profile_revision{ 20U },
			.Host            = git_host_id{ 30U },
			.HostAuthority   = opaque<host_authority_fingerprint>(13U),
			.Account         = github_account_id{ 40U },
			.Viewer          = github_viewer_actor_id{ 50U },
			.Repository      = repository_id{ 101U },
			.Cache           = opaque<repository_cache_scope_id>(14U),
			.PullRequest     = pull_request_id{ 202U },
			.Number          = pull_request_number{ 17U },
			.Scope           = scope,
		};
		return file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = { .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_base, .Epoch = epoch_ordinal{ 1 } },
			.RightAnchor = { .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_head, .Epoch = epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = opaque<file_identity>(31U),
			.Options     = *options_digest(aOptions),
			.Algorithm   = diff_algorithm_revision{ 1U },
		};
	}

	/// Two texts and the input that compares them; the input points into it.
	struct comparison
	{
		std::vector<std::uint8_t> Left;
		std::vector<std::uint8_t> Right;
		diff_compute_limits       Limits;

		[[nodiscard]] auto input() const -> diff_compute_input
		{
			return {
				.Key        = comparison_key({ }),
				.Options    = { },
				.LeftBytes  = std::span<const std::uint8_t>{ Left },
				.RightBytes = std::span<const std::uint8_t>{ Right },
				.Limits     = Limits,
			};
		}
	};

	[[nodiscard]] auto bytes(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	/// One line of aLength letters; every aStride-th letter differs when
	/// aEdited is set, so the two versions pair as similar.
	[[nodiscard]] auto long_line(const std::size_t aLength, const std::size_t aStride, const bool aEdited) -> std::string
	{
		auto line = std::string{ };
		for (std::size_t index = 0U; index < aLength; ++index) {
			const auto letter = static_cast<char>('a' + static_cast<char>(index % 23U));
			line.push_back(aEdited && index % aStride == aStride / 2U ? 'Z' : letter);
		}
		line.push_back('\n');
		return line;
	}

	[[nodiscard]] auto computed(const diff_compute_outcome& aOutcome, const std::string_view aMessage) -> const computed_file_diff&
	{
		expect(aOutcome.Reason == diff_reason_code::none && aOutcome.Result.has_value(), aMessage);
		expect(validate_computed_file_diff(*aOutcome.Result).valid(), aMessage);
		return *aOutcome.Result;
	}

	[[nodiscard]] auto all_similar(const computed_file_diff& aDiff) -> bool
	{
		for (const auto& pair : aDiff.Alignment.Pairs) {
			if (pair.Kind != line_pair_kind::similar) {
				return false;
			}
		}
		return !aDiff.Alignment.Pairs.empty();
	}

	/// Asks for a stop once the computation has counted aStep steps.
	struct stop_at_step final : diff_compute_stop
	{
		explicit stop_at_step(const std::uint64_t aStep) noexcept
			: Step{ aStep }
		{
		}

		[[nodiscard]] auto stop_requested(const std::uint64_t aSteps) -> bool override
		{
			if (aSteps >= Step && !StoppedAt.has_value()) {
				StoppedAt = aSteps;
			}
			return StoppedAt.has_value();
		}

		std::uint64_t                Step = 0U;
		std::optional<std::uint64_t> StoppedAt;
	};

	auto test_intraline_ranges_are_dtl_common_scalars() -> void
	{
		auto       pair = comparison{ .Left = bytes("prefix alpha suffix\n"), .Right = bytes("prefix beta suffix\n") };
		const auto diff = computed(product_file_diff(pair.input()), "a similar pair computes");
		expect(diff.Alignment.Pairs.size() == 1U && diff.Alignment.Pairs.front().Kind == line_pair_kind::similar, "the two lines pair as similar");
		// A shortest edit keeps "prefix ", one "a" and " suffix".
		auto spans = std::vector<std::pair<std::string, std::string>>{ };
		for (const auto& edit : diff.Intraline) {
			const auto take = [](const std::vector<std::uint8_t>& aText, const byte_span aSpan) {
				return std::string{ aText.begin() + static_cast<std::ptrdiff_t>(aSpan.Begin), aText.begin() + static_cast<std::ptrdiff_t>(aSpan.End) };
			};
			spans.emplace_back(take(pair.Left, edit.Left), take(pair.Right, edit.Right));
		}
		auto removed = std::string{ };
		auto added   = std::string{ };
		for (const auto& [left, right] : spans) {
			removed += left;
			added += right;
		}
		expect(!spans.empty(), "[[BPR-1481]] the similar pair carries intraline ranges");
		expect(removed.size() == 4U && added.size() == 3U,
			   "[[BPR-1481]] the ranges are a shortest edit: alpha and beta share one \"a\", so four scalars go and three come");
	}

	auto test_intraline_does_not_charge_the_matrix_budget() -> void
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		for (std::size_t line = 0U; line < 20U; ++line) {
			left += long_line(80U + line, 7U, false);
			right += long_line(80U + line, 7U, true);
		}
		auto       pair    = comparison{ .Left = bytes(left), .Right = bytes(right) };
		const auto full    = computed(product_file_diff(pair.input()), "the twenty pairs compute");
		const auto pairing = full.MatrixCellsConsumed;
		expect(all_similar(full) && full.Alignment.Pairs.size() == 20U && !full.AlignmentApproximate, "every line pairs as similar");
		// Under the table the pairs' intraline grids, 81 x 81 cells and
		// up each, would have been charged on top of pairing's.
		pair.Limits.MaximumMatrixCells = pairing;
		const auto exact               = computed(product_file_diff(pair.input()), "the pairs compute in pairing's budget");
		expect(exact.MatrixCellsConsumed == pairing,
			   "[[BPR-1481]] the matrix budget counts pairing only: intraline comparison charges nothing");
		expect(!exact.AlignmentApproximate && exact.Intraline == full.Intraline && exact.Alignment == full.Alignment,
			   "[[BPR-1481]] a budget pairing exactly fills still gives every pair its intraline emphasis");
		std::cout << "BPR-1481 twenty pairs: pairing " << pairing << " matrix cells, " << full.Intraline.size() << " intraline edits\n";
	}

	auto test_a_pair_too_large_for_dtl_keeps_the_budgeted_table() -> void
	{
		// 1,500 scalars a side: a 1,501 x 1,501 grid is past dtl's bound.
		auto       large    = comparison{ .Left = bytes(long_line(1'500U, 9U, false)), .Right = bytes(long_line(1'500U, 9U, true)) };
		const auto full     = computed(product_file_diff(large.input()), "the long pair computes");
		const auto table    = std::uint64_t{ 1'501U } * 1'501U;
		const auto consumed = full.MatrixCellsConsumed;
		expect(all_similar(full) && !full.Intraline.empty() && !full.AlignmentApproximate, "the long lines pair with intraline ranges");
		expect(consumed >= table + 1U, "[[BPR-1481]] the long pair's intraline table is charged to the matrix budget, as before");
		large.Limits.MaximumMatrixCells = consumed - 1U;
		const auto tight                = computed(product_file_diff(large.input()), "the long pair computes in a tighter budget");
		expect(all_similar(tight) && tight.Intraline.empty() && tight.AlignmentApproximate,
			   "[[BPR-0719]] [[BPR-1481]] past dtl's bound an exhausted budget still omits the pass instead of refusing the file");

		// 1,000 scalars a side is inside the bound: nothing is charged.
		auto       inside = comparison{ .Left = bytes(long_line(1'000U, 9U, false)), .Right = bytes(long_line(1'000U, 9U, true)) };
		const auto within = computed(product_file_diff(inside.input()), "the shorter long pair computes");
		expect(all_similar(within) && !within.Intraline.empty() && within.MatrixCellsConsumed < std::uint64_t{ 1'001U } * 1'001U * 2U,
			   "[[BPR-1481]] inside dtl's bound the pair is compared by dtl, uncharged");
	}

	auto test_a_stop_inside_dtl_abandons_the_computation() -> void
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		for (std::size_t line = 0U; line < 3U; ++line) {
			left += long_line(1'000U + line, 5U, false);
			right += long_line(1'000U + line, 5U, true);
		}
		const auto pair  = comparison{ .Left = bytes(left), .Right = bytes(right) };
		const auto full  = compute_file_diff(pair.input(), diff_compute_control{ }, product_matching());
		const auto total = full.Statistics.Steps;
		expect(full.Outcome.has_value() && full.Outcome->Result.has_value() && all_similar(*full.Outcome->Result), "the three long pairs compute");
		const auto assemble = full.Statistics.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::assemble)];
		const auto validate = full.Statistics.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::validate)];
		expect(validate > assemble + 100'000U, "[[BPR-1481]] dtl's comparisons are counted: the assemble phase does most of the work");
		expect(full.Statistics.LargestPollDelay <= 4'096U + 1'024U,
			   "[[BPR-1481]] dtl's comparisons are counted in increments of at most 4,096");
		for (std::size_t sample = 1U; sample < 16U; ++sample) {
			const auto request = assemble + (validate - assemble) * sample / 16U;
			auto       stop    = stop_at_step{ request };
			const auto slice   = compute_file_diff(pair.input(), diff_compute_control{ .Stop = &stop, .CheckInterval = 1U }, product_matching());
			expect(!slice.Outcome.has_value() && stop.StoppedAt.has_value(), "[[BPR-1481]] a stop inside dtl abandons the computation");
			expect(*stop.StoppedAt - request <= 4'096U + 1'024U && *stop.StoppedAt < total,
				   "[[BPR-1481]] a stop inside dtl is honoured within one increment of its comparisons");
			const auto again = compute_file_diff(pair.input(), diff_compute_control{ }, product_matching());
			expect(again.Outcome.has_value() && again.Outcome->Result == full.Outcome->Result && again.Statistics == full.Statistics,
				   "[[BPR-1485]] [[BPR-1481]] computed again after a stop inside dtl, the result and steps are the uninterrupted ones");
		}
	}
} // namespace

int main()
{
	test_intraline_ranges_are_dtl_common_scalars();
	test_intraline_does_not_charge_the_matrix_budget();
	test_a_pair_too_large_for_dtl_keeps_the_budgeted_table();
	test_a_stop_inside_dtl_abandons_the_computation();
	std::cout << "BPR-1481 dtl intraline tests passed\n";
	return EXIT_SUCCESS;
}
