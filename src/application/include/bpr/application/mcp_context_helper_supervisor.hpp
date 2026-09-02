#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MCP_CONTEXT_HELPER_SUPERVISOR_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MCP_CONTEXT_HELPER_SUPERVISOR_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/port.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	/// \brief The exact child one supervisor would start, as values.
	///
	/// \details Stated so a reviewer — and a contract — can read the whole
	/// launch rather than infer it from a process that already ran. There is no
	/// host, port, endpoint, URL, or credential field here, and no argument
	/// vector at all: the helper takes none, and a supervisor that could pass
	/// one could pass `--listen`.
	struct mcp_helper_launch
	{
		std::string Executable;
		/// The frozen snapshot root, which is also the child's only directory.
		std::string WorkingDirectory;
		/// Exactly the two entries `environment_names` declares, in that order.
		std::vector<std::pair<std::string, std::string>> Environment;

		auto operator<=>(const mcp_helper_launch&) const = default;
	};

	/// \brief Why the shipped helper was not started.
	///
	/// \details Each value is a different mistake with a different repair, and
	/// `network_location` is deliberately one of them rather than folded into a
	/// general bad-path answer: a snapshot root that names a host is the one
	/// refusal RFC-BPR-0012's non-goals make load-bearing.
	enum class mcp_helper_refusal : std::uint8_t {
		/// Not a refusal. Exactly one child was started.
		none,
		/// No shipped helper executable was composed, so there is nothing to
		/// start. Refused rather than searched for on a path.
		no_helper,
		/// No frozen snapshot and pinned binding have been supplied.
		not_pinned,
		/// The helper path or the snapshot root names a URI scheme or a network
		/// or UNC authority. Refused before a process exists.
		network_location,
		/// One helper is already running. A second would answer from the same
		/// snapshot with a second lifetime nobody could stop by name.
		already_running,
	};

	/// \brief The number of declared helper refusals.
	inline constexpr std::size_t mcp_helper_refusal_count = 5U;

	static_assert(
		static_cast<std::size_t>(mcp_helper_refusal::already_running) + 1U
			== mcp_helper_refusal_count,
		"mcp_helper_refusal_count must cover every declared refusal");

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(mcp_helper_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief Records directly observable helper-lifecycle activity.
	struct mcp_helper_supervisor_statistics
	{
		std::uint64_t Pins        = 0U;
		std::uint64_t Starts      = 0U;
		std::uint64_t Refusals    = 0U;
		std::uint64_t Stops       = 0U;
		std::uint64_t Completions = 0U;

		auto operator<=>(const mcp_helper_supervisor_statistics&) const
			= default;
	};

	/// \brief Owns the shipped read-only MCP context helper's whole lifetime.
	///
	/// \details The application half of RFC-BPR-0012's helper: BPR-0501 shipped
	/// the executable and proved what it refuses; this decides when one exists,
	/// what frozen snapshot it is pinned to, and when it stops.
	///
	/// **Three properties are held here rather than asserted about the child.**
	/// The child's environment is exactly the two names `environment_names`
	/// declares, built here and nowhere else, so a parent secret has no entry
	/// to occupy. The child is given no arguments, so `--listen` is not a flag
	/// this side could pass. And the child's only directory is the snapshot
	/// root, which is refused outright if it names a URI scheme or a network or
	/// UNC authority.
	///
	/// The two environment names are written here as literals because the
	/// application layer links no adapter, and
	/// `application_agent_finding_composition_contract` compares
	/// `environment_names()` against `adapters::mcp::mcp_environment_allowlist`
	/// field for field, so a rename fails rather than diverges.
	class mcp_context_helper_supervisor final
	{
	public:
		mcp_context_helper_supervisor(
			std::filesystem::path aHelperExecutable,
			agent_process_port&   aProcesses,
			effect_executor&      aEffects,
			decision_executor&    aDecisionLane);
		~mcp_context_helper_supervisor();

		mcp_context_helper_supervisor(const mcp_context_helper_supervisor&)
			= delete;
		auto operator=(const mcp_context_helper_supervisor&)
			-> mcp_context_helper_supervisor&                          = delete;
		mcp_context_helper_supervisor(mcp_context_helper_supervisor&&) = delete;
		auto operator=(mcp_context_helper_supervisor&&)
			-> mcp_context_helper_supervisor& = delete;

		/// \brief Pins one frozen snapshot root and one revision binding.
		///
		/// \param aPinnedBinding The canonical one-line spelling
		/// `adapters::mcp::encode_revision_binding` produces. It arrives as
		/// text rather than as the value because the spelling belongs to the
		/// adapter that decodes it, and this layer links no adapter.
		void pin(
			std::filesystem::path aSnapshotRoot, std::string aPinnedBinding);

		/// \brief Forgets the pin. A helper already running is left to stop.
		void unpin();

		/// \brief The exact child this supervisor would start, if it can.
		[[nodiscard]] auto describe() const -> std::optional<mcp_helper_launch>;

		/// \brief Starts one helper and hands it one request line.
		///
		/// \details Explicit only. Nothing in this class starts a helper on
		/// observation, refresh, navigation, or head movement, which is the
		/// same rule every other RFC-BPR-0012 entry point obeys.
		[[nodiscard]] auto start(std::string aRequest) -> mcp_helper_refusal;

		/// \brief Stops the running helper, if there is one.
		void stop() noexcept;

		[[nodiscard]] auto running() const noexcept -> bool;
		/// \brief Everything the last helper wrote back, as it wrote it.
		[[nodiscard]] auto answer() const -> std::string;
		[[nodiscard]] auto statistics() const noexcept
			-> mcp_helper_supervisor_statistics;

		/// \brief The only two environment names a helper child may hold.
		[[nodiscard]] static auto environment_names() noexcept
			-> std::array<std::string_view, 2U>;

		/// \brief Whether a path names a URI scheme or a network/UNC authority.
		[[nodiscard]] static auto names_network_location(
			std::string_view aPath) noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MCP_CONTEXT_HELPER_SUPERVISOR_HPP
