#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_MACRO_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_MACRO_HPP

#include "bpr/core/agent_question.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0125 milestone 1. A suggested prompt as a macro: a short
/// name on the chip, a template that expands into what is sent, one place,
/// what it is about, how it is asked, and up to four conditions that decide
/// whether it shows.
///
/// \details The owner, 2026-11-12: "make suggested prompts configurable
/// macros ... the thing shown as the suggestion to click isn't literally
/// what gets sent to the agent". Everything here is a value and a pure
/// decision over facts the host gathered from what a window already holds.
/// No condition makes a request, and a macro never sends itself: the offers
/// decide appearance only. "Macro" is the design word; the product says
/// "suggested prompt".
///
/// Substitution is one linear scan for `{name}` against a closed list
/// (owner, Q1: no template library). Variables name things and carry no
/// content (owner, Q3), so a macro cannot widen what a request discloses.
namespace bpr::core {
	/// \brief Where a macro's chip is offered. A macro has exactly one place.
	enum class prompt_place : std::uint8_t {
		/// The Agent tab, with no conversation attached.
		agent_tab,
		/// The Agent tab while a conversation is attached ([[BPR-1773]]).
		conversation,
		/// A tour stop's side chat.
		tour_stop,
		/// The chat about the whole tour.
		tour_whole,
	};
	inline constexpr std::size_t prompt_place_count = 4U;
	/// \brief `agent-tab`, `conversation`, `tour-stop`, `tour`.
	[[nodiscard]] auto stable_name(prompt_place aPlace) noexcept -> std::string_view;

	/// \brief What a question asked by a macro is about (the editor's About).
	enum class prompt_subject : std::uint8_t {
		pull_request,
		open_file,
		selected_lines,
		conversation,
		stop,
		selected_text,
		tour,
		/// [[BPR-2248]]. The commits selected in the Commits tab: a prompt
		/// placed on the Commits selection menu asks about them.
		selected_commits,
	};
	inline constexpr std::size_t prompt_subject_count = 8U;
	[[nodiscard]] auto           stable_name(prompt_subject aSubject) noexcept -> std::string_view;

	/// \brief How a macro is asked (the editor's Ask as).
	enum class prompt_ask : std::uint8_t {
		question,
		/// A review with findings; the Agent tab only, about the pull
		/// request, the open file or the selected commits.
		review,
	};
	[[nodiscard]] auto stable_name(prompt_ask aKind) noexcept -> std::string_view;

	/// \brief RFC-BPR-0128. Where a quick ask is asked: about a thread, a
	/// file or the pull request. Widest last.
	enum class prompt_scope : std::uint8_t {
		thread,
		file,
		pull_request,
	};
	inline constexpr std::size_t prompt_scope_count = 3U;
	/// \brief `thread`, `file`, `pull-request`.
	[[nodiscard]] auto stable_name(prompt_scope aScope) noexcept -> std::string_view;

	/// \brief RFC-BPR-0128. The scopes a quick ask is on; each can be on.
	struct prompt_scopes
	{
		bool Thread      = false;
		bool File        = false;
		bool PullRequest = false;

		[[nodiscard]] auto has(prompt_scope aScope) const noexcept -> bool;
		void               set(prompt_scope aScope, bool aOn) noexcept;
		[[nodiscard]] auto any() const noexcept -> bool { return Thread || File || PullRequest; }

		auto operator<=>(const prompt_scopes&) const = default;
	};

	/// \brief [[BPR-2179]]. Where prompts appear: the Ask menus and
	/// suggested chips of the pull request, a file and a thread, and the
	/// tour's two chats. The decisions over surfaces are in
	/// `prompt_surfaces.hpp`; the values are here so a macro set carries them.
	enum class prompt_surface : std::uint8_t {
		pull_request_menu,
		pull_request_chips,
		file_menu,
		file_chips,
		thread_menu,
		thread_chips,
		tour_stop,
		whole_tour,
		/// [[BPR-2248]]. The Commits tab's menu over the selected commits: a
		/// menu of its own, not one of the Ask menus, never chips.
		commits_menu,
	};
	inline constexpr std::size_t prompt_surface_count = 9U;

	/// \brief [[BPR-2179]]. Every surface's placements, in order, by prompt
	/// identity; one prompt is on one surface at most once.
	struct prompt_surfaces
	{
		std::array<std::vector<std::string>, prompt_surface_count> Lists;

		auto operator<=>(const prompt_surfaces&) const = default;
	};

	/// \brief [[BPR-2179]]. A profile's change to one surface: its list as
	/// the reviewer left it, and the prompts the fields would put there that
	/// the reviewer took off. A prompt the fields put there that is in
	/// neither (a built-in a later release adds) goes to the end.
	struct prompt_surface_change
	{
		prompt_surface           Surface = prompt_surface::pull_request_menu;
		std::vector<std::string> Ids;
		std::vector<std::string> Removed;

		auto operator<=>(const prompt_surface_change&) const = default;
	};

	/// \brief RFC-BPR-0128 (Q13). A built-in row that stands for one of the
	/// host's agent actions. What it sends stays in Built-in prompts; the row
	/// carries its name, Quick ask, scopes and On.
	enum class prompt_action : std::uint8_t {
		/// An ordinary macro: it sends its template.
		none,
		/// What changed since my review (RFC-BPR-0116).
		what_changed,
		/// Was this addressed? (RFC-BPR-0116, widened by RFC-BPR-0128).
		was_addressed,
		/// Explain this file (RFC-BPR-0117).
		explain_file,
	};
	inline constexpr std::size_t prompt_action_count = 4U;
	[[nodiscard]] auto           stable_name(prompt_action aAction) noexcept -> std::string_view;

