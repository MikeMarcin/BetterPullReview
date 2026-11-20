#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MATERIAL_BRIDGE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MATERIAL_BRIDGE_HPP

#include <cstdint>

/// \file
/// \brief How a read of one file's review material ended (RFC-BPR-0054).
/// \details The tagged bridge that also served a Review Cycle's stored objects
/// was retired with Review Cycles ([[BPR-2308]]); a local draft's file view
/// keeps reporting its reads in these words.
namespace bpr::application {
	enum class review_material_status : std::uint8_t {
		available,
		missing,
		/// The request names an authority the reader does not serve.
		wrong_authority,
		too_large,
		corrupt,
		cancelled,
		failure,
	};
}

#endif
