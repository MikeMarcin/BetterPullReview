#ifndef BPR_TESTS_EVALUATION_DIFF_ENGINE_HARNESS_ORACLE_HPP
#define BPR_TESTS_EVALUATION_DIFF_ENGINE_HARNESS_ORACLE_HPP

// [[RFC-BPR-0082]] [[BPR-1466]]. Agreement with a recorded GitHub patch: its
// hunk headers against the hunks BPR draws from an alignment
// (`github_review_hunks`, three context lines), and the comment-range lines
// whose in-hunk membership differs.

#include "bpr/core/diff_compute.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace bpr::evaluation {
	/// \brief One recorded hunk header, `@@ -a,b +c,d @@`, as written.
	using oracle_hunk = std::array<std::uint32_t, 4U>;

	struct oracle_agreement
	{
		/// BPR's comment hunks (`github_review_hunks`, which joins only
		/// touching hunks) equal the recording's on both sides.
		bool Exact = false;
		/// The candidate's hunks, joined as GitHub's file list joins them
		/// (one unchanged line apart, `--inter-hunk-context=1`), equal the
		/// recording's: agreement of the diff itself.
		bool ExactGithubJoin = false;
		/// The same with Git's own join (touching hunks only).
		bool          ExactGitJoin   = false;
		std::uint32_t OracleHunks    = 0U;
		std::uint32_t CandidateHunks = 0U;
		/// Lines, on both sides, inside a hunk on one side of the comparison
		/// and outside on the other: a multi-line comment's admission flips.
		std::uint64_t CommentFlips = 0U;
	};

	[[nodiscard]] auto compare_with_oracle(const core::file_alignment& aAlignment, const std::vector<oracle_hunk>& aOracle) -> oracle_agreement;
} // namespace bpr::evaluation

#endif // BPR_TESTS_EVALUATION_DIFF_ENGINE_HARNESS_ORACLE_HPP
