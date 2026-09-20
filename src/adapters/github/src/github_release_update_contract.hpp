#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_RELEASE_UPDATE_CONTRACT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_RELEASE_UPDATE_CONTRACT_HPP

#include <cstdint>
#include <string_view>

namespace bpr::adapters::github::detail {
	/// \brief Pinned release-only routes; no configurable feed or PR transport changes.
	inline constexpr std::string_view release_update_api_origin        = "https://api.github.com";
	inline constexpr std::string_view release_update_latest_path       = "/repos/MikeMarcin/BetterPullReview/releases/latest";
	inline constexpr std::string_view release_update_asset_path_prefix = "/repos/MikeMarcin/BetterPullReview/releases/assets/";
	inline constexpr std::string_view release_update_api_version       = "2022-11-28";
	inline constexpr std::string_view release_update_metadata_accept   = "application/vnd.github+json";
	inline constexpr std::string_view release_update_asset_accept      = "application/octet-stream";
	inline constexpr std::string_view release_update_asset_origin      = "https://release-assets.githubusercontent.com";
	inline constexpr std::uint32_t    release_update_maximum_redirects = 3U;
}

#endif
