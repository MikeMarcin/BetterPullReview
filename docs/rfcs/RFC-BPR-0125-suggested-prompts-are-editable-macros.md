---
id: "RFC-BPR-0125"
title: "Suggested prompts are editable macros with rules for where and when they show"
status: "In Progress"
metadata: {"origin":"owner idea, 2026-11-12; drafted with author-rfc, frontend-design loaded for the UX","inspected_revision":"d8123f30a467aaff5ada11a1b9acae95288143f5","inspected_lane":"claude/v0-5-0-prerelease-targets-52265d at 451f6eac7b0738dcdd543ecf7a5394f4fbea4711","baseline":"ce5ffa1461c9a0006510621a997fbccc303a4e48","mandate":"owner, 2026-11-12: implement with Q1 to Q10 answered; milestone 1 started on main"}
depends_on: ["RFC-BPR-0092", "RFC-BPR-0101", "RFC-BPR-0102", "RFC-BPR-0107"]
---

# Suggested prompts are editable macros with rules for where and when they show

## Problem and grounded current behavior

The owner, 2026-11-12:

> I wonder if we can make suggested prompts configurable macros we can
> add/remove/edit in the settings. Where the thing shown as the suggestion to
> click isn't literally what gets sent to the agent, it expands and sends or
> drafts (shift) when you click it. And then maybe they can have triggers or
> rules as to when and where they appear. I haven't fully considered this
> macro editor so you should come up with a design including functionality
> and UX.

Two revisions were read: `main`'s source at `d8123f30a`, and the lane
`claude/v0-5-0-prerelease-targets-52265d` at `451f6eac7`, where the code of
RFC-BPR-0102 (subjects) and RFC-BPR-0107 (the exchange) is built and not yet in
`main`'s source. Where they differ the lane is described, because this RFC
builds on it. Files that exist only on the lane are named, not linked.

### Where suggested prompts appear

Exactly two surfaces build a suggestion list, both with one component,
`agent_suggestion_list`
([agent_suggestion_list.cpp](../../src/adapters/qt/src/agent_suggestion_list.cpp)).

| Place | Offered | Shown when | Sent as |
|---|---|---|---|
| Agent tab | Review this pull request | Always | A review of the pull request (`run_ai_review`) |
| | Review *path* | A file is open in Review | A review, that file as subject |
| | What could this change break? | Always | A question about the pull request (`open_full_agent_session`) |
| | Explain *path* line *n*, or Explain the selected block (*path* lines *a–b*) | Lines are selected on one side | A question, the lines and their text as subject (`ask_about_selection`) |
| Agent tab with a conversation attached ([RFC-BPR-0094](RFC-BPR-0094-ask-the-agent-about-a-conversation.md)) | Summarize this conversation; Is the concern raised here right?; then Draft a reply to this conversation, or for a private note Explain the code this note is on | The conversation chip is attached; these replace the four above | A question about the conversation (`ask_about_conversation`) |
| Tour chat, a stop | Explain the selected block; What does this code do?; Why does this change matter?; Rewrite this explanation more simply; Is this the right place for this stop? | The chat is empty; the first only while text is selected, and then at any time | A question in that stop's thread, the selected passage quoted |
| Tour chat, Problem or Solution | Explain the selected block; Make this clearer; What's missing?; Make it shorter | As a stop's | As a stop's |
| Tour chat, the whole tour | Too many stops?; Suggest a better order; Focus on …; What's not covered? | The chat is empty | A question; Focus on … is put in the field to finish |

Overview has no chips of its own: the Agent tab beside it offers the two
prompts that need no file. The selected-lines side chat, Review Cycles, the
comment workshop's six operations and Enhance offer none; they are fixed
actions. [RFC-BPR-0119](RFC-BPR-0119-ask-the-agent-why-a-check-failed.md),
still proposed, would add a third list about a failed check.

### How they are defined, and what they send

- **Hard-coded.** The Agent tab's are `tr()` strings assembled in the widget
  (`refresh_suggestions`,
  [agent_tab_view.cpp](../../src/adapters/qt/src/agent_tab_view.cpp)); on the
  lane a map from the chip's text to its subject and review flag
  (`SuggestionSubjects`) rides beside them. The tour's are three constant
  arrays in
  `tour_chat_prompts.hpp` (removed by milestone 2),
  put there by [[BPR-1934]] "so Agent settings can later make them editable".
  No translation is shipped.
- **The chip's text is the message.** Each is "sent as written".
  [RFC-BPR-0102](RFC-BPR-0102-agent-questions-say-what-they-point-at.md)
  defines a suggestion as "a subject and a text".
- **What goes with it.** Besides the message, a request carries the pull
  request's brief (title, description, base, file list, threads); the subject
  the core decides (`decide_agent_question`, lane file
  `core/agent_question.hpp`: the pull request with every changed file, one
  file, lines with their text up to 32 KiB, or a conversation); where the
  reviewer is looking; the review skill and the suggested-comment format; and
  up to four skills the message names with `/name`
  ([skill_references.hpp](../../src/core/include/bpr/core/skill_references.hpp)).
  Private notes and drafts go only by their own explicit actions. A message
  is at most 16 KiB (`maximum_chat_message_bytes`,
  `maximum_tour_question_bytes`).
- **Send and draft exist, for the chip's text.** [[BPR-1872]]:
  `decide_agent_suggestion_press`
  ([agent_turn.hpp](../../src/core/include/bpr/core/agent_turn.hpp)). A
  click, Return, Enter or Space sends. Shift with any of them puts the text in
  the question field. Words already typed are never replaced or sent with it.
  On the lane a prompt put in the field and then sent is a typed question: it
  no longer carries its subject or its review flag.
- **When the list shows.** `decide_agent_suggestions`: an agent is offered,
  setup has finished, no turn runs and the reviewer's toggle is on. The
  toggle is kept per profile in `agent-setup.sqlite3`.
