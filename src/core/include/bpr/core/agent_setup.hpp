#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SETUP_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SETUP_HPP

#include "bpr/core/agent_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-PENDING-AQ1]], RFC-BPR-0091. The agent a setup row offers,
	/// as a reviewer names it.
	///
	/// \details Owner, 2026-10-22, Q4 accepted: the Agent tab says Claude Code
	/// and Codex. Codex App Server and Codex JSONL are one agent here; which of
	/// the two protocols it speaks is `codex_protocol_choice`, a setting. The
	/// plain-text harness (the external terminal protocol) is the third, and
	/// is offered only once the reviewer has chosen its program.
	enum class setup_agent : std::uint8_t {
		claude_code,
		codex,
		plain_text_harness,
	};

	inline constexpr std::size_t setup_agent_count = 3U;

	[[nodiscard]] auto stable_name(setup_agent aAgent) noexcept -> std::string_view;

	/// \brief "Claude Code", "Codex", or "A harness that reads plain text".
	[[nodiscard]] auto plain_name(setup_agent aAgent) noexcept -> std::string_view;

	/// \brief The setup agent a protocol belongs to.
	[[nodiscard]] auto setup_agent_of(agent_protocol aProtocol) noexcept -> setup_agent;

	/// \brief Which of Codex's two protocols a Codex launch speaks. App Server
	/// is the default (Q4).
	enum class codex_protocol_choice : std::uint8_t {
		app_server,
		jsonl,
	};

	/// \brief The built-in profile one setup agent launches.
	[[nodiscard]] auto setup_profile(setup_agent aAgent, codex_protocol_choice aCodex) noexcept -> agent_profile_id;

	/// \brief How a detected agent's program was found.
	enum class agent_found_how : std::uint8_t {
		/// An absolute entry of the application's own `PATH`.
		on_path,
		/// One of the fixed usual install folders, searched after `PATH`.
		usual_folder,
		/// The program path the reviewer saved for that agent.
		saved_program,
	};

	[[nodiscard]] auto stable_name(agent_found_how aHow) noexcept -> std::string_view;

	/// \brief One agent the host found, by file metadata alone (Q2). An agent
	/// with no detection was not found.
	struct agent_detection
	{
		setup_agent     Agent = setup_agent::claude_code;
		std::string     Path;
		agent_found_how How = agent_found_how::on_path;

		auto operator<=>(const agent_detection&) const = default;
	};

	/// \brief One row of the setup's Agent step.
	struct agent_setup_row
	{
		setup_agent              Agent = setup_agent::claude_code;
		agent_profile_id         Profile;
		std::string              Path;
		agent_found_how          How = agent_found_how::on_path;
		agent_invocation_verdict Verdict;

		auto operator<=>(const agent_setup_row&) const = default;
	};

	/// \brief The Agent step's rows, from what the host found.
	///
	/// \details One row per setup agent, from its first detection, in a fixed
	/// order: Claude Code, Codex, then the plain-text harness. A plain-text
	/// harness found any way but by its saved program is dropped: it is never
	/// searched for. An agent whose profile is not in `aProfiles` is dropped.
	/// Each row carries the host policy's verdict for its profile, so a row the
	/// host forbids stays visible and says so rather than disappearing. A
	/// detection with an empty or over-long path is dropped.
	[[nodiscard]] auto agent_setup_rows(std::span<const agent_detection> aFound, std::span<const agent_profile> aProfiles,
										const std::optional<agent_host_policy>& aPolicy, codex_protocol_choice aCodex) -> std::vector<agent_setup_row>;

	/// \brief The agent the step chooses without asking.
	///
	/// \details The saved choice when its row is present and permitted; else
	/// the only permitted row; else none, so several found agents wait for the
	/// reviewer and none found asks them to install one.
	[[nodiscard]] auto choose_setup_agent(std::span<const agent_setup_row> aRows, std::optional<setup_agent> aSaved) noexcept -> std::optional<setup_agent>;

	/// \brief The agents a search looked for under a host's policy: Claude
	/// Code and Codex where the policy permits their profiles. The none-found
	/// state names exactly these.
	[[nodiscard]] auto searched_setup_agents(std::span<const agent_profile> aProfiles, const std::optional<agent_host_policy>& aPolicy,
											 codex_protocol_choice aCodex) -> std::vector<setup_agent>;

	/// \brief What the agent's own status command reported (Q3 A), decoded
	/// at the process boundary by the host.
	enum class agent_status : std::uint8_t {
		signed_in,
		not_signed_in,
		/// It ran and said something that is neither.
		unreadable,
	};

	[[nodiscard]] auto stable_name(agent_status aStatus) noexcept -> std::string_view;

	/// \brief The longest version text a check keeps.
	inline constexpr std::size_t maximum_agent_version_bytes = 64U;

	/// \brief One status check's decoded answer.
	struct agent_status_check
	{
		agent_status Status = agent_status::unreadable;
		/// The version the agent reported, or empty.
		std::string Version;

		auto operator<=>(const agent_status_check&) const = default;
	};

	/// \brief Why a setup step, or a later Ask, could not go on. A closed set,
	/// so the presentation says what happened and what to do instead of a
	/// stable outcome name.
	enum class agent_setup_failure : std::uint8_t {
		/// The program was not found where the launch looks, or is gone
		/// from where it was found.
		agent_not_found,
		/// The saved program path names something that cannot be run.
		program_refused,
		/// The plain-text harness has no program chosen.
		no_program_chosen,
		/// The agent answered that it is not signed in.
		not_signed_in,
		/// The program could not be started, or ended before answering.
		failed_to_start,
		/// The status command answered something neither signed in nor not.
		status_unreadable,
		/// The check ran past its bound.
		timed_out,
		/// The agent's workspace could not be made; no agent ran.
		workspace_unavailable,
	};

	[[nodiscard]] auto stable_name(agent_setup_failure aFailure) noexcept -> std::string_view;

	/// \brief Where one row of Check it works stands.
	enum class setup_check_state : std::uint8_t {
		not_run,
		running,
		passed,
		failed,
	};

	/// \brief One row of Check it works.
	struct setup_check
	{
		setup_check_state                  State = setup_check_state::not_run;
		std::optional<agent_setup_failure> Failure;
		/// For the agent row: the version a passed check read.
		std::string Version;

		auto operator<=>(const setup_check&) const = default;
	};

	/// \brief The agent row after its status check answered.
	[[nodiscard]] auto apply_status_check(const agent_status_check& aCheck) -> setup_check;

	/// \brief The agent row after its check could not run to an answer.
	[[nodiscard]] auto fail_check(agent_setup_failure aFailure) -> setup_check;

	/// \brief The four steps, in order (Q1 A).
	enum class setup_step : std::uint8_t {
		agent,
		repository,
		check,
		ready,
	};

	[[nodiscard]] auto stable_name(setup_step aStep) noexcept -> std::string_view;

	/// \brief What the setup knows, as values.
	struct agent_setup_state
	{
		std::vector<agent_setup_row> Rows;
		std::optional<setup_agent>   Chosen;
		/// The repository has a linked clone or a recorded choice of the
		/// private copy (RFC-BPR-0091 Repository).
		bool        RepositoryChosen = false;
		setup_check AgentCheck;
		setup_check WorkspaceCheck;

		auto operator<=>(const agent_setup_state&) const = default;
	};

	/// \brief The first unfinished step. A finished step stays folded; a
	/// check that has not passed is never taken as passed.
	[[nodiscard]] auto current_setup_step(const agent_setup_state& aState) noexcept -> setup_step;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SETUP_HPP
