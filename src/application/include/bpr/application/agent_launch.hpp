#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_profile.hpp"

#include <compare>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	/// \brief What one agent process is started with.
	///
	/// \details **There is no field here a credential could occupy.**
	/// `Environment` is a list of name/value pairs the host materialised from
	/// the profile's allowlist, and the allowlist is names only — so a value
	/// present here is present because a profile named the variable, never
	/// because this process happened to have it.
	///
	/// `Arguments` is deliberately separate from `Input`, and the context
	/// supplement has no path into the former. Command lines are world-readable
	/// on every platform this ships to: `ps` on POSIX, the process list on
	/// Windows. A supplement in `Arguments` naming an unreleased feature, a
	/// customer, or a private path would be visible to every other user of a
	/// shared machine.
	struct agent_launch_request
	{
		std::string              Executable;
		std::vector<std::string> Arguments;
		/// Materialised from the profile's allowlist. Pairs rather than a whole
		/// environment, because the child gets exactly these and nothing else.
		std::vector<std::pair<std::string, std::string>> Environment;
		/// Everything the agent is told, over its input channel.
		std::string Input;

		auto operator<=>(const agent_launch_request&) const = default;
	};

	/// \brief Reads one variable of the environment this process is running in.
	///
	/// \details A port so the suite can present a parent environment it
	/// controls, and so the materialisation below can be tested without the
	/// test's own environment deciding the answer.
	using environment_reader
		= std::function<std::optional<std::string>(const std::string&)>;

	/// \brief Builds the child environment one profile allows.
	///
	/// \details An allowlist, not an inheritance. The result contains a
	/// variable exactly when the profile named it *and* the parent had it; a
	/// name the profile did not list cannot appear however the parent is
	/// configured, because this function never enumerates the parent.
	///
	/// That is the shape of the guarantee. A function that started from the
	/// parent's environment and removed known-sensitive names would be a
	/// denylist wearing an allowlist's name, and would pass every secret
	/// nobody had thought to list.
	[[nodiscard]] auto materialize_agent_environment(
		const core::agent_profile& aProfile,
		const environment_reader&  aParent)
		-> std::vector<std::pair<std::string, std::string>>;

	/// \brief Builds the launch request for one profile and supplement.
	///
	/// \details The supplement is serialised into `Input` and reaches
	/// `Arguments` by no path at all. `Arguments` carries only what the
	/// protocol needs to start, which is the profile's own protocol switch.
	[[nodiscard]] auto build_agent_launch(
		const core::agent_profile&            aProfile,
		const core::agent_context_supplement& aSupplement,
		const environment_reader&             aParent) -> agent_launch_request;

	/// \brief Renders one supplement as the bytes an agent reads.
	///
	/// \details A stable, self-describing encoding carrying the schema version
	/// first, so a tool can tell what it received before parsing the rest.
	[[nodiscard]] auto encode_agent_supplement(
		const core::agent_context_supplement& aSupplement) -> std::string;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
