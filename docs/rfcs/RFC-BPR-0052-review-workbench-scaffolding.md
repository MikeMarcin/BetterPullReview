---
id: "RFC-BPR-0052"
title: "Review Workbench Scaffolding: Title Row, Review Bar, Folding Updates Line and Document Tabs"
status: "Proposed"
area: "BPR"
milestone: "Review workbench scaffolding"
depends_on: ["RFC-BPR-0025", "RFC-BPR-0028", "RFC-BPR-0029", "RFC-BPR-0037"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# RFC-BPR-0052: Review Workbench Scaffolding: Title Row, Review Bar, Folding Updates Line and Document Tabs

## Status and scope

Registered on 2026-09-24 from the owner's decision on [[BPR-1106]]. The design
round that decision `ece7a6f3` asked for began with where two buttons sit.
The owner widened it in the session to the workbench's outer scaffolding,
designed around the review-tour pack (RFC-BPR-0039 to 0046) and the
Conventional Comments pack (RFC-BPR-0047 to 0051). The decided design is
[round v8](../design/BPR-1106-scaffolding-v8.html), which carries the owner's two
same-day revisions of [round v6](../design/BPR-1106-scaffolding-v6.html) (the
first drawn as [v7](../design/BPR-1106-scaffolding-v7.html)). The rounds
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

**Owner amendment, 2026-09-25: activated ahead of canonical order.** The
owner, verbatim: "The design session is complete, add RFC-BPR-0052 to the
active work which should include BPR-1129 to BPR-1135". This RFC is active
work now, although the index still lists it after RFC-BPR-0038, and its
slices start without waiting for RFC-BPR-0028 to RFC-BPR-0038 to close. The
amendment names this RFC only: decision `1d9104f8` still holds for
RFC-BPR-0039 to RFC-BPR-0051, which stay Proposed with their cards in Backlog
until RFC-BPR-0028 to RFC-BPR-0038 close. Nothing else changes: the criteria,
the cards' boundaries, and the owner's attended look at the built chrome on
macOS and Windows stand as written. The [milestone
plan](plans/rfc-bpr-0052-review-workbench-scaffolding/MILESTONES.md) groups
the slices by their dependencies: BPR-1129, BPR-1131 and BPR-1134 first, then
BPR-1130, BPR-1132 and BPR-1133, then BPR-1135's closeout.

**Owner verdicts on the built chrome, 2026-09-25 (attended session 06ad5734,
macOS).** The owner answered all fourteen questions on the frozen m2 app
(`ba9d2723`, executable `a2e5128a…8a83`); each verdict binds that candidate.
Accepted as built: the title row, the Updates fold and disclosure, the
incomplete-history mark, the pair-ready words in the tooltip, Reply as
Comment's default on a thread's line, the interim arrow and menu, Reply on the
cards beside the code, the plus duplicating the active view, and the tray's
VoiceOver announcement and Right Arrow. Decided: the readiness words take the
track when open and the summary when folded (confirming the paragraph below;
[[BPR-1201]]), and a tab over an older pair hides later threads until Show them
([[BPR-1205]]). Two are **owner amendments**. First, the review bar puts file
navigation before change navigation, "File contains changes so file should be
left of change" (decision `fd158272`); AC-03 and the bar's list below now say
so ([[BPR-1202]]). Second, the strip's plus makes the new tab active, and tabs
"expand to fill the available space and shrink to a minimum size before
overflowing and allowing scrolling through", like Xcode's and Ghostty's
(decision `c1a81e29`; [[BPR-1206]]). Two were **rejected**. The kept-draft
mark led to a Compose review page that told the owner nothing ([[BPR-1203]]),
and "Findings" does not say what the second tray tab holds ([[BPR-1204]]); the
owner's direction on the tab's purpose, below, settles it. The owner also found that
Comment opened a page with no way to comment ([[BPR-1207]]). Windows's native
look and Narrator, and the Windows and Linux station runs, remain owed.
[[BPR-1135]] records each decision.

**Owner amendment of the milestone plan, 2026-09-25.** In chat, relayed by
the coordinator, the owner approved placing [[BPR-1201]] to [[BPR-1207]]
before the closeout. The [milestone
plan](plans/rfc-bpr-0052-review-workbench-scaffolding/MILESTONES.md) now has
a milestone between m2 and m3, `m2b-owner-verdict-follow-ups`, which holds
those seven slices, and m3 waits for it. The plan's scope grows by those
seven cards. The outcomes and exits of m1, m2 and m3 are unchanged, and so
are the criteria.

**Owner amendment, 2026-09-25: the second tray tab is Notes, the reviewer's
private space.** In chat, relayed by the coordinator, the owner settled what
[[BPR-1204]]'s tab is for, verbatim: "It's the place for your non-public
notes and AI conversations/review feedback. You can promote things from there
to draft in the Comments / Public tab to publish them." In the same direction:

- "move the thread controls to well the thread, right click menu maybe or a
  button";
- "Move Dif out of the right drawer entirely, he can live in the lower left
  corner of the status bar by default just like AllTheThings unless he's got
  a prime spot on a screen like on the first time experience home".

AI or Agent alone was rejected as the tab's framing, and the owner offered
Local, Notes or Personal as its name. BPR-1204 quotes the whole direction. The
owner then delegated the name ("We can always change it later if we don't
like it") and every other open choice in this batch. The decisions below were
made under that delegation, and each card records its rationale.

- **The name is Notes.** It names what the tab holds, as Comments and Tour
  do, and it is already the product's word for this material.
- **A thread's defect controls take both forms.** Every thread card gets a
  context menu, a defect's card gets a badge button, and defect navigation
  moves beside Next new comment ([[BPR-1208]]).
- **Dif's default place is the status bar's corner on every surface.** His
  prime spots are the signed-out first-time Home and the Compose workshop slot
  that [[BPR-0966]]'s verdict chose. His cues play wherever he is. [[BPR-1209]]
  builds this, carries [[BPR-0800]], and brings an owner amendment of
  RFC-BPR-0031's placement.
- **Private notes are written in the tab.** A note or an AI reply is promoted
  to a draft at Comment's target, and findings keep RFC-BPR-0012's existing
  conversion ([[BPR-1210]]).

AC-09 and the tray's design text below now say this. Under the same
delegation, [[BPR-1208]] to [[BPR-1210]] join `m2b-owner-verdict-follow-ups`,
because AC-09 as amended cannot be met without them. [[BPR-1204]] builds the
header after them. No other criterion changes.

**Owner amendment, 2026-09-26:** hand-back 49 asks that each navigation label
precede its arrows and that the association be visible. Both File and Changes
remain named even without a selection and at the width floor. Under the owner's
design delegation, a separate palette-derived horizontal rail joins each label
and its two arrows ([[BPR-1202]]).

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
alone across the whole window just below the review bar, so the bar and its
disclosure never move when it opens or folds; folded, it leaves no row. Its
label and its "Comparing …" menu live in the review bar, which names the
comparison and its dates either way. A new window starts with it folded,
and each window remembers its own choice. Nothing folds or opens on its own.

**The review bar**, full width and always shown, sits directly under the
title row.
From its left it reads:

- the Updates disclosure, then the comparison and its dates, with the
  "Comparing …" menu;
- the File label and position, then previous and next file;
- the Changes label and position, then previous and next change (file before change, by the
  owner's amendment of 2026-09-25: "File contains changes so file should be
  left of change");
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
comparison; so does Threads ([[BPR-1293]]; Conversations until [[BPR-1341]]), whose finder keeps that
comparison underneath it. Tab names elide from the left, where file names differ, and at a
platform's width floor the strip shows the active tab and a count of the rest.
By the owner's amendment of 2026-09-25, the strip's plus duplicates the active
tab's exact view and makes the copy the active tab, and tabs share the strip
as Xcode's and Ghostty's do: they expand to fill it, shrink to a minimum width
and then scroll. The owner left the width floor to the implementer ("pick one
example and run with it, don't ask me"), and Xcode's editor tabs were chosen
as the one reference. The same fill, shrink and scroll rule holds at every
width, the floor included. Scrolling replaces the floor's active tab and
count, the active tab is always scrolled fully into view, and Ctrl+Tab
reaches the rest ([[BPR-1206]]). No acceptance criterion changes.

**Composition and conversation.** The composition entry row goes. Compose
review opens the dedicated page RFC-BPR-0037 built, as before. "Draft comment
or reply" becomes Comment, which starts a comment on the current line, or a
reply to the selected thread, and whose menu names the target. Every thread
card offers Reply, beside the code as in Discuss. Today a card beside the code
offers Comment here, which starts a second thread instead of answering the
one shown. A tab over an older pair places only the threads that belong to its
revisions, says how many are on later updates, and offers no new comment off
the current head.

**The right tray** takes tabs in its header: Comments and Notes, and Tour
once RFC-BPR-0039 provides it. Each tab's tools sit beneath the tabs, so the
conversations column stops stacking the AI tools, the finding queue and the
defect controls under the threads. **By the owner's amendment of
2026-09-25**, Notes is the reviewer's private space. It holds their
non-public notes and the AI conversations and review feedback: the agent
panel, the side chat and the finding queue. Anything in it can be promoted to
a draft in Comments, and only Comments publishes. A thread's controls sit on
the thread, in its card's menu and, for a defect, on a badge on its card.
Defect navigation sits beside Next new comment. Dif is not in the tray: he
rests in the status bar's corner unless a screen gives him a prime spot. The
tab was drawn and built as Findings until the owner rejected that name, and
Notes was chosen under his delegation ([[BPR-1204]], [[BPR-1208]] to
[[BPR-1210]]).

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
separately. [[BPR-1130]] asks before it removes [[BPR-0795]]'s row. The owner
confirmed it on 2026-09-25 ("Move to the track when open and summary when
folded", decision `5b349ec3`); [[BPR-1201]] builds it.

## Architecture

The core decides, the host performs, Qt presents. Tab collections, each tab's
comparison, the window's comparison and the fold are window state. Like the
comparison and the reading position RFC-BPR-0024 already restores, they belong
to the core and its restoration store, not to widgets. A completion names the
tab it was asked for; a late one cannot populate another. No new provider,
GitHub, Git or storage capability is introduced. The kept-draft mark is a
projection of the review workflow's existing draft state. The Notes tab's
notes are RFC-BPR-0007's existing private ledger notes, written through its
existing port. Promoting one produces an ordinary local draft, which only the
existing explicit Publish sends. Nothing here changes publication, authority
or review semantics.

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
- **RFC-BPR-0009.** Its defect controls move from the tray onto the thread
  card ([[BPR-1208]]). Their commands and semantics are unchanged.
- **RFC-BPR-0007, RFC-BPR-0012, RFC-BPR-0023 and RFC-BPR-0048.** Notes are
  RFC-BPR-0007's private ledger notes, and findings keep RFC-BPR-0012's
  conversion to drafts. Promoting a single note or AI reply to a draft
  ([[BPR-1210]]) is the single-item seam that RFC-BPR-0048 anticipates.
  RFC-BPR-0023's private threads and Squash to Draft stay Deferred.
- **RFC-BPR-0031.** The owner amended its placement on 2026-09-25, and
  RFC-BPR-0031 records the amendment. Dif leaves the tray, and his default
  place is the status bar's corner ([[BPR-1209]]).
- **RFC-BPR-0039 to 0051.** Tours and Conventional Comments fill the places
  drawn here: the Tour tray tab, the tour-stop keys, Comment's labels, the
  review page's preflight and the label icons on cards.

## Acceptance criteria

- AC-01 The chrome above the comparison is the title row, then the review bar, then the Updates line's timeline while it is open, with no other row between them in any state, and the comparison's header is the document tab strip. The Updates line folds from the review bar without moving it, a new window starts with it folded, and each window restores its own choice.
- AC-02 The title row holds Home, the drawer toggles, the pull request's name, the modality at its right beside refresh, the comments toggle carrying the new-comment count, account and settings, with the platform's window controls. No navigation, progress or composition control sits in it, and every control stays top level at each platform's width floor.
- AC-03 The review bar reads, from its left, the Updates disclosure with the comparison and its dates, open or folded; then File and its position before previous and next file, Changes and its position before previous and next change, and private progress as an icon whose tooltip names the active tab's file state; and at its right Compose review and Comment as icons whose tooltips name the command and its key. Open, the Updates line's timeline runs the full width just below it. Compose review marks a kept review draft, and Review details is reached from the comparison's menu.
- AC-04 The Updates line carries no permanent caption. Its tooltip carries the history statement, and the line shows a mark whenever history is incomplete or ambiguous, as RFC-BPR-0029 requires.
- AC-05 Document tabs follow RFC-BPR-0025's tab contract: a tab is a file, a comparison and a reading position; clicking a file reuses the active tab, Cmd-click (Ctrl-click) and Open in New Tab open another, Duplicate Tab copies the exact view, previous and next file change the active tab, closing a tab follows the neighbour rule, and each window keeps its pull request's tabs across restart.
- AC-06 Each tab has its own comparison. New tabs take the window's comparison, a tab with its own pair says so on its label, and the tab's menu returns it to the window's comparison or makes its pair the one new tabs take. The Updates line shows the active tab's pair, Overview and Discuss show the window's, late work cannot reach another tab, and no exact view is silently retargeted. Amended 2026-10-06 ([[BPR-1293]]): Conversations, too, has no tab and shows the window's comparison underneath its finder. Amended 2026-10-08 ([[BPR-1341]]): that modality is now named Threads.
- AC-07 A tab over an older pair places only the threads that belong to its revisions, says how many are on later updates with a way to show them, and offers no new comment off the current head.
- AC-08 Comment starts a comment on the current line, or a reply to the selected thread, and its menu names the target; its arrow is where RFC-BPR-0048's labels are offered. Every thread card offers Reply, beside the code as in Discuss.
- AC-09 The right tray's header holds tabs, Comments and Notes, and Tour once RFC-BPR-0039 provides it, with each tab's tools beneath the tabs rather than stacked in one column. Notes is the reviewer's private space for their non-public notes and the AI conversations and review feedback, and anything in it can be promoted to a draft in Comments; a thread's controls sit on the thread, and Dif is not in the tray.
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
| [[BPR-1201]] | Readiness words in the track or the summary (owner decision, 2026-09-25) | AC-01 |
| [[BPR-1202]] | File navigation before change navigation (owner amendment, 2026-09-25) | AC-03 |
| [[BPR-1203]] | The Compose review page says what is happening (kept-draft mark rejected) | AC-03 |
| [[BPR-1204]] | The second tray tab is Notes, the reviewer's private space ("Findings" rejected; owner amendment, name under the owner's delegation) | AC-09 |
| [[BPR-1205]] | Later threads hidden until Show them (owner decision, 2026-09-25) | AC-07 |
| [[BPR-1206]] | The plus activates its tab; tabs fill, shrink, then scroll (owner amendment) | AC-05, AC-11 |
| [[BPR-1207]] | Comment never opens an unusable page | AC-08 |
| [[BPR-1208]] | A thread's defect controls sit on the thread (owner amendment, 2026-09-25) | AC-09 |
| [[BPR-1209]] | Dif leaves the right drawer for the status bar's corner (owner amendment, 2026-09-25) | AC-09 |
| [[BPR-1210]] | Private notes are written in Notes and promoted to drafts in Comments (owner amendment, 2026-09-25) | AC-09 |
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
