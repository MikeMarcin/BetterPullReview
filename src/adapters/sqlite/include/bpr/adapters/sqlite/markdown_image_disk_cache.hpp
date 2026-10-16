#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_MARKDOWN_IMAGE_DISK_CACHE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_MARKDOWN_IMAGE_DISK_CACHE_HPP

#include "bpr/application/markdown_image_ports.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

namespace bpr::adapters::sqlite {
	/// \brief RFC-BPR-0084. Seconds since the epoch, for the cache's expiry.
	using markdown_image_clock = std::function<std::int64_t()>;

	/// \brief Creates the profile's lazy image cache, `markdown-images.sqlite3`,
	/// in an admitted profile-private root; construction performs no I/O.
	///
	/// \details The file is owner-private like every profile store, keeps a
	/// SHA-256 digest of each key and never the key itself, and is one of the
	/// files a profile purge removes. `aNow` defaults to the system clock and
	/// `aBound` to `core::markdown_image_disk_cache_bytes`; tests pass both.
	[[nodiscard]] auto make_markdown_image_disk_cache(std::filesystem::path aRoot, markdown_image_clock aNow = { },
													  std::size_t aBound = core::markdown_image_disk_cache_bytes)
		-> std::unique_ptr<application::markdown_image_disk_cache_port>;
} // namespace bpr::adapters::sqlite

#endif
