# RFC-BPR-0128 verification

Plan `ask-v1` ([milestones](../rfcs/plans/ask-v1/MILESTONES.md)). Lane
`claude/v0-5-0-prerelease-targets-52265d`; RFC base
`f85b216ecc7d4a7df2c7c5c263a0cf8be1f3d93a`; work began at `bfab0d305`.
Frontend-design was loaded before the Qt work; the accepted design is
[docs/design/ask-v1](../design/ask-v1/README.md), whose Ask menu, thread Ask
and macro editor frames the controls follow in the window's own roles (no
QSS). Debug timings are nongating; nothing below is a timing claim.

## Owner decisions in force

Option 1C (the Ask menu); quick asks are macros with a Quick ask checkbox,
filtered by context; Ask at the pull request, a file and a thread; Was this
addressed? on any thread; a merge of main as one line; answers live in the
Agent tab; the long report is read in a **file tab** (Q8: "I think file tab is
more flexible go with that."). RFC-BPR-0116's partial work and rejected looks
fold into this RFC. Q1 to Q7 and Q9 to Q14 are built as the RFC's defaults.

**Q5 (assumption).** The owner named no threshold. Built: a press that would
start more than five runs (one a thread, or a file) asks first
(`core::quick_ask_confirm_above`), and five or fewer start at once, one after
another. Put to the owner in the look.

## Milestone `ask-menu` ([[BPR-2163]], [[BPR-2164]])

What was built:

- **Core** (`bpr/core/prompt_macro.hpp`, `bpr/core/quick_ask.hpp`). A macro
  gains `QuickAsk` and `prompt_scopes` (Thread, File, Pull request, each on or
  off); `subject_scope` and `prompt_scope_allowed` say which scope is a
  subject's own and that a narrower one cannot be on; validation adds
  `quick-ask-not-for-subject`, `quick-ask-without-scope` and
  `scope-narrower-than-subject` (all blocking). Three built-in action rows
  (Q13): `agent-tab.what-changed`, `agent-tab.explain-file` and
  `conversation.was-addressed` (Thread, File and Pull request); what they send
  stays in Built-in prompts, an action row is never a chip (the chips are as
  before) and an own macro cannot claim an action. `decide_quick_asks` decides
  an anchor's menu: sections (the review bar's pull request, then file, then
  thread, Q2), action rows first, each row shown, greyed with a typed
  `quick_ask_reason`, or not listed with its `prompt_unmet` (Q1); a wider scope
  asks once for each thread or file and names the count. No request is made.
- **Storage** (RFC-BPR-0125 milestone 3's store, [[BPR-2146]]). The profile's
  `agent-setup.sqlite3` holds only differences (`prompt_macros`,
  `prompt_macro_orders`, `prompt_macro_revision`), Quick ask and scopes with
  them; a write names its revision and an older one is refused; a row a newer
  build wrote is kept and never rewritten; `prompt_macro_host` reads at start
  and when the window comes to the front.
- **Settings** (`prompt_macros_panel`). Suggested prompts between the agents and
  Built-in prompts: a list a place, each row its name, state in words
  (Built-in, Changed, quick ask, off), sentence and On; Return or a double
  click opens the sheet; Alt with Up or Down, or a drag, orders a place; Add
  prompt…; Restore built-ins…. The sheet: Shown as, Where, About, Ask as,
  **Quick ask for Thread, File, Pull request** (a scope narrower than About is
  unavailable, with its tooltip saying so), Sends with its byte count, the
  preview and Shows now, conditions, the core's messages, Reset to built-in or
  Delete…, Duplicate, Cancel, Save prompt (Ctrl or Cmd with Return). An action
  row's Sends says it is in Built-in prompts.
- **Window.** The review bar's **Ask** beside the comparison; the file header's
  **Ask** in Explain this file's place (Q3); the open thread's **Ask** beside
  Reply (and beside the label picker while a reply is written); **Ask** in a
  thread's and a file tab's right-click menus, replacing Ask agent about this
  conversation, Was This Addressed? and Explain This File. The thread header's
  Ask agent and Was this addressed? controls are gone. Each menu is decided
  when it opens; its rows carry the agent glyph in the accent, what a press
  reads at their right, and a greyed row's reason as tooltip, status tip and
  What's This; greyed rows are reachable by the arrow keys and show their
  reason. Custom question… opens the Agent tab on the narrowest subject (the
  thread attached as Ask agent did) and sends nothing; Edit Quick Asks… opens
  Settings at the suggested prompts. Presses go through the existing launch
  paths: What changed and Was this addressed? are RFC-BPR-0116's, Explain
  this file RFC-BPR-0117's, an ordinary quick ask its suggested prompt's own
  send from the Agent tab.

Evidence (dev, Debug, macOS):

| Check | Criteria |
|---|---|
| `core_quick_ask` | AC03, AC04 (legality, built-in rows, every menu, greyed and hidden, reviewer choices, Q5) |
| `core_prompt_macro` | RFC-BPR-0125's set with the three action rows; the chips unchanged |
| `sqlite_agent_setup_storage` | AC04 storage: Quick ask and scopes saved, stale write refused, newer row kept |
| `application_ask_menu_public_path` | AC01, AC02, AC03, AC04, AC08 through the shipped window on returning-review-v1 |
| `qt_conversation_cards_view` | AC01: the thread's Ask at its reply row and in its menu, the header's agent controls gone |
| `application_explain_file_public_path` | Q3: Explain this file from the file's Ask and the tab's Ask |
| `application_returning_review_{latest,addressed,gone}_public_path` | What changed and Was this addressed? unchanged in what they send |

## Milestone `addressed-anywhere` ([[BPR-2165]])

What was built: Was this addressed? is offered on any review thread on a
file, whoever started it, once the head is past the commit the thread started
on (Q6, the first comment's commit). At a file or the pull request it is one
run a thread, run one after another (Q5 above). Each answer's claim becomes
one chip, **Addressed**, **Partly**, **Not addressed** or **Unclear**, in the
open thread's header (ordinary roles, the glyph in the accent, at the header's
tool height, Q14) and in a closed card's meta; a press on the chip shows the
thread's lines as it started and as they are over the code, with Open the
answer. Nothing is resolved, posted or published. The returning-review-v1
fixture now names each thread's start commit (all but README.md's on the first
head), so threads by others are asked about.