	/// \brief RFC-BPR-0128. The scope a subject is asked at itself: a
	/// conversation is a thread, the open file a file, the pull request the
	/// pull request. None for subjects no quick ask can have (lines, tour).
	[[nodiscard]] auto subject_scope(prompt_subject aSubject) noexcept -> std::optional<prompt_scope>;
	/// \brief RFC-BPR-0128. Whether a quick ask about `aSubject` in `aPlace`
	/// may be on at `aScope`: the subject has a scope and `aScope` is not
	/// narrower than it (a wider one asks once for each).
	[[nodiscard]] auto prompt_scope_allowed(prompt_place aPlace, prompt_subject aSubject, prompt_scope aScope) noexcept -> bool;

	/// \brief Whether `aPlace` offers `aSubject` as About.
	[[nodiscard]] auto prompt_subject_allowed(prompt_place aPlace, prompt_subject aSubject) noexcept -> bool;
	/// \brief Whether `aKind` may be asked about `aSubject` in `aPlace`.
	[[nodiscard]] auto prompt_ask_allowed(prompt_place aPlace, prompt_subject aSubject, prompt_ask aKind) noexcept -> bool;

	/// \brief One fact a condition reads.
	enum class prompt_fact : std::uint8_t {
		/// A file is open in Review. Agent tab.
		file_open,
		/// Lines are selected: any, one line, several lines. Agent tab.
		selected_lines,
		/// The open file's path matches patterns. Agent tab.
		path,
		/// The pull request is a draft. Every place.
		draft,
		/// The pull request's author is the signed-in reviewer. Every place.
		author_is_me,
		/// A check on the head commit ended in failure, timeout,
		/// cancellation or action required. Every place.
		checks_failing,
		/// The repository (`owner/name`) matches patterns. Every place.
		repository,
		/// The attached conversation takes replies. About a conversation.
		takes_replies,
		/// The chat holds no turn yet. Agent tab and the tour chats.
		empty_chat,
		/// [[BPR-2179]]. Commits followed my review or Seen of the pull request.
		pushes_since_review,
		/// [[BPR-2179]]. The file has an unresolved review thread.
		file_unresolved_threads,
		/// [[BPR-2179]]. I have not marked the file reviewed.
		file_unreviewed,
		/// [[BPR-2179]]. The thread is not resolved.
		thread_unresolved,
		/// [[BPR-2179]]. Replies or pushes (`prompt_condition::Activity`)
		/// followed the thread's first comment.
		thread_activity,
		/// [[BPR-2179]]. Someone replied after my last comment on the thread.
		replies_since_mine,
	};
	inline constexpr std::size_t prompt_fact_count = 15U;
	[[nodiscard]] auto           stable_name(prompt_fact aFact) noexcept -> std::string_view;
	/// \brief Whether a condition on `aFact` may be written in `aPlace`.
	[[nodiscard]] auto prompt_fact_allowed(prompt_place aPlace, prompt_fact aFact) noexcept -> bool;

	/// \brief [[BPR-2179]], RFC-BPR-0128 Q1. Which way a fact leaned before
	/// look prompts-v003: one about whether the subject is there hid an Ask
	/// menu row; one about whether it could run yet greyed it. Read only to
	/// migrate a stored prompt to its `prompt_when_unmet`.
	enum class prompt_condition_kind : std::uint8_t {
		greys,
		hides,
	};
	[[nodiscard]] auto prompt_fact_kind(prompt_fact aFact) noexcept -> prompt_condition_kind;

	/// \brief [[BPR-2179]] look prompts-v003 (owner: "Disable on a condition
	/// still doens't make sense ... in context of the groups or 'any of
	/// these'"). The coordinator's proposal: one choice a prompt, what its
	/// Ask menu row does while its conditions do not hold. A chip is never
	/// offered then.
	enum class prompt_when_unmet : std::uint8_t {
		/// Show it disabled, with the first unmet condition's reason.
		disable,
		/// Hide it.
		hide,
	};
	[[nodiscard]] auto stable_name(prompt_when_unmet aWhen) noexcept -> std::string_view;

	/// \brief [[BPR-2179]]. How a prompt's conditions combine.
	enum class prompt_match : std::uint8_t {
		/// All of these are true.
		all,
		/// Any of these is true.
		any,
		/// [[BPR-2179]] look prompts-v001 (owner: "You should be able to nest
		/// conditionals"): none of these is true, a Not over Any.
		none,
	};
	[[nodiscard]] auto stable_name(prompt_match aMatch) noexcept -> std::string_view;

	/// \brief [[BPR-2179]]. What "New replies or pushes since the comment"
	/// counts.
	enum class prompt_activity_value : std::uint8_t {
		replies_or_pushes,
		replies,
		pushes,
	};

	/// \brief The value a `selected_lines` condition compares with.
	enum class prompt_lines_value : std::uint8_t {
		any,
		one,
		several,
	};