- **What Settings edits today.** Settings › Agents & context › Built-in
  prompts ([[BPR-1917]]) edits instructions: the review skill and the writing
  hooks. It does not hold the suggested prompts.
  [RFC-BPR-0107](RFC-BPR-0107-agent-and-prompt-sharing.md) notes that the
  tour-chat suggestions would join its exchange "if Settings ever makes them
  editable". The owner on that editor's completion: "this skill ux popup with
  the / command is awesome".

So a reviewer cannot reword a chip, add one, remove one never used, or say
where one belongs. And because the chip's text is the message, a chip cannot
stand for more than one short sentence.

## Proposed behavior and boundaries

Each suggested prompt becomes a **macro**: a short name on the chip, a longer
template that expands into what is sent, and a small set of rules for where
and when the chip shows. The shipped prompts are macros too, and with nothing
edited every surface behaves exactly as today. "Macro" is this RFC's word;
the product keeps saying **suggested prompt**.

Frames: [mockup](RFC-BPR-0125-suggested-prompts-are-editable-macros-mockup.html).
The Settings frames follow the accepted
[Agents roster](../design/BPR-2051-agents-roster-and-footer/README.md) and the
[setup](../design/BPR-1747-agent-setup-v1/index.html) the owner likes.

### What a macro is

| Part | In the editor | Meaning | Bound |
|---|---|---|---|
| Name | Shown as | The chip's text. One line. May use variables | 120 bytes |
| Template | Sends | Expands into the message | 8 KiB, a writing hook's bound; the expansion at most the 16 KiB message bound |
| Place | Where | One of five places, below | Closed set |
| Subject | About | What the question is about, from the place's own choices | Closed set |
| Kind | Ask as | A question, or a review with findings | Agent tab only |
| Rules | Shows when | Up to four conditions, all of which must hold | 4 |
| On | The row's checkbox | Off: kept, never offered | |
| Identity | Not shown | A fixed name for a built-in; minted by the host for the reviewer's own | |

A macro has one place. The same prompt in a second place is a duplicate,
because a place fixes which subjects and variables exist.

| Place | About | Ask as |
|---|---|---|
| Agent tab | The pull request; the open file; the selected lines | A question; for the pull request and the open file also a review |
| Agent tab, about a conversation | The conversation | A question |
| Tour chat: a stop | The stop; the selected text | A question |
| Tour chat: Problem and Solution | The part; the selected text | A question |
| Tour chat: the whole tour | The tour | A question |

About and Ask as feed the existing decisions unchanged: `decide_agent_question`
picks the subject and entry point, and the tour chat quotes the selected
passage as it does now. This RFC adds no subject, entry point or field to a
request.

### Template and variables

A template is plain text with variables written `{name}`.

- `{{` is a literal `{`. Any other brace is literal.
- A `{name}` whose name is not in the table for the macro's place is refused
  when the macro is saved. A misspelt variable is never sent as text.
- Expansion is one pass. A value is inserted as written and never read again:
  not as a variable, not as a skill reference, not as a slash command.
- A value is one line. Line breaks and control characters in it become
  spaces. A value is never shortened; an expansion over the message bound
  makes the macro not offered.
- `/skill-name` in the template's own text is a skill reference, exactly as
  in a typed question.

| Variable | Fills in | Places | Needs |
|---|---|---|---|
| `{pr.number}`, `{pr.title}`, `{pr.url}`, `{pr.author}` | The pull request's number, title, web address and author's login | All | A pull request |
| `{pr.base}`, `{pr.head}` | The branch names | All | A pull request |
| `{repo}` | `owner/name` | All | |
| `{file}`, `{file.name}` | The open file's path, and its last segment | Agent tab | A file open in Review |
| `{lines}` | `line 34` or `lines 34–36` | Agent tab | Lines selected |
| `{lines.first}`, `{lines.last}` | The two numbers | Agent tab | Lines selected |
| `{conversation.file}` | The file the conversation is on | About a conversation | A conversation on a file |
| `{stop.number}`, `{stop.title}`, `{part}` | The stop, and `the Problem`, `the Solution` or `Stop 3` | Tour chats | |
| `{cursor}` | Nothing. Marks where the caret goes | All | |

**Variables name things; they carry no content.** Every value is something
the request's brief or subject already holds, or the chip already shows.
Selected text, a diff, a description, a thread's words, notes, drafts and
check output reach the agent only as they do today, through the subject and
the brief. So a macro cannot widen what is disclosed, which keeps
[RFC-BPR-0012](RFC-BPR-0012-ai-review-chat-findings-mcp.md)'s rule that
navigation adds no context. Content variables such as `{selection}` are out
of scope (owner, Q3).

**A macro you finish.** A template that contains `{cursor}` is never sent by
a click. It goes to the question field with the caret at the mark. The
shipped "Focus on …" is the template `Focus on {cursor}`.

**Missing context.** A variable's Needs column is an implied rule: a macro
that uses `{file}` is offered only while a file is open, in its name or its
template. So a chip on screen can always be expanded, as RFC-BPR-0102
requires of a suggestion. A press expands against the facts at the press; if
the macro no longer applies, the press does nothing and the list refreshes.

### Where and when it shows

A condition is a fact, "is" or "is not", and a value chosen from a list. A
macro shows when all its conditions hold, with those its subject and
variables imply.

| Fact | Reads in the editor | Places |
|---|---|---|
| File open | A file is, or is not, open in Review | Agent tab |
| Selected lines | Selected lines are, or are not: any, one line, several lines | Agent tab |
| Path | The open file's path matches, or doesn't match, *patterns* | Agent tab |
| Draft | The pull request is, or is not, a draft | All |
| Author | The author is, or is not, me | All |
| Checks | Checks are, or are not, failing | All |
| Repository | The repository matches, or doesn't match, *patterns* | All |
| Takes replies | The conversation takes, or doesn't take, replies | About a conversation |
| Empty chat | This chat is, or is not, empty | Agent tab; tour chats |

