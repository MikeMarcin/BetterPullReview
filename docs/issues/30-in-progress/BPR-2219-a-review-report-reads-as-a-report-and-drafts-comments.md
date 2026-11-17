---
schema: "implement-rfc.card.v1"
id: "BPR-2219"
title: "A review ask's report reads as a report and drafts comments from its findings"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2219--BPR-2219--f5f7c1180ea8.json","origin":"owner, 2026-11-16, about BPR-2166"}
---

# A review ask's report reads as a report and drafts comments from its findings

## Origin

The owner, 2026-11-16: "Doing a 'Review this pull request' ask creates a
report but that report is not acceptable. First it reads much more like an
agent transcript than an actual report. Second we lost the most useful
feature of the agent integration: it no longer gives the ability to draft a
comment from the agent response. That UX needs to return into the report.
You can reproduce this and maybe have a deep worker try to do the
/frontend-design:frontend-design for how it integrates into the report."

The owner's review had 12 files, +94/-236. The report is RFC-BPR-0128's
([[BPR-2166]]): a card where it was asked and a tab in the files' strip. The
earlier feature is the one keep action on an answer and on a finding card
(Draft comment, RFC-BPR-0127, `core::decide_keep`; RFC-BPR-0117 AC02 and
AC03 recorded as superseded by RFC-BPR-0128).

## Scope and acceptance

- **AC-a.** A Review this pull request (and Review {file}) answer from a real
  agent reads as a report: a verdict and summary first, then findings
  grouped by what they need, each at its place in the code. What the agent
  said while it worked (its progress, its tool use) is not in the report.
- **AC-b.** Each finding in the report has Draft comment, which writes a
  private draft comment at that finding's file and lines in the review; it
  then reads Go to draft. A finding with no place a draft can be anchored to
  offers Keep local. Nothing is published until the reviewer publishes.
- **AC-c.** The report's summary can be kept as a general comment draft.
- **AC-d.** The core reads the report's findings and decides each one's keep
  action; the host performs it; Qt presents it.
- **AC-e.** The Fixture agent answers a review with a realistic structured
  review, and a composed course asserts the report's structure and that
  Draft comment creates the anchored draft.