	/// \brief One condition: a fact, "is" or "is not", and a value.
	///
	/// \details `Lines` is read only by `selected_lines`; `Patterns` only by
	/// `path` and `repository`. A condition whose fact the window does not
	/// hold yet satisfies neither polarity.
	struct prompt_condition
	{
		prompt_fact Fact = prompt_fact::file_open;
		/// "is not", "doesn't match", "doesn't take".
		bool                     Negated  = false;
		prompt_lines_value       Lines    = prompt_lines_value::any;
		std::vector<std::string> Patterns = { };
		/// Read only by `thread_activity`.
		prompt_activity_value Activity = prompt_activity_value::replies_or_pushes;

		auto operator<=>(const prompt_condition&) const = default;
	};

	/// \brief [[BPR-2179]], look prompts-v001. A nested group of conditions:
	/// All, Any or None of its conditions and groups, the way the owner's
	/// tag queries nest And, Or and Not groups. Each leaf keeps its kind.
	struct prompt_condition_group
	{
		prompt_match                        Match = prompt_match::all;
		std::vector<prompt_condition>       Conditions;
		std::vector<prompt_condition_group> Groups;

		// Spelled out: a defaulted comparison cannot see through the
		// recursion.
		[[nodiscard]] auto operator<=>(const prompt_condition_group& aOther) const -> std::strong_ordering;
		[[nodiscard]] auto operator==(const prompt_condition_group& aOther) const -> bool;
	};
	/// \brief A prompt stored before `prompt_when_unmet` existed: Hide when
	/// any of its conditions leaned to hiding, else Disable.
	[[nodiscard]] auto migrated_when_unmet(const std::vector<prompt_condition>& aConditions, const std::vector<prompt_condition_group>& aGroups) -> prompt_when_unmet;
	/// \brief The deepest nesting: the prompt's own list, then two levels.
	inline constexpr std::size_t maximum_prompt_condition_depth = 3U;
	/// \brief The most conditions in a prompt's whole tree.
	inline constexpr std::size_t maximum_prompt_condition_leaves = 12U;

	/// \brief The longest name, in bytes, as written.
	inline constexpr std::size_t maximum_prompt_name_bytes = 120U;
	/// \brief The longest template, a writing hook's bound.
	inline constexpr std::size_t maximum_prompt_template_bytes = 8U * 1024U;
	/// \brief The longest expansion: the message bound of the Agent tab and
	/// the tour chats alike.
	inline constexpr std::size_t maximum_prompt_expansion_bytes = 16U * 1024U;
	inline constexpr std::size_t maximum_prompt_conditions      = 4U;
	inline constexpr std::size_t maximum_prompt_patterns        = 8U;
	inline constexpr std::size_t maximum_prompt_pattern_bytes   = 128U;
	/// \brief The most macros a profile holds, the built-ins included.
	inline constexpr std::size_t maximum_prompt_macros = 64U;
	/// \brief The most macros shown as chips; further matches go to More.
	inline constexpr std::size_t maximum_prompt_chips = 5U;

	/// \brief The editable part of a macro.
	struct prompt_macro_body
	{
		/// The chip's text, one line; may use variables.
		std::string Name;
		/// What the chip expands into and sends.
		std::string                   Template;
		prompt_subject                Subject    = prompt_subject::pull_request;
		prompt_ask                    Kind       = prompt_ask::question;
		std::vector<prompt_condition> Conditions = { };
		/// RFC-BPR-0128. Listed in the Ask menus at `Scopes`.
		bool          QuickAsk = false;
		prompt_scopes Scopes   = { };
		/// [[BPR-2179]]. All of these, any of these, or none of these.
		prompt_match Match = prompt_match::all;
		/// [[BPR-2179]], look prompts-v001. Groups nested under the list,
		/// combined with `Conditions` by `Match`.
		std::vector<prompt_condition_group> Groups;
		/// [[BPR-2179]], look prompts-v003. The Ask menu row while the
		/// conditions do not hold.
		prompt_when_unmet WhenUnmet = prompt_when_unmet::disable;

		auto operator<=>(const prompt_macro_body&) const = default;
	};

	/// \brief RFC-BPR-0125 AC24 (owner amendment, 2026-11-18). Where one of
	/// the reviewer's own prompts came from: the prompt pack it was last
	/// imported from or exported in, the pack version that copy was written
	/// against, and the place and body as the pack had them. The prompt is
	/// edited since when its place or body differs from these.
	struct prompt_pack_link
	{
		/// The pack's UUID.
		std::string       Pack;
		std::string       Label;
		std::uint32_t     Version = 0U;
		prompt_place      Place   = prompt_place::agent_tab;
		prompt_macro_body Body;

		auto operator<=>(const prompt_pack_link&) const = default;
	};

	/// \brief One macro.
	struct prompt_macro
	{
		/// A built-in's fixed name, or the identity the host minted for one
		/// of the reviewer's own: a lowercase version 4 UUID (owner,
		/// 2026-11-18), or `own.` and 32 hexadecimal digits for one made
		/// before UUIDs and not exported since. Never shown.
		std::string       Id;
		bool              BuiltIn = false;
		prompt_place      Place   = prompt_place::agent_tab;
		prompt_macro_body Body;
		/// Off: kept, never offered.
		bool On = true;
		/// RFC-BPR-0128. A built-in action row; never one of the reviewer's
		/// own. An action row is never an Agent tab chip.
		prompt_action Action = prompt_action::none;
		/// [[BPR-2179]]. The surface this copy is offered on
		/// (`placed_prompt_macros`); none for a library entry, which the
		/// decisions read by its own fields.
		std::optional<prompt_surface> Surface;
		/// RFC-BPR-0125 AC24. The pack one of the reviewer's own prompts is
		/// linked to; none for a built-in, a prompt of their own making and a
		/// duplicate.
		std::optional<prompt_pack_link> Pack;

