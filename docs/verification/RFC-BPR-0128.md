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

## Owner look ask-v002 (session `7268b4daaedd9f9318ced6f27e78c873`)

Frozen from `612194781`, on the Fixture agent. Handed back 2026-11-14 at
sequence 107. Verdicts, verbatim; the attachments stay in the session's state
(`/Users/jaeger/.attended-review-state/ask-v002-2026-11-14/state`) and are
described here:

- `BPR-2164-review-bar-ask`, **Accept**.
- `BPR-2164-file-ask`, **Accept**.
- `BPR-2164-thread-ask`, **Accept**: "I'm surprised there's no ask on the
  threads mode."
- `BPR-2165-claim-chips`, **Accept**.
- `BPR-2176-buttons-gone`, **Accept**.
- `ask-click-queues`, **Accept**.
- `ask-opens-agent-tab`, **Accept**.
- `ask-shift-click-drafts`, **Accept**.
- `ask-right-click-menu`, **Accept**: "Drop edit and hide from the individual
  question menu, the edit quick ask on the top level ask dropdown is
  sufficient.

  Otherwise good." (Its attachment shows the review bar's Ask menu with a
  row's menu open: Queue, Draft in Agent Tab, Edit Quick Ask…, Hide from Quick
  Asks.)
- `BPR-2166-report-file-tab`, **Reject**: "Aren't we supposed to have a mack
  agent for testing now? " (Its attachment shows the Agent tab with "Codex
  stopped before answering", "Codex didn't start. It wrote: codex was not
  found on PATH or in the usu…", and the footer "Codex · Default model".)
- `RFC-BPR-0128-Q5-threshold`, **Reject**: "cant tell missing mock agent and
  response."
- `BPR-2163-quick-ask-editor`, **Reject**: "It's not really clear that you can
  open and edit these. I feel like we can do better on the UX design. I also
  think we should split agents and prompts into separate settings sections."
  (Two attachments: Settings > Agents & context > Suggested prompts, a list of
  checkbox rows; and a prompt's sheet.)

Why the look had no fixture answer: the picker took its first offered agent,
Codex's app-server profile, before the setup had chosen one (the host's policy
had not arrived, so the found Claude was not yet permitted, and a pick made
while the setup is known is kept); nothing pointed that profile at the fixture
agent, so its launch searched the person's PATH. The setup also stopped at
Check it works. Fixed ([[BPR-2177]]): a person's session on a tests-on build
offers the fixture agent alone, Claude's protocol first; Codex's profiles are
the fixture agent too; the fixture agent's Check runs once by itself when a
review opens, so the setup is ready with no step. Explain this file and What
could this change break? answer as reports. The returning-review fixture has a
seventh thread, so the pull request's Was this addressed? reads six threads and
asks first (Q5). `freeze --check` now asks What could this change break? in the
frozen build and fails unless the Fixture agent answers it as a report
(`FIXTURE_AGENT_SILENT`). Course `application_fixture_agent_guard_public_path`
now fails on a setup that needs a step or a picker not on the fixture agent.

The row menu is Queue and Draft in Agent Tab (`qt_ask_menu`). BPR-2163 reopens
for a Prompts section of its own; mockups go to the next look before anything
is built.

The first freeze of ask-v003 (from `8d51a468e`) failed its new check
(`ATTENDED_FIXTURE_AGENT_FAILED:…step=agent`): the check waited for the setup
before asking, and the setup only looks for agents once the Agent tab is
opened. The check now asks straight away, as a person would, before the Agent
tab was ever opened; the press opens it, the setup finds the fixture agent, its
Check runs by itself, and the answer arrives as a report
(`ATTENDED_FIXTURE_AGENT_ANSWERED:Fixture agent:report=1` on the dev build).

## Owner look ask-v003 (session `09daeb8e2bb15c2c5f23a3dbbddce62c`)

Frozen from `be416d9f6`. Handed back 2026-11-14 at sequence 17. One verdict,
verbatim; `BPR-2179-prompts-mockup`, `RFC-BPR-0128-Q5-threshold` and
`ask-right-click-menu` were left unanswered (no decision):

- `BPR-2166-report-file-tab`, **Reject**: "The footer does not say fixture
  agent" (Its attachment shows the Agent tab with its quick asks and no answer
  yet, and the status bar's agent footer reading "Claude Code · Default
  model".)

