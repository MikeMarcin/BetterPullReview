#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_WORKSPACE_EXPIRY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_WORKSPACE_EXPIRY_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-2293]], BPR-2288 Change 5. A retained agent workspace
	/// unused this many days is removed, unless the setting says otherwise;
	/// zero keeps them until removed by hand.
	inline constexpr std::uint32_t default_agent_workspace_keep_days = 7U;
	inline constexpr std::uint32_t maximum_agent_workspace_keep_days = 3650U;
	/// \brief The most the retained workspaces may hold together, in GB
	/// (10^9 bytes, as the Settings row counts them): the oldest go first
	/// beyond it.
	inline constexpr std::uint32_t default_agent_workspace_cap_gb = 20U;
	inline constexpr std::uint32_t minimum_agent_workspace_cap_gb = 1U;
	inline constexpr std::uint32_t maximum_agent_workspace_cap_gb = 100000U;

	/// \brief The expiry policy: days unused (0: never by age) and the total
	/// cap in GB.
	struct agent_workspace_expiry_policy
	{
		std::uint32_t KeepDays = default_agent_workspace_keep_days;
		std::uint32_t CapGb    = default_agent_workspace_cap_gb;

		auto operator<=>(const agent_workspace_expiry_policy&) const = default;
	};

	[[nodiscard]] constexpr auto agent_workspace_cap_bytes(const std::uint32_t aGb) noexcept -> std::uint64_t
	{
		const auto held = aGb < minimum_agent_workspace_cap_gb ? minimum_agent_workspace_cap_gb : aGb > maximum_agent_workspace_cap_gb ? maximum_agent_workspace_cap_gb
																																	   : aGb;
		return static_cast<std::uint64_t>(held) * 1000ULL * 1000ULL * 1000ULL;
	}

	/// \brief What the host observed about one retained workspace.
	struct retained_workspace_fact
	{
		/// The workspace's last use, in seconds since the epoch: the later of
		/// the last turn that used it and the newest file in it.
		std::int64_t  LastUseSeconds = 0;
		std::uint64_t Bytes          = 0U;
		/// A turn holds it (its turn lease could not be taken), or its
		/// conversation's last question may still be in flight.
		bool InUse = false;
		/// No selected conversation reaches it any more: the reviewer
		/// started another, or moved on from its revision.
		bool Ended = false;

		auto operator<=>(const retained_workspace_fact&) const = default;
	};

	enum class workspace_expiry_reason : std::uint8_t {
		/// Its conversation was ended or replaced.
		ended,
		/// Unused for more than the policy's days.
		unused,
		/// The oldest beyond the total cap.
		over_cap,
		/// Remove in Settings: every one not in use.
		removed_by_reviewer,
	};

	/// \brief One workspace chosen for removal: its index in the facts, and why.
	struct workspace_expiry_choice
	{
		std::size_t             Index  = 0U;
		workspace_expiry_reason Reason = workspace_expiry_reason::ended;

		auto operator<=>(const workspace_expiry_choice&) const = default;
	};

	/// \brief Which workspaces the policy removes, oldest first.
	///
	/// \details Pure. Those of an ended conversation and those unused for more
	/// than `KeepDays` days at `aNowSeconds` go first; then, while the total
	/// of every workspace (those in use count, and stay) is over the cap, the
	/// least recently used of the rest. A workspace in use is never chosen.
	/// Ties in last use are broken by index, so equal facts give equal
	/// choices.
	[[nodiscard]] auto choose_expired_workspaces(std::span<const retained_workspace_fact> aFacts, const agent_workspace_expiry_policy& aPolicy, std::int64_t aNowSeconds)
		-> std::vector<workspace_expiry_choice>;

	/// \brief Remove in Settings: every workspace not in use, oldest first.
	[[nodiscard]] auto choose_all_idle_workspaces(std::span<const retained_workspace_fact> aFacts) -> std::vector<workspace_expiry_choice>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_WORKSPACE_EXPIRY_HPP