		auto operator<=>(const prompt_macro&) const = default;
	};

	/// \brief The closed list of variables.
	enum class prompt_variable : std::uint8_t {
		pr_number,
		pr_title,
		pr_url,
		pr_author,
		pr_base,
		pr_head,
		repo,
		file,
		file_name,
		lines,
		lines_first,
		lines_last,
		conversation_file,
		stop_number,
		stop_title,
		part,
		/// [[BPR-2248]]. The selected commits' short IDs, oldest first.
		commits,
		/// Fills in nothing; marks where the caret goes. A template holding
		/// it is put in the question field, never sent by a click.
		cursor,
	};
	inline constexpr std::size_t prompt_variable_count = 18U;
	/// \brief The name written between braces: `pr.number`, `file.name`,
	/// `cursor`.
	[[nodiscard]] auto stable_name(prompt_variable aVariable) noexcept -> std::string_view;
	/// \brief The variable written `aName`, if there is one.
	[[nodiscard]] auto find_prompt_variable(std::string_view aName) noexcept -> std::optional<prompt_variable>;
	/// \brief Whether `aPlace` can fill `aVariable` in.
	[[nodiscard]] auto prompt_variable_allowed(prompt_place aPlace, prompt_variable aVariable) noexcept -> bool;
	/// \brief The variables `aPlace` can fill in, in the list's order (what
	/// Insert offers).
	[[nodiscard]] auto prompt_variables(prompt_place aPlace) -> std::vector<prompt_variable>;

	/// \brief One run of a scanned template: literal text, or a variable.
	struct prompt_template_piece
	{
		std::string                    Literal;
		std::optional<prompt_variable> Variable;

		auto operator<=>(const prompt_template_piece&) const = default;
	};

	/// \brief Why a `{name}` was refused.
	enum class prompt_template_refusal_reason : std::uint8_t {
		/// No variable has this name.
		unknown_variable,
		/// The variable exists, but this place cannot fill it in.
		other_place_variable,
	};

	/// \brief A refused `{name}`: where it starts (the `{`), how long it is
	/// with its braces, and the name between them.
	struct prompt_template_refusal
	{
		prompt_template_refusal_reason Reason = prompt_template_refusal_reason::unknown_variable;
		std::size_t                    Offset = 0U;
		std::size_t                    Length = 0U;
		std::string                    Name;

		auto operator<=>(const prompt_template_refusal&) const = default;
	};

	/// \brief A scanned template.
	struct prompt_template_scan
	{
		std::vector<prompt_template_piece>   Pieces;
		std::vector<prompt_template_refusal> Refusals;

		auto operator<=>(const prompt_template_scan&) const = default;
	};

	/// \brief Scans `aText` once, left to right.
	///
	/// \details `{{` is a literal `{`. `{` followed by a variable-shaped name
	/// (a lowercase letter, then lowercase letters, digits, `.`, `-` or `_`)
	/// and `}` is a variable, refused unless `aPlace` can fill it in. Every
	/// other brace is literal. A refused name stays in `Pieces` as literal
	/// text so the scan is total, but a template with any refusal is never
	/// expanded.
	[[nodiscard]] auto scan_prompt_template(std::string_view aText, prompt_place aPlace) -> prompt_template_scan;

	/// \brief The pull request's names, as the window shows them.
	struct prompt_pull_request
	{
		std::uint64_t Number = 0U;
		std::string   Title;
		std::string   Url;
		std::string   Author;
		std::string   Base;
		std::string   Head;

		auto operator<=>(const prompt_pull_request&) const = default;
	};

	/// \brief Selected lines, one-based and inclusive.
	struct prompt_line_range
	{
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;

		auto operator<=>(const prompt_line_range&) const = default;
	};

	/// \brief One tour chat's part: its place and its names.
	struct prompt_tour_part
	{
		/// `tour_stop` or `tour_whole`.
		prompt_place Place = prompt_place::tour_whole;
		/// `{part}`: `Stop 3`; empty for the whole tour.
		std::string Part;
		/// A stop's one-based number and title.
		std::optional<std::uint32_t> StopNumber;
		std::string                  StopTitle;

		auto operator<=>(const prompt_tour_part&) const = default;
	};

	/// \brief What one window holds when its offers are decided.
	///
	/// \details An empty optional is "unknown" for the yes-or-no facts and
	/// the repository, and "none" for the open file, the selected lines and
	/// the tour selection, which the window always knows. A path condition
	/// with no file open reads an unknown path.
	struct prompt_facts
	{
		/// The place being offered.
		prompt_place                       Place = prompt_place::agent_tab;
		std::optional<prompt_pull_request> PullRequest;
		/// `owner/name`.
		std::optional<std::string>       Repository;
		std::optional<std::string>       OpenFile;
		std::optional<prompt_line_range> Lines;
		/// The attached conversation's file, when it is on one.
		std::optional<std::string> ConversationFile;
		std::optional<bool>        TakesReplies;
		std::optional<bool>        Draft;
		std::optional<bool>        AuthorIsMe;
		std::optional<bool>        ChecksFailing;
		std::optional<bool>        ChatEmpty;
		/// [[BPR-2179]]. The newer conditions' facts.
		std::optional<bool> PushesSinceReview;
		std::optional<bool> FileUnresolvedThreads;
		std::optional<bool> FileUnreviewed;
		std::optional<bool> ThreadUnresolved;
		std::optional<bool> ThreadReplies;
		std::optional<bool> ThreadPushes;
		std::optional<bool> RepliesSinceMine;
		/// A tour chat's own part.
		std::optional<prompt_tour_part> Thread;
		/// The part where tour text is selected, this chat's or another's.
		std::optional<prompt_tour_part> Selection;
		/// [[BPR-2248]]. The commits selected in the Commits tab, full IDs
		/// oldest first; empty where none are.
		std::vector<std::string> Commits;