- **Patterns.** Up to 8, separated by commas, each at most 128 bytes, read as
  `.gitignore` and Find in files read them: a pattern without a slash is
  matched against the file's name, one with a slash against the path, and
  `**` crosses folders. The core's existing `wildcard_match`
  ([review_cycle_ignore.hpp](../../src/core/include/bpr/core/review_cycle_ignore.hpp))
  does the matching. A language is chosen by pattern (`*.cpp, *.hpp`); there
  is no language condition.
- **Checks failing** means a check on the head commit ended in failure,
  timeout, cancellation or action required, in the list Overview already
  holds under [RFC-BPR-0068](RFC-BPR-0068-inspect-commit-checks.md).
- **Unknown is not a match.** A fact the window does not hold yet satisfies
  neither "is" nor "is not". No request is ever made to answer a condition.
- **Rules decide appearance only.** A macro never sends itself. Every agent
  action still starts from a visible press
  ([ai-experience.md](../ux/ai-experience.md)).

**Why a list of conditions and not a rule language.** Nine facts, two
polarities and closed values can be tested as a table, explained one
condition at a time ("not shown: no lines are selected"), built from pickers
with no syntax to learn, read by a screen reader, and validated on import
without running anything. "Or" is a second macro. Expressions, nesting and
regular expressions would need a parser, would make "why is this chip here"
unanswerable at a glance, and would turn shared macros into something that
must be reviewed like code.

### Order and limits

- The list of each place is in the reviewer's order. The first **5** macros
  that are on and whose conditions hold are chips. Five is today's largest
  list (a tour stop with text selected).
- Further matches sit behind one quiet last chip, **More prompts (n)…**, a
  menu in the same order with the same press rules.
- A profile holds at most 64 macros, the 22 built-ins included.
- The chips are rebuilt only when their names or order change, so the
  keyboard's place survives a refresh that changes nothing.
- Unchanged: none before setup finishes, none while a turn runs, none when
  the reviewer's toggle is off, and the lead line only in an empty thread.

### Pressing, previewing, and what the thread shows

- **Click, Return, Enter, Space** expand and send, with the macro's subject
  and kind. **Shift** with any of them puts the expansion in the question
  field. `decide_agent_suggestion_press` still decides, so typed words are
  kept as [[BPR-1872]] defines.
- **A drafted macro stays what it was (Q2).** While the field holds a macro's
  draft, a removable chip beside the lines chip names it. Sending keeps its
  subject and kind. Removing the chip, or emptying the field, makes the text
  a typed question. Today's lane drops the subject silently.
- **Preview.** A chip's tooltip is its name when shortened, then the
  expansion (its first 12 lines), then "Click to send. Shift-click to edit
  before sending." A macro you finish says "Click to put it in the question
  field." The accessible name is the chip's text; the accessible description
  holds the expansion and the same sentence.
- **The thread shows what was sent.** A turn's question is the expanded text.
  When that differs from the name, the name is its first line in the weight
  "About *conversation*" uses, and text past 6 lines folds behind Show all.
  History keeps both.
- **Values cannot command.** On a direct send, a skill reference is found
  only in the template's own text, and an expansion is never read as
  `/model` or `/effort`. A pull request's title cannot bring in a skill.

### The built-in macros

The shipped set is today's prompts, 22 macros, each with a fixed identity.
Names and templates are equal unless shown.

| Place | Built-in macros, in order |
|---|---|
| Agent tab | Review this pull request (review of the pull request); `Review {file}` (review of the open file); What could this change break?; `Explain {file} {lines}` (when one line is selected); `Explain the selected block ({file} {lines})` (when several are) |
| About a conversation | Summarize this conversation; Is the concern raised here right?; Draft a reply to this conversation (when it takes replies); Explain the code this note is on (when it does not) |
| Tour chat: a stop | Explain the selected block (about the selected text); then, while the chat is empty: What does this code do?; Why does this change matter?; Rewrite this explanation more simply; Is this the right place for this stop? |
| Tour chat: Problem and Solution | Explain the selected block (about the selected text); then, while the chat is empty: Make this clearer; What's missing?; Make it shorter |
| Tour chat: the whole tour | While the chat is empty: Too many stops?; Suggest a better order; Focus on … (sends `Focus on {cursor}`); What's not covered? |

- A built-in can be edited, turned off and reordered. It cannot be deleted
  or moved to another place; Duplicate makes a copy that can.
- A changed built-in says **Changed** and offers **Reset to built-in**.
- An unedited built-in follows the shipped text of each release. A changed
  one keeps the reviewer's text and records the shipped version it was
  written against, as RFC-BPR-0107 records for prompt overrides; its sheet
  then offers Compare and Reset to built-in.
- A built-in added by a later release appears on, after its place's other
  built-ins.
- In a tour chat, a prompt about text selected in another part names that
  part after its name ("… in the Solution"), as today. The macros about the
  selected text are those of the selected part's place; they lead the list,
  in that place's order, and the chat's own place follows with the rest.
- The Agent tab's widget also words a selection with no file open ("Explain
  the selected line (34)"). The composition gathers lines only while a file
  is open, so no reviewer sees it, and it has no built-in.

### Storage, versions and privacy

- **Where.** The profile's `agent-setup.sqlite3`, beside the agents, the
  review skill and the suggestions toggle. Macros belong to a BetterPullReview
  profile ([RFC-BPR-0024](RFC-BPR-0024-bpr-profiles-multiple-windows.md)),
  not to a repository or a host. A settings file was not chosen: the store
  already holds what macros are edited and exchanged with, and it commits
  with RFC-BPR-0107's import journal.
- **What.** Only differences from the shipped set: changes to built-ins, the
  reviewer's own macros in full, and each place's order.
- **Revisions.** The set has a revision. A write names the revision it was
  made from, and a write from an older one is refused with nothing changed,
  so two windows cannot overwrite each other.
- **Upgrade.** A store without the tables is the shipped set. Nothing is
  migrated and nothing visible changes on upgrade.
- **Downgrade.** A row naming a place, fact or variable this build does not
  know is kept, listed as needing a newer BetterPullReview, never offered and
  never rewritten.
