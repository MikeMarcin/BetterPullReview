#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_LIFETIME_GUARD_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_WORKSPACE_LIFETIME_GUARD_HPP

#include <array>
#include <compare>
#include <cstdint>
#include <string>

namespace bpr::application {
	/// \brief Exact read-only lifetime lock retained by the launcher and supervisor.
	/// The adapter interprets native identity; no descriptor crosses this port.
	struct agent_workspace_lifetime_guard
	{
		std::string                   Path;
		std::array<std::uint64_t, 4U> FileIdentity{ };
		auto                          operator<=>(const agent_workspace_lifetime_guard&) const = default;
	};
}

#endif
