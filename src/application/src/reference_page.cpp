#include "bpr/application/reference_page.hpp"

#include "bpr/application/device_authorization_application_host.hpp"

namespace bpr::application {
	auto reference_page_address(const core::reference_page_destination& aDestination) -> std::string
	{
		return "https://" + std::string{ aDestination.Hostname } + std::string{ aDestination.Path };
	}

	auto open_reference_page(const core::reference_page aPage, device_authorization_browser_port* const aBrowser) -> core::markdown_link_result
	{
		const auto destination = core::admit_reference_page(aPage, aBrowser != nullptr);
		if (!destination) {
			return destination.error();
		}
		try {
			const auto address = reference_page_address(*destination);
			if (!address_matches_verification_authority(address, destination->Hostname)) {
				return core::markdown_link_result::refused;
			}
			return aBrowser->open(address) ? core::markdown_link_result::opened : core::markdown_link_result::failed;
		} catch (...) {
			return core::markdown_link_result::failed;
		}
	}
} // namespace bpr::application
