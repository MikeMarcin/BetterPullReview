#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_STEERING_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_STEERING_HPP

#include "bpr/core/agent_cost.hpp"
#include "bpr/core/agent_profile.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-2153]], RFC-BPR-0126 Q14/Q17. Whether a running turn can
	/// take a native correction, or why it cannot.
	enum class agent_steer_support : std::uint8_t {
		/// The driver accepts a correction after its current tool call.
		available,
		/// No turn of this conversation is running on a live connection.
		not_running,
		/// The run is not on a kept native connection (a one-shot, a replay,
		/// a fresh path or an external terminal).
		no_connection,
		/// The driver has no qualified steering channel (Codex exec, an
		/// external terminal).
		protocol,
		/// The driver's observed version is older than the qualified one, or
		/// it reported none.
		version,
		/// The run has taken as many corrections as one run may.
		exhausted,
	};

	/// \brief The oldest Claude Code whose stream JSON was observed to accept
	/// a UUID-bearing `priority: "next"` user message and to name it in a
	/// result's `user_message_uuids` (2.1.319, zero-cost probe 2026-11-14).
	inline constexpr std::string_view qualified_claude_steer_version = "2.1.319";

	/// \brief Decides steering from local observations only.
	///
	/// \details Pure; never a paid probe. Codex App Server's `turn/steer`
	/// names the turn it expects, and a server that lacks it refuses the
	/// request outright without consuming the input, so the protocol alone
	/// qualifies it. Claude's stream JSON accepts any user message, so only
	/// a version that reports the input identities a correction is settled
	/// by qualifies; an unreported version does not.
	/// \param aProtocol The running turn's protocol.
	/// \param aConnected Whether the turn runs on a kept native connection.
	/// \param aToolVersion The version the runtime reported for this
	/// connection (Claude's `claude_code_version`), empty when none.
	[[nodiscard]] auto agent_steer_support_of(agent_protocol aProtocol, bool aConnected, std::string_view aToolVersion) noexcept
		-> agent_steer_support;

	/// \brief Why Steer is unavailable, in the reviewer's words; empty when
	/// it is available.
	[[nodiscard]] auto agent_steer_unavailable_words(agent_steer_support aSupport) noexcept -> std::string_view;

	/// \brief What became of one correction.
	enum class agent_steer_disposition : std::uint8_t {
		/// Refused before any byte was written: it was never sent.
		unsent,
		/// Written to the runtime; its terminal disposition is still open.
		sent,
		/// The runtime refused it definitively without consuming it (Codex
		/// App Server's error reply to `turn/steer`).
		refused,
		/// The runtime correlated it to the run: Claude named it in a
		/// result's `user_message_uuids`; Codex accepted it for the turn
		/// that then completed.
		consumed,
		/// Written, and the run ended without correlating it. Never resent.
		unknown,
	};

	/// \brief [[BPR-2153]] AC07. The corrections of one run, by how they
	/// ended. Each is counted once, under its terminal disposition.
	struct agent_steer_tally
	{
		std::uint32_t Sent     = 0U;
		std::uint32_t Consumed = 0U;
		std::uint32_t Refused  = 0U;
		std::uint32_t Unknown  = 0U;

		auto operator<=>(const agent_steer_tally&) const = default;
	};

	/// \brief The most corrections one run may hand off, matching the
	/// queue's sixteen pending inputs (RFC-BPR-0126 Q6).
	inline constexpr std::uint32_t maximum_steers_per_run = 16U;

	/// \brief A correction's input identity, UUID-shaped as Claude requires.
	///
	/// \details Pure and deterministic: the conversation's native turn and
	/// the correction's ordinal within that run. Native turns only grow
	/// within a conversation, so no two corrections of one conversation
	/// share an identity; it is distinct from any result envelope UUID.
	[[nodiscard]] auto agent_steer_identity(std::uint64_t aNativeTurn, std::uint32_t aOrdinal) -> std::string;

	/// \brief The run's cost after one more reported figure from the same
	/// runtime session.
	///
	/// \details [[BPR-2153]] AC07. A steered Claude run can end in more than
	/// one result (a correction consumed by a successor native turn). Claude
	/// reports `total_cost_usd` cumulatively over its session, so the run's
	/// figure is its last report, and the existing turn accounting
	/// (`account_agent_turn_cost`) then counts every part once. A later
	/// figure below an earlier one means the runtime started counting again
	/// mid-run: the run's cost is unknown from then on, never a sum and
	/// never a guess. A part that reports nothing leaves the figure as it
	/// was.
	struct agent_run_cost_reading
	{
		std::optional<agent_cost> Figure;
		bool                      Unknown = false;
		std::uint32_t             Reports = 0U;

		auto operator<=>(const agent_run_cost_reading&) const = default;
	};

	[[nodiscard]] auto read_run_cost_report(const agent_run_cost_reading& aReading, std::optional<agent_cost> aReported) -> agent_run_cost_reading;
} // namespace bpr::core

#endif
