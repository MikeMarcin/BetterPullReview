#ifndef BPR_SRC_ADAPTERS_UPDATE_SRC_MACOS_RELEASE_UPDATE_MOUNTS_HPP
#define BPR_SRC_ADAPTERS_UPDATE_SRC_MACOS_RELEASE_UPDATE_MOUNTS_HPP
#include <optional>
#include <string_view>

namespace bpr::adapters::update::detail {
	/// \brief Parses a bounded native image inventory; malformed observations refuse eligibility.
	[[nodiscard]] auto release_update_volume_is_image(std::string_view aInventory, std::string_view aDevice) -> std::optional<bool>;
}
#endif
