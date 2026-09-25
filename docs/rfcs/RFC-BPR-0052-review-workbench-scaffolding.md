---
id: "RFC-BPR-0052"
title: "Review Workbench Scaffolding: Title Row, Review Bar, Folding Updates Line and Document Tabs"
status: "Proposed"
area: "BPR"
milestone: "Review workbench scaffolding"
depends_on: ["RFC-BPR-0025", "RFC-BPR-0028", "RFC-BPR-0029", "RFC-BPR-0037"]
---

# RFC-BPR-0052: Review Workbench Scaffolding: Title Row, Review Bar, Folding Updates Line and Document Tabs

## Status and scope

Registered on 2026-09-24 from the owner's decision on [[BPR-1106]]. The design
round that decision `ece7a6f3` asked for began with where two buttons sit.
The owner widened it in the session to the workbench's outer scaffolding,
designed around the review-tour pack (RFC-BPR-0039 to 0046) and the
Conventional Comments pack (RFC-BPR-0047 to 0051). The decided design is
[round v7](../design/BPR-1106-scaffolding-v7.html), the owner's same-day
revision of [round v6](../design/BPR-1106-scaffolding-v6.html). The rounds
behind it are [v2](../design/BPR-1106-composition-entries-v2.html),
[v4](../design/BPR-1106-scaffolding-v4.html) and
[v5](../design/BPR-1106-scaffolding-v5.html). BPR-1106 records the owner's
answers verbatim.

Asked where the work should live, the owner chose an RFC of its own
("Its own RFC"). This RFC therefore stands to RFC-BPR-0028 as RFC-BPR-0028
stood to RFC-BPR-0025. It supersedes RFC-BPR-0028's chrome where they
conflict: the composition entry row, the strip's navigation, progress and
details controls, the modality's place, the Updates line's permanent caption,
and the non-goal that set tabs aside. RFC-BPR-0028 closes on what it built,
with AC-01 as the owner amended it on 2026-09-24. This RFC also schedules
RFC-BPR-0025's browser-style tab contract, which RFC-BPR-0025's closeout left
superseded or unscheduled, not implemented and not passed.

