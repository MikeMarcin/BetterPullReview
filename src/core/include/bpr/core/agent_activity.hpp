#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ACTIVITY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ACTIVITY_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-2004]], RFC-BPR-0103. What a running turn's agent is
	/// doing, as its runtime's own events say it. Thinking is a state; its
	/// text is never kept.
	enum class agent_doing : std::uint8_t {
		/// Nothing from the runtime yet.
		starting,
		thinking,
		/// `Subject` is the file.
		reading,
		/// `Subject` is the pattern.
		searching,
		/// `Subject` is the command, whole ([[BPR-2285]]).
		running,
		/// `Subject` is the file.
		editing,
		/// `Subject` is the tool's name.
		using_tool,
		writing,
		/// Its own message has ended and its helpers are still out.
		waiting,
	};

	/// \brief One helper's state: a sub-agent or background task the
	/// runtime reported it started.
	enum class agent_helper_state : std::uint8_t {
		running,
		finished,
		failed,
		/// The turn ended while it was still running.
		abandoned,
	};

	[[nodiscard]] auto stable_name(agent_doing aDoing) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(agent_helper_state aState) noexcept -> std::string_view;

	struct agent_helper
	{
		/// The runtime's own identity for it.
		std::string Id;
		/// Its own description, as the runtime reported it.
		std::string        Description;
		agent_helper_state State = agent_helper_state::running;
		/// What it last did, as the runtime reported it; empty for nothing yet.
		std::string Last;
		/// Milliseconds into the turn.
		std::uint64_t                StartedMs = 0U;
		std::optional<std::uint64_t> EndedMs;

		auto operator<=>(const agent_helper&) const = default;
	};

	enum class agent_activity_kind : std::uint8_t {
		/// The agent itself moved on: `Doing`, with `Subject`.
		doing,
		/// `Helper` started; `Subject` is its description.
		helper_started,
		/// `Helper` did something; `Subject` says what, and `Doing` what
		/// kind of thing (a read or a command is counted).
		helper_progress,
		/// `Helper` ended as `Ended`; `Subject`, when not empty, is its last word.
		helper_ended,
	};

	/// \brief One decoded activity event, as the decoder reports it.
	struct agent_activity_update
	{
		agent_activity_kind Kind  = agent_activity_kind::doing;
		agent_doing         Doing = agent_doing::thinking;
		std::string         Subject;
		std::string         Helper;
		agent_helper_state  Ended = agent_helper_state::finished;

		auto operator<=>(const agent_activity_update&) const = default;
	};

	/// The most helper rows a turn keeps; later helpers are counted only.
	inline constexpr std::size_t maximum_agent_helpers = 64U;
	/// The most bytes a subject or a helper's words keep.
	inline constexpr std::size_t maximum_agent_activity_subject_bytes = 240U;
	/// [[BPR-2285]]. The most bytes a command keeps, every line of it: the
	/// Agent tab shows it shortened and opens to the whole of it.
	inline constexpr std::size_t maximum_agent_command_bytes = 4096U;
	/// [[BPR-2007]]. The most commands and scratch files a turn lists.
	inline constexpr std::size_t maximum_agent_activity_listed = 16U;
	/// RFC-BPR-0103: the run is quiet after this long without any frame.
	inline constexpr std::uint64_t agent_quiet_after_ms = 120'000U;

	/// \brief What one turn has done so far.
	struct agent_turn_activity
	{
		agent_doing               Doing = agent_doing::starting;
		std::string               Subject;
		std::vector<agent_helper> Helpers;
		/// Helpers past `maximum_agent_helpers`, counted but not kept.
		std::uint32_t UnlistedHelpers = 0U;
		std::uint32_t FilesRead       = 0U;
		std::uint32_t Commands        = 0U;
		/// [[BPR-2007]], RFC-BPR-0104. The commands run, in order, each once,
		/// the first `maximum_agent_activity_listed` of them.
		std::vector<std::string> CommandsRun;
		/// The files the turn left written in its scratch folder, relative to
		/// it, as the host found them when the turn ended.
		std::vector<std::string> ScratchFiles;
		/// [[BPR-2305]]. The warnings the runtime wrote during the turn (Codex
		/// configuration warnings), as it wrote them, for the folded block.
		std::vector<std::string> Warnings;
		/// When the runtime last sent anything, milliseconds into the turn.
		std::uint64_t LastFrameMs      = 0U;
		std::uint64_t LongestSilenceMs = 0U;

		auto operator<=>(const agent_turn_activity&) const = default;
	};

	/// \brief Applies one update received `aAtMs` into the turn. Pure.
	///
	/// \details Every update is also a frame (`note_agent_frame`). A
	/// progress or end for a helper that was never reported started is
	/// taken as its start, so nothing reported is lost. A helper that has
	/// ended stays ended.
	[[nodiscard]] auto apply_agent_activity(agent_turn_activity aActivity, const agent_activity_update& aUpdate, std::uint64_t aAtMs) -> agent_turn_activity;

	/// \brief The runtime sent a frame at `aAtMs`: the silence before it is
	/// measured and "last activity" starts again.
	[[nodiscard]] auto note_agent_frame(agent_turn_activity aActivity, std::uint64_t aAtMs) -> agent_turn_activity;

	/// \brief The turn ended at `aAtMs`: every helper still running is
	/// abandoned.
	[[nodiscard]] auto end_agent_activity(agent_turn_activity aActivity, std::uint64_t aAtMs) -> agent_turn_activity;

	[[nodiscard]] auto outstanding_agent_helpers(const agent_turn_activity& aActivity) noexcept -> std::size_t;

	/// \brief How many helpers ended each way, for the folded line and the
	/// diagnosis.
	struct agent_helper_tally
	{
		std::uint32_t Started   = 0U;
		std::uint32_t Finished  = 0U;
		std::uint32_t Failed    = 0U;
		std::uint32_t Abandoned = 0U;

		auto operator<=>(const agent_helper_tally&) const = default;
	};

	[[nodiscard]] auto tally_agent_helpers(const agent_turn_activity& aActivity) noexcept -> agent_helper_tally;

	/// \brief RFC-BPR-0103. Whether the quiet notice shows: 120 seconds since
	/// the last frame, and, once the reviewer chose Keep waiting, 120 seconds
	/// since that too. It never stops the run.
	[[nodiscard]] auto agent_run_is_quiet(std::uint64_t aSinceFrameMs, std::optional<std::uint64_t> aSinceKeptWaitingMs) noexcept -> bool;
}

#endif
