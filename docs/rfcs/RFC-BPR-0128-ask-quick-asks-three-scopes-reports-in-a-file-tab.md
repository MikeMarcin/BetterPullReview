---
id: "RFC-BPR-0128"
title: "Ask: quick asks at three scopes, and long answers read in a file tab"
status: "Auto-Verified"
metadata: {"origin":"owner design session 2026-11-12 and 2026-11-13 (options 1A to 1C, 2a to 2c, then the Ask mockups); imported 2026-11-13","inspected_lane":"claude/v0-5-0-prerelease-targets-52265d","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"baseline":"f85b216ecc7d4a7df2c7c5c263a0cf8be1f3d93a","accepted_design":"docs/design/ask-v1/index.html","mandate":"owner, 2026-11-13: option 1C (the Ask menu), quick asks as macros with a Quick ask checkbox filtered by context, Ask at the pull request, a file and a thread, Was this addressed? on any thread, a merge of main as one line, and the report read in a file tab; imported with cards and a plan, no card started"}
depends_on: ["RFC-BPR-0101", "RFC-BPR-0102", "RFC-BPR-0116", "RFC-BPR-0117", "RFC-BPR-0125"]
---

# Ask: quick asks at three scopes, and long answers read in a file tab

## Problem and grounded current behavior

The agent-assistance actions arrived one control at a time. RFC-BPR-0116
put "What changed since my review" and "Was this addressed?" on a returning
pull request, and limited the second to threads I commented in. RFC-BPR-0117
put Explain this file in the file's header and answers it in a card under
that header. RFC-BPR-0118 put Suggest the change on a draft field's
right-click menu. RFC-BPR-0119 offers Why did a check fail? on a failed
check. RFC-BPR-0125 made the suggested prompts editable macros, each with one
place, shown as chips in the Agent tab. Each of these is reached its own way,
and a long answer (what changed after eighteen commits, a check across twelve
threads) has nowhere to be read except a chat that is narrower than the code.

The owner, after options 1A to 1C and 2a to 2c of the returning-review round
(2026-11-12), verbatim:

> So I think its 1C - but answers in the agent tab by answers to some
> questions can show up in a frame like where we could do some like light box
> popup style. I like the presentation of 1A sort of but it doesn't scale with
> large changes. Also we need to make sure that things like merges from master
> don't show up all an individual changes but as a simple merged line. And
> also I don't think all suggested prompts go in the ask box, but rather it's
> something like a "quick ask" sort of like jira quick filters. In the macro
> editor we have a check box for quick ask or not. And then they're still
> filtered by context so invalid ones get greyed out or not shown in the Ask
> menu.
>
> Although I think 2a and 2b are cool looks presentations they miss the mark.
> I don't think we should restrict checking if a comment was addressed to
> comments I leave. And I don't think we necessarily want to check all
> comments on a PR or even a file always, but that is also useful, but
> checking just a single thread is also useful. I don't know how to fit all
> that in and not take a ton of space. Maybe this is also just part of ask,
> Maybe there's an ask on a thread and a file in addition to the PR. It could
> be in rlght-click or maybe its at the bottom where you would reply. I dunno.

On where a long answer is read, after a reader pane and a window-row tab were
drawn: "I think that's never going to work, but I could see it as either a
file tab or a lightbox style take pop-over or even a separate child window
like settings is." Shown all three, on 2026-11-13: "I think file tab is more
flexible go with that."

The design the owner chose from is
[docs/design/ask-v1](../design/ask-v1/README.md) (live HTML with light and
dark renders; content invented).

## Proposed behavior and boundaries

### One Ask control at three anchors

- The **review bar's Ask** (option 1C's pull-down, beside the comparison)
  asks about the pull request. The **file header's Ask** asks about the open
  file; it stands where Explain this file stands today. The **open thread's
  reply row** has Ask beside the label picker (Plain, Ask, Publish) and asks
  about that thread. It is the same control and the same kind of menu each
  time, and no new row is added anywhere.
- A thread's right-click menu and a file tab's right-click menu carry **Ask**
  as one item with that scope's quick asks under it. It replaces the agent
  items there today (Ask agent about this conversation, Was This Addressed?,
  Explain This File), so neither menu grows. A closed thread is asked about
  from its menu.

### The menu holds quick asks

- A **quick ask** is a suggested prompt (an RFC-BPR-0125 macro) with **Quick
  ask** ticked. The menu lists quick asks only; every other macro stays a chip
  in the Agent tab, as RFC-BPR-0125 has it.
- A quick ask is filtered by context: one whose subject is not there (no
  thread selected, a path rule that leaves the file out) is not shown; one
  whose subject is there but which cannot run yet (no commits since, no failed
  check, no threads on the file) is greyed, its tooltip giving the reason
  (open question Q1).
- A row's right end says what a press will read ("18 commits", "12 threads").
  **Custom question…** opens the Agent tab on that subject and sends nothing;
  **Edit Quick Asks…** opens Settings at the macros.
- The review bar's menu lists the pull request's quick asks, then the open
  file's, then the selected thread's, so one menu reaches every scope by
  keyboard (Q2).