Evidence: `application_ask_addressed_public_path` (AC05 on the composed
application with the scoped writing stub), `application_returning_review_addressed_public_path`.
Live installed-agent run (2026-11-14, product candidate `7b3fe6eaa`): the same
course with the installed Claude Code 2.1.319 (`/opt/homebrew/bin/claude`) in
place of the stub, run by hand outside the sandbox (`ask_menu_public_path
addressed /opt/homebrew/bin/claude`): two runs for the file's two threads, two
claims, tbutler's thread's chip "Partly", nothing resolved or posted
(`ask-menu-addressed:pass`).

## Integration pass

At `7b3fe6eaa` (dev, Debug, macOS), 51 of 51: the rows above with
`architecture_boundaries`, `application_composition_source_contract`,
`application_review_round_trip_composition_contract` (app inventory and source
pin resealed), `test_network_isolation_manifest` (entries added for this lane's
unlisted courses), `qt_settings_window`, `qt_workbench_window_scale_{100,125,200}`,
`qt_prompt_macro_chips`, `qt_tour_side_chat`, `qt_side_chat_panel`,
`qt_explain_file_card_*`, `qt_github_conversation_panel`,
`qt_agent_configuration_dialogs`, `qt_workbench_agent_send_refusal`,
`application_agent_chat_*`, `application_agent_invocation_composition_contract`,
`application_agent_keep_action_public_path`, `application_discuss_busy_public_path`,
`application_suggest_change_public_path`, `application_local_comments_*`,
`sqlite_workbench_settings_storage`, `core_returning_review`. clang-format 22 on
every changed file. Not run: `coding-tidy`, the full suite.

## Owner look ask-v001 (session `6102c30f502abd70884a1a0f9d331a8e`)

Handed back 2026-11-14 ("handed back until you fix that": the look ran the
installed Claude Code; see [[BPR-2177]]). One verdict, verbatim:

- `BPR-2163-quick-ask-editor`, **Reject**: "First we need shift click to from
  ask dropdown to skill draft the message instead of send it. I don't think
  righy click should send the message either that feels unintuitive for
  someone that's not sure if there is a context menu.. in fact there should be
  a context menu.

  img 1: I'm not sure this is beneficial.  I think just opening the agent
  drawer in the session and scrolling to the question and response is probably
  good enough." (Its attachment shows Explain this file's answer bar, "Claude
  answered in 0:00." with a struck Draft, Continue in Agent tab and Dismiss.)

The other five were not decided. Refinement (owner, through the coordinator):
"I would modify this to a plain click queues it. We might not have set up the
agent yet or we might be running other things already."

Built in response: a plain click queues the quick ask on the session
(RFC-BPR-0126's queue, [[BPR-2151]]); Shift+click drafts it in the Agent tab;
a right click opens the row's own menu (Queue, Draft in Agent Tab, Edit Quick
Ask…, Hide from Quick Asks) and never sends (`qt_ask_menu`); Explain this
file's answer bar is gone, and every Ask opens the Agent tab at its turn; the
agent's state greys no row. Host 44478 stopped; root `ask-v001.29vwy7ai`
removed (no app ran from it).

## Milestone `reports-in-a-file-tab` ([[BPR-2166]], [[BPR-2167]]), first round

Built, cards not claimable while [[BPR-2164]] waits on the look:

- Core: the document tabs' report kind (opens after the active tab, never
  loads, never persisted (Q10), a code reference opening its file's tab beside
  it), a report by its shape (Q7: two headings or more than 40 lines) with
  outline and card, and a commit listing that folds a merge of the base
  branch to one line (with whether it changed this pull request's files, from
  the commit filter's trees) and groups the rest by area. `core_document_tabs`,
  `core_agent_report`.
- Qt and app: a long answer is a report card in the Agent tab; Open report
  opens it as a tab in the files' strip (agent glyph, Close Report alone in its
  menu) with its outline beside it; while it shows, the review bar's file
  controls are drawn in place and off (Q9); a reference opens the file's tab
  and the report's tab returns to the report. What changed's request carries
  the commit listing (`returning.commits`). `application_ask_report_public_path`.
- Not yet: Q4 (Explain this file answering as a report by default rather than
  by shape), the merge's commit count (GitHub's commit list does not say how
  many commits a merge brought; the count needs the merge's ancestry from the
  review's objects), the Tour tab and tour chat checks of AC06, and AC07's other
  places commits are listed (the Commits filter).