Why: the picker named the fixture agent (its profile's name), but the footer's
words come from the run settings, which named the built-in agents by their
plain names ("Claude Code", "Codex"); so did the setup, its Check and
Settings. Fixed ([[BPR-2177]]): a tests-on person's session gives the setup
host the names its agents go by (`agent_setup_projection::Names`: "Fixture
agent", "Fixture agent (Codex)"), and the setup, Check, Settings and the
footer's run settings use them. `application_fixture_agent_guard_public_path`
fails on any widget naming the agent otherwise (it failed with "Claude Code ·
Default model" before the fix), and on a setup whose Ready line is not
"Fixture agent is ready". `freeze --check` fails unless the frozen build's
footer says Fixture agent (`ATTENDED_FIXTURE_AGENT_ANSWERED:…:footer=…`).
Host 4137/4149 stopped; root `ask-v003.vfh_7ety` removed.

## Owner look ask-v004 (session `88b6acef99947ab22775b220057b2189`)

Frozen from `e97dc8bca`. Handed back 2026-11-14 at sequence 20 ("still doesnt
work", through the coordinator). One verdict, verbatim; the other four were
left unanswered (no decision):

- `BPR-2166-report-file-tab`, **Reject**: "What went wrong: A line of its
  output isn't in the format its profile names, or arrived out of order.
  (malformed_frame)

  It wrote 136 bytes of output and 0 bytes of errors.

  First line this app couldn't read:
  {"type":"system","subtype":"init","session_id":"fixture-agent","tools":["Read","Grep"],"mcp_servers":[],"plugins":[],"model":"fixture"}"
  (Its attachment shows the Agent tab after What changed since my review,
  "Native continuation", "Fixture agent stopped before answering — Fixture
  agent answered in a form this app can't read.", the footer "Fixture agent ·
  Default model" and a "renderer.cpp 6" chip in the composer.)

Root cause: What changed since my review is a tool-free writing run
(`--tools ""` and the rest of `claude_writing_restrictions`); the app reads
Claude's init frame and refuses one that reports any tool or MCP server
(BPR-1375). The fixture agent always reported Read and Grep and always read a
file, so every tool-free run failed; the app was right. The freeze check had
asked only What could this change break?, an ordinary chat. Fixed
([[BPR-2177]]): the fixture agent reports the tools its launch gave
(`--tools`): none for a tool-free run, which then answers without reading, the
read-only set for a read-only run (`fixture_agent_answers`). The check now asks
everything a look offers, at the push the look opens on and before the Agent
tab is opened: What changed first with the line-6 chip, then every quick ask of
the review bar, the file and a thread (more than five runs confirmed), a typed
question with the line chip, a second in the same conversation and two queued
at once; every turn must be answered. The new course
`application_fixture_agent_every_ask` runs the application itself that way
(17 asks); with the old fixture agent it fails on What changed with the
owner's exact malformed_frame line. `freeze --check` runs the same at the next
push. Host 12600/12617 stopped; root `ask-v004.vxhvuzyw` removed.

## Owner look ask-v005 (session `b33d123bc164a8497b8f66b86087644f`)

Frozen from `25f8ac774`. Handed back 2026-11-14 at sequence 180. Verdicts,
verbatim; the attachments are described here (the coordinator saved them
outside the repository):

- `BPR-2166-report-file-tab`, **Reject**: "I think we can make both the agent
  drawer report card and the file tab report card look a little better. Start
  a deep worker to give a couple improved front-end design mockups.

  Filetab ask should be on the tab and smaller than the tab so it doesnt read
  like another tab. Should only be shown for the active tab. Shouldn't be
  shown for report card tabs." (Two attachments: the Agent tab's report card
  for What changed since your review, with the file strip's "Ask ⌄" overlapping
  the third tab; and the report open as a file tab, the strip's "Ask ⌄" over
  the next tab's name.)
- `BPR-2177-footer-says-fixture-agent`, **Accept**: "It does although I don't
  know why it says setup agent on launch if the agent is already set up."
  (Attachment: just after launch, the footer reads "Set up an agent…" beside
  "Fixture agent · Default model".)
- `BPR-2179-prompts-mockup`, **Reject**: "I like parts of all of these. I like
  the filter of C. I like the larger edit area of A. I like the drag
  reordering of B. I like being able to see at a glance if they're in the
  quick ask or suggested chips.

  A single prompt can probably be shown in multiple places. The main
  difference is the context right. We have context for a review or a file or a
  thread.

  There's like a concept missing of conditions or triggers. We need conditions
  to know when an ask should be suggested. I dunno it all feels a bit too
  loose to me I think we need a grill-rfc to align on what our requirements
  are." (The coordinator runs that grill-rfc; BPR-2179 is not built here.)
- `ask-right-click-menu`, **Accept**.
- `RFC-BPR-0128-Q5-threshold`, **Reject**: "Why would we do it this one
  instead of having one large prompt with all 6 threads? We could still have
  it build structured output if we want to display each thread result
  separately."

What was done:

- **The file's Ask on its tab** (`32654c05d`): a small glyph control drawn on
  the active tab itself, smaller than the tab, never on a report's tab
  (`document_tab_strip::set_active_tab_control`); tests first in
  `application_ask_menu_public_path` and `application_ask_report_public_path`.
  The report card's look is left to the separate design worker.
- **Set up an agent… at launch** (`5b328f731`): the footer was drawn while the
  picker was still hidden (the agents not yet known), offered Set up an
  agent…, and never redrew when the picker showed. It now follows the picker
  (`qt_agent_footer`, test first; it failed with the button still shown). The
  every-ask check fails on a setup offered at launch, and failed so without the
  fix.
- **RFC-BPR-0128 amendment, Q5:** one ask carries every thread of a file or
  the pull request (`returning_operation::addressed_threads`), its answer one
  claim a thread in a `bpr-claims` block (`core::read_addressed_claims`), each
  reaching its thread's chip; the five-run threshold and its confirmation are
  removed (`core_returning_review`, `core_quick_ask`,
  `application_ask_addressed_public_path`: the file's two threads and the pull
  request's six each in one launch). Recorded in the RFC's implementation
  notes.

Host 65778/65790 stopped; root `ask-v005.a6ilb3sh` removed.

## Owner look ask-v006 (session `98f8f0c11b1460bb51f16ca307b793d8`)

Frozen from `3b0c12a4c`. Handed back 2026-11-14 at sequence 24. Verdicts,
verbatim (attachments described; the coordinator saved them outside the
repository):

- `BPR-2164-file-ask-on-the-tab`, **Accept**: "Yes but Can we get the same
  style as the comments use." (Attachment: a crop of the comment drawer's
  thread foot, the Reply field's rounded "R" key beside the thread's square
  framed "Ask ⌄".)
- `BPR-2177-no-setup-at-launch`, **Accept**.
- `RFC-BPR-0128-Q5-one-ask`, **Reject**: "I think this might need to wait
  until report cards land because the actual output in the app is not good
  enough" (Attachment: the Agent tab's "Was this addressed? (6 threads)" turn,
  its prose and the claims' summary.)

Done:

- **The Ask's style** (`application_ask_menu_public_path`, test first): the
  file tab's Ask and the thread's Ask now wear the comment drawer's own
  secondary button (`note_button` secondary): a rounded outline (radius 6) on
  the ground, the edge a rule that darkens under the pointer or with focus,
  the words in the ink, with the agent glyph and a chevron
  (`ask_menu_button::wear_comment_look`). The choice is named in the next
  look; it is folded into that look rather than shown alone.
- **Q5, parked on the report cards.** The one-ask mechanism stays as built
  (`addressed_threads`). Its acceptance waits on the report-card redesign
  (the coordinator's design round for [[BPR-2166]]): how a many-thread answer
  reads is a report card's question. Recorded on [[BPR-2166]] and the
  closeout [[BPR-2168]].

Host 61578/61588 stopped; root `ask-v006.0gi2v365` removed.

## The report card and the opened report, as chosen (2026-11-14)

Built from [docs/design/report-card-v4/SPEC.md](../design/report-card-v4/SPEC.md)
(the owner's choices: round 1's C, Attachment for the card; round 4's C,
Floating for the opened report). Frontend-design was loaded.

- **Core** (`core_agent_report`): `report_card` is the title (the heading the
  answer opens with, else the title given, else the first line), the
  sections at their shallowest level counted without the title (defect 2),
  the summary (what stands between the title and the first section, Markdown
  kept; with no prose there the first paragraph after the title, wherever it
  stands; defect 3) and the outline the menu lists (depth from the shallowest
  heading). The old lead and line count are gone.
- **The card** (`agentTurnReportCard`): the reply card is the only surface;
  the summary shows where the answer shows, rendered, references as links;
  under it, 9 points down, the attachment row (`agentTurnReportOpen`): window
  surface colour, one point border, 7 point corners, a page icon carrying the
  agent glyph, the title semibold over "Report, N sections" (round 1's
  wording; "Report, 1 section"; "Report" with none), a chevron. The whole row
  opens the report's tab or goes to it; Space and Return press it; its name is
  "Open report: " and the title. While the report's tab is active the row is
  selected (1.5 point outline, selection tint), and not otherwise.
- **The opened report** (`agentReportView`): the side pane and the View source
  and Copy source buttons are gone. One centred column (572 points at most)
  on the code background; the title once, 22 points bold, 22 points under the
  strip (defect 5: the heading the answer opens with is not repeated in the
  body); under it the question as a link with a speech-bubble icon and who
  answered, how long ago and at which head; the body in place, as tall as its
  content, the page scrolling as one (defect 1). The floating menu button
  (32 by 28, the pane's top right, 14 by 12 points in, named "Report menu")
  opens a popover hanging from it: Summary and the sections (sub-sections
  indented 14 points), the current one (the last heading at or above the
  pane's top; Summary before the first; at the end, the last on screen)
  marked by a bar in the link colour and the selection tint; then Show in
  conversation, View source (Rendered while the source shows) and Copy source.
  Choosing a section scrolls it to the top and closes the popover; Esc closes
  it and returns focus to the button; Up, Down, Home, End, Return and Space
  work in it. Show in conversation and the question show the Agent tab at the
  turn, the report's tab staying active.
- **The tab** (defect 4): a report's title is cut at its end, the glyph kept.
- **Not built:** the Suggested change box and its Add as draft comment. The
  Agent tab's draft path takes a finding (a file, lines and a comment) from
  the answer's `bpr-findings` block; a report's suggestion is Markdown with no
  such structure, so it cannot go through that path as it stands. It needs the
  report to carry its suggestions as findings (or a structured block) first.
  Also short of the spec: section headings keep the body's size in place
  (the in-place body sets its own heading scale); the floating button's
  shadow and the popover's arrow are drawn but not checked on every platform.

Courses: `application_ask_report_public_path` (summary and row, "Report, 2
sections", the row's name and focus, selected while the tab is active, the
title once, the byline, the body in place, the button's size and place, the
popover's rows and its current section, Return choosing a section and
closing), `application_explain_file_public_path`, `application_fixture_agent_every_ask`.

## Owner look ask-v007 (session `4a98f0b4da5edd59a50568ac513513ac`)

Frozen from `f2e28a932`. Handed back 2026-11-14 at sequence 44. Verdicts,
verbatim (the coordinator saved the two attachments outside the repository):

- `BPR-2164-ask-style`, **Accept**.
- `BPR-2166-report-card`, **Accept**.
- `BPR-2166-opened-report`, **Accept**: "Yes but I think we need a different
  icon for the tab header, because that one reaads as though this report is
  going to do something with the agent. It should read as this is a report not
  a file."
- `RFC-BPR-0128-Q5-one-ask`, **Reject**: "Why is your mockup report attachment
  cutting of text. Why does asking was this addressed on the PR in the fixture
  agaent in the fronzen build not show an attachment report?"

Done (`47dc394c8`):

- **The report tab's glyph.** A report's tab wears a page with a folded corner
  and three lines of text, drawn in the tab's ink (`report_glyph` in
  `document_tab_strip.cpp`), not the agent sparkle; the sparkle stays on the
  card's page icon, where it says who wrote the report.
- **The row cut its text** (root cause): the attachment row was vertically
  Fixed and sized its hint from its own `width()`, which is stale (or the
  default) before layout; at the card's real, narrower width the title wrapped
  to two lines inside a one-line height and the meta line fell outside it. The
  row now has height-for-width (the title at most two lines, then elided), its
  layout asking for the height its width needs. `application_ask_menu_public_path`
  asserts the row has height-for-width, a narrow row is taller than a wide one
  and the row is at least as tall as its width needs, the title and meta inside.
- **The pull-request Was this addressed? showed no report** (root cause): the
  report decision excluded `addressed_threads` explicitly, and the Fixture
  agent's answer to it had no sections. Q4 made Explain and What changed
  reports whatever their length; a many-thread answer is the same kind of
  thing (a verdict per thread), so it is now a report too, titled with the
  question ("Was this addressed? (6 threads)"): a summary, one section a thread
  headed with its file, line and claim, then the `bpr-claims` block, which
  still posts each claim on its thread. The live prompt (`agent_launch.cpp`)
  asks for that shape; the Fixture agent answers in it. The one-thread ask
  stays a plain answer on its thread.
- **The every-ask check** (the freeze's `--check` and
  `application_fixture_agent_every_ask`) now fails
  `ATTENDED_FIXTURE_AGENT_FAILED:no-report` unless the turn shows a report
  card for every ask expected to be one: What changed, Explain this file, What
  could break, and Was this addressed? on the pull request and on a file.
