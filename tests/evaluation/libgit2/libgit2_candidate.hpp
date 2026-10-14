#ifndef BPR_TESTS_EVALUATION_LIBGIT2_LIBGIT2_CANDIDATE_HPP
#define BPR_TESTS_EVALUATION_LIBGIT2_LIBGIT2_CANDIDATE_HPP

/// \file
/// \brief Candidate L of RFC-BPR-0083: libgit2 answering the C1-C6
/// questions, for evaluation only ([[BPR-1458]]).
/// \details Built only when `BPR_EVALUATE_LIBGIT2` is on; the product never
/// links it. No libgit2 type appears here. The shared harness header declares
/// the same factory under `BPR_HAVE_LIBGIT2_CANDIDATE`.

#include "git_metadata_candidate.hpp"

#include <memory>

namespace bpr::tests::workspace_contract {
	/// \brief libgit2 with every global and system configuration search path
	/// emptied and its home directory cleared, so no global or system config,
	/// excludes or attributes file is read.
	[[nodiscard]] auto make_libgit2_candidate() -> std::unique_ptr<git_metadata_candidate>;
}

#endif
