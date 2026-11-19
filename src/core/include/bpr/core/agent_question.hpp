#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUESTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUESTION_HPP

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/returning_review.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-2002]], RFC-BPR-0102. What one question to an agent is
	/// about.
	///
	/// \details Decided by the core from what the reviewer did, never chosen
	/// from a list. The reviewer's reading position is not a subject: it is
	/// sent beside the subject as where the reviewer is looking, and it never
	/// narrows a question whose subject is the pull request.
	enum class agent_question_subject : std::uint8_t {
		/// The whole change; every changed file is in scope.
		pull_request,
		/// One file of the change, named by a suggestion; the rest of the
		/// change is context.
		file,
		/// The lines attached to the question, with their text.
		lines,
		/// A review thread or a private note ([[BPR-1773]]), unchanged.
		conversation,
	};

	inline constexpr std::size_t agent_question_subject_count = 4U;

	static_assert(
		static_cast<std::size_t>(agent_question_subject::conversation) + 1U
			== agent_question_subject_count,
		"agent_question_subject_count must cover every declared subject");

	[[nodiscard]] auto stable_name(agent_question_subject aSubject) noexcept
		-> std::string_view;

	/// \brief The sentence the agent is told for one subject.
	///
	/// \details Fixed per subject so the agent reads the same words for the
	/// same subject whatever the question is.
	[[nodiscard]] auto agent_question_statement(agent_question_subject aSubject) noexcept
		-> std::string_view;

	/// \brief What a suggested prompt names as its subject.
	enum class agent_suggested_subject : std::uint8_t {
		pull_request,
		open_file,
		selected_lines,
	};

	/// \brief What the composer knows when a question is sent.
	struct agent_question_facts
	{
		/// The selected lines are attached to the question (the chip shows).
		bool LinesAttached = false;
		/// Lines are selected in the diff, attached or not.
		bool LinesSelected = false;
		/// A conversation chip is attached.
		bool ConversationAttached = false;
		/// The suggestion pressed, if the question is one; none for a typed
		/// question.
		std::optional<agent_suggested_subject> Suggested;
		/// The suggestion asks for a review rather than a question.
		bool Review = false;
		/// [[BPR-2280]]. The suggestion asks for the pull request's
		/// description (`prompt_ask::description`).
		bool Description = false;
		/// The path of the file open in Review; empty when none is.
		std::string OpenFile;
		/// RFC-BPR-0116. A returning question was pressed: What changed since
		/// my review, or Was this addressed?
		std::optional<returning_operation> Returning;
		/// RFC-BPR-0117 ([[BPR-2076]]). Explain this file was pressed on this
		/// file; empty otherwise.
		std::string Explain;
		/// RFC-BPR-0118 ([[BPR-2078]]). Suggest the change was pressed on a
		/// draft comment's lines in this file; empty otherwise.
		std::string SuggestChange;

		auto operator<=>(const agent_question_facts&) const = default;
	};

	/// \brief One question's subject and the explicit action that sends it.
	struct agent_question
	{
		agent_question_subject Subject    = agent_question_subject::pull_request;
		agent_entry_point      EntryPoint = agent_entry_point::open_full_agent_session;
		/// The file subject's path; empty for every other subject.
		std::string File;
		/// RFC-BPR-0118. The comment workshop operation the question is,
		/// with `workshop_comment`; none for every other question.
		std::optional<workshop_operation> Operation;
		/// [[BPR-2280]]. The answer is the pull request's description: it is
		/// asked with no comment guidance, and offered to the description
		/// editor rather than as a comment.
		bool Description = false;

		auto operator<=>(const agent_question&) const = default;
	};

	/// \brief Decides one question's subject and entry point.
	///
	/// \details Pure. In order:
	///
	/// 0. RFC-BPR-0118. Suggest the change is about the draft comment's
	///    lines (`workshop_comment` with `suggest_change`) whatever else is
	///    attached or selected.
	///    RFC-BPR-0117. Explain this file is about that file
	///    (`analyze_current_file`) whatever is attached or selected.
	///    RFC-BPR-0116. What changed since my review is about the pull
	///    request (`analyze_latest_upload`) whatever is attached; Was this
	///    addressed? is about the attached conversation
	///    (`evaluate_proposed_fix`) and is not sent without one.
	/// 1. A conversation chip makes the conversation the subject
	///    (`ask_about_conversation`).
	/// 2. A suggestion names its own subject: the pull request, the open file
	///    or the selected lines. A review suggestion is sent with
	///    `run_ai_review`, which needs no selection; [[BPR-2280]] a
	///    description suggestion is about the pull request and its answer is
	///    a description (`agent_question::Description`); selected lines are sent
	///    with `ask_about_selection`; any other suggestion about the pull
	///    request with `open_full_agent_session`.
	/// 3. A typed question is about the attached lines when they are
	///    attached, and otherwise about the pull request.
	///
	/// \returns Nothing for a suggestion that cannot be sent as it stands:
	/// one naming the open file with none open, or the selected lines with
	/// none selected. Such a suggestion is not offered.
	[[nodiscard]] auto decide_agent_question(const agent_question_facts& aFacts)
		-> std::optional<agent_question>;

	/// \brief The longest text attached lines may carry.
	inline constexpr std::size_t maximum_subject_text_bytes = 32U * 1024U;
	/// The longest path a subject or a reading position may name.
	inline constexpr std::size_t maximum_subject_path_bytes = 4096U;
	/// The most changed files a pull request subject lists as in scope.
	inline constexpr std::size_t maximum_subject_files = 4'096U;

	/// \brief The subject as one supplement carries it.
	///
	/// \details `Path` names the file of a file or lines subject. A lines
	/// subject engages its one-based `FirstLine` and `LastLine`, which side
	/// (`Before`, the change's base) and the lines' `Text`. A pull request
	/// subject lists `InScope`, every changed file.
	struct supplement_subject
	{
		agent_question_subject   Subject = agent_question_subject::pull_request;
		std::string              Path;
		std::uint32_t            FirstLine = 0U;
		std::uint32_t            LastLine  = 0U;
		bool                     Before    = false;
		std::string              Text;
		std::vector<std::string> InScope;

		auto operator<=>(const supplement_subject&) const = default;
	};

	/// \brief Whether a subject engages exactly what its kind means, within
	/// its bounds.
	[[nodiscard]] auto valid(const supplement_subject& aSubject) -> bool;

	/// \brief Where the reviewer is looking, named as such: the open file
	/// and, when a line is selected, that line (one-based).
	struct supplement_looking_at
	{
		std::string                  Path;
		std::optional<std::uint32_t> Line;
		bool                         Before = false;

		auto operator<=>(const supplement_looking_at&) const = default;
	};

	[[nodiscard]] auto valid(const supplement_looking_at& aLooking) -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUESTION_HPP
