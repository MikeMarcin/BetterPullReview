#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_CHECK_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_CHECK_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0120 ([[BPR-2189]]), as rewritten without evidence on
/// 2026-11-15. The agent plans a check of a local draft's candidate, the
/// author presses Run, and the agent runs it in a check copy of that
/// candidate; the run ends as a report read in a file tab.
/// \details Pure decisions over what the agent answered and what the host
/// observed: the plan an answer holds, whether Run is available and why not,
/// the report a run ends in, and the text of both requests. Nothing here
/// names a folder, starts a program or reads a worktree; the host makes the
/// check copy and hands in what it saw. Nothing is recorded beyond the
/// conversation, scored or published.
namespace bpr::core {
	/// \brief The built-in prompt that briefs a plan and a run (P02).
	inline constexpr std::string_view local_check_purpose = "local-draft.run-check";

	/// \brief The most commands one plan or report lists.
	inline constexpr std::size_t maximum_check_commands = 16U;
	/// \brief The most bytes of one command or of one line of what it shows.
	inline constexpr std::size_t maximum_check_command_bytes = 1024U;
	/// \brief The most output lines a report keeps, and bytes of each.
	inline constexpr std::size_t maximum_check_output_lines = 40U;
	inline constexpr std::size_t maximum_check_line_bytes   = 512U;
	inline constexpr std::size_t maximum_check_references   = 16U;
	/// \brief The most changed tracked files a report lists.
	inline constexpr std::size_t maximum_check_changed_files = 64U;
	/// \brief The bound of a plan's or report's prose fields.
	inline constexpr std::size_t maximum_check_prose_bytes = 4096U;

	/// \brief One command of a plan: what runs and what it shows.
	struct local_check_step
	{
		std::string Command;
		std::string Shows;
		auto        operator<=>(const local_check_step&) const = default;
	};

	/// \brief Where a plan stands.
	enum class local_check_state : std::uint8_t {
		/// Waiting for Run or Discard.
		planned,
		discarded,
		/// Run was pressed; a report follows it.
		ran,
	};
	[[nodiscard]] auto name(local_check_state aState) noexcept -> std::string_view;

	/// \brief A plan card: the commands in order, the grants it needs, and
	/// what it will not cover. Nothing has run.
	struct local_check_plan
	{
		std::vector<local_check_step> Steps;
		/// RFC-BPR-0104's two grants, as the plan needs them.
		bool        NeedsScratch = false;
		bool        NeedsBuilds  = false;
		std::string NotCovered;
		/// The candidate the plan was made on.
		std::uint64_t     Candidate                                  = 0;
		local_check_state State                                      = local_check_state::planned;
		auto              operator<=>(const local_check_plan&) const = default;
	};

	/// \brief A report's one outcome word, stated as the agent's report of its run.
	enum class local_check_outcome : std::uint8_t {
		passed,
		failed,
		couldnt_tell,
		didnt_run,
		stopped,
	};
	[[nodiscard]] auto name(local_check_outcome aOutcome) noexcept -> std::string_view;
	/// \brief "Passed", "Failed", "Couldn't tell", "Didn't run", "Stopped".
	[[nodiscard]] auto outcome_words(local_check_outcome aOutcome) noexcept -> std::string_view;

	/// \brief One command the run ran.
	struct local_check_command
	{
		std::string Command;
		/// Its exit status, where the agent's report carries one.
		std::optional<int> Exit;
		/// Not one of the plan's commands.
		bool OutOfPlan                                     = false;
		auto operator<=>(const local_check_command&) const = default;
	};

	/// \brief A code reference of a report: a path of the candidate and a line.
	struct local_check_reference
	{
		std::string   Path;
		std::uint32_t Line                                            = 0;
		auto          operator<=>(const local_check_reference&) const = default;
	};

	/// \brief A finished, failed or stopped check, as its report reads.
	struct local_check_report
	{
		local_check_outcome Outcome = local_check_outcome::couldnt_tell;
		/// The candidate it checked.
		std::uint64_t Candidate = 0;
		/// The agent's own words about the run, Markdown kept.
		std::string                      Summary;
		std::vector<local_check_command> Commands;
		/// The output lines that explain a failure.
		std::vector<std::string>           Output;
		std::vector<local_check_reference> References;
		std::string                        NotCovered;
		/// Files Git tracks that the run changed in its copy: the change was
		/// not tested as written.
		std::vector<std::string> Changed;
		/// The plan it ran, so Run again runs the same plan.
		local_check_plan Plan;
		auto             operator<=>(const local_check_report&) const = default;
	};

	/// \brief What a turn of a draft's conversation holds beyond its text.
	struct local_check
	{
		std::optional<local_check_plan>   Plan;
		std::optional<local_check_report> Report;
		auto                              operator<=>(const local_check&) const = default;
	};
	[[nodiscard]] auto valid(const local_check& aCheck) -> bool;
	[[nodiscard]] auto encode_local_check(const local_check& aCheck) -> std::string;
	[[nodiscard]] auto decode_local_check(std::string_view aText) -> std::optional<local_check>;