- **AC-f.** (Owner decision, 2026-11-16: "How is 1 sentence useful to anyone
  trying to tweak the review request to their particular circumstances?")
  The shipped Review this pull request prompt carries its real instructions
  as its visible, editable body in Settings › Prompts: what to review, the
  report's sections, focus, the severity scheme, tone. Restore built-ins
  restores it. The format the app parses (the level-one title, the
  `**Verdict:**` line, the findings block's shape, the places) stays the
  app's and is always added, so an edited prompt cannot break Draft comment;
  it shows read-only under the editor ("Always added: the report format
  BetterPullReview reads"), so nothing is hidden. The reader tolerates a
  missing optional section: with the verdict guidance removed the report
  still shows, without a verdict. The same holds for every other
  `prompt_ask::review` built-in (Review {file}).

Follow-up candidate, out of scope here: the other shipped instruction
resources that have no visible body (the tour prompts and the rest of
`src/application/resources/review-preparation/`).

Load `frontend-design:frontend-design` before the design and Qt work and say
so in the report. The owner's look on a frozen build (Fixture agent only, a
fresh short session, one Accept/Reject a criterion) is filed, not waited on.

## Cause

Reproduced on 2026-11-16 with the installed Claude Code 2.1.319 (Sonnet,
medium effort; one turn, 35 seconds, 4,554 output tokens, $0.16 as the agent
reported it) on MikeMarcin/BPR-Test#185 (14 files, +140/-254, a small feed
poller written for this card with real defects in it), offscreen, through
`live_second_account_course --course agent-review`, which presses Review this
pull request in the review bar's Ask as a reviewer does.

What that run showed:

1. **No draft action anywhere in the report.** The page offered Add as draft
   comment only under an item's `suggestion` block, and the agent wrote none:
   it gave its fixes as plain code blocks. Eight findings, no action. Which
   item offered the action, and the comment's words, were decided in
   `qt::agent_report_view`, not in core. The seven finding cards and Draft
   all did arrive, but under the report card in the Agent tab's drawer,
   below its fold; the report the reviewer reads had none of it.
2. **The summary and Not reviewed talk about the agent, not the change**: "I
   read the full diff and the surrounding `scheduler.py` ... I couldn't run
   the test suite because the shell denied the command, so the findings
   below come from reading the code." The review skill asks every answer to
   end with what was reviewed and what could not be verified, in a chat
   voice; the report format did not say otherwise. The findings' reasons ran
   to six sentences each, and there was no verdict.
3. **The place line was read too narrowly.** A finding at two places
   (`` `feedsync/client.py:55`, `feedsync/cache.py:21-22` ``) lost its place,
   and `` `feedsync/client.py:30,71` `` was read as a file name.
4. **The one-line prompt hid everything that shaped the review** (AC-f).

What it did not show, and is guarded all the same: the Claude stream decoder
appends every text block of a turn to the one answer
(`application/src/agent_launch.cpp`), so words an agent says between its tool
calls stand in front of the report, where the title is then missed and the
notes become the summary. This run's agent said nothing between its four
tool calls; a longer review commonly does. The fixture agent answered a
review with one text block and a findings block that listed findings its
report did not have, so no course could see any of the above.

## Design

`frontend-design:frontend-design` was loaded first.
[docs/design/review-report-v1](../../design/review-report-v1/README.md) draws
three candidates, light and dark: A, the keep action at the foot of each
finding; B, a rail of actions with a draft-the-rest band; C, the comment
shown as it would be drafted. **A is built.** The report's hand, which the
owner chose on 2026-11-14, is kept.

## Built

- **Core** (`agent_report.hpp`): `read_report_answer` splits an answer at its
  report's title, so what the agent said while it worked is not the report;
  `dressed_report` carries the `**Verdict:**` line and its colour;
  `report_findings` gives every finding (an item headed with a Conventional
  Comments label, or one that proposes a change) its keep action by
  `decide_keep`'s rule and the comment it starts from, preferring the
  comment the agent suggested for the same place; `report_summary_comment`
  is the summary as a comment on the pull request.
- **Host** (`window_quick_ask.cpp`): the report's keep actions go through
  `act_on_agent_card`, the finding cards' own path: Draft comment drafts at
  the finding's lines, Keep local keeps a Local comment on its file or on
  the pull request; Go to draft opens the file at the lines.
- **Qt** (`report_page`, `agent_report_view`, `agent_tab_view`): the page
  draws the verdict over the summary, Keep summary under it and one keep
  action at each finding's foot, as button-shaped links of its own text; the
  view takes the findings from core, keeps what was kept by the report's
  turn, and reads the card and the report from the report part of the answer.
- **Prompt** (AC-f): the two review built-ins keep their names and send
  their instructions as their body, visible and editable in Settings ›
  Prompts (what to review, the focus, the sections, the severity scheme, the
  verdict, the tone); Restore built-ins restores them. The form the
  application reads (`report-templates.md`, now `report-templates-v2`: the
  report is the whole last message and opens with its title, is about the
  change and not the agent, gives a verdict line when asked for one, names
  one place a finding, writes its reasons as the comment and a fix as a
  suggestion block) is always added and shows read-only under the editor as
  **Result: Findings report**. In a review every item that names a place is
  a finding, whatever it is headed with, so an edited severity scheme still
  drafts; a report with no verdict shows without one.
- **Core, from the run**: several places on the place line (the first is the
  finding's, the others show beside it), and lines written with a comma.
- **Fixture agent**: a review is three working notes beside tool calls, then
  a report with a verdict whose findings block lists the report's findings.

## Verification

On the lane at `cf4702b54` (built in the card's own worktree, level with the
lane), dev preset, macOS:

- `core_agent_report`, `core_prompt_macro`, `core_quick_ask`,
  `core_agent_question`, `core_review_composition`,
  `core_feedback_private_promotion`.
- `qt_agent_report_view`, `qt_prompts_page`, `qt_prompt_macro_chips`,
  `qt_workbench_window_scale_100`.
- `application_review_report_public_path` (new; composed, fixture agent):
  the turn's answer holds the agent's working notes before the report; the
  card's summary and the opened report hold none of them; verdict, sections
  and five findings with their keep actions; Draft comment makes a draft
  conversation at line 12 of `src/engine/renderer.cpp`, then reads Drafted
  and Go to draft, and is not made twice; Keep local and Keep summary keep;
  nothing is published; the report opened again still says what was kept;
  Settings › Prompts shows the body and the read-only section.
- `application_ask_menu_public_path`, `application_ask_report_public_path`,
  `application_ask_addressed_public_path`, `application_ask_queue_public_path`,
  `application_prompt_macros_public_path`, `application_prompt_macro_host`,
  `application_fixture_agent_every_ask`, `fixture_agent_answers`,
  `application_agent_chat_composition_contract`,
  `application_agent_chat_subject_contract`,
  `application_agent_chat_conversation_contract`,
  `sqlite_agent_setup_storage`, `architecture_boundaries`,
  `architecture_boundary_contract`.
- Not run: the whole suite; `qt_workbench_window` at 125 and 200;
  `application_agent_chat_replies` (not built).
- Real agent, after: Claude Code 2.1.319, Sonnet, medium effort, one turn,
  31 seconds, $0.17 as reported, on MikeMarcin/BPR-Test#185 with the new
  visible prompt: a verdict (Needs changes), a summary about the change,
  eight findings under Must fix and Worth a look, two with suggestion
  blocks, each with its keep action. Its Not reviewed rows still say "I did
  not" twice.

## Owner look (filed, not waited on)

Attended-review session `61844d36f1e7580e88fb7552e98de6b6`, external state
directory `~/.attended-review-state/review-report-v001-2026-11-16/`
(`state/` the session, `manifest.json`, `serve.log`, `serve.pid`), served on
the tailnet, hosting since 2026-11-16. Four Accept/Reject questions on the
frozen build `review-report-v001` (root `~/review-report-v001.d9f1mwjz`,
frozen from `cf4702b541f6`, Fixture agent only): the review reads as a
report; Draft comment from a finding; the layout (A of three); the review
prompt's visible body. The before and after real-agent reports and the
three mockups are its images. Lifecycle: pending.

### Verdicts, look review-report-v001 (handed back 2026-11-16)

- **Accept** `review-reads-as-a-report` (decision `0c6ff8f0`).
- **Accept** `review-report-layout` (decision `5c9839ff`): A, the action at
  each finding's foot.
- **Reject** `draft-comment-from-a-finding` (decision `574fca80`), verbatim:
  "It's close however... The more I look at it the more this review theme
  just doesnt really work in the app. I know I asked for this but let's see
  if we can redesign the report styling and formatting to feel more native
  but also still special compared with a normal file tab. 2nd img: All of
  this extra context from a prompt we select should be hidden.. it's just
  noise. Maybe you can see it with a click or disclosure if you really want
  but one of the big points behind the prompts is to keep the agent
  interfaces streamlined and readable."
- **Reject** `review-prompt-visible-body` (decision `b6308481`), verbatim:
  "Yes but needs syntax highlighting and the result needs to look
  non-editable becaus right now it reads the same. Actually in fact I would
  say we shouldn't show the result prompt by default it should require an
  extra click. And then those controls at the bottom look like they're
  attached to the result but they're really for modifying the prompt."

### Round 2 (from those notes)

- **AC-g.** The report's styling is the app's own (its fonts, palette,
  spacing, controls) while still special beside a plain file tab; the serif
  review theme goes. The accepted structure and layout stay.
- **AC-h.** A turn that came from a prompt (a chip or a menu, in the Agent
  tab and the tour chats) shows the prompt's title as the user's turn; the
  full text sent is behind a small disclosure.
- **AC-i.** The prompt editor: Markdown syntax highlighting in the body;
  the Result section collapsed behind a disclosure and plainly read-only;
  the Insert, variables and byte-count row directly under the body it
  edits, above the Result.

## Known gaps

- What a report's keep actions made is remembered by the report view for the
  session, by the report's turn. It is not stored, and it is not shared with
  the Agent tab's finding cards for the same findings.
- The report has no Draft all; the Agent tab's Draft all still drafts every
  finding card of the answer.
- Pressing the report's row within moments of Go to draft can lose the front
  to the file's own open (seen once in eight course runs; the course presses
  until it takes).
- The Claude stream decoder still appends every text block of a turn to the
  answer; the report is cut at its title in core. Moving an agent's working
  notes out of the answer is the Agent tab's (RFC-BPR-0129, [[BPR-2222]]).

## Intended verification

A real-agent reproduction (Claude Code, Sonnet, medium effort, a mid-sized
pull request on MikeMarcin/BPR-Test) before and after; core tests of the
report model; the composed fixture-agent course above.