		auto operator<=>(const prompt_facts&) const = default;
	};

	/// \brief Where one value sits in an expansion: bytes `[Begin, End)`.
	struct prompt_value_span
	{
		std::size_t Begin = 0U;
		std::size_t End   = 0U;

		auto operator<=>(const prompt_value_span&) const = default;
	};

	/// \brief A template expanded against facts.
	struct prompt_expansion
	{
		std::string Text;
		/// The byte offset of the first `{cursor}`, when the template has
		/// one.
		std::optional<std::size_t> Cursor;
		/// RFC-BPR-0125 milestone 2. The bytes each value filled in, in
		/// order; everything else is the template's own text.
		std::vector<prompt_value_span> Values;

		auto operator<=>(const prompt_expansion&) const = default;
	};

	/// \brief Expands a scan with no refusals in one pass.
	///
	/// \details A value is inserted as written and never read again: not as
	/// a variable, a skill reference or a command. A value is one line: each
	/// line break and control character in it becomes a space. A value is
	/// never shortened.
	/// \returns Nothing when the scan has a refusal or a value is missing.
	[[nodiscard]] auto expand_prompt_template(const prompt_template_scan& aScan, const prompt_facts& aFacts) -> std::optional<prompt_expansion>;

	/// \brief Why a macro is not offered, or what the first unmet need or
	/// condition is. Logged by stable name, never with the macro's text.
	enum class prompt_unmet : std::uint8_t {
		/// Turned off.
		off,
		/// It does not validate (a blocking problem); stored rows only.
		invalid,
		/// Its About or a variable needs a file open in Review.
		needs_file,
		/// Its About or a variable needs selected lines.
		needs_lines,
		/// A variable needs the conversation to be on a file.
		needs_conversation_file,
		/// It is about selected tour text and none is selected in its place.
		needs_selected_text,
		/// A variable needs a tour stop.
		needs_stop,
		/// A variable needs a tour part.
		needs_part,
		/// A variable needs the pull request's names.
		needs_pull_request,
		/// A variable needs the repository's name.
		needs_repository,
		/// A condition row fails; `prompt_evaluation::Condition` says which.
		condition,
		/// The expansion is longer than `maximum_prompt_expansion_bytes`.
		expansion_too_long,
		/// [[BPR-2248]]. Its About or a variable needs selected commits.
		needs_commits,
	};
	[[nodiscard]] auto stable_name(prompt_unmet aUnmet) noexcept -> std::string_view;

	/// \brief One chip, or one item of More.
	struct prompt_offer
	{
		std::string Id;
		/// The chip's text: the name expanded, and for text selected in
		/// another tour part, ` in ` and that part.
		std::string      Label;
		prompt_expansion Expansion;
		prompt_place     Place   = prompt_place::agent_tab;
		prompt_subject   Subject = prompt_subject::pull_request;
		prompt_ask       Kind    = prompt_ask::question;
		/// [[BPR-2248]]. The commits an offer about selected commits asks
		/// about, full IDs oldest first, as decided at the press; empty for
		/// every other offer.
		std::vector<std::string> Commits;

		/// \brief Whether a click puts the expansion in the question field
		/// rather than sending it (the template has `{cursor}`).
		[[nodiscard]] auto finish() const noexcept -> bool { return Expansion.Cursor.has_value(); }

		auto operator<=>(const prompt_offer&) const = default;
	};

	/// \brief One macro decided against facts: its offer, or why not.
	struct prompt_evaluation
	{
		std::optional<prompt_offer> Offer;
		prompt_unmet                Unmet = prompt_unmet::off;
		/// The failing condition's place in the tree, for
		/// `prompt_unmet::condition` (`prompt_conditions_outcome::Leaf`).
		std::size_t Condition = 0U;

		auto operator<=>(const prompt_evaluation&) const = default;
	};

	/// \brief Whether one condition holds. Unknown satisfies neither
	/// polarity. Patterns are matched as `.gitignore` reads them: one with no
	/// slash against the last segment, one with a slash against the whole
	/// path, `**` crossing folders; a repository folds ASCII case.
	[[nodiscard]] auto prompt_condition_holds(const prompt_condition& aCondition, const prompt_facts& aFacts) -> bool;

	/// \brief [[BPR-2179]]. A prompt's conditions decided together.
	struct prompt_conditions_outcome
	{
		/// All of them (or, for Any, one of them) hold; true with none.
		bool Holds = true;
		/// Not holding, and the prompt says Hide it.
		bool Hides = false;
		/// The condition whose reason a disabled row gives: the first unmet
		/// one (a true one under None, its polarity turned). None while
		/// `Holds` or `Hides`.
		std::optional<prompt_condition> Reason;
		/// Review N5. While not holding, the place in the tree of the leaf
		/// that is the reason, numbered as `validate_prompt_macro` numbers
		/// them (the list's conditions, then each group depth first), whether
		/// or not the prompt hides.
		std::size_t Leaf = 0U;

