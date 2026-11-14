#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_QUICK_ASK_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_QUICK_ASK_HPP

#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/returning_review.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0128 ([[BPR-2163]]). What an Ask menu lists: the macros with
/// Quick ask ticked, at the scopes the menu reaches, each shown, greyed with a
/// typed reason, or not listed.
///
/// \details The owner, 2026-11-12: "it's something like a "quick ask" sort of
/// like jira quick filters. In the macro editor we have a check box for quick
/// ask or not. And then they're still filtered by context so invalid ones get
/// greyed out or not shown in the Ask menu." Q1 (default): a quick ask whose
/// subject is not there is not listed; one whose subject is there but which
/// cannot run yet is greyed with its reason. Everything here is a pure
/// decision over facts the window already holds: no request is made, and
/// nothing is sent.
namespace bpr::core {
	/// \brief Which menu is asked for.
	enum class quick_ask_anchor : std::uint8_t {
		/// The review bar's Ask: the pull request's, then the open file's,
		/// then the selected thread's (Q2).
		review_bar,
		/// The file header's Ask and a file tab's menu.
		file,
		/// The open thread's reply row and a thread's menu.
		thread,
	};
	[[nodiscard]] auto stable_name(quick_ask_anchor aAnchor) noexcept -> std::string_view;
	/// \brief The scopes `aAnchor` lists, in the menu's order.
	[[nodiscard]] auto quick_ask_scopes(quick_ask_anchor aAnchor) -> std::vector<prompt_scope>;

	/// \brief One thread of the pull request, as an Ask menu reads it.
	struct quick_ask_thread
	{
		std::string Identity;
		/// The file it is on; empty for a conversation about the pull request.
		std::string Path;
		/// A review thread on GitHub (not my own Local comment or draft).
		bool                Review = true;
		std::optional<bool> TakesReplies;
		/// Whether the head is past the commit the thread started on (Q6);
		/// none when the read does not name that commit.
		std::optional<bool> Changed;
		/// How many of the pull request's commits follow that commit; none
		/// when the commit list does not hold it (a rebased branch).
		std::optional<std::uint32_t> CommitsSince;

		auto operator<=>(const quick_ask_thread&) const = default;
	};

	/// \brief Whether the agent can take a question now.
	enum class quick_ask_agent : std::uint8_t {
		ready,
		not_set_up,
		busy,
	};

	/// \brief What one window holds when a menu is decided.
	struct quick_ask_facts
	{
		/// The pull request's facts; `Place`, `OpenFile`, `Lines` and the
		/// conversation's facts are set per scope here.
		prompt_facts    Base;
		quick_ask_agent Agent = quick_ask_agent::ready;
		/// What changed since my review: the reviewer has a review or a Seen
		/// to return to, the state of the offer, and the commits since.
		bool                Reviewed           = false;
		latest_upload_state LatestUpload       = latest_upload_state::hidden;
		std::uint32_t       CommitsSinceReview = 0U;
		/// The menu's file: the open file, or the file tab the menu is on.
		std::optional<std::string> File;
		/// The menu's thread, by identity.
		std::optional<std::string> Thread;
		/// Every thread of the pull request.
		std::vector<quick_ask_thread> Threads;
		/// The pull request's changed files, for a file's quick ask at the
		/// pull request.
		std::vector<std::string> Files;

		auto operator<=>(const quick_ask_facts&) const = default;
	};

	/// \brief Why a listed quick ask cannot run yet. Logged by stable name.
	enum class quick_ask_reason : std::uint8_t {
		none,
		/// No agent is set up.
		agent_not_set_up,
		/// The agent is answering another question.
		agent_busy,
		/// What changed: no review or Seen to return to.
		not_reviewed,
		/// What changed: no commits since the review or Seen.
		no_commits_since_review,
		/// What changed: the earlier code is still being read.
		reading_commits,
		/// What changed: the earlier code can't be read here.
		earlier_unreadable,
		/// No review threads at this scope.
		no_threads,
		/// Was this addressed?: no commits since any of the threads started.
		no_commits_since_threads,
		/// Was this addressed?: no commits since this thread started.
		no_commits_since_thread,
		/// Was this addressed?: the read does not name the commit this thread
		/// started on.
		thread_commit_unknown,
		/// A thread's quick ask at a wider scope: no thread here meets it.
		no_threads_match,
		/// A file's quick ask at the pull request: no file meets it.
		no_files_match,
	};
	[[nodiscard]] auto stable_name(quick_ask_reason aReason) noexcept -> std::string_view;

	/// \brief What a row's right end counts.
	enum class quick_ask_unit : std::uint8_t {
		none,
		commits,
		threads,
		files,
	};

	/// \brief One row of an Ask menu.
	struct quick_ask_row
	{
		std::string    Id;
		std::string    Label;
		prompt_scope   Scope   = prompt_scope::pull_request;
		prompt_action  Action  = prompt_action::none;
		prompt_subject Subject = prompt_subject::pull_request;
		prompt_ask     Kind    = prompt_ask::question;
		bool           Enabled = true;
		/// Why it is greyed; `none` while enabled.
		quick_ask_reason Reason = quick_ask_reason::none;
		/// What a press reads: "18 commits", "12 threads".
		std::uint32_t  Count = 0U;
		quick_ask_unit Unit  = quick_ask_unit::none;
		/// A wider scope asks once for each: the threads, or the files, a
		/// press runs on, in the pull request's order.
		std::vector<std::string> Each;

		auto operator<=>(const quick_ask_row&) const = default;
	};

	struct quick_ask_section
	{
		prompt_scope               Scope = prompt_scope::pull_request;
		std::vector<quick_ask_row> Rows;

		auto operator<=>(const quick_ask_section&) const = default;
	};

	/// \brief An Ask menu: its sections in order (an empty section is not
	/// listed), and the quick asks not listed with why.
	struct quick_ask_menu
	{
		std::vector<quick_ask_section> Sections;
		std::vector<prompt_withheld>   Hidden;

		auto operator<=>(const quick_ask_menu&) const = default;
	};

	/// \brief Decides `aAnchor`'s menu from the macros in the reviewer's order.
	///
	/// \details Only macros that are on, valid, Quick ask and on the scope.
	/// Within a section the action rows lead, then the rest in order. A
	/// scope's subject absent (no file, no thread, an unmet implied need or a
	/// condition) is not listed; one present that cannot run is greyed.
	[[nodiscard]] auto decide_quick_asks(std::span<const prompt_macro> aOrdered, const quick_ask_facts& aFacts, quick_ask_anchor aAnchor) -> quick_ask_menu;

	/// \brief The row named `aId` at `aScope`, decided again at a press: a
	/// press acts on the facts it is made under.
	[[nodiscard]] auto find_quick_ask(const quick_ask_menu& aMenu, std::string_view aId, prompt_scope aScope) -> const quick_ask_row*;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_QUICK_ASK_HPP
