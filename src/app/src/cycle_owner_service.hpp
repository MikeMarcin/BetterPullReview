#ifndef BPR_APP_CYCLE_OWNER_SERVICE_HPP
#define BPR_APP_CYCLE_OWNER_SERVICE_HPP
#include "bpr/application/tour_owner_channel.hpp"
#include "cycle_commands.hpp"
#include <array>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
namespace bpr::app {
	/// \brief [[BPR-1168]]. Serves forwarded `bpr cycle` commands for the
	/// profile owner, on its own store, one at a time.
	/// \details Each command runs exactly as the terminal would run it, on a
	/// session of the owner's store under the owner's lease; the caller names
	/// a registry command and its arguments only. A request repeated under the
	/// same operation identity is answered as it was first answered and never
	/// run again; the same identity with another request is refused. The store
	/// itself returns, rather than repeats, every write made under an
	/// operation it has seen, so this holds across owners too.
	class cycle_owner_service final
	{
	public:
		using draft_guard = std::function<cycle_result<void>(core::cycle_command, core::review_cycle_id)>;

		struct answer
		{
			/// The encoded owner response.
			application::tour_channel_bytes Bytes;
			/// The cycle a successful writing command wrote.
			std::optional<core::review_cycle_id> Changed;
		};

		cycle_owner_service(std::filesystem::path aRoot, core::bpr_profile_id aProfile);
		/// \brief Runs one admitted request. `aPayload` is the exact request
		/// bytes, whose digest identifies the request; blocking.
		[[nodiscard]] auto serve(std::span<const std::uint8_t> aPayload, const application::cycle_owner_request& aRequest, const draft_guard& aGuard, const std::function<bool()>& aCancelled) -> answer;

	private:
		struct remembered
		{
			std::string                     Operation;
			std::uint64_t                   Step = 0;
			std::array<std::uint8_t, 32>    Digest{ };
			application::tour_channel_bytes Bytes;
		};
		/// Answers kept for repeats, oldest first.
		static constexpr std::size_t remembered_answers = 64U;

		std::filesystem::path  Root;
		core::bpr_profile_id   Profile;
		std::mutex             Mutex;
		std::deque<remembered> Answers;
	};
}
#endif
