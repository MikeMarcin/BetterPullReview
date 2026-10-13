// [[RFC-BPR-0082]] [[BPR-1466]]. See oracle.hpp.

#include "harness/oracle.hpp"

#include "bpr/core/review_comment_range.hpp"

#include <algorithm>
#include <cstdint>
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
		const auto candidateLeft  = core::github_review_hunks(aAlignment, core::diff_side::left);
		const auto candidateRight = core::github_review_hunks(aAlignment, core::diff_side::right);
		auto       result         = oracle_agreement{
			.Exact          = candidateLeft == oracleLeft && candidateRight == oracleRight,
			.OracleHunks    = static_cast<std::uint32_t>(aOracle.size()),
			.CandidateHunks = static_cast<std::uint32_t>(std::max(candidateLeft.size(), candidateRight.size())),
		};
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
