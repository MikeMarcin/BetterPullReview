#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_PROFILE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_PROFILE_HPP

#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	struct agent_profile_id_tag;

	/// \brief Names one installed agent tool.
	using agent_profile_id = strong_id<agent_profile_id_tag>;

	/// \brief The machine-readable interface an agent tool exposes.
	///
	/// \details A closed set. `external_terminal` is mandatory rather than a
	/// fallback of convenience: a corporate harness that exposes no supported
	/// interface must still be launchable, and pretending otherwise would push
	/// reviewers onto a personal provider their host forbids.
	enum class agent_protocol : std::uint8_t {
		codex_app_server,
		codex_jsonl_exec,
		claude_stream_json,
		external_terminal,
	};

	inline constexpr std::size_t agent_protocol_count = 4U;

	static_assert(
		static_cast<std::size_t>(agent_protocol::external_terminal) + 1U
			== agent_protocol_count,
		"agent_protocol_count must cover every declared protocol");

	[[nodiscard]] auto stable_name(agent_protocol aProtocol) noexcept
		-> std::string_view;

	/// \brief Where an agent is allowed to work.
	enum class agent_workspace_policy : std::uint8_t {
		/// A disposable workspace pinned to the exact review head, isolated
		/// from the active worktree. The default for bounded review.
		disposable_review_workspace,
		/// The reviewer's existing checkout, for repository-wide context. A
		/// deliberate selection rather than something a profile may assume.
		existing_checkout,
	};

	[[nodiscard]] auto stable_name(agent_workspace_policy aPolicy) noexcept
		-> std::string_view;

	/// \brief The longest environment variable name a profile may allow.
	inline constexpr std::size_t maximum_environment_name_bytes = 256U;
	/// \brief The most environment names one profile may allow.
	inline constexpr std::size_t maximum_environment_allowlist = 64U;
	/// \brief The longest display name or executable a profile may carry.
	inline constexpr std::size_t maximum_agent_text_bytes = 1024U;

	/// \brief One separately installed agent tool BetterPullReview may launch.
	///
	/// \details **There is no field here a credential could occupy.** Not a
	/// token, not a key, not a generic string map a caller could put one in.
	/// `AllowedEnvironment` is a list of variable *names* the host may pass
	/// through from the reviewer's own environment — it carries no values, so
	/// there is nothing here for a secret to be written into.
	///
	/// That absence is what makes "BetterPullReview passes no credential" a
	/// property rather than a promise. The convenience it forecloses is
	/// obvious and would be the first thing added: the agent needs GitHub
	/// access, the application holds a GitHub token, so pass it along and save
	/// a setup step. That would carry the reviewer's own authority into a
	/// process this product does not audit, and the RFC's non-goals say
	/// plainly that it cannot claim to audit one.
	///
	/// The agent authenticates as itself, with whatever the reviewer already
	/// configured for it, and BetterPullReview never learns what that is.
	struct agent_profile
	{
		agent_profile_id       Id;
		std::string            DisplayName;
		std::string            Executable;
		agent_protocol         Protocol = agent_protocol::external_terminal;
		agent_workspace_policy Workspace
			= agent_workspace_policy::disposable_review_workspace;
		/// Variable names, never values. An allowlist rather than an
		/// inheritance: a host that passed the reviewer's whole environment
		/// would pass every secret in it.
		std::vector<std::string> AllowedEnvironment;

		auto operator<=>(const agent_profile&) const = default;
	};

	/// \brief Checks whether the supplied profile satisfies its contract.
	[[nodiscard]] auto valid(const agent_profile& aProfile) noexcept -> bool;

	/// \brief What one Git host allows.
	///
	/// \details A host names the profiles it permits and, optionally, the one
	/// it requires. A required profile is not merely preferred: selecting any
	/// other on that host is refused, which is how a corporate host keeps
	/// review context inside its own harness.
	struct agent_host_policy
	{
		git_host_id                     Host;
		std::vector<agent_profile_id>   Allowed;
		std::optional<agent_profile_id> Required;

		auto operator<=>(const agent_host_policy&) const = default;
	};

	/// \brief Why an explicit invocation was refused.
	enum class agent_invocation_refusal : std::uint8_t {
		none,
		/// The host has no policy at all. Nothing is permitted until one is
		/// mapped, which is the opposite of the default a permissive reading
		/// would choose.
		host_not_mapped,
		/// The host maps a policy and this profile is not in it.
		profile_forbidden_on_host,
		/// The host requires a different profile.
		profile_not_required_one,
		/// The profile itself is malformed.
		profile_invalid,
		/// No such profile is installed.
		profile_unknown,
	};

	[[nodiscard]] auto stable_name(agent_invocation_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief The verdict on one explicit invocation.
	///
	/// \details A refusal names the host that produced it. "Not allowed" is
	/// not an answer a reviewer can act on; "your corporate host requires the
	/// corporate harness" is.
	struct agent_invocation_verdict
	{
		bool                       Permitted = false;
		agent_invocation_refusal   Refusal   = agent_invocation_refusal::none;
		std::optional<git_host_id> RefusedBy;
		/// The profile the host requires, when that is why this was refused.
		std::optional<agent_profile_id> RequiredInstead;

		auto operator<=>(const agent_invocation_verdict&) const = default;
	};

	/// \brief Decides whether one profile may be invoked on one host.
	///
	/// \details Decided from the supplied values alone. No process is started,
	/// no clock is read, and no provider is asked — legality is a property of
	/// the policy and the profile, and a decision that needed any of those
	/// could not be replayed.
	///
	/// An absent policy refuses everything. A host nobody has mapped is a host
	/// nobody has thought about, and permitting an agent there would put review
	/// context into a tool the reviewer's organisation never approved.
	[[nodiscard]] auto decide_agent_invocation(
		const agent_profile&                    aProfile,
		const std::optional<agent_host_policy>& aPolicy) noexcept
		-> agent_invocation_verdict;

	/// \brief Finds the policy for one host, if it is mapped at all.
	[[nodiscard]] auto policy_for_host(
		std::span<const agent_host_policy> aPolicies,
		git_host_id                        aHost) noexcept
		-> std::optional<agent_host_policy>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_PROFILE_HPP
