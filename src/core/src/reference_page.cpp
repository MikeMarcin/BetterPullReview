#include "bpr/core/reference_page.hpp"

namespace bpr::core {
	auto admit_reference_page(const reference_page aPage, const bool aBrowserAvailable)
		-> std::expected<reference_page_destination, markdown_link_result>
	{
		if (!aBrowserAvailable) {
			return std::unexpected(markdown_link_result::unavailable);
		}
		switch (aPage) {
		case reference_page::conventional_comments:
			return reference_page_destination{ .Hostname = "conventionalcomments.org", .Path = "/" };
		case reference_page::game_core_tech:
			return reference_page_destination{ .Hostname = "gamecoretech.com", .Path = "/" };
		case reference_page::source_repository:
			return reference_page_destination{ .Hostname = "github.com", .Path = "/MikeMarcin/BetterPullReview" };
		case reference_page::product_site:
			return reference_page_destination{ .Hostname = "betterpullreview.com", .Path = "/" };
		}
		return std::unexpected(markdown_link_result::refused);
	}
} // namespace bpr::core
