#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_SEND_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_SEND_HPP

#include "bpr/core/agent_profile.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_policy.hpp"
#include "bpr/core/review_cycle_share.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief Send to Agent (RFC-BPR-0060, [[BPR-1180]]): one frozen round handed
/// to one agent profile the reviewer chose, under the policies that profile
/// already answers to.
/// \details The adapter is a thin client of the round. What it sends is the
/// round's readable document, the bytes Copy and Save give. Whether it may
/// send is the round's channel decision (`agent_adapter`) and the agent host
/// policy every launch of that profile answers to; a refusal sends nothing
/// and offers no other profile or route in its place. A session is never
/// chosen for the reviewer: a send starts a new one, said as such, or
/// continues exactly one session that the same harness reported to an earlier
/// send of this cycle. What the adapter saw is recorded as delivery
/// observations. A finished turn is said apart from them. Nothing here sends
/// again, falls back, captures a revision or verifies a note.
namespace bpr::core {
	/// The largest readable document a send hands over: half the agent input
	/// bound, so framing it as a protocol message (which escapes it) still fits.
	inline constexpr std::uint64_t maximum_round_send_bytes = 4U * 1024U * 1024U;
	/// The longest session name a send binds to or records.
	inline constexpr std::size_t maximum_send_session_bytes = 128U;
	/// \brief A session name as a harness reports it: 1 to
	/// `maximum_send_session_bytes` of ASCII letters, digits, `.`, `_` and
	/// `-`, starting with a letter or digit.
	[[nodiscard]] auto send_session_name(std::string_view aName) noexcept -> bool;
	/// \brief Whether a harness of this protocol can be told to continue an
	/// existing session: Claude stream JSON (`--resume`) only.
	[[nodiscard]] auto continues_sessions(agent_protocol aProtocol) noexcept -> bool;

	/// \brief A session as a recipient acknowledgment records it: the
	/// harness protocol and the session it reported, as `protocol/session`.
	/// Empty when the name is not a session name.
	[[nodiscard]] auto send_session_record(agent_protocol aProtocol, std::string_view aSession) -> std::string;
	/// \brief The protocol and session of a recorded acknowledgment; nothing
	/// for any other text.
	[[nodiscard]] auto parse_send_session_record(std::string_view aRecord) -> std::optional<std::pair<agent_protocol, std::string>>;

	/// \brief The session one send runs in.
	struct cycle_send_session
	{
		/// The exact session to continue; none starts a new session.
		std::optional<std::string> Continue;
		auto                       operator<=>(const cycle_send_session&) const = default;
	};
	/// \brief What the reviewer chose: one installed profile, one session.
	struct cycle_send_request
	{
		agent_profile_id   Profile;
		cycle_send_session Session;
		auto               operator<=>(const cycle_send_request&) const = default;
	};

	/// The agent hosts a Send answers to, as the composition maps them.
	inline constexpr git_host_id send_github_host{ 1U };
	inline constexpr git_host_id send_enterprise_host{ 2U };
	/// \brief The agent host policy a cycle's Send answers to: the corporate
	/// host's when the source is known enterprise material or the reviewer
	/// acts under an account on a host other than github.com; GitHub.com's
	/// otherwise. A local folder of unknown origin is gated by its channel
	/// decision, which allows nothing until it is configured.
	[[nodiscard]] auto cycle_send_policy(const cycle_workspace_origin& aOrigin, std::optional<std::string_view> aAccountHost) -> agent_host_policy;

