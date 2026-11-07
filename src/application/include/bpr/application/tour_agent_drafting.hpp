#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_AGENT_DRAFTING_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TOUR_AGENT_DRAFTING_HPP

#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1900]]. What the tour session asks for a whole-tour draft,
/// how it reads the answer, and what a read-only run's tool uses become in
/// its transcript.
namespace bpr::application {
	/// \brief The schema a whole-tour draft answers in.
	inline constexpr std::string_view tour_draft_schema = "bpr.tour-draft.v1";
	/// \brief The most stops a draft is asked for.
	inline constexpr std::size_t requested_tour_stops = 12U;

	/// \brief A file of the pull request as the draft is told about it.
	struct tour_draft_file
	{
		std::string Path;
		/// `added`, `modified`, `removed`, `renamed`.
		std::string Change;
		/// [[BPR-1900]]. The head lines of each hunk, when GitHub gave the
		/// patch; the request asks for stops on them.
		std::optional<std::vector<std::pair<std::uint32_t, std::uint32_t>>> Hunks = std::nullopt;
	};

	/// \brief The whole-tour request: the read-only working rules, the answer
	/// schema, and the pull request and the tour so far as untrusted evidence.
	/// Nothing when the evidence is not valid UTF-8 or too large.
	[[nodiscard]] auto build_tour_draft_prompt(std::string_view aTitle, std::string_view aDescription, std::span<const tour_draft_file> aFiles, const core::review_tour& aDraft) -> std::optional<std::string>;

	/// \brief What a read-only Draft from PR context or Suggest an outline
	/// request is prefixed with: it may read and search the repository, and
	/// its last message is the JSON object alone.
	[[nodiscard]] auto read_only_writing_preamble() -> std::string_view;

	/// \brief Reads a whole-tour answer from the agent's last message: one
	/// JSON object of `tour_draft_schema`, alone or in one fenced block.
	/// Nothing for anything else. Lines and paths are not checked here; the
	/// core anchors them (`core::propose_tour`).
	[[nodiscard]] auto decode_tour_draft_answer(std::string_view aText) -> std::optional<core::tour_draft_answer>;

	/// \brief A read-only run's tool uses as transcript lines, with the
	/// workspace's own folder taken off the front of a path.
	[[nodiscard]] auto tour_transcript_of(const std::vector<std::pair<std::string, std::string>>& aActivity, std::string_view aWorkspace) -> std::vector<core::tour_transcript_entry>;
}

#endif
