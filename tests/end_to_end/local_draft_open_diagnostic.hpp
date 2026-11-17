#ifndef BPR_TESTS_END_TO_END_LOCAL_DRAFT_OPEN_DIAGNOSTIC_HPP
#define BPR_TESTS_END_TO_END_LOCAL_DRAFT_OPEN_DIAGNOSTIC_HPP

/// \file
/// \brief [[BPR-2215]]. Says why a local draft did not open, before a course
/// fails on it, so a platform-only refusal names its outcome and cause
/// instead of only the failed expectation.

#include "bpr/application/local_draft_workbench.hpp"

#include <iostream>

namespace bpr::tests::end_to_end {
	inline void print_local_draft_open(const application::local_draft_workbench_projection& aView)
	{
		std::cout << "LOCAL_DRAFT_OPEN outcome=" << application::name(aView.Outcome) << " recovery=" << application::name(aView.Recovery)
				  << " stage=" << static_cast<int>(aView.Stage) << " source=" << static_cast<int>(aView.Source) << " location=" << aView.Location
				  << " draft=" << aView.Draft.has_value();
		if (aView.Store) {
			std::cout << " store=" << static_cast<int>(aView.Store->Failure);
		}
		if (aView.Capture) {
			std::cout << " capture=" << static_cast<int>(aView.Capture->Failure) << " capture_path=" << aView.Capture->Path;
			if (aView.Capture->Git) {
				std::cout << " capture_git=" << static_cast<int>(aView.Capture->Git->Failure) << " capture_git_detail=" << aView.Capture->Git->Detail;
			}
		}
		if (aView.Git) {
			std::cout << " git=" << static_cast<int>(aView.Git->Failure) << " git_detail=" << aView.Git->Detail;
		}
		if (aView.ChainRefusal) {
			std::cout << " chain_refused=1";
		}
		std::cout << '\n';
	}
}

#endif // BPR_TESTS_END_TO_END_LOCAL_DRAFT_OPEN_DIAGNOSTIC_HPP