- The button is ordinary; its glyph and rows carry the agent accent, because
  the rows start runs and the button does not (RFC-BPR-0101's accent rule).

### The macro editor

A macro's sheet gains one row: a **Quick ask** checkbox, and **Thread**,
**File** and **Pull request** as toggles that can each be on. A scope wider
than the prompt's subject means once for each (on a file, each of its
threads); a scope narrower than the prompt's subject cannot be on. What
changed, Was this addressed? and Explain this file each gain a built-in row
in Suggested prompts so their Quick ask and scopes are set in one place; what
they send stays in Built-in prompts (Q12, Q13).

### Was this addressed?, on any thread

- One quick ask at three scopes: on a thread about that thread, on a file
  about each of its threads, on the pull request about all of them. It is
  offered on any review thread, whoever started it; nothing is restricted to
  threads I commented in.
- A claim lands as **one chip in its thread's header** (one of four words:
  Addressed, Partly, Not addressed, Unclear), taking no height and moving
  nothing. A press on the chip shows the evidence over the code: the lines as
  they were when the thread started and as they are. Nothing is resolved or
  posted.

### Answers, and the report as a file tab

- A short answer is a reply where it was asked. A long one is a **report
  card** there (title, range, counts, first lines), and the card opens the
  report as a **file tab**: a tab in the strip the files' tabs are in, marked
  with the agent glyph, with an outline.