		auto operator<=>(const prompt_conditions_outcome&) const = default;
	};
	/// \brief Decides a prompt's conditions in three values: a leaf whose
	/// fact is unknown is neither true nor false, so it satisfies no group
	/// it would settle (None of an unknown condition does not hold).
	[[nodiscard]] auto decide_prompt_conditions(const prompt_macro_body& aBody, const prompt_facts& aFacts) -> prompt_conditions_outcome;
	/// \brief Review N3. Whether a prompt has any condition, in its list or a
	/// nested group: what the "has conditions" mark reads.
	[[nodiscard]] auto prompt_has_conditions(const prompt_macro_body& aBody) noexcept -> bool;

	/// \brief Review N3. What an unmet condition says about the window: its
	/// fact, and whether that fact is there (an unmet "is not" condition) or
	/// not. The words for a disabled row's reason read this alone.
	struct prompt_unmet_reading
	{
		prompt_fact Fact = prompt_fact::file_open;
		/// The fact is there: an unmet "is not" condition.
		bool                  Present  = false;
		prompt_activity_value Activity = prompt_activity_value::replies_or_pushes;

		auto operator<=>(const prompt_unmet_reading&) const = default;
	};
	[[nodiscard]] auto prompt_unmet_reading_of(const prompt_condition& aCondition) noexcept -> prompt_unmet_reading;

	/// \brief Decides one macro against `aFacts`, whatever place `aFacts`
	/// names: the caller chooses which macros a place considers.
	///
	/// \details In order: off; invalid; the needs its About and variables
	/// imply; each condition row; the expansion's bound. The first that fails
	/// is the answer, so "Shows now" can name it.
	[[nodiscard]] auto evaluate_prompt_macro(const prompt_macro& aMacro, const prompt_facts& aFacts) -> prompt_evaluation;

	/// \brief A macro that was considered and not offered.
	struct prompt_withheld
	{
		std::string  Id;
		prompt_unmet Unmet     = prompt_unmet::off;
		std::size_t  Condition = 0U;

		auto operator<=>(const prompt_withheld&) const = default;
	};

	/// \brief What one place offers: the chips, More, and the rest with
	/// their reasons.
	struct prompt_offers
	{
		std::vector<prompt_offer>    Chips;
		std::vector<prompt_offer>    More;
		std::vector<prompt_withheld> Withheld;

		auto operator<=>(const prompt_offers&) const = default;
	};

	/// \brief RFC-BPR-0125 milestone 2 (AC09). The text a direct send reads
	/// skill references from: each `/name` of the expansion that lies wholly
	/// in the template's own text, in order, separated by spaces.
	///
	/// \details A value never brings in a skill: a pull request titled
	/// `/some-skill` is sent as text. A reference that a value touches, or
	/// that only reads as one because of a value beside it, is not a
	/// reference here. With no values the result names exactly the
	/// references of `Text`.
	[[nodiscard]] auto prompt_skill_text(const prompt_expansion& aExpansion) -> std::string;

	/// \brief RFC-BPR-0125 milestone 2. What an Agent tab offer names to
	/// `decide_agent_question` (RFC-BPR-0102), unchanged: the pull request,
	/// the open file or the selected lines. None for the other places'
	/// subjects, which the conversation and the tour chats decide as before.
	[[nodiscard]] auto suggested_subject_of(prompt_subject aSubject) noexcept -> std::optional<agent_suggested_subject>;

	/// \brief Decides `aFacts.Place`'s offers from macros in the reviewer's
	/// order.
	///
	/// \details When tour text is selected, the macros about selected text
	/// of the selected part's place lead, in that place's order, expanded
	/// with that part's names and labelled ` in ` that part when it is not
	/// this chat's. Then come this place's other macros, in order. The first
	/// `maximum_prompt_chips` offers are chips; the rest are More.
	[[nodiscard]] auto decide_prompt_offers(std::span<const prompt_macro> aOrdered, const prompt_facts& aFacts) -> prompt_offers;

	/// \brief What a macro's validation found.
	enum class prompt_problem : std::uint8_t {
		/// Blocks. "Give the prompt a name."
		no_name,
		/// Blocks. Over `maximum_prompt_name_bytes`; `Excess` says by how many.
		name_too_long,
		/// Blocks. A line break or control character in the name.
		name_not_one_line,
		/// Blocks. "Write what the prompt sends."
		no_template,
		/// Blocks. Over `maximum_prompt_template_bytes`; `Excess` says by how
		/// many.
		template_too_long,
		/// Blocks. `{name}` no place fills in; `Offset`, `Length`, `Detail`.
		unknown_variable,
		/// Blocks. Another place's variable; `Offset`, `Length`, `Detail`.
		other_place_variable,
		/// Blocks. About is not one of the place's.
		subject_not_for_place,
		/// Blocks. Ask as is not allowed for this About.
		kind_not_for_subject,
		/// Blocks. More than `maximum_prompt_conditions`.
		too_many_conditions,
		/// Blocks. A fact this place does not hold; `Condition`.
		fact_not_for_place,
		/// Blocks. A path or repository condition with no pattern.
		no_patterns,
		/// Blocks. More than `maximum_prompt_patterns`; `Condition`.
		too_many_patterns,
		/// Blocks. A pattern over `maximum_prompt_pattern_bytes`.
		pattern_too_long,
		/// Blocks. A pattern that cannot match (an unterminated bracket, an
		/// unknown class, a trailing backslash); `Detail` is the pattern.
		bad_pattern,
		/// Advice. Two conditions, or a condition and an implied need, that
		/// cannot hold together.
		conditions_contradict,
		/// Advice. Another macro of this place has this name.
		duplicate_name,
		/// Advice. The text holds a well-known credential prefix.
		credential_prefix,
		/// Blocks. RFC-BPR-0128. Quick ask on an About no Ask menu asks
		/// (selected lines, the tour), or in a tour chat.
		quick_ask_not_for_subject,
		/// Blocks. RFC-BPR-0128. Quick ask with no scope on.
		quick_ask_without_scope,
		/// Blocks. RFC-BPR-0128. A scope narrower than the About; `Detail`
		/// is the scope's stable name.
		scope_narrower_than_subject,
	};
	[[nodiscard]] auto stable_name(prompt_problem aProblem) noexcept -> std::string_view;
	/// \brief Whether `aProblem` blocks Save.
	[[nodiscard]] auto prompt_problem_blocks(prompt_problem aProblem) noexcept -> bool;

