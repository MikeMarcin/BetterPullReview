// [[RFC-BPR-0082]] [[BPR-1466]]. See oracle.hpp.

#include "harness/oracle.hpp"

#include "bpr/core/review_comment_range.hpp"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

namespace bpr::evaluation {
	namespace {
		[[nodiscard]] auto membership(const std::vector<core::line_span>& aHunks, const std::uint32_t aLines) -> std::vector<bool>
		{
			auto result = std::vector<bool>(aLines, false);
			for (const auto& hunk : aHunks) {
				for (auto line = hunk.Begin; line < std::min(hunk.End, aLines); ++line) {
					result[line] = true;
				}
			}
			return result;
		}
		/// Hunks drawn from the alignment's change groups with three lines of
		/// context, joining groups at most 2 x 3 + aInterHunk equal lines apart.
		[[nodiscard]] auto drawn_hunks(const core::file_alignment& aAlignment, const std::uint32_t aInterHunk) -> std::vector<oracle_hunk>
		{
			constexpr auto context = core::github_hunk_context_lines;
			auto           result  = std::vector<oracle_hunk>{ };
			const auto&    groups  = aAlignment.Groups;
			for (std::size_t first = 0U; first < groups.size();) {
				auto last = first;
				while (last + 1U < groups.size() && groups[last + 1U].Left.Begin - groups[last].Left.End <= 2U * context + aInterHunk) {
					++last;
				}
				const auto leftBegin  = groups[first].Left.Begin > context ? groups[first].Left.Begin - context : 0U;
				const auto leftEnd    = std::min(aAlignment.LeftLineCount, groups[last].Left.End + context);
				const auto rightBegin = groups[first].Right.Begin > context ? groups[first].Right.Begin - context : 0U;
				const auto rightEnd   = std::min(aAlignment.RightLineCount, groups[last].Right.End + context);
				// As a header: a side with no lines names the line it follows.
				const auto header = [](const std::uint32_t aBegin, const std::uint32_t aEnd) {
					return std::pair{ aEnd == aBegin ? aBegin : aBegin + 1U, aEnd - aBegin };
				};
				const auto [leftStart, leftCount]   = header(leftBegin, leftEnd);
				const auto [rightStart, rightCount] = header(rightBegin, rightEnd);
				result.push_back({ leftStart, leftCount, rightStart, rightCount });
				first = last + 1U;
			}
			return result;
		}
	} // namespace

	auto compare_with_oracle(const core::file_alignment& aAlignment, const std::vector<oracle_hunk>& aOracle) -> oracle_agreement
	{
		auto oracleLeft  = std::vector<core::line_span>{ };
		auto oracleRight = std::vector<core::line_span>{ };
		for (const auto& [left, leftCount, right, rightCount] : aOracle) {
			if (leftCount != 0U) {
				oracleLeft.push_back({ .Begin = left - 1U, .End = left - 1U + leftCount });
			}
			if (rightCount != 0U) {
				oracleRight.push_back({ .Begin = right - 1U, .End = right - 1U + rightCount });
			}
		}
		const auto& oracleHeaders  = aOracle;
		const auto  candidateLeft  = core::github_review_hunks(aAlignment, core::diff_side::left);
		const auto  candidateRight = core::github_review_hunks(aAlignment, core::diff_side::right);
		auto        result         = oracle_agreement{
			.Exact          = candidateLeft == oracleLeft && candidateRight == oracleRight,
			.OracleHunks    = static_cast<std::uint32_t>(aOracle.size()),
			.CandidateHunks = static_cast<std::uint32_t>(std::max(candidateLeft.size(), candidateRight.size())),
		};
		result.ExactGithubJoin = drawn_hunks(aAlignment, 1U) == oracleHeaders;
		result.ExactGitJoin    = drawn_hunks(aAlignment, 0U) == oracleHeaders;
		for (const auto& [candidate, oracle, lines] : { std::tuple{ &candidateLeft, &oracleLeft, aAlignment.LeftLineCount },
														std::tuple{ &candidateRight, &oracleRight, aAlignment.RightLineCount } }) {
			const auto mine   = membership(*candidate, lines);
			const auto theirs = membership(*oracle, lines);
			for (std::uint32_t line = 0U; line < lines; ++line) {
				result.CommentFlips += mine[line] != theirs[line] ? 1U : 0U;
			}
		}
		return result;
	}
} // namespace bpr::evaluation