	/// \brief One installed profile as Send lists it: refused ones stay
	/// listed, refused.
	struct cycle_send_offer
	{
		agent_profile_id Profile;
		std::string      DisplayName;
		/// [[BPR-1282]]. Its harness as a person names it (`harness_name`),
		/// and the protocol that says which harness that is.
		std::string              Harness;
		agent_protocol           Protocol = agent_protocol::external_terminal;
		agent_invocation_verdict Verdict;
		/// Its harness can continue a session.
		bool Continues = false;
		/// Sessions its harness reported to earlier sends of this cycle,
		/// newest first; empty when it cannot continue one.
		std::vector<std::string> Sessions;
		[[nodiscard]] auto       offered() const noexcept -> bool
		{
			return Verdict.Permitted;
		}
		auto operator<=>(const cycle_send_offer&) const = default;
	};
	/// \brief Sessions the harness of `aProtocol` reported to this cycle's
	/// sends, newest first, each once.
	[[nodiscard]] auto send_sessions(std::span<const cycle_delivery_record> aDeliveries, agent_protocol aProtocol) -> std::vector<std::string>;
	/// \brief Every installed profile, in order, with the host's verdict.
	[[nodiscard]] auto send_offers(std::span<const agent_profile> aProfiles, const agent_host_policy& aPolicy, std::span<const cycle_delivery_record> aDeliveries)
		-> std::vector<cycle_send_offer>;

	enum class cycle_send_problem : std::uint8_t {
		/// The round's channel decision refused it; see `Channel`.
		channel_refused,
		/// No such profile is installed.
		profile_unknown,
		/// The host policy refuses this profile; see `Verdict`.
		profile_refused,
		/// This profile's harness cannot continue a session.
		session_not_continuable,
		/// Not a session this harness reported to an earlier send of this cycle.
		session_unknown,
	};
	[[nodiscard]] auto stable_name(cycle_send_problem aValue) noexcept -> std::string_view;
	struct cycle_send_refusal
	{
		cycle_send_problem       Problem = cycle_send_problem::profile_unknown;
		cycle_authorization      Channel = cycle_authorization::allowed;
		agent_invocation_verdict Verdict;
		auto                     operator<=>(const cycle_send_refusal&) const = default;
	};
	/// \brief Decides one send before anything is frozen or started.
	/// `aChannel` is the round's channel decision for `agent_adapter`.
	[[nodiscard]] auto decide_send(cycle_authorization aChannel, std::span<const agent_profile> aProfiles, const agent_host_policy& aPolicy,
								   std::span<const cycle_delivery_record> aDeliveries, const cycle_send_request& aRequest) -> std::optional<cycle_send_refusal>;

	/// \brief How one send ended, as the adapter saw it.
	enum class cycle_send_end : std::uint8_t {
		/// The harness finished its turn.
		completed,
		/// Nothing was started: no harness, no workspace, or input over its bound.
		not_started,
		/// The harness stopped without finishing its turn: an exit, a failure
		/// it reported or output it did not frame.
		stopped,
		/// It did not finish in time and was ended.
		timed_out,
		/// The reviewer stopped it.
		cancelled,
	};
	[[nodiscard]] auto stable_name(cycle_send_end aValue) noexcept -> std::string_view;
	/// \brief What the adapter observed about one send.
	struct cycle_send_result
	{
		cycle_send_end End = cycle_send_end::not_started;
		/// The session the harness reported, when it reported a session name.
		std::optional<std::string> Session;
		/// The turn's reply text, in order; complete only when completed.
		std::string Reply;
		auto        operator<=>(const cycle_send_result&) const = default;
	};
	/// \brief What one send records about `aRound`, in order: `failed` when
	/// nothing was started; otherwise `accepted_by_adapter` with the bytes
	/// handed over, `recipient_acknowledged` (with its session) when the
	/// harness reported one, and `uncertain` when the turn did not finish.
	/// A finished turn adds nothing: finishing is not receipt or reading.
	[[nodiscard]] auto send_observations(const cycle_round& aRound, const cycle_delivery_output& aOutput, agent_protocol aProtocol, const cycle_send_result& aResult)
		-> std::vector<cycle_delivery_observation>;

	/// \brief The structured response a finished turn's reply holds: the whole
	/// reply when it is a JSON object, or its one ```json block. Nothing for
	/// prose, or for more than one block; the response model judges the rest.
	[[nodiscard]] auto reply_response(std::string_view aReply) -> std::optional<std::string_view>;
}

#endif