	/// \brief One finding of `validate_prompt_macro`.
	struct prompt_macro_problem
	{
		prompt_problem Problem = prompt_problem::no_name;
		/// For a variable: whether it is in the name rather than the
		/// template.
		bool        InName    = false;
		std::size_t Offset    = 0U;
		std::size_t Length    = 0U;
		std::size_t Condition = 0U;
		std::size_t Excess    = 0U;
		std::string Detail;

		auto operator<=>(const prompt_macro_problem&) const = default;
	};

	/// \brief Validates one macro, in the order the sheet lists problems.
	/// \param aSiblings The other macros, for the duplicate-name advice;
	/// `aMacro` itself (by identity) is skipped.
	[[nodiscard]] auto validate_prompt_macro(const prompt_macro& aMacro, std::span<const prompt_macro> aSiblings = { }) -> std::vector<prompt_macro_problem>;
	/// \brief Whether `validate_prompt_macro` finds no blocking problem.
	[[nodiscard]] auto prompt_macro_valid(const prompt_macro& aMacro) -> bool;

	/// \brief Splits the editor's pattern field on commas, trimming spaces
	/// and dropping empty entries. Bounds are `validate_prompt_macro`'s.
	[[nodiscard]] auto parse_prompt_patterns(std::string_view aText) -> std::vector<std::string>;

	/// \brief One shipped macro and the revision of its shipped text.
	struct prompt_builtin
	{
		prompt_macro  Macro;
		std::uint32_t Revision = 1U;

		auto operator<=>(const prompt_builtin&) const = default;
	};

	/// \brief RFC-BPR-0125 milestone 4. The sample values the editor's
	/// preview expands against when no pull request window is at hand.
	[[nodiscard]] auto sample_prompt_facts(prompt_place aPlace) -> prompt_facts;

	/// \brief Today's suggested prompts as built-ins, each place in order.
	[[nodiscard]] auto shipped_prompt_macros() -> std::vector<prompt_builtin>;

	/// \brief The reviewer's change to one built-in.
	struct prompt_builtin_change
	{
		std::string Id;
		/// The reviewer's text; none follows the shipped text of each
		/// release.
		std::optional<prompt_macro_body> Body;
		/// The reviewer's On; none follows the shipped On.
		std::optional<bool> On;
		/// The shipped revision `Body` was written against.
		std::uint32_t WrittenAgainst = 0U;

		auto operator<=>(const prompt_builtin_change&) const = default;
	};

	/// \brief One place's order, by identity.
	struct prompt_place_order
	{
		prompt_place             Place = prompt_place::agent_tab;
		std::vector<std::string> Ids;

		auto operator<=>(const prompt_place_order&) const = default;
	};

	/// \brief [[BPR-2179]] (spec section 11). One repository's change to one
	/// app-wide prompt: each field it set, on its own; a field it left follows
	/// later app-wide edits.
	struct prompt_override
	{
		std::string                                  Id;
		std::optional<std::string>                   Name;
		std::optional<std::string>                   Template;
		std::optional<prompt_ask>                    Kind;
		std::optional<std::vector<prompt_condition>> Conditions;
		std::optional<prompt_match>                  Match;
		/// Set with `Conditions`: the nested groups as this repository has them.
		std::optional<std::vector<prompt_condition_group>> Groups;
		/// Set with `Conditions`: Hide it or Show it disabled.
		std::optional<prompt_when_unmet> WhenUnmet;
		std::optional<bool>              On;

		auto operator<=>(const prompt_override&) const = default;
	};

	/// \brief [[BPR-2179]]. One repository's change to one surface: the
	/// placements it added and removed, and its own order once it reorders
	/// (the coordinator's default: a repository may reorder). A placement
	/// added app-wide afterwards goes to the end of a reordered surface.
	struct prompt_repository_surface
	{
		prompt_surface                          Surface = prompt_surface::pull_request_menu;
		std::vector<std::string>                Added;
		std::vector<std::string>                Removed;
		std::optional<std::vector<std::string>> Order;

		auto operator<=>(const prompt_repository_surface&) const = default;
	};

