#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REFERENCE_PAGE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REFERENCE_PAGE_HPP

#include "bpr/core/markdown_navigation.hpp"
#include "bpr/core/reference_page.hpp"

#include <string>

namespace bpr::application {
	class device_authorization_browser_port;

	/// \brief [[BPR-1566]]. The address of an admitted reference page: HTTPS,
	/// the admitted hostname and the admitted path, and nothing else.
	[[nodiscard]] auto reference_page_address(const core::reference_page_destination& aDestination) -> std::string;

	/// \brief [[BPR-1566]]. Opens one named reference page in the window's browser.
	///
	/// \details The core admits the page; the address is built here and must
	/// pass the exact-host HTTPS check the device verification address passes
	/// (`address_matches_verification_authority`) before it reaches
	/// `aBrowser`, whose production opener checks it once more. A null browser
	/// is `unavailable`; a browser that declines, or throws, is `failed`.
	[[nodiscard]] auto open_reference_page(core::reference_page aPage, device_authorization_browser_port* aBrowser) -> core::markdown_link_result;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REFERENCE_PAGE_HPP
