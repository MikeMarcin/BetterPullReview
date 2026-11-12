#ifndef BPR_SQLITE_LOCAL_DRAFT_STORE_HPP
#define BPR_SQLITE_LOCAL_DRAFT_STORE_HPP

#include "bpr/application/local_draft_store.hpp"

#include <filesystem>
#include <memory>

/// \file
/// \brief RFC-BPR-0108. `state/v1/local-drafts.sqlite3`: the profile's local
/// PR drafts and the exact material their candidates keep.
/// \details Private review data: Reset review data and Reset everything
/// remove it with the profile's other local review work, and the full purge
/// removes the profile. It holds no account, credential or pull request
/// authority; an association records only a verified address.
namespace bpr::adapters::sqlite {
	/// \brief Opens (creating) the store under the profile root; absent when
	/// it cannot be opened as this build's store.
	[[nodiscard]] auto open_local_draft_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::local_draft_store>;
}

#endif