	/// \brief [[BPR-2179]]. A repository's differences from the app-wide
	/// prompts and surfaces, applied to its pull requests only.
	struct prompt_repository_layer
	{
		/// `owner/name`, as the window names it.
		std::string                  Repository;
		std::vector<prompt_override> Overrides;
		/// The repository's own prompts ("Only here").
		std::vector<prompt_macro>              Own;
		std::vector<prompt_repository_surface> Surfaces;

		auto operator<=>(const prompt_repository_layer&) const = default;
	};
	/// \brief The most repositories a profile keeps a layer for.
	inline constexpr std::size_t maximum_prompt_repositories = 64U;

	/// \brief What a profile stores: only differences from the shipped set.
	struct prompt_macro_differences
	{
		std::vector<prompt_builtin_change> Changes;
		/// The reviewer's own macros, in full.
		std::vector<prompt_macro>       Own;
		std::vector<prompt_place_order> Orders;
		/// [[BPR-2179]]. Each surface the reviewer changed.
		std::vector<prompt_surface_change> Surfaces;
		/// [[BPR-2179]]. Each repository's layer.
		std::vector<prompt_repository_layer> Repositories;

		auto operator<=>(const prompt_macro_differences&) const = default;
	};

	/// \brief One macro of a merged set, with its state in words.
	struct prompt_set_entry
	{
		prompt_macro Macro;
		/// A built-in whose text differs from the shipped text.
		bool Changed = false;
		/// A changed built-in written against an older shipped revision: its
		/// sheet offers Compare.
		bool Outdated = false;

		auto operator<=>(const prompt_set_entry&) const = default;
	};

	/// \brief The shipped set with a profile's differences applied.
	struct prompt_macro_set
	{
		/// Every place in `prompt_place` order, each in the reviewer's order.
		std::vector<prompt_set_entry> Entries;
		/// Stored rows not applied, by identity: a change naming no built-in,
		/// an own macro reusing an identity or flagged built-in, and own
		/// macros past `maximum_prompt_macros`.
		std::vector<std::string> Dropped;
		/// [[BPR-2179]]. Where each prompt appears: the surfaces the fields
		/// describe, with the stored surface changes applied.
		prompt_surfaces Surfaces;
		/// [[BPR-2179]]. The repositories' layers, applied by
		/// `prompt_set_for_repository`.
		std::vector<prompt_repository_layer> Repositories;

		/// \brief The macros alone, in order, for `decide_prompt_offers`.
		[[nodiscard]] auto macros() const -> std::vector<prompt_macro>;

		auto operator<=>(const prompt_macro_set&) const = default;
	};

	/// \brief Merges `aShipped` with `aStored`.
	///
	/// \details A change replaces a built-in's text and On; an unchanged
	/// built-in keeps the shipped text. Each place follows its stored order;
	/// an identity the order does not name goes after the place's other
	/// built-ins when it is a built-in (one added by a later release), and
	/// last when it is the reviewer's own. With nothing stored the result is
	/// the shipped set.
	[[nodiscard]] auto merge_prompt_macros(std::span<const prompt_builtin> aShipped, const prompt_macro_differences& aStored) -> prompt_macro_set;

	/// \brief The longest turn name kept: a chip's text, expanded.
	inline constexpr std::size_t maximum_prompt_turn_name_bytes = 1024U;
	/// \brief The most turn names a profile keeps; the least recently sent
	/// go first.
	inline constexpr std::size_t maximum_prompt_turn_names = 256U;

	/// \brief RFC-BPR-0125 milestone 2 (AC07). The name a turn was sent
	/// under, for a turn whose question is a macro's expansion and differs
	/// from its name.
	struct prompt_turn_name
	{
		/// The question as the thread keeps it: the expansion sent.
		std::string Asked;
		/// The chip's text it was sent as.
		std::string Name;

		auto operator<=>(const prompt_turn_name&) const = default;
	};

	/// \brief [[BPR-2237]]. Whether a turn sent from a prompt is the
	/// prompt's own expansion, as offered: then the prompt's name stands for
	/// it. Text the reviewer changed or added to is theirs and is shown.
	/// Leading and trailing white space aside.
	[[nodiscard]] auto prompt_sent_unedited(std::string_view aSent, std::string_view aExpansion) noexcept -> bool;

	/// \brief Whether a record is within its bounds, names something, and
	/// differs from its question (a name equal to it says nothing).
	[[nodiscard]] auto valid(const prompt_turn_name& aName) noexcept -> bool;

	/// \brief The turn names a profile keeps, so a thread read back after a
	/// restart shows the name each turn was sent under.
	///
	/// \details Pure and bounded: at most `maximum_prompt_turn_names`, the
	/// least recently remembered going first. One question has one name, the
	/// last it was sent under.
	class prompt_turn_names
	{
	public:
		/// \brief Remembers one name.
		/// \returns Whether what is kept changed; false for an invalid record
		/// or one already kept as the newest.
		auto remember(prompt_turn_name aName) -> bool;
		/// \brief Takes the store's records, least recently remembered first.
		/// A name remembered here before is newer and is kept.
		void adopt(const std::vector<prompt_turn_name>& aNames);
		/// \brief The name `aAsked` was sent under, if one is kept.
		[[nodiscard]] auto recall(std::string_view aAsked) const -> std::optional<std::string>;
		/// \brief Every kept record, least recently remembered first.
		[[nodiscard]] auto records() const noexcept -> const std::vector<prompt_turn_name>& { return Names; }

	private:
		std::vector<prompt_turn_name> Names;
	};
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_MACRO_HPP
