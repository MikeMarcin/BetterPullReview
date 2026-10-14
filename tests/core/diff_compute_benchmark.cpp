// [[BPR-1366]] Times uninterrupted diff computations over large synthetic
// pairs: medians and spread for several sizes. With BPR_DIFF_BENCH_BASELINE
// defined it uses only the one-call entry point, so the same source times a
// core from before the stoppable engine; otherwise it also times the stoppable
// entry point with a predicate that never fires (the prepared path) and
// reports the longest wall time between two polls of that predicate.
// [[BPR-1480]] It also times the refine phase, the secondary pairing of each
// replacement, from a run polled every phase_check_interval steps: the time
// between the first polls at or past the phase's first step and the next
// phase's. The step counts are the engine's, so the column compares cores
// that count steps alike.
// [[BPR-1485]] The primary matches are the product's: the linked xdiff line
// matcher, called on the computing thread.
//
// Usage: bench_better_pull_review_diff_compute [repetitions] [largest line count]

#include "bpr/core/diff_compute.hpp"
#include "product_diff.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32U> bytes{ };
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
			.LeftAnchor  = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_base,
				.Epoch       = epoch_ordinal{ 1 },
			},
			.RightAnchor = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_head,
				.Epoch       = epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<file_identity>(31U),
			.Options   = *options_digest(aOptions),
			.Algorithm = diff_algorithm_revision{ 1U },
		};
	}

	/// A seeded pair of related source files: the right side edits, deletes,
	/// inserts and moves lines of the left.
	[[nodiscard]] auto synthetic_pair(const std::uint64_t aLines, const std::uint32_t aEditedPermille)
		-> std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
	{
		auto       state = std::uint64_t{ 7U * 0x9E37'79B9'7F4A'7C15ULL + 1U };
		const auto below = [&state](const std::uint64_t aBound) {
			state ^= state << 13U;
			state ^= state >> 7U;
			state ^= state << 17U;
			return state % aBound;
		};
		const auto line = [&below](const std::uint64_t aOrdinal) -> std::string {
			switch (below(12U)) {
			case 0U:
				return "";
			case 1U:
				return "    }";
			default:
				return "    auto value_" + std::to_string(aOrdinal) + " = compute(" + std::to_string(below(100'000U)) + ", item" + std::to_string(below(1'000U)) + ") + token" + std::to_string(below(50'000U)) + ";";
			}
		};
		auto left = std::vector<std::string>{ };
		for (std::uint64_t ordinal = 0U; ordinal < aLines; ++ordinal) {
			left.push_back(line(ordinal));
		}
		auto right = std::vector<std::string>{ };
		auto moved = std::vector<std::string>{ };
		for (std::uint64_t ordinal = 0U; ordinal < aLines; ++ordinal) {
			const auto roll = below(1'000U);
			if (roll < 2U && ordinal + 30U < aLines) {
				moved.insert(moved.end(), left.begin() + static_cast<std::ptrdiff_t>(ordinal), left.begin() + static_cast<std::ptrdiff_t>(ordinal + 20U));
				ordinal += 19U;
				continue;
			}
			if (roll < 12U) {
				continue;
			}
			if (roll < 22U) {
				right.push_back(line(aLines + ordinal));
			}
			if (roll >= 1'000U - aEditedPermille) {
				auto edited = left[ordinal];
				if (edited.empty()) {
					edited = "  // note";
				} else {
					edited.insert(edited.size() / 2U, "x_" + std::to_string(below(9U)));
				}
				right.push_back(edited);
			} else {
				right.push_back(left[ordinal]);
			}
			if (!moved.empty() && below(1'000U) < 5U) {
				right.insert(right.end(), moved.begin(), moved.end());
				moved.clear();
			}
		}
		right.insert(right.end(), moved.begin(), moved.end());
		const auto join = [](const std::vector<std::string>& aText) {
			auto bytes = std::vector<std::uint8_t>{ };
			for (const auto& text : aText) {
				bytes.insert(bytes.end(), text.begin(), text.end());
				bytes.push_back(static_cast<std::uint8_t>('\n'));
			}
			return bytes;
		};
		return { join(left), join(right) };
	}

	struct spread
	{
		double Median = 0.0;
		double Lowest = 0.0;
		double Q1     = 0.0;
		double Q3     = 0.0;
		double Worst  = 0.0;
	};

	[[nodiscard]] auto spread_of(std::vector<double> aSamples) -> spread
	{
		std::ranges::sort(aSamples);
		const auto at = [&aSamples](const double aFraction) {
			return aSamples[static_cast<std::size_t>(std::llround(aFraction * static_cast<double>(aSamples.size() - 1U)))];
		};
		return { .Median = at(0.5), .Lowest = aSamples.front(), .Q1 = at(0.25), .Q3 = at(0.75), .Worst = aSamples.back() };
	}

	[[nodiscard]] auto milliseconds(const std::chrono::steady_clock::time_point aStart) -> double
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - aStart).count();
	}

#ifndef BPR_DIFF_BENCH_BASELINE
	/// Never stops; records the longest wall time between two polls.
	class poll_timer final : public diff_compute_stop
	{
	public:
		[[nodiscard]] auto stop_requested(const std::uint64_t /*aSteps*/) -> bool override
		{
			const auto now = std::chrono::steady_clock::now();
			Longest        = std::max(Longest, std::chrono::duration<double, std::milli>(now - Last).count());
			Last           = now;
			return false;
		}

		void reset()
		{
			Last    = std::chrono::steady_clock::now();
			Longest = 0.0;
		}

		std::chrono::steady_clock::time_point Last    = std::chrono::steady_clock::now();
		double                                Longest = 0.0;
	};

	inline constexpr std::uint64_t phase_check_interval = 4U * 1024U;

	/// Never stops; records the time of every poll by its step.
	class phase_timer final : public diff_compute_stop
	{
	public:
		[[nodiscard]] auto stop_requested(const std::uint64_t aSteps) -> bool override
		{
			Polls.emplace_back(aSteps, std::chrono::steady_clock::now());
			return false;
		}

		/// Milliseconds between the first polls at or past each step; the
		/// end of the run for a step no poll reached.
		[[nodiscard]] auto between(const std::uint64_t aBegin, const std::uint64_t aEnd,
								   const std::chrono::steady_clock::time_point aFinished) const -> double
		{
			const auto at = [this, aFinished](const std::uint64_t aStep) {
				const auto found = std::ranges::find_if(Polls, [aStep](const auto& aPoll) { return aPoll.first >= aStep; });
				return found == Polls.end() ? aFinished : found->second;
			};
			return std::chrono::duration<double, std::milli>(at(aEnd) - at(aBegin)).count();
		}

		std::vector<std::pair<std::uint64_t, std::chrono::steady_clock::time_point>> Polls;
	};
#endif
} // namespace

int main(int aArgc, char** aArgv)
{
	const auto repetitions = aArgc > 1 ? std::max(3, std::atoi(aArgv[1])) : 9;
	const auto largest     = aArgc > 2 ? std::strtoull(aArgv[2], nullptr, 10) : 100'000ULL;
	auto       options     = diff_options{ };
	options.Moves          = move_detection_policy::deterministic_exact;
	std::printf("BPR-1366 diff compute benchmark: %d repetitions, milliseconds (median, lowest, q1-q3, worst)\n", repetitions);
	for (const auto lines : { std::uint64_t{ 2'000U }, std::uint64_t{ 10'000U }, std::uint64_t{ 40'000U }, std::uint64_t{ 100'000U } }) {
		if (lines > largest) {
			continue;
		}
		for (const auto edited : { std::uint32_t{ 30U }, std::uint32_t{ 300U } }) {
			const auto [left, right] = synthetic_pair(lines, edited);
			const auto input         = diff_compute_input{
				.Key        = comparison_key(options),
				.Options    = options,
				.LeftBytes  = std::span<const std::uint8_t>{ left },
				.RightBytes = std::span<const std::uint8_t>{ right },
			};
			auto       matching  = bpr::application::diff_line_matching{ *bpr::tests::product_line_matcher() };
			const auto reference = compute_file_diff(input, matching);
			if (!reference.Result.has_value()) {
				std::printf("FAILED: the synthetic pair computes\n");
				return EXIT_FAILURE;
			}
			auto oneCall = std::vector<double>{ };
#ifndef BPR_DIFF_BENCH_BASELINE
			auto polled  = std::vector<double>{ };
			auto timer   = poll_timer{ };
			auto longest = 0.0;
			auto refine  = std::vector<double>{ };
#endif
			for (int repetition = 0; repetition < repetitions; ++repetition) {
				const auto start   = std::chrono::steady_clock::now();
				const auto outcome = compute_file_diff(input, matching);
				oneCall.push_back(milliseconds(start));
				if (outcome.Result != reference.Result) {
					std::printf("FAILED: a repetition differs\n");
					return EXIT_FAILURE;
				}
#ifndef BPR_DIFF_BENCH_BASELINE
				timer.reset();
				const auto polledStart = std::chrono::steady_clock::now();
				const auto slice       = compute_file_diff(input, diff_compute_control{ .Stop = &timer }, matching);
				polled.push_back(milliseconds(polledStart));
				longest = std::max(longest, timer.Longest);
				if (!slice.Outcome.has_value() || slice.Outcome->Result != reference.Result) {
					std::printf("FAILED: the stoppable entry point differs\n");
					return EXIT_FAILURE;
				}
				auto       phases   = phase_timer{ };
				const auto timed    = compute_file_diff(input, diff_compute_control{ .Stop = &phases, .CheckInterval = phase_check_interval }, matching);
				const auto finished = std::chrono::steady_clock::now();
				if (!timed.Outcome.has_value() || timed.Outcome->Result != reference.Result) {
					std::printf("FAILED: the phase-timed entry point differs\n");
					return EXIT_FAILURE;
				}
				const auto& starts = timed.Statistics.PhaseStarts;
				refine.push_back(phases.between(starts[static_cast<std::size_t>(diff_compute_phase::refine)],
												starts[static_cast<std::size_t>(diff_compute_phase::moves)], finished));
#endif
			}
			const auto one = spread_of(oneCall);
			std::printf("lines=%6llu edited=%3u bytes=%8zu/%8zu one-call %8.2f %8.2f %8.2f-%-8.2f %8.2f",
						static_cast<unsigned long long>(lines), edited, left.size(), right.size(), one.Median, one.Lowest, one.Q1, one.Q3, one.Worst);
#ifndef BPR_DIFF_BENCH_BASELINE
			const auto stoppable = spread_of(polled);
			const auto pairing   = spread_of(refine);
			std::printf(" | polled %8.2f %8.2f %8.2f-%-8.2f %8.2f | longest between polls %.3f | refine %8.2f %8.2f %8.2f-%-8.2f %8.2f",
						stoppable.Median, stoppable.Lowest, stoppable.Q1, stoppable.Q3, stoppable.Worst, longest, pairing.Median, pairing.Lowest, pairing.Q1, pairing.Q3,
						pairing.Worst);
#endif
			std::printf("\n");
		}
	}
	return EXIT_SUCCESS;
}