- **Failure.** If the store cannot be read, the shipped chips still work and
  Settings says the reviewer's edits are unavailable.
- **Privacy.** A template is the reviewer's own text. It is stored in the
  profile's private store, sent to the chosen agent as the message exactly
  as previewed, and written to an export only when chosen there. Diagnostics
  log a macro's identity and reason codes, never its text
  ([RFC-BPR-0027](RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md)).
- **Secrets.** The editor says once that a prompt goes to the agent and into
  exports. It also notes a template holding a well-known credential prefix
  (`ghp_`, `github_pat_`, `sk-`, `-----BEGIN`). The note is advice. It does
  not block saving and proves nothing about a template without it.
- **Untrusted values.** A title, branch, login, path or stop title is written
  by someone else. It is inserted as one line of data under the rules above
  and is visible in the tooltip before a press.

### Settings: Suggested prompts

A new section of Settings › Agents & context, between the agents' settings
group and Built-in prompts. It is a roster like the agents': one compact row
per macro, and a row opens a sheet. The list stays calm and the editor
appears only when asked for.

```text
Suggested prompts
The questions offered as one-click chips. A click sends what the prompt
expands to. Shift-click puts it in the question field first.

 Agent tab                                   5 show at once; the rest are under More
┌────────────────────────────────────────────────────────────────────────────────┐
│ ⠿ [✓] Review this pull request                                    Built-in   › │
│        A review of the pull request. Always shown.                             │
│ ⠿ [✓] Review {file}                                               Built-in   › │
│        A review of the open file.                                              │
│ ⠿ [ ] What could this change break?                          Built-in, off   › │
│        A question about the pull request. Always shown.                        │
│ ⠿ [✓] Security pass on {file.name}                                           › │
│        A question about the open file, when its path matches src/**, *.cpp     │
│        and the pull request is not a draft.                                    │
│ ⠿ [✓] Explain {file} {lines}                                       Changed   › │
│        A question about the selected lines, when one line is selected.         │
└────────────────────────────────────────────────────────────────────────────────┘
 Agent tab, about a conversation
┌ four rows ─────────────────────────────────────────────────────────────────────┐
 Tour chat: a stop      Tour chat: Problem and Solution      Tour chat: the whole tour
┌ their rows ────────────────────────────────────────────────────────────────────┐

 [ Add prompt… ]
┌────────────────────────────────────────────────────────────────────────────────┐
│ Bring back the built-in prompts as shipped             [ Restore built-ins… ]  │
└────────────────────────────────────────────────────────────────────────────────┘

Built-in prompts
 (the instruction editor, unchanged)
```

- **A row** is the drag handle, the On checkbox, the name as written with
  its variables, one sentence saying what it sends as and when it shows, its
  state in words (Built-in, Changed, off) and a chevron.
- **Order.** Drag within a place, or Alt with Up or Down on the selected
  row; the move is announced ("Moved to 3 of 6").
- **Add prompt…** opens the sheet empty, in the place of the selected row.
- **Restore built-ins…** returns every built-in to its shipped text, order
  and On after a confirmation that names how many are changed. The
  reviewer's own macros stay.
- Remove every agent and set up again leaves macros alone.

```text
┌ Suggested prompt ─────────────────────────────────────────────────────────┐
│ Shown as   [ Security pass on {file.name}                              ]  │
│            The chip reads: Security pass on renderer.cpp                  │
│                                                                           │
│ Where      [ Agent tab ▾ ]    About [ The open file ▾ ]                   │
│ Ask as     (•) A question    ( ) A review with findings                   │
│                                                                           │
│ Sends                                                      [ Insert ▾ ]   │
│ ┌───────────────────────────────────────────────────────────────────────┐ │
│ │ Check {file} for injection, unsafe deserialisation and missing        │ │
│ │ authorisation checks. Say which lines, and what an attacker could     │ │
│ │ do. /security-review                                                  │ │
│ └───────────────────────────────────────────────────────────────────────┘ │
│ 168 of 8,192 bytes                                                        │
│                                                                           │
│ Preview    with [ This pull request (#482) ▾ ]          Shows now: yes    │
│ ┌───────────────────────────────────────────────────────────────────────┐ │
│ │ Check src/render/renderer.cpp for injection, unsafe deserialisation   │ │
│ │ and missing authorisation checks. Say which lines, and what an        │ │
│ │ attacker could do. /security-review                                   │ │
│ └───────────────────────────────────────────────────────────────────────┘ │
│                                                                           │
│ Shows when all of these are true                                          │
│  [ The open file's path ▾ ] [ matches ▾ ] [ src/**, *.cpp          ]  ✕   │
│  [ The pull request     ▾ ] [ is not  ▾ ] [ a draft ▾              ]  ✕   │
│  + Add a condition                                                        │
│  It also needs a file open in Review, because it uses {file}.             │
│───────────────────────────────────────────────────────────────────────────│
│ [ Delete… ] [ Duplicate ]                       [ Cancel ] [ Save prompt ]│
└───────────────────────────────────────────────────────────────────────────┘
```

- **Shown as** and **Sends** tint each variable in the agent accent, and the
  preview tints each value the same way, so what was written and what is
  sent can be matched by eye. Sends uses the fixed-pitch, wrapping editor of
  Built-in prompts, with its `/skill` completion.
- **Insert** lists the place's variables with what each fills in. Typing `{`
  opens the same list at the caret.
- **Preview** expands against the pull request window Settings was opened
  from, with names that window already shows, or against sample values when
  there is none. It updates as the reviewer types, without saving.
