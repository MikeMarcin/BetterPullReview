#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REFERENCE_PAGE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REFERENCE_PAGE_HPP

#include "bpr/core/markdown_navigation.hpp"

#include <cstdint>
#include <expected>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1566]]. A page outside any pull request that the
	/// application links to by name: the explanation behind one of its own
	/// conventions.
	///
	/// \details A reader names the page and never an address. The comment
	/// label chip's menu and Settings › Comments › Labels both
	/// name `conventional_comments`, for reviewers who do not know where the
	/// twelve labels and their decorations come from (owner, 2026-10-16).
	/// [[BPR-1720]] adds the two pages Help › About links to: the
	/// maker's site and the application's own source repository.
	enum class reference_page : std::uint8_t {
		conventional_comments,
		game_core_tech,
		source_repository,
	};

	/// \brief [[BPR-1566]]. The admitted parts of one reference page's address.
	///
	/// \details Always HTTPS: the host joins its scheme, `Hostname` and `Path`
	/// and adds nothing else. No part comes from a reader, a setting or remote
	/// text; every part is this table's.
	struct reference_page_destination
	{
		std::string_view Hostname;
		/// Absolute, starting with "/".
		std::string_view Path;

		auto operator==(const reference_page_destination&) const -> bool = default;
	};

	/// \brief [[BPR-1566]]. Admits one named reference page.
	///
	/// \details `unavailable` when this window has no browser capability;
	/// `refused` for a value that names no page. Nothing is opened here.
	[[nodiscard]] auto admit_reference_page(reference_page aPage, bool aBrowserAvailable)
		-> std::expected<reference_page_destination, markdown_link_result>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REFERENCE_PAGE_HPP
