#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_GRANT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_GRANT_HPP

#include "bpr/core/agent_wrapper.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-2007]], RFC-BPR-0104. The most a scratch folder may hold.
	/// A turn that starts with the folder at or past it gets no write
	/// allowance there, and the agent is told why.
	inline constexpr std::uint64_t maximum_agent_scratch_bytes = 256ULL * 1024ULL * 1024ULL;

	/// \brief What BPR lets one turn do beyond reading, in the words of the
	/// grant line. A local draft's change run adds a fourth level later
	/// (RFC-BPR-0115's "Edits this worktree"); nothing here assumes three.
	enum class agent_grant_level : std::uint8_t {
		reads_only,
		scratch_folder,
		builds_and_tests,
	};

	/// \brief [[BPR-2008]]. Where Build and run tests stands for one agent in
	/// one pull request.
	enum class agent_builds_state : std::uint8_t {
		/// The agent's Build and run tests setting is off.
		off,
		/// The setting is on, but the workspace is the reviewer's own
		/// checkout: BPR never lets an agent write there.
		own_checkout,
		/// On, in the disposable checkout, and this pull request has not
		/// been asked yet: the next question asks.
		unasked,
		/// The reviewer declined for this pull request.
		declined,
		/// The reviewer allowed it for this pull request.
		allowed,
	};

	/// \brief Everything the grant for one turn is decided from.
	struct agent_grant_facts
	{
		/// Which runtime takes the allowance; none for a plain-text harness,
		/// which is given none.
		std::optional<agent_program> Program;
		/// The agent's Scratch folder starts setting.
		bool ScratchStarts = true;
		/// This pull request's choice for this agent, once the reviewer made
		/// one; none follows `ScratchStarts`, including a later change to it.
		std::optional<bool> ScratchChoice;
		/// The agent's Build and run tests setting.
		bool Builds = false;
		/// The turn runs in a checkout the reviewer chose.
		bool OwnCheckout = false;
		/// This pull request's answer to the build question, once asked.
		std::optional<bool> BuildAnswer;

		auto operator<=>(const agent_grant_facts&) const = default;
	};

	/// \brief The grant one turn launches with.
	struct agent_grant
	{
		/// Whether the runtime can be given an allowance at all.
		bool               Applies = false;
		bool               Scratch = false;
		agent_builds_state Builds  = agent_builds_state::off;

		auto operator<=>(const agent_grant&) const = default;
	};

	/// \brief Decides one turn's grant. Pure.
	[[nodiscard]] auto resolve_agent_grant(const agent_grant_facts& aFacts) noexcept -> agent_grant;
	/// \brief Whether the launch carries the build allowance.
	[[nodiscard]] constexpr auto builds_granted(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && aGrant.Builds == agent_builds_state::allowed;
	}
	/// \brief Whether the launch carries the scratch allowance.
	[[nodiscard]] constexpr auto scratch_granted(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && aGrant.Scratch;
	}
	[[nodiscard]] auto grant_level(const agent_grant& aGrant) noexcept -> agent_grant_level;
	/// \brief Whether the next question must ask the build question first.
	[[nodiscard]] constexpr auto build_question_due(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && aGrant.Builds == agent_builds_state::unasked;
	}

	[[nodiscard]] auto stable_name(agent_grant_level aLevel) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(agent_builds_state aState) noexcept -> std::string_view;

	/// \brief The grant line: "Reads only", "Writes only in its scratch
	/// folder", "Builds and tests in its disposable checkout".
	[[nodiscard]] auto agent_grant_line(agent_grant_level aLevel) -> std::string_view;
	/// \brief The footer picker's checkable item.
	inline constexpr std::string_view agent_scratch_item_text = "Scratch folder in this pull request";
	/// \brief The footer picker's builds line for `aAgent`: "Builds and tests
	/// are off for Claude Code", "Builds and tests in its disposable
	/// checkout", and the states between.
	[[nodiscard]] auto agent_builds_line(agent_builds_state aState, std::string_view aAgent) -> std::string;
	/// \brief Why Build and run tests is unavailable in the reviewer's own
	/// checkout, for the setting and the picker.
	inline constexpr std::string_view agent_builds_own_checkout_reason = "BetterPullReview does not let an agent write into your own working tree.";
	/// \brief The question asked once per pull request.
	inline constexpr std::string_view agent_build_question = "Building runs this pull request's code on this computer with your account's rights. Allow for this pull request?";
	/// \brief What the runtime itself enforces of `aGrant`, for the agent's
	/// run details. Never calls a permission rule a sandbox: only a runtime
	/// that enforces a boundary is said to.
	[[nodiscard]] auto agent_grant_enforcement(const agent_grant& aGrant, std::optional<agent_program> aProgram) -> std::string;
	/// \brief The words the agent is told about its scratch folder at
	/// `aPath`, with the limit; `aOverLimit` says the folder is full and no
	/// write allowance was given this turn.
	[[nodiscard]] auto agent_scratch_instruction(std::string_view aPath, bool aOverLimit) -> std::string;
	/// \brief The words the agent is told about building in `aWorkspace`.
	[[nodiscard]] auto agent_builds_instruction(std::string_view aWorkspace) -> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_GRANT_HPP
