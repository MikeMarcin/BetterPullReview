#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_ADMISSION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_ADMISSION_HPP

#include "bpr/application/github_provider_port.hpp"
#include "bpr/core/github_read_backoff.hpp"

#include <functional>
#include <vector>

namespace bpr::application {
	/// \brief Shared PR-read cooldowns for all windows of a profile runtime.
	/// \details Calls are serialized on the shared decision lane. Entries contain
	/// no credential, request or callback, and survive session retirement. Hostname
	/// and viewer node bind the actual server/account across connection profiles.
	/// At capacity, unknown keys fail closed until a retained deadline expires.
	class github_read_admission final
	{
	public:
		using clock = std::function<core::github_read_time()>;
		explicit github_read_admission(clock aClock = { });
		[[nodiscard]] auto refusal(const github_profile_authority& aProfile, const github_viewer_record& aViewer,
								   bool aUsesGraphql) const -> std::optional<github_provider_error>;
		void               observe(const github_profile_authority& aProfile, const github_viewer_record& aViewer,
								   const github_provider_error& aError);
		void               succeeded(const github_profile_authority& aProfile, const github_viewer_record& aViewer, bool aUsesGraphql);

	private:
		struct entry
		{
			github_canonical_hostname Host;
			github_actor_id           Account;
			github_provider_error     Error;
			core::github_read_backoff Policy;
		};
		clock                     Clock;
		core::github_read_backoff Overflow;
		std::vector<entry>        Entries;
	};
}
#endif