In the RFC index it follows RFC-BPR-0038 and precedes the review-tour pack,
whose tour tab and Conventional Comments labels build on the tray and bar
drawn here. It starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038
close (the owner's decision `1d9104f8`, "Not yet: finish RFC-0028 to RFC-0038
first"). Registration baseline: `0453103ddd11337ee8b3adf5053b25149104acd8`. Registration is not implementation.

## Problem and outcome

The owner, on round v2: the title bar is "too busy … it's getting distracting
and overwhelming", while "I do love maxing out the space available to the
review surface itself". The review surface also has to make room for tours and
labelled comments, which are coming, and the owner wants RFC-BPR-0025's tabs
back ("now that we have something more or less functional I want to bring
them back").

The outcome is a calm title row, one review bar that holds what acts on the
review, an Updates line that folds away, and document tabs. At 1280×852 on
macOS the comparison measures 744 pixels with the Updates line folded, and 710
open, against 707 today. On Windows the same figures are 746, 712 and 712.

## Decided design

**The title row** carries the window, the pull request and the modality:
Home, the drawer toggles and the pull request's name on the left; the
modality at the right, beside refresh, the comments toggle, account and
settings; and on Windows the application menu and the caption buttons. The
comments toggle carries the count of new comments; Next new comment keeps its
key. No navigation, progress or composition control sits in the title row, so
the name has the room they took. At the Windows 902-pixel floor it shows the
name and the whole modality words, where today it shows neither.

**The Updates line** stays one full-width row, as [[BPR-0737]] placed it, and
shows the active tab's pair. It loses its permanent caption: "Known updates ·
display order is not push order" becomes its tooltip, and a mark on the line
says when history is incomplete or ambiguous, which is the statement
RFC-BPR-0029 requires the line to make. It folds. Open, its timeline runs
alone across the whole window, fully above the review bar; folded, it leaves no
row. Its label and its "Comparing …" menu live in the review bar, which names
the comparison and its dates either way. A new window starts with it folded,
and each window remembers its own choice. Nothing folds or opens on its own.

**The review bar**, full width and always shown, sits under the Updates line.
From its left it reads:

- the Updates disclosure, then the comparison and its dates, with the
  "Comparing …" menu;
- previous and next change, with the position;
- previous and next file, with the position;
- private progress (Seen, Read, Accept, Exclude), as an icon whose tooltip
  names this file's state.

On the bar's right are Compose review and Comment, as icons whose tooltips
name the command and its key. Compose review
shows a mark, in the amber of my last review's tick, while a review draft is
kept. Comment's arrow is where the Conventional Comments labels are offered
once RFC-BPR-0048 lands. Review details moves into the comparison's menu,
since what it discloses are the comparison's anchors.

**Document tabs** fill the comparison's header. Each tab holds a file, a
comparison and a reading position, under RFC-BPR-0025's tab contract. The
window has a comparison; new tabs take it; a tab with its own pair says so on
its label, so two tabs on one file are never mistaken for each other. The
tab's menu returns a tab to the window's comparison, or makes its pair the one
new tabs take. Overview and Discuss have no tab, so they show the window's
comparison. Tab names elide from the left, where file names differ, and at a
platform's width floor the strip shows the active tab and a count of the rest.

**Composition and conversation.** The composition entry row goes. Compose
review opens the dedicated page RFC-BPR-0037 built, as before. "Draft comment
or reply" becomes Comment, which starts a comment on the current line, or a
reply to the selected thread, and whose menu names the target. Every thread
card offers Reply, beside the code as in Discuss. Today a card beside the code
offers Comment here, which starts a second thread instead of answering the
one shown. A tab over an older pair places only the threads that belong to its
revisions, says how many are on later updates, and offers no new comment off
the current head.

**The right tray** takes tabs in its header: Comments and Findings, and Tour
once RFC-BPR-0039 provides it. Each tab's tools sit beneath the tabs, so the
conversations column stops stacking the AI tools, the finding queue and the
defect controls under the threads.

**Keys**, as the owner accepted them: Option-R (Alt+R) Compose review,
Option-C (Alt+C) Comment or Reply, Ctrl+Tab and Ctrl+Shift+Tab the next and
previous tab, Cmd-W (Ctrl+W) Close tab, and Esc back from a composition page.
If Cmd-W closes the window today, the window moves to Cmd-Shift-W
(Ctrl+Shift+W). Option-] and Option-[ are reserved for RFC-BPR-0040's next and
previous tour stop. Both composition commands also appear in the Pull Request
menu, with their keys.

**Carried from the rounds, to confirm before it is built.** While a pull
request loads, the readiness words take the Updates line's track, or the
review bar's summary when folded, so no row comes or goes above the
comparison. The rounds drew this from v2 on, but the owner was never asked it
separately. [[BPR-1130]] asks before it removes [[BPR-0795]]'s row.

## Architecture

The core decides, the host performs, Qt presents. Tab collections, each tab's
comparison, the window's comparison and the fold are window state. Like the
comparison and the reading position RFC-BPR-0024 already restores, they belong
to the core and its restoration store, not to widgets. A completion names the
tab it was asked for; a late one cannot populate another. No new provider,
GitHub, Git or storage capability is introduced. The kept-draft mark is a
projection of the review workflow's existing draft state. Nothing here changes
publication, authority or review semantics.

## Relation to other RFCs

- **RFC-BPR-0028.** This RFC supersedes its chrome where they conflict and
  lifts its tabs non-goal. RFC-BPR-0028 closes on its own criteria, with AC-01
  as amended on 2026-09-24.
- **RFC-BPR-0025.** This RFC schedules its tab contract. RFC-BPR-0025's closed
  status and its recorded evidence are unchanged.
- **RFC-BPR-0029.** Its incomplete-history statement moves from a permanent
  caption to a mark on the line and the line's tooltip.
- **RFC-BPR-0037.** The composition pages, their retained drafts and their
  publication semantics are unchanged; only how they are reached moves.
- **RFC-BPR-0039 to 0051.** Tours and Conventional Comments fill the places
  drawn here: the Tour tray tab, the tour-stop keys, Comment's labels, the
  review page's preflight and the label icons on cards.

## Acceptance criteria

- AC-01 The chrome above the comparison is the title row, then the Updates line while it is open, then the review bar, with no other row between them in any state, and the comparison's header is the document tab strip. The Updates line folds from the review bar, a new window starts with it folded, and each window restores its own choice.
- AC-02 The title row holds Home, the drawer toggles, the pull request's name, the modality at its right beside refresh, the comments toggle carrying the new-comment count, account and settings, with the platform's window controls. No navigation, progress or composition control sits in it, and every control stays top level at each platform's width floor.
- AC-03 The review bar reads, from its left, the Updates disclosure with the comparison and its dates, open or folded; then previous and next change with the position, previous and next file with the position, and private progress as an icon whose tooltip names the active tab's file state; and at its right Compose review and Comment as icons whose tooltips name the command and its key. Open, the Updates line's timeline runs the full width above it. Compose review marks a kept review draft, and Review details is reached from the comparison's menu.
- AC-04 The Updates line carries no permanent caption. Its tooltip carries the history statement, and the line shows a mark whenever history is incomplete or ambiguous, as RFC-BPR-0029 requires.
- AC-05 Document tabs follow RFC-BPR-0025's tab contract: a tab is a file, a comparison and a reading position; clicking a file reuses the active tab, Cmd-click (Ctrl-click) and Open in New Tab open another, Duplicate Tab copies the exact view, previous and next file change the active tab, closing a tab follows the neighbour rule, and each window keeps its pull request's tabs across restart.
- AC-06 Each tab has its own comparison. New tabs take the window's comparison, a tab with its own pair says so on its label, and the tab's menu returns it to the window's comparison or makes its pair the one new tabs take. The Updates line shows the active tab's pair, Overview and Discuss show the window's, late work cannot reach another tab, and no exact view is silently retargeted.
- AC-07 A tab over an older pair places only the threads that belong to its revisions, says how many are on later updates with a way to show them, and offers no new comment off the current head.
- AC-08 Comment starts a comment on the current line, or a reply to the selected thread, and its menu names the target; its arrow is where RFC-BPR-0048's labels are offered. Every thread card offers Reply, beside the code as in Discuss.
- AC-09 The right tray's header holds tabs, Comments and Findings, and Tour once RFC-BPR-0039 provides it, with each tab's tools beneath the tabs rather than stacked in one column.
- AC-10 Option-R (Alt+R) composes the review, Option-C (Alt+C) comments or replies, Ctrl+Tab and Ctrl+Shift+Tab move between tabs, Cmd-W (Ctrl+W) closes the tab, with the window moving to Cmd-Shift-W (Ctrl+Shift+W) if the tab takes its key, Esc returns from a composition page, and Option-] and Option-[ are reserved for RFC-BPR-0040's tour stops. Both composition commands are in the Pull Request menu, and no key sequence is used twice in the assembled window.
- AC-11 The rows are measured on every supported platform and the tests follow them: application_uiforetw_selected_diff_contract and its course script take the measured stacks with the Updates line open and folded, BPR-0940's window course asserts the new arrangement, and each platform's width floor is swept again.

## Implementation entry

| Card | Slice | Criteria |
|---|---|---|
| [[BPR-1129]] | The title row and the review bar replace the composition entry row | AC-01, AC-02, AC-03, AC-10, AC-11 |
| [[BPR-1130]] | The Updates line folds and loses its permanent caption | AC-01, AC-04, AC-11 |
| [[BPR-1131]] | Document tabs in the core and the host | AC-05, AC-06 |
| [[BPR-1132]] | The document tab strip in the comparison's header | AC-05, AC-06, AC-07, AC-10 |
| [[BPR-1133]] | Comment and Reply where the line and the thread are | AC-08, AC-10 |
| [[BPR-1134]] | The right tray's tabs | AC-09 |
| [[BPR-1135]] | Verify and close RFC-BPR-0052 | all |

## Verification

Use the focused Qt, host and composed courses that already pin the chrome,
changed to the new arrangement rather than loosened:

- the window course's allow-list ([[BPR-0940]]);
- the focus-order and unique-shortcut course;
- the strip's exact order in the UIforETW capture;
- the UIforETW height floors, in the capture and in its course script, with
  the digests the capture's source is pinned by;
- the Windows width sweep ([[BPR-0653]]).

Tabs need core and host tests for per-tab comparison, window-default
inheritance, restoration and stale completion, and a composed course that
opens a file in a new tab, compares two pairs of one file side by side,
restarts and returns. The rendered frames under
`docs/verification/BPR-1106-scaffolding-v6/` are the design, not evidence
that it is built. The owner judges the built chrome on macOS and Windows in
attended review.