- The Agent tab, the Tour tab and the tour's chat keep their places and stay
  usable while a report is open; the drawer's composer asks about the report.
  A code reference in the report is one press to that file's tab at the line,
  and one press back (the report's tab beside it).
- While a report is the active tab, the review bar's file controls (File n
  of m, Changes, Seen, Read, Accept and the file's Ask) are drawn in place
  and off (Q9).
- The core's tab model gains a report kind beside a file and its comparison.

### What changed, and a merge of main

What changed is grouped by area. Wherever commits are listed, a merge of the
base branch is **one line** ("Merged main (24 commits)"), with whether the
merge changed any of this pull request's files (Q11).

### Boundaries

- Quick asks, scopes and reports change where agent questions are reached
  and read, not what an agent may read or do: RFC-BPR-0101's fresh and resumed
  rule, RFC-BPR-0102's subjects and RFC-BPR-0125's macro rules hold.
- No model call is made to build or filter a menu. Was this addressed? at a
  file or the pull request is one ask carrying every thread, its answer one
  claim a thread (Q5, as amended 2026-11-14).
- Suggest the change (RFC-BPR-0118) keeps its entry on the draft field's
  right-click menu; it is not a quick ask in this design.
- RFC-BPR-0127 (comments absorb notes) governs what an answer keeps (Draft
  comment or Keep local); this RFC governs where it is asked and read.

### Architectural allocation

- **Core:** a macro's Quick ask flag and scopes with their legality; the
  availability of each quick ask at a scope (shown, greyed with a typed
  reason, hidden); Was this addressed? over a set of threads with a baseline
  per thread; the report's shape and outline; the tab model's report kind;
  commit listing with a merge of the base branch folded.
- **Host:** one run a thread for a wide-scope ask; results marshalled per
  thread; the report kept with its conversation turn.
- **Qt:** the Ask control at three anchors and in two context menus, the
  macro editor's row, the claim chip and its evidence, the report card and
  the report tab with its outline and code references.

## Amendments to adopted RFCs (owner, 2026-11-13)

Recorded on each RFC as an owner amendment; each keeps every obligation not
named, and no acceptance criterion of a closed RFC is rewritten.

| RFC | What changes |
|---|---|
| RFC-BPR-0116 | Was this addressed? is offered on any review thread, not only one I commented in, and also on a file and on the pull request; its control leaves the thread's header for the thread's Ask; its claim is a chip on the thread. What changed answers as a report card and report. A merge of the base branch is one line. |
| RFC-BPR-0117 | Explain this file is a quick ask in the file's Ask and the file tab's menu; the file header's Ask takes its control's place (Q3). Its answer is a report card and report (Q4). |
| RFC-BPR-0118 | Suggest the change keeps its draft-field right-click entry; the Ask model gives it no Ask-menu row and no new visible control. |
| RFC-BPR-0119 | Why did a check fail? is a quick ask on the pull request, greyed while no check has failed; its row on the check stays. |
| RFC-BPR-0125 | A macro gains Quick ask and Thread, File and Pull request scopes (a quick ask can have up to three places, Q12); the Ask menus are a new place a macro shows, beside the chips; the three agent actions gain built-in rows (Q13). |

## Owner answers and open questions (2026-11-13)

Answered by the owner: option 1C; quick asks as macros with a Quick ask
checkbox, filtered by context; Ask at the pull request, a file and a thread;
Was this addressed? on any thread; a merge of main as one line; the report
read in a **file tab** (Q8 below). The rest are the design's recommendations,
put to the owner as questions; each is built as its default unless the owner
answers otherwise.

| # | Question | Default |
|---|---|---|
| Q1 | Grey or hide: hidden when the subject is not there, greyed with a reason when it is there and cannot run yet? | Yes |
| Q2 | The review bar's Ask lists the pull request's, then the file's, then the thread's quick asks (not the pull request's only)? | Yes |
| Q3 | The file header's Ask replaces the Explain this file control? | Yes |
| Q4 | Explain this file answers as a report card and report, not a card under the header? | Yes |
| Q5 | Was this addressed? on a file or pull request is one run a thread; should a large count ask first, and from how many? | One run a thread, as drawn; the design names no threshold, so none is built until the owner gives one |
| Q6 | Baseline for a thread I did not write: the commit the thread started on (not its last comment)? | The commit it started on |
| Q7 | An answer is a report by its shape (sections, or longer than the tab shows), not a setting? | By shape |
| Q8 | Which host for a report? | **Answered: a file tab** ("I think file tab is more flexible go with that.") |
| Q9 | While a report tab is active, the review bar's file controls are drawn in place and off? | Yes |
| Q10 | Is a report kept: does an open report's tab come back when the window is reopened? | Only through its card while the turn is in the chat |
| Q11 | The merge line says whether the merge changed this pull request's files, and a rebase onto main is one line the same way? | Yes, both |
| Q12 | One prompt at several scopes (the toggles), rather than one scope a prompt? | Toggles |
| Q13 | What changed, Was this addressed? and Explain this file gain built-in rows in Suggested prompts? | Yes |
| Q14 | The claim chip in ordinary roles with the glyph in the accent (not the accent's wash)? | Ordinary roles |

## Implementation notes (2026-11-14)

Recorded in the [verification record](../verification/RFC-BPR-0128.md).

- **Q5, amended by the owner (look ask-v005, 2026-11-14).** The assumed
  threshold (more than five runs ask first) was rejected: "Why would we do it
  this one instead of having one large prompt with all 6 threads? We could
  still have it build structured output if we want to display each thread
  result separately." Was this addressed? at a file or the pull request is now
  one ask (`returning_operation::addressed_threads`) carrying every thread it
  reads (each with its path, line, starting commit, conversation and lines then
  and now, up to 32); the answer lists one claim a thread in a `bpr-claims`
  block, and each claim reaches its thread's chip as before. A thread's own Ask
  is unchanged. With one ask a press, the threshold and its confirmation are
  gone.
- **Built as the defaults:** Q1, Q2, Q3, Q6, Q12, Q13, Q14. Q4, Q7, Q9, Q10 and
  Q11 belong to milestone `reports-in-a-file-tab`.
- **RFC-BPR-0125's store and sheet** did not exist; AC04 needs them. Its
  store and roster ([[BPR-2146]]) were built, and the sheet with the Quick ask
  row, so a Quick ask is saved with its macro.
- **The thread's Ask** stands beside Reply in the open thread's foot, and
  beside the label picker while a reply is written; the header's Ask agent and
  Was this addressed? controls left for it. Custom question… attaches the
  thread as Ask agent did. Edit Quick Asks… is in every Ask menu.
- **Why did a check fail?** (RFC-BPR-0119) is not built, so it has no row.
- **What a row reads.** What changed names its commits only when the pull
  request's commit list holds the reviewed commit; a rebased branch's row
  names none.

## Acceptance criteria

- **AC01:** The review bar, the open file's header and the open thread's reply row each have one Ask control, and a thread's and a file tab's right-click menus each have one Ask item, whose menus list the quick asks of their scope; the agent items they replace are gone and no new row is added to the window.
- **AC02:** An Ask menu lists only macros with Quick ask ticked and a scope that applies there; each row names what a press will read; Custom question… opens the Agent tab on that subject and sends nothing, and Edit Quick Asks… opens Settings at the macros.
- **AC03:** A quick ask whose subject is absent is not listed, and one whose subject is present but cannot run is listed disabled with its reason as its tooltip and accessible description; building or filtering a menu makes no agent call.
- **AC04:** The macro editor's Quick ask checkbox and Thread, File and Pull request toggles are saved with the macro, a scope narrower than its subject cannot be set, and What changed, Was this addressed? and Explain this file each have a built-in row whose sent prompt is unchanged.
- **AC05:** Was this addressed? runs on a thread I did not comment in, on each thread of a file and on every thread of the pull request; each claim appears as a chip in its thread's header without changing the drawer's layout height, its press shows the thread's lines at its baseline and now, and nothing is resolved, posted or published.
- **AC06:** A long answer appears as a report card where it was asked; the card opens the report as a tab in the files' tab strip with an outline; while it is open the Agent tab, Tour tab and tour chat keep their places and accept input; a code reference opens that file's tab at the line and the report's tab returns to the report.
- **AC07:** Wherever commits are listed, a merge of the base branch is one line with its commit count and whether it changed this pull request's files, and the other commits are grouped by area.
- **AC08:** Every Ask control, menu row, claim chip and report tab is reachable and operable by keyboard alone and is named to assistive technology, including a greyed row's reason.

## Closeout (2026-11-14)

Closed by [BPR-2168](../issues/60-done/BPR-2168-close-rfc-0128.md): every
criterion evidenced on the [verification record](../verification/RFC-BPR-0128.md),
the owner's looks accepted through ask-v008 and report-templates-v004.

