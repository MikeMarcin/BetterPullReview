#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_RECOVERY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_RELEASE_UPDATE_RECOVERY_HPP
#include <cstddef>
#include <expected>
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
	using release_update_saved_drafts = std::expected<std::vector<release_update_saved_draft>, release_update_recovery_failure>;
	/// \brief Append-only recovery storage. Existing saved text is never overwritten by an update.
	class release_update_recovery_port
	{
	public:
		virtual ~release_update_recovery_port()                                                         = default;
		[[nodiscard]] virtual auto save(const std::vector<release_update_saved_draft>& aDrafts) -> bool = 0;
		[[nodiscard]] virtual auto load() -> release_update_saved_drafts                                = 0;
	};
}
#endif
