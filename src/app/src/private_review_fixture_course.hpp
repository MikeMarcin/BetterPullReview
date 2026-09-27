#ifndef BPR_SRC_APP_SRC_PRIVATE_REVIEW_FIXTURE_COURSE_HPP
#define BPR_SRC_APP_SRC_PRIVATE_REVIEW_FIXTURE_COURSE_HPP
#include "bpr/app/github_application.hpp"
namespace bpr::app::detail {
	/// Drives real private-review widgets in the public executable against its safe fixture.
	void schedule_private_review_course(github_application&, std::string aPhase, std::string aRepresentation);
}
#endif
