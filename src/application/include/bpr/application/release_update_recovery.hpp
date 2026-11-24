#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_RECOVERY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_RECOVERY_HPP
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace bpr::application {
	/// \brief Private, inert recovery text. It grants no publication or target authority.
	struct release_update_saved_draft
	{
		std::string Identity;
		std::string Description;
		std::string Body;
		bool        Indeterminate                                               = false;
		auto        operator==(const release_update_saved_draft&) const -> bool = default;
	};
	inline constexpr std::size_t release_update_maximum_saved_drafts = 4096;
	inline constexpr std::size_t release_update_maximum_saved_bytes  = 16U * 1024U * 1024U;
	enum class release_update_recovery_failure { unavailable };
	/// \brief Exact recovery row identity. One source identity can have several saved versions.
	struct release_update_recovery_id
	{
		std::int64_t                 Sequence;
		std::array<std::uint8_t, 32> Fingerprint;
		auto                         operator==(const release_update_recovery_id&) const -> bool = default;
	};
	struct release_update_recovery_row
	{
		release_update_recovery_id Id;
		release_update_saved_draft Draft;
		bool                       Shown                                                        = false;
		auto                       operator==(const release_update_recovery_row&) const -> bool = default;
	};
	using release_update_saved_drafts = std::expected<std::vector<release_update_recovery_row>, release_update_recovery_failure>;
	/// \brief Private recovery copies. An update never overwrites a saved version.
	class release_update_recovery_port
	{
	public:
		virtual ~release_update_recovery_port()                                                         = default;
		[[nodiscard]] virtual auto save(const std::vector<release_update_saved_draft>& aDrafts) -> bool = 0;
		[[nodiscard]] virtual auto load() -> release_update_saved_drafts                                = 0;
		/// \brief Removes exactly the observed rows atomically. An unknown identity refuses the batch.
		[[nodiscard]] virtual auto discard(std::span<const release_update_recovery_id> aIdentities) -> bool = 0;
		/// \brief Persists that these exact rows were presented without changing their text.
		[[nodiscard]] virtual auto mark_shown(std::span<const release_update_recovery_id> aIdentities) -> bool = 0;
	};
}
#endif