- **Shows now** says yes, or names the first condition that fails ("No: no
  lines are selected"). It is the answer to "why isn't my chip there".
- **Conditions** are three pickers a row, up to four rows. Implied needs are
  stated in a sentence under them and take no row.
- **Buttons.** A built-in shows Reset to built-in in place of Delete…, when
  it is changed. Delete removes one of the reviewer's own, and the section
  offers Undo until Settings closes. Cancel with changes asks before
  discarding. Ctrl or Cmd with Return saves, as composers that save do under
  [RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md).

| Case | What the sheet says | Blocks Save |
|---|---|---|
| No name | Give the prompt a name. It is what the chip says. | Yes |
| Name too long, or more than one line | The name is too long by *n*. A name is one line. | Yes |
| Nothing to send | Write what the prompt sends. | Yes |
| Unknown variable | `{flie}` isn't something this place can fill in. Insert lists what it can. | Yes |
| Another place's variable | `{stop.title}` is filled in only in a tour stop's chat. | Yes |
| Template over 8 KiB | Too long by *n* bytes. A prompt can be 8 KiB. | Yes |
| A pattern that cannot match | `[abc` isn't a pattern. Close the bracket. | Yes |
| Conditions that cannot hold together | These can't both be true, so the prompt would never show. | No |
| A name already used in this place | Another prompt here has this name. | No |
| A credential prefix | This looks like it holds a token. A prompt goes to the agent and into exports. | No |
| Changed in another window | Suggested prompts changed in another window. Your edit is kept: look at the list, then save again. | Until saved again |
| The store refused the write | Couldn't save. Nothing changed. | Until saved again |

**Empty and error states.** A place with every macro off says "No prompts
show here. Turn one on, or add your own." A row from a newer version says
"Needs a newer BetterPullReview" and can only be deleted. An unreadable store
replaces the roster with one sentence and the reason, and the chips fall back
to the shipped set.

**Keyboard and accessibility.** Every action above has a keyboard path: Up
and Down move through the roster, Space toggles On, Return opens the sheet,
Delete asks to delete. Rows expose name, place, state and On to a screen
reader; each picker of a condition is labelled; the preview is a read-only
region named "What is sent". One polite, combined announcement follows a
change. State is said in words, never by colour alone. Widgets inherit the
palette and style with no QSS, use `quiet_push_button` and
`disclosure_button`, and fit the 760 by 480 minimum Settings window without
sideways scrolling.

### At the point of use

```text
 Ask about this pull request, or try:
 ( Review this pull request )
 ( Review src/render/renderer.cpp )         ┌ tooltip ───────────────────────────┐
 ( Security pass on renderer.cpp )  ◄────── │ Check src/render/renderer.cpp for  │
 ( Explain renderer.cpp lines 5–8 )         │ injection, unsafe deserialisation… │
 ( Too clever? )                            │ and 2 more lines                   │
 ( More prompts (2)… )                      │ Click to send. Shift-click to edit │
┌────────────────────────────────────────┐  │ before sending.                    │
│ [renderer.cpp 5–8 ✕] [Security pass ✕] │  └────────────────────────────────────┘
│ Check src/render/renderer.cpp for …    │
│ [✦] [+]                Claude ▾ [Send] │   after Shift-click: the draft keeps
└────────────────────────────────────────┘   its macro as a removable chip (Q2)

 the turn, once sent                         a chip's context menu
┌──────────────────────────────── 10:42 ─┐  ┌──────────────────────────────┐
│ Security pass on renderer.cpp          │  │ Edit this prompt…            │
│ Check src/render/renderer.cpp for      │  │ Turn off                     │
│ injection, unsafe deserialisation …    │  │ Suggested prompts settings…  │
│ Show all                               │  └──────────────────────────────┘
└────────────────────────────────────────┘
```

- Chips keep their component, look and agent accent. Only their content
  changes.
- **A chip's menu** (right-click, or the Menu key on a focused chip): Edit
  this prompt… opens Settings on that macro's sheet; Turn off removes it and
  the status bar offers Undo; Suggested prompts settings… opens the section.
- **Typing a name (Q6).** In a composer that offers `/skill` completion,
  `/` and letters list skills only, exactly as today. Typing `/p` and a
  space opens the same popup on this place's macros instead: the letters
  after `/p ` keep those whose name has a word starting with them. Choosing
  one replaces the typed `/p …` with its expansion, as a draft. Nothing is
  resolved at send: `/model`, `/effort`, a typed `/skill-name` and a typed
  `/p …` that was never chosen from the popup behave as they do today.

### Sharing

Macros join RFC-BPR-0107's exchange as its last milestone here, because the
owner asked of that exchange, on 2026-11-11, to "export all prompts so I can
improve them agenticly with context from my org and import them back".

> Owner amendment (2026-11-18): "I want to make sure that we can import that
> such that it merges in. Aka doesn't replace other different prompts,
> updates to the prompt pack can update older imports in-place. This likely
> means every prompt needs a stable id (and gets a new one when duplicated)."
>
> Owner decision (2026-11-18), on the form of those IDs: "I'm thinking uuids
> probably make the most sense so that we dont get collisions if multiple
> people all over are making prompts".

The shared unit is a **prompt pack**: the reviewer's own prompts they chose,
under one pack identity.

- **The file.** Exchange version 2 adds a pack and its macros. The pack has
  a stable pack ID, a label and a version that only increases. Each macro
  carries its portable stable ID, its place and its body: name, template,
  About, Ask as, conditions, Quick ask and scopes. On and placements stay
  with each reviewer. An export that includes no macro is still written as
  version 1, byte for byte as before, and a version 1 file imports exactly
  as before. An older build refuses a version 2 file as written by a newer
  version, which is the refusal it already gives any newer file.
- **Stable IDs are UUIDs.** A reviewer's own prompt and a pack are each
  identified by a random (version 4) UUID, written in lowercase. The host
  mints every UUID and passes it in, so the core stays deterministic. A
  built-in keeps its fixed name and is never exported: to share one, the
  reviewer duplicates it. Duplicating any prompt, including an imported
  one, mints a new UUID and drops the copy's link to its pack, so the copy
  is the reviewer's own.
- **Prompts made before UUIDs** keep their stored identity until they are
  first exported. That export gives each of them a new UUID and rewrites
  every reference to it (order, surfaces, repository layers) in the same
  all-or-none write that records the pack. The store is not migrated in
  bulk: only an exported prompt's ID is ever read by another profile.
- **Export** offers each of the reviewer's own prompts, with its exact text,
  under a pack: a new one (a label, a new pack ID, version 1) or one the
  profile already holds (its ID, its highest version plus one). Each starts
  unticked, so an export stays version 1 unless the reviewer chooses a
  prompt. The pack is laid out one member a line so it can be edited in
  place. Exporting
  records, on each exported prompt, the pack and the version and body it
  was exported as. The exporter's own copies are then the pack's, so a file
  edited elsewhere (by an agent, say) and imported back as a later version
  updates them in place.
- **Import matches by macro ID, never by name.** Against each prompt of the
  file:
  - No prompt here has its ID: **Added**, on, with the placements its
    fields describe.
  - A prompt here has its ID, and the reviewer has not edited it since they
    last imported (or exported) that pack: **Updated in place**. Its body
    and place take the file's; its On, placements and order stay.
  - A prompt here has its ID and the reviewer has edited it since, or it was
    never linked to this pack: a **conflict**, Replace or Keep mine,
    starting at Keep mine. Each copy records the pack version it was
    written against, as an edited built-in hook records the built-in
    version it was written against; Keep mine leaves that record as it is.
  - A prompt here equal to the file's: **Unchanged**, nothing to choose.
  - A prompt here that an earlier version of the pack held and this version
    drops is offered as **Remove** or **Keep**: Remove starts chosen when
    the reviewer has not edited it, Keep when they have. Keep makes it the
    reviewer's own. Nothing is removed silently.
  - Prompts not from the pack, built-ins included, are never touched.
  - A file whose pack version is lower than the highest version of that
    pack already here is **flagged as older**, and its updates and removals
    start unchosen.
- Import previews each item with its standing and choice, applies all
  chosen or none, refuses after an edit made meanwhile (the prompts' stored
  revision is part of what the preview was made from), and launches
  nothing.
- An item naming a place, About, Ask as, fact or variable the importing
  version lacks, or whose ID is not a UUID, is listed as unsupported and
  not applied. An item that names a built-in's ID, or is listed twice, is
  refused with that reason.

This settles the versioning question raised against RFC-BPR-0129, where the
exchange was ambiguous about which version carries suggested prompts:
version 2 is the prompt-pack format defined here, version 1 stays readable
and is still written without macros, and RFC-BPR-0129's own change to the
file builds on this version 2 rather than redefining it.

A file of macros in the reviewed repository is out of scope: text written by
a repository's contributors, one click from being sent, needs its own trust
design. The Repository condition gives a reviewer macros for one repository
without it.

### Who owns what

| Layer | Owns |
|---|---|
| Core | The macro value and its bounds; the template scan and its refusals; the fact and condition catalogue; evaluation with a reason for each unmet condition; expansion; order and the five-chip limit; the merge of the shipped set with stored differences; the press, through the existing press and question decisions; the exchange's semantics |
| Application host | The shipped set as a resource; the store port and its SQLite adapter; identities for the reviewer's own macros; revisions and serialized completions; each window's facts, gathered from what the session already holds; the projection Qt receives |
| Qt | The chips, More menu, tooltip, turn and composer chip; the roster and sheet; the completion list. Intents out, projections in, no effect |

Every rule is tested without `QApplication` and runs in the `core-only`
preset. Milestone 1 is pure core and needs none of the lane's code: a
macro's About and Ask as are its own closed values, and milestone 2 hands
the Agent tab's three to RFC-BPR-0102's `decide_agent_question` unchanged.
Milestones 2 onward start from a base that holds the lane's RFC-BPR-0102 and
RFC-BPR-0107 code.

### The library question for core

[src/core/AGENTS.md](../../src/core/AGENTS.md) asks that the owner be
consulted before a major general-purpose system is hand-written in core.
This design is shaped to need none:

- Conditions are stored as rows and built from pickers. There is no rule
  text, so there is nothing to parse.
- Patterns reuse `wildcard_match`. The exchange uses glaze, as RFC-BPR-0107
  does.
- Substitution is one linear scan for one fixed form, `{name}`, against a
  closed list.

This RFC judges that scan not to be a major general-purpose system, and the
owner confirmed it (Q1, 2026-11-12): closed-variable substitution, no
template library. The candidates that were weighed for a template language
(conditionals, loops, filters) were:

| Candidate | Licence | Layer | Fit |
|---|---|---|---|
| {fmt} named arguments | MIT; already approved in core | Core, behind a private wrapper | Poor. A name cannot contain a dot, `:` begins a format specification no reviewer meant, a stray brace is an error, and it has no conditionals either |
| kainjow/Mustache | Boost Software License 1.0, to be confirmed at adoption review | Core, one header behind a private wrapper | Fits a logic-less language with sections. Costs a new dependency, a licensing review and a second syntax, for sections over lists the facts do not contain |
| inja | MIT, to be confirmed | Core | Poor. It requires nlohmann/json, which is not approved (glaze is), and brings loops, includes and callbacks that conditions are designed to avoid |

Recommendation, accepted by the owner: adopt none.

### Proposed amendments to adopted RFCs

Applied by the owning milestones, not at drafting.

- **RFC-BPR-0102.** "A suggestion is a subject and a text" becomes a subject,
  a name and a template. A drafted suggestion keeps its subject (Q2).
- **RFC-BPR-0107.** The exchange gains suggested prompts as a prompt pack
  and file version 2, merged by UUID on import (see Sharing); a file with no
  macro stays version 1.
  Its remark that tour-chat suggestions would be prompt purposes is replaced
  by macro items.
- **[[BPR-1934]].** `tour_chat_prompts` gives way to the shipped set.
- **RFC-BPR-0119**, when built: its two prompts become built-ins of a sixth
  place, about a check, instead of a new hard-coded list.

### Out of scope

- A macro that sends itself, on any event.
- A rule or template language: expressions, "or", nesting, regular
  expressions, conditionals, loops, user-defined variables.
- Content variables (selected text, diff, hunk, description, thread text,
  notes, drafts, check output) (owner, Q3).
- A language condition. Patterns cover it.
- A prompt that asks for values in a form before sending. `{cursor}` covers
  finishing a sentence.
- A macro's own agent, model or effort, and a keyboard shortcut per macro.
- New places: Overview buttons, a file's header
  ([RFC-BPR-0117](RFC-BPR-0117-explain-this-file-where-i-am-reading.md)),
  check rows, Review Cycles, the selected-lines side chat.
- The comment workshop's operations, Enhance and the built-in instruction
  prompts. They stay as they are.
- Macros read from the reviewed repository, automatic distribution, and a
  `bpr` command for macros.
- Translating the built-in texts.
- Any change to what a request carries.

### Open questions for the owner, by impact

Each has a recommendation. A recommendation is not an answer; every one is
now answered, and the design above follows the answers.

Owner answers (2026-11-12): **Q1** closed-variable substitution, no template
library. **Q2** a drafted macro keeps its subject and kind as a removable chip.
**Q3** names and numbers only, no content variables. **Q4** the nine conditions as listed. **Q5** sharing through RFC-BPR-0107 in
this RFC. **Q6** `/` alone searches skills only; prompts are reached with
`/p <prompt name>`, and only after `/p ` does the popup search prompts.
**Q7** five chips, then More. **Q8** one place per macro. **Q9** "Suggested
prompts" in the product. **Q10** a section with a sheet.

1. **Q1. Is closed-variable substitution enough, or is a template language
   wanted?** It gates the template scan of milestone 1. *Recommended:*
   substitution, no library, as argued above.
2. **Q2. Does a drafted macro keep its subject and kind?** *Recommended:*
   yes, shown by a removable chip in the field. Otherwise Shift-clicking
   "Review this pull request" to add a sentence silently turns a review into
   a question. It holds one criterion of milestone 2.
3. **Q3. Names only, or content variables too?** *Recommended:* names only
   in this RFC. `{selection}` would repeat what a lines subject already
   sends and is the first step to macros that disclose more than the chip
   shows.
4. **Q4. Is this the right list of conditions?** Nine facts. *Recommended:*
   as listed. Dropping Checks removes the one fact that can be unknown for
   long; adding a language or label fact needs data the window does not
   hold.
5. **Q5. Share macros through RFC-BPR-0107's exchange in this RFC?**
   *Recommended:* yes, as the last milestone; repository files later.
6. **Q6. Type a macro's name through the `/` popup?** *Recommended:* yes,
   under its own heading in the popup the owner called awesome, expanding
   only when chosen. The alternative is a separate key and a second popup.
   *Answered otherwise:* `/` alone stays skills only; `/p ` and a prompt's
   name reach the prompts in the same popup.
7. **Q7. Five chips, then More?** *Recommended:* five, today's maximum.
8. **Q8. One place per macro?** *Recommended:* yes, with Duplicate. Several
   places per macro make the variable list depend on a combination.
9. **Q9. Call them "Suggested prompts" in the product?** *Recommended:* yes.
   It is the name the tooltips already use; "macro" stays a design word.
10. **Q10. A section with a sheet, or a Settings page of its own?**
    *Recommended:* the section and sheet drawn here, matching the Agents
    roster.

## Milestones

One card each, in order; each ends shippable. No issue ID is allocated here;
the cards are DRAFT-0001 to DRAFT-0007. Its criteria are under the same
heading in the next section.

1. **The macro and its rules, in core.** Value types and bounds, validation,
   facts, conditions, expansion, order and the limit, the merge of the
   shipped set with stored differences, and the shipped set proved equal to
   today's lists. Nothing visible changes.
2. **Chips come from the core.** The Agent tab and the tour chats present
   the core's offers from the shipped set. The hard-coded lists go. Adds the
   expansion's tooltip, the turn that shows what was sent, More, and the
   value rules, and the drafted macro's chip. Loads frontend-design first.
3. **Storage and the roster.** The store, its revisions and failure rules;
   the Settings roster with On, order and Restore built-ins.
4. **The sheet.** Add, edit, duplicate, delete, reset; Insert and the
   variable tint; the preview and Shows now; the condition pickers; every
   message in the table. Ends with the owner's look on a frozen build.
5. **In reach.** A chip's menu, and names in the popup after `/p `.
6. **Share.** Version 2 of RFC-BPR-0107's exchange: prompt packs that
   merge by stable UUID (owner amendment, 2026-11-18).
7. **Closeout.** Documentation and the gate.

Milestones 1 to 3 suffice to turn prompts off and reorder them; 4 delivers
the owner's idea in full; 5 and 6 can be dropped or deferred without
weakening it.

### Owner amendment (2026-11-13): comments absorb notes

The owner accepted [RFC-BPR-0127](RFC-BPR-0127-comments-absorb-notes.md) on 2026-11-13: one comment is Local, Draft or Published, private notes become Local comments (marked with a computer icon, "Only on this computer"), and the Notes tab goes. For this RFC: the built-in prompt "Explain the code this note is on" reads "this comment"; prompts about notes are about Local comments. Every other obligation here stands; no acceptance criterion is rewritten, and the change is built and verified under RFC-BPR-0127.

### Owner amendment (2026-11-13): the Ask model

The owner chose the Ask model of [RFC-BPR-0128](RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md) on 2026-11-13: one Ask control at the pull request, a file and a thread listing the quick asks (macros with Quick ask ticked) filtered by context, Was this addressed? on any thread, a merge of main as one line, and a long answer read as a report in a file tab. For this RFC: a macro gains a Quick ask checkbox and Thread, File and Pull request scopes (a quick ask can show in up to three places); the Ask menus are a new place a macro shows, beside the chips; What changed, Was this addressed? and Explain this file gain built-in rows whose sent prompts stay in Built-in prompts. Every other obligation here stands; no acceptance criterion is rewritten, and the change is built and verified under RFC-BPR-0128.

## Acceptance criteria

### Milestone 1: the macro and its rules

- **AC01:** With the shipped set and nothing stored, for every row of the
  inventory above the core offers the same names in the same order under
  the same facts, and each expansion, subject and entry point equals what is
  sent today.
- **AC02:** The scan reads `{name}` and `{{`, leaves every other brace
  alone, and refuses an unknown name and a name of another place with its
  position. A value that contains `{file}` is inserted as written. A value
  with a line break arrives as one line. An expansion over 16 KiB makes the
  macro not offered, with that reason.
- **AC03:** For each fact, "is" and "is not" each hold exactly for their
  value and neither holds when the fact is unknown. A subject or variable's
  need withholds the macro without a condition row. A fifth condition, a
  ninth pattern and an unterminated bracket are refused.
- **AC04:** With seven macros matching, the first five in order are offered
  as chips and two as More; equal inputs give equal offers and reasons. The
  tests run without `QApplication` in the `core-only` preset, and both
  architecture-boundary tests pass.

### Milestone 2: chips come from the core

- **AC05:** On the fixture pull request, the Agent tab, a conversation and
  each kind of tour chat show exactly today's chips, and a click on each
  built-in sends a request equal, byte for byte, to today's. Neither widget
  nor `tour_chat_prompts` builds a prompt's text any more.
- **AC06:** For a macro whose name differs from its template, a click sends
  the expansion and not the name; Shift-click puts the expansion in the
  field with the caret at its end, sending nothing; typed words survive as
  before; a macro with `{cursor}` is put in the field by a plain click, the
  caret at the mark.
- **AC07:** A chip's tooltip and accessible description hold the expansion
  and how to send or edit it. The turn shows the sent text under the macro's
  name, folded past six lines, and shows the same after a restart.
- **AC08:** With more than five matching, five chips and More prompts (n)…
  show, and a menu item sends or drafts as a chip does. A refresh that
  changes no name leaves keyboard focus on its chip.
- **AC09:** On a direct send, a title containing `/some-skill` brings in no
  skill, and an expansion beginning `/model` changes no model and is sent as
  text.
- **AC10:** A macro drafted with Shift, edited and sent keeps its subject
  and kind and shows its chip in the field; with the chip removed the same
  text is sent as a typed question.

### Milestone 3: storage and the roster

- **AC11:** Settings › Agents & context lists every macro under its place
  with its name, its sentence and its state in words. Turning one off
  removes its chip from an open window without a restart; a new order, made
  by drag and by keyboard, reaches the chips; both survive a restart.
- **AC12:** A profile whose store lacks the tables shows the shipped set. An
  unreadable store leaves the shipped chips working and Settings says edits
  are unavailable. A write from an older revision is refused and changes
  nothing. A row from a newer version is kept, named as such and never
  offered.
- **AC13:** The suggestions toggle still hides every chip. Remove every
  agent and set up again changes no macro. Restore built-ins… returns the
  built-ins to shipped text, order and On after confirmation and keeps the
  reviewer's own.

### Milestone 4: the sheet

- **AC14:** Add prompt… makes a macro in a chosen place. Name, template,
  About, Ask as and conditions can be edited and saved. Duplicate copies
  one. Delete removes one of the reviewer's own and Undo returns it. A
  built-in cannot be deleted or moved and offers Reset to built-in once
  changed. Cancel with changes asks first.
- **AC15:** Insert and a typed `{` offer exactly the place's variables. Each
  case of the message table produces its sentence, and Save is unavailable
  exactly where the table says. The byte count is shown.
- **AC16:** The preview shows the chip's text and the expansion against the
  pull request Settings was opened from, and against sample values when
  there is none, changing as the reviewer types. Shows now says yes, or
  names the first unmet condition.
- **AC17:** A macro with a path condition of `*.cpp` is a chip while a
  `.cpp` file is open and is absent for a `.md` file, through the shipped
  composition; its roster sentence states the condition.
- **AC18:** The roster and sheet are completed by keyboard alone; a screen
  reader hears each row's name, place, state and On, each condition picker's
  label and one announcement a change; the section fits a 760 by 480
  Settings window without sideways scrolling and uses no QSS.
- **AC19:** On a frozen build the owner accepts the roster.
- **AC20:** On the same build the owner accepts the sheet.
- **AC21:** On the same build the owner accepts the chips, the tooltip and
  the sent turn.

### Milestone 5: in reach

- **AC22:** A chip's menu opens by mouse and by the Menu key. Edit this
  prompt… opens Settings on that macro's sheet. Turn off removes the chip
  and Undo returns it.
- **AC23:** Typing `/` and letters lists skills only, as before. Typing
  `/p ` and letters lists this place's matching macros. Choosing one
  replaces the typed `/p …` with the expansion and sends nothing. `/model`,
  `/effort`, a typed `/skill-name` and an unchosen `/p …` behave as
  before.

### Milestone 6: share

- **AC24:** Export offers each of the reviewer's own prompts with its exact
  text under a new or existing pack, writes version 2 with the pack's ID,
  label and version and each prompt's UUID, and writes version 1 when none
  is chosen. Import matches by ID, never by name: an unmatched prompt is
  Added; a matched one not edited since the last import or export of that
  pack is updated in place with its On and placements kept; an edited one
  is a conflict, Replace or Keep mine; one the pack dropped is offered as
  Remove or Keep; an older pack version is flagged; prompts not from the
  pack are never changed. Duplicate mints a new UUID and drops the pack
  link. Import applies all chosen or none, refuses after a concurrent edit
  and launches nothing. A version 1 file imports as before. An item naming
  an unknown place, About, fact or variable, or with an ID that is not a
  UUID, is listed as unsupported and not applied.
- **AC26:** A pack exported from one profile and imported into a clean
  profile adds every prompt; re-importing a later version of that pack
  updates them in place and leaves the profile's other prompts unchanged.

### Milestone 7: closeout

- **AC25:** `docs/ux/ai-experience.md` and the feature allocation in
  `docs/architecture/system-architecture.md` describe suggested prompts as macros, the amendments above are
  applied to their RFCs, and the documentation gate passes.
