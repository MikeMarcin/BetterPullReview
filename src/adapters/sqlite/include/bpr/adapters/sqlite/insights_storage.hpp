#ifndef BPR_SQLITE_INSIGHTS_STORAGE_HPP
#define BPR_SQLITE_INSIGHTS_STORAGE_HPP

#include "bpr/application/insights_host.hpp"

#include <filesystem>

namespace bpr::adapters::sqlite {
	/// \brief Opens the profile's private Insights metadata and measures local stores without reading their contents.
	/// \details Git sizes arrive from its adapter's inventory, never a second walk by SQLite.
	[[nodiscard]] auto make_insights_storage(const std::filesystem::path&                                    aRoot,
											 std::function<std::vector<application::insights_storage_row>()> aGitInventory) -> std::unique_ptr<application::insights_storage_port>;
	/// \brief Retains a local display label for one already accepted cache identity, never exported.
	[[nodiscard]] auto store_insights_repository_label(const std::filesystem::path& aRoot, const std::string& aCache, const std::string& aName) -> bool;
	/// \brief Byte-only section for an anonymized frozen export; contains no local names or paths.
	[[nodiscard]] auto insights_storage_export(const std::filesystem::path& aRoot) -> std::string;
}

#endif
