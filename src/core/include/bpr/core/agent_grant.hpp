#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_GRANT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_GRANT_HPP

#include "bpr/core/agent_wrapper.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-2007]], RFC-BPR-0104 as amended 2026-11-11 (owner look
	/// e97c6a3d: "This needs to be a configurable setting"). The most a
	/// scratch folder may hold, in MiB: a setting, 256 to begin with. A turn
	/// that starts with the folder at or past it gets no write allowance
	/// there; a turn that passes it is stopped after the step that did.
	inline constexpr std::uint32_t default_agent_scratch_limit_mib = 256U;
	inline constexpr std::uint32_t minimum_agent_scratch_limit_mib = 16U;
	inline constexpr std::uint32_t maximum_agent_scratch_limit_mib = 65536U;
	/// \brief The setting as it applies: zero is the default, and anything
	/// else is held between the minimum and the maximum.
	[[nodiscard]] constexpr auto agent_scratch_limit_mib(const std::uint32_t aMiB) noexcept -> std::uint32_t
	{
		if (aMiB == 0U) {
			return default_agent_scratch_limit_mib;
		}
		return aMiB < minimum_agent_scratch_limit_mib ? minimum_agent_scratch_limit_mib : aMiB > maximum_agent_scratch_limit_mib ? maximum_agent_scratch_limit_mib
																																 : aMiB;
	}
	[[nodiscard]] constexpr auto agent_scratch_limit_bytes(const std::uint32_t aMiB) noexcept -> std::uint64_t
	{
		return static_cast<std::uint64_t>(agent_scratch_limit_mib(aMiB)) * 1024ULL * 1024ULL;
	}
	/// \brief The default bound in bytes.
	inline constexpr std::uint64_t maximum_agent_scratch_bytes = agent_scratch_limit_bytes(default_agent_scratch_limit_mib);
	/// \brief Scratch folders untouched this many days are removed, unless
	/// the setting says otherwise; zero keeps them.
	inline constexpr std::uint32_t default_agent_scratch_keep_days = 30U;
	inline constexpr std::uint32_t maximum_agent_scratch_keep_days = 3650U;

	/// \brief Review finding 1 (2026-11-11). Why BPR passes no grant to an
	/// agent at all: what it would pass could widen the reviewer's own
	/// configuration, so it passes nothing.
	enum class agent_grant_refusal : std::uint8_t {
		/// Codex has the reviewer's own permissions profile, which BPR's
		/// would replace.
		codex_own_permissions,
		/// Codex asks before running commands ("untrusted", or an approval
		/// BPR cannot read); BPR's would not ask.
		codex_asks_approval,
		/// Codex's configuration could not be read.
		codex_config_unreadable,
		/// The agent's own arguments set its sandbox, approvals or profile.
		custom_arguments,
	};

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
		/// This pull request's answer to the build question, or its toggle,
		/// once either was given.
		std::optional<bool> BuildAnswer;
		/// Review finding 4. A scratch folder can be made and named here.
		bool ScratchPlace = true;
		/// The session's folder was at or past its limit when last measured.
		bool ScratchFull = false;
		/// Nothing is granted, and why.
		std::optional<agent_grant_refusal> Refusal;

		auto operator<=>(const agent_grant_facts&) const = default;
	};

	/// \brief The grant one turn launches with.
	struct agent_grant
	{
		/// Whether the runtime can be given an allowance at all.
		bool Applies = false;
		/// The pull request's choice (or the agent's starting state).
		bool                               Scratch      = false;
		agent_builds_state                 Builds       = agent_builds_state::off;
		bool                               ScratchPlace = true;
		bool                               ScratchFull  = false;
		std::optional<agent_grant_refusal> Refusal;

		auto operator<=>(const agent_grant&) const = default;
	};

	/// \brief Decides one turn's grant. Pure.
	[[nodiscard]] auto resolve_agent_grant(const agent_grant_facts& aFacts) noexcept -> agent_grant;
	/// \brief Whether the launch carries the build allowance.
	[[nodiscard]] constexpr auto builds_granted(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && !aGrant.Refusal && aGrant.Builds == agent_builds_state::allowed;
	}
	/// \brief Whether the launch carries a scratch folder (written to, or,
	/// when full, only run from).
	[[nodiscard]] constexpr auto scratch_granted(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && !aGrant.Refusal && aGrant.Scratch && aGrant.ScratchPlace;
	}
	/// \brief Whether the launch lets the agent write in its folder.
	[[nodiscard]] constexpr auto scratch_writable(const agent_grant& aGrant) noexcept -> bool
	{
		return scratch_granted(aGrant) && !aGrant.ScratchFull;
	}
	[[nodiscard]] auto grant_level(const agent_grant& aGrant) noexcept -> agent_grant_level;
	/// \brief Whether the next question must ask the build question first.
	[[nodiscard]] constexpr auto build_question_due(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && !aGrant.Refusal && aGrant.Builds == agent_builds_state::unasked;
	}

	[[nodiscard]] auto stable_name(agent_grant_level aLevel) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(agent_builds_state aState) noexcept -> std::string_view;

	/// \brief Owner look e97c6a3d (2026-11-11): the footer picker's menu
	/// holds two toggles for this pull request, and every explanation is in
	/// their tooltips. "Use scratch folder", or "Use scratch folder (full)".
	[[nodiscard]] auto                agent_scratch_toggle_text(const agent_grant& aGrant) -> std::string;
	inline constexpr std::string_view agent_builds_toggle_text = "Allow build and run tests";
	[[nodiscard]] constexpr auto      agent_scratch_toggle_enabled(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && !aGrant.Refusal && aGrant.ScratchPlace;
	}
	/// \brief Checked only while the launch carries the folder.
	[[nodiscard]] constexpr auto agent_scratch_toggle_checked(const agent_grant& aGrant) noexcept -> bool
	{
		return scratch_granted(aGrant);
	}
	[[nodiscard]] constexpr auto agent_builds_toggle_enabled(const agent_grant& aGrant) noexcept -> bool
	{
		return aGrant.Applies && !aGrant.Refusal && aGrant.Builds != agent_builds_state::own_checkout;
	}
	/// \brief Checked only while the launch carries builds.
	[[nodiscard]] constexpr auto agent_builds_toggle_checked(const agent_grant& aGrant) noexcept -> bool
	{
		return builds_granted(aGrant);
	}
	/// \brief What the scratch toggle means for this agent now: what it
	/// allows and who enforces it, or why it is off or unavailable. Never
	/// calls a permission rule a sandbox.
	[[nodiscard]] auto agent_scratch_tooltip(const agent_grant& aGrant, std::optional<agent_program> aProgram, std::uint32_t aLimitMiB) -> std::string;
	/// \brief What the builds toggle means for this agent now. Review
	/// finding 6: Claude's build rules are disclosed as unconfined.
	[[nodiscard]] auto agent_builds_tooltip(const agent_grant& aGrant, std::optional<agent_program> aProgram) -> std::string;
	[[nodiscard]] auto agent_grant_refusal_reason(agent_grant_refusal aRefusal) -> std::string_view;
	[[nodiscard]] auto stable_name(agent_grant_refusal aRefusal) noexcept -> std::string_view;
	/// \brief Review finding 1. Whether BPR's Codex grant may be passed: it
	/// names its own permissions profile, which replaces the configured one,
	/// and asks for no approval, so it is passed only when the reviewer's
	/// configuration has no profile of its own and does not ask before
	/// commands. `aConfigs` are the configuration files' texts (none: none
	/// exists); `aUnreadable`, one exists and could not be read;
	/// `aArguments`, the agent's own arguments. A conservative scan: a key
	/// it cannot judge refuses. Pure.
	[[nodiscard]] auto codex_grant_refusal(std::span<const std::string_view> aConfigs, bool aUnreadable, std::span<const std::string> aArguments) -> std::optional<agent_grant_refusal>;
	/// \brief Why Build and run tests is unavailable in the reviewer's own
	/// checkout, for the setting and the picker.
	inline constexpr std::string_view agent_builds_own_checkout_reason = "BetterPullReview does not let an agent write into your own working tree.";
	/// \brief The question asked once per pull request.
	inline constexpr std::string_view agent_build_question = "Building runs this pull request's code on this computer with your account's rights. Allow for this pull request?";
	/// \brief The words the agent is told about its scratch folder at
	/// `aPath`, with the limit and the programs that run its files;
	/// `aOverLimit` says the folder is full and no write allowance was given
	/// this turn.
	[[nodiscard]] auto agent_scratch_instruction(std::string_view aPath, bool aOverLimit, std::uint32_t aLimitMiB) -> std::string;
	/// \brief The words the agent is told about building in `aWorkspace`.
	[[nodiscard]] auto agent_builds_instruction(std::string_view aWorkspace) -> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_GRANT_HPP
