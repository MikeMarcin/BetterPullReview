#include "attended_source_identity.hpp"

namespace bpr::app {
	constinit const std::string_view attended_source_sha256{ BPR_ATTENDED_REVIEW_SOURCE_SHA256, sizeof(BPR_ATTENDED_REVIEW_SOURCE_SHA256) - 1U };
}