	/// \brief One agent's two grants for one local draft (RFC-BPR-0104).
	struct local_check_grants
	{
		std::string Agent;
		bool        Scratch = false;
		bool        Builds  = false;
		/// The draft's first build question was answered.
		bool Asked                                        = false;
		auto operator<=>(const local_check_grants&) const = default;
	};
	/// \brief The question the first build in a draft asks.
	inline constexpr std::string_view local_check_build_question = "Building runs this change's code on this computer with your account's rights. Allow for this local draft?";
	/// \brief The grants for `aAgent`, or the starting state (both off).
	[[nodiscard]] auto grants_for(std::span<const local_check_grants> aGrants, std::string_view aAgent) -> local_check_grants;

	// --- Run ---------------------------------------------------------------------------

	/// \brief What decides whether Run is available on a plan card.
	struct local_check_run_facts
	{
		local_check_grants Grants;
		/// Another check (or question) is with the agent in this draft.
		bool Running = false;
		/// The draft has a candidate to check.
		bool Candidate                                       = true;
		auto operator<=>(const local_check_run_facts&) const = default;
	};
	/// \brief Why Run is unavailable, first reason first.
	enum class local_check_block : std::uint8_t {
		none,
		discarded,
		ran,
		no_candidate,
		running,
		scratch_off,
		builds_off,
	};
	[[nodiscard]] auto name(local_check_block aBlock) noexcept -> std::string_view;
	[[nodiscard]] auto decide_check_run(const local_check_plan& aPlan, const local_check_run_facts& aFacts) -> local_check_block;
	/// \brief The reason in the author's words, naming the toggle; empty for none.
	[[nodiscard]] auto check_block_words(local_check_block aBlock) -> std::string;

	// --- The requests ---------------------------------------------------------------------

	/// \brief What one plan or run sends: the prompt's instructions and the
	/// answer format apart from the run's material.
	struct local_check_request
	{
		std::string Instructions;
		std::string Text;
		auto        operator<=>(const local_check_request&) const = default;
	};
	/// \brief A plan request on candidate `aCandidate` with the author's
	/// words, or none: then the agent chooses the checks that matter most.
	/// `aPaths` are the candidate's changed paths.
	[[nodiscard]] auto compose_check_plan_request(std::string_view aTitle, std::uint64_t aCandidate, std::span<const std::string> aPaths, std::string_view aWords,
												  std::string_view aInstructions, std::string_view aVersion) -> local_check_request;
	/// \brief A run request: run `aPlan` on candidate `aCandidate`, whose
	/// changed paths are `aPaths`.
	[[nodiscard]] auto compose_check_run_request(std::string_view aTitle, std::uint64_t aCandidate, std::span<const std::string> aPaths, const local_check_plan& aPlan,
												 std::string_view aInstructions, std::string_view aVersion) -> local_check_request;
	/// \brief The author's turn a plan or a run records.
	[[nodiscard]] auto check_plan_question(std::string_view aWords) -> std::string;
	[[nodiscard]] auto check_run_question(std::uint64_t aCandidate, bool aAgain) -> std::string;

	// --- The answers ----------------------------------------------------------------------

	/// \brief A plan answer read: its prose and the plan of its one
	/// `bpr-check-plan` block. A malformed block, or one with no command,
	/// is left in the prose and plans nothing.
	struct local_check_plan_answer
	{
		std::string                     Prose;
		std::optional<local_check_plan> Plan;
	};
	[[nodiscard]] auto read_check_plan(std::string_view aAnswer, std::uint64_t aCandidate) -> local_check_plan_answer;

	/// \brief What the host saw of a run: the commands the agent's activity
	/// listed, the tracked files changed in the copy, and whether Stop ended it.
	struct local_check_observation
	{
		std::vector<std::string> Commands;
		std::vector<std::string> Changed;
		bool                     Stopped = false;
		/// The agent ended without an answer.
		bool Unanswered = false;
	};
	/// \brief A run answer read: its prose and the report the run ends in.
	/// \details The outcome is the agent's word from its `bpr-check-report`
	/// block; Stop makes it Stopped; with no readable block it is Couldn't
	/// tell when anything ran, else Didn't run. Commands are the reported
	/// ones in order, then any the activity listed and the report did not;
	/// one not among the plan's is marked out of plan.
	struct local_check_report_answer
	{
		std::string        Prose;
		local_check_report Report;
	};
	[[nodiscard]] auto read_check_report(std::string_view aAnswer, const local_check_plan& aPlan, std::uint64_t aCandidate, const local_check_observation& aObserved)
		-> local_check_report_answer;

	// --- The report ---------------------------------------------------------------------

	/// \brief The link a report's code reference carries.
	[[nodiscard]] auto check_reference_link(const local_check_reference& aReference) -> std::string;
	/// \brief A report link's reference; none when it is not one.
	[[nodiscard]] auto read_check_reference_link(std::string_view aLink) -> std::optional<local_check_reference>;
	/// \brief "Checked candidate 3", or, once the draft moved on, "Checked
	/// candidate 3; the draft is now at candidate 4".
	[[nodiscard]] auto check_candidate_words(std::uint64_t aChecked, std::uint64_t aLatest) -> std::string;
	/// \brief Whether the report offers Run again: the draft has a newer candidate.
	[[nodiscard]] constexpr auto offers_run_again(const local_check_report& aReport, const std::uint64_t aLatest) noexcept -> bool { return aLatest > aReport.Candidate; }
	/// \brief The report as its file tab reads it, Markdown.
	[[nodiscard]] auto render_check_report(const local_check_report& aReport, std::uint64_t aLatest) -> std::string;
}

#endif
