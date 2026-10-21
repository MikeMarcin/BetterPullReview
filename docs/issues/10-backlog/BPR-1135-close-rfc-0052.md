---
schema: "implement-rfc.card.v1"
id: "BPR-1135"
title: "Verify and close RFC-BPR-0052"
status: "Backlog"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129","BPR-1130","BPR-1131","BPR-1132","BPR-1133","BPR-1134","BPR-1197","BPR-1201","BPR-1202","BPR-1203","BPR-1204","BPR-1205","BPR-1206","BPR-1207","BPR-1208","BPR-1209","BPR-1210"]
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
closeout_for: ["RFC-BPR-0052"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0052

## Objective

Reconcile [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
against its linked implementation cards, direct evidence and the owner's
attended look at the built chrome on macOS and Windows.

RFC base: `0453103ddd11337ee8b3adf5053b25149104acd8`.
Requested RFC set: RFC-BPR-0052; workstream position: 1 of 1.
Planned cards: BPR-1129, BPR-1130, BPR-1131, BPR-1132, BPR-1133, BPR-1134, BPR-1135, BPR-1197, from the owner's 2026-09-25 verdicts BPR-1201 to BPR-1207, and from the owner's direction on the second tray tab the same day BPR-1208 to BPR-1210. Add subsequently allocated required slices to
this list and dependencies before closeout. Registration is not implementation.

## Closeout checklist

| Criterion | Direct check / evidence | Result at the reconciliation | Producing commit |
|---|---|---|---|
| AC-01 The chrome above the comparison is the title row, then the review bar, then the Updates line's timeline while it is open, with no other row between them in any state, and the comparison's header is the document tab strip. The Updates line folds from the review bar without moving it, a new window starts with it folded, and each window restores its own choice. | E0/E1: `qt_workbench_window_scale_{100,125,200}` (BPR-0940 arrangement and fold), `core_workbench_interaction`, `application_workbench_host`, `sqlite_workbench_settings_storage`; E4 geometry. | Partial on macOS: loaded arrangement, tab header and per-window fold verified; the owner accepted the fold (2026-09-25). **Unsatisfied during loading:** BPR-0795's readiness row still sits above the comparison and the test permits it. The owner answered `BPR-1130-readiness` (track when open, summary when folded); BPR-1201 builds it. Windows look and station acceptance pending. | `96a86433`, `fb4b8364`, `0994aa0e` |
| AC-02 The title row holds Home, the drawer toggles, the pull request's name, the modality at its right beside refresh, the comments toggle carrying the new-comment count, account and settings, with the platform's window controls. No navigation, progress or composition control sits in it, and every control stays top level at each platform's width floor. | E0: `qt_workbench_title_row_scale_{100,125,200}`, `qt_workbench_window_scale_{100,125,200}`; exact title controls and 1920-to-900 width sweep. | Mac automated checks pass; the owner accepted the title row on macOS (2026-09-25). Windows 902-width sweep and native controls, and Linux station evidence remain pending. | `96a86433`; retained through `0994aa0e` |
| AC-03 The review bar reads, from its left, the Updates disclosure with the comparison and its dates, open or folded; then File and its position before previous and next file, Changes and its position before previous and next change, and private progress as an icon whose tooltip names the active tab's file state; and at its right Compose review and Comment, whose tooltips name the command and its key: Comment as an icon, and Compose review as an icon while a review is in progress and otherwise as "Approve or comment…" beside its icon, shortened to "Approve…" first as the bar narrows and dropped to the icon last. Open, the Updates line's timeline runs the full width just below it. Compose review marks a kept review draft, and Review details is reached from the comparison's menu. | E0/E1: window scales, `qt_epoch_comparison_scale_{100,125,200}`, `core_review_submission`; E4 exact 14-action review-bar order and full-width open timeline. | Mac automated checks pass for the order as built (change before file), active-file progress, kept-draft mark and Review details. The owner amended AC-03 to file before change (2026-09-25; BPR-1202) and rejected the kept-draft mark, whose Compose review page explained nothing (BPR-1203). Station evidence pending. | `96a86433`, `fb4b8364`, `0994aa0e` |
| AC-04 The Updates line carries no permanent caption. Its tooltip carries the history statement, and the line shows a mark whenever history is incomplete or ambiguous, as RFC-BPR-0029 requires. | E0/E1: epoch/window scales assert no caption, history tooltip/description and incomplete/ambiguous-history marks in every modality. | Mac automated checks pass; the owner accepted the history mark and the pair-ready tooltip on macOS (2026-09-25). Station evidence remains pending. | `fb4b8364`; retained through `0994aa0e` |
| AC-05 Document tabs follow RFC-BPR-0025's tab contract: a tab is a file, a comparison and a reading position; clicking a file reuses the active tab, Cmd-click (Ctrl-click) and Open in New Tab open another, Duplicate Tab copies the exact view, previous and next file change the active tab, closing a tab follows the neighbour rule, and each window keeps its pull request's tabs across restart. | E0: core/host/restoration/store tab suites; E1: tab scales, Files tree and `application_document_tabs_restoration_public_path`; E2: full profile-window restoration. | Mac automated checks pass: exact pairs/positions, background file open, duplicate, neighbour close, restart, Resume and explicit reopen. The owner accepted the plus duplicating the view and amended the strip: the plus activates its tab, and tabs fill, shrink to a minimum, then scroll (BPR-1206). Native/station acceptance remains pending. | `e94e67f1`, `0994aa0e` |
| AC-06 Each tab has its own comparison. New tabs take the window's comparison, a tab with its own pair says so on its label, and the tab's menu returns it to the window's comparison or makes its pair the one new tabs take. The Updates line shows the active tab's pair, Overview and Discuss show the window's, late work cannot reach another tab, and no exact view is silently retargeted. Amended 2026-10-06 ([[BPR-1293]]): Conversations, too, has no tab and shows the window's comparison underneath its finder. Amended 2026-10-08 ([[BPR-1341]]): that modality is now named Threads. | E0: `core_document_tabs`, `application_document_tab_host`; E1: exact two-pair tab/restart course, tab scales and single-request workbench composition. | Mac automated checks pass: window versus tab pair, menu actions, modality return, exact reading and stale rejection. Required station/attended acceptance remains pending. | `e94e67f1`, `0994aa0e` |
| AC-07 A tab over an older pair places only the threads that belong to its revisions, says how many are on later updates with a way to show them, and offers no new comment off the current head. | E1: `qt_document_tabs_scale_{100,125,200}` tests exact revision placement, later-thread count, three Show them routes and disabled Comment off current head. | Mac automated checks pass. Later threads remain listed but unplaced; the owner chose hidden until Show them (2026-09-25; BPR-1205). Native/station acceptance remains pending. | `0994aa0e` |
| AC-08 Comment starts a comment on the current line, or a reply to the selected thread, and its menu names the target; its arrow is where RFC-BPR-0048's labels are offered. Every thread card offers Reply, beside the code as in Discuss. | E0/E1: window scales and conversation cards; E2: `application_threaded_reply_contract` and `application_review_submission_composition_public_path`; E5 combined mutation composition. | Mac automated checks pass: named line/reply target, own-thread Reply and no automatic Publish. The owner accepted Reply as the default, the interim arrow and menu, and Reply beside the code (2026-09-25), and found Comment opening a page with no way to comment (BPR-1207). Native/station acceptance pending. RFC-0048 labels remain future conditional scope. | `7c99a1eb`, `0994aa0e`; later diagnostics `9734f0cb`, `12830eff` |
| AC-09 The right tray's header holds tabs, Comments and Notes, and Tour once RFC-BPR-0039 provides it, with each tab's tools beneath the tabs rather than stacked in one column. Notes is the reviewer's private space for their non-public notes and the AI conversations and review feedback, and anything in it can be promoted to a draft in Comments; a thread's controls sit on the thread, and Dif is not in the tray. | E0/E1: window scales (tray content, scroll/selection, keyboard and accessibility names), focus-order scales, composed GitHub, conversation panel/cards/reveal. | Mac automated checks pass for the tabs as built at `2770ebb1`, named Comments and Findings. The owner rejected the tray header: "Findings" does not say what it holds (2026-09-25; BPR-1204). The same day the owner amended AC-09: the tab is Notes, the reviewer's private space, with the name chosen under his delegation. BPR-1204 and BPR-1208 to BPR-1210 build it, and none of them is built yet. The owner's native listen accepted the macOS VoiceOver announcement and Right Arrow; the listen is repeated for "Notes" only. Narrator and Windows/Linux evidence remain pending. Tour is conditional on RFC-0039, which remains unstarted. | `2770ebb1`; retained through `0994aa0e` |
| AC-10 Option-R (Alt+R) composes the review, Option-C (Alt+C) comments or replies, Ctrl+Tab and Ctrl+Shift+Tab move between tabs, Cmd-W (Ctrl+W) closes the tab, with the window moving to Cmd-Shift-W (Ctrl+Shift+W) if the tab takes its key, Esc returns from a composition page, and Option-] and Option-[ are reserved for RFC-BPR-0040's tour stops. Both composition commands are in the Pull Request menu, and no key sequence is used twice in the assembled window. | E0/E1: window/focus-order/tab scales deliver composition and tab keys and assert unique assembled bindings; source inspection finds no Alt-bracket tour binding. | Mac synthetic Qt key delivery passes; no existing Cmd-W window binding required a move. Native/platform delivery remains pending. Tour-stop keys stay reserved for RFC-0040. | `96a86433`, `7c99a1eb`, `0994aa0e` |
| AC-11 The rows are measured on every supported platform and the tests follow them: application_uiforetw_selected_diff_contract and its course script take the measured stacks with the Updates line open and folded, BPR-0940's window course asserts the new arrangement, and each platform's width floor is swept again. | E0/E1: BPR-0940 window arrangement, title/tab width sweeps; E4: `application_uiforetw_selected_diff_contract` raw v7 geometry in all four appearance profiles. | Partial: Mac measured 872 folded / 838 open at 1280x1000; tab strip 28 px. Linux 876/842 is derived; Windows 884/850 is unmeasured. Both station runs deferred by owner; Windows 850 open budget unchanged. | `0994aa0e` |

## Completion rules

Record the exact commands, results and producing commits for each criterion.
Every required linked card must be done, including slices allocated later.
Use focused changed-surface checks and a composed public path. Limit native
and attended claims to actual evidence. Run the documentation gate, then
reconcile the RFC's index status only when canonical order and its
dependencies allow.

## m2 exit acceptance — macOS engineering, 2026-09-26

**m2 is accepted for its macOS engineering boundary; m2b is the next eligible
milestone.** This is the current continuation and supersedes the earlier
statements below that m2's exits still await judgment. Assignment
`rfc0052-20260925 / m2-exit-acceptance-20260926`, mandate
`owner-20260926-unattended-worktrees`, runs in
`/Users/jaeger/.codex/worktrees/rfc0052-owner-followups/BetterPullReview`, branch
`codex/rfc0052-owner-followups`, from `fdeea1ac`. The coordinator confirmed this
bounded reconciliation and the fresh-implementer boundary before m2b.

The acceptance judges the original m2 exits against product
`0994aa0ee80e542e980c5b5b8682e1aac0161ec0`, frozen review source
`ba9d272339ad235f1d99688c5c1af198b0c9c7ef`, executable SHA-256
`a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83`, and the
fourteen final owner verdicts in `06ad5734`, handed back at sequence 54. It
does not requalify later product changes. The current
[binding](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/m2-exit-checkpoint.json),
[observation](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/m2-exit-observation.json)
and [handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-m2-2026-09-26.md)
retain the exact assignment, six judgments, source candidates and next action.

The plan's m2 boundary explicitly says station runs and attended acceptance
stay pending until supplied. The owner deferred Windows/Linux execution;
those obligations remain on this closeout, as they did when m1 admitted m2.
This accepts the available-target engineering boundary, without accepting
the missing platform evidence or the full RFC. The three m2 cards remain Done
on macOS; this card remains Backlog and the RFC remains Proposed. The plan,
the original exits, and all eleven RFC criteria are unchanged.

| Original m2 exit | Evidence and judgment |
|---|---|
| 1. Readiness answer before changing the row; per-window, explicit fold | **Pass on macOS.** Owner decision `5b349ec3` supplies the prerequisite answer: track when open, summary when folded. BPR-1130's core/host/store and window courses prove the explicit fold, default, restoration and fixed bar. Owner `74ee4e1c` accepts the fold. The existing readiness row is preserved until BPR-1201 implements the answer; RFC AC-01 remains incomplete. |
| 2. Caption removed; history tooltip and mark in every modality | **Pass on macOS.** BPR-1130's epoch/window courses at 100/125/200 percent cover the caption, statement and incomplete/ambiguous-history marks. Owner `2edcf8e2` accepts the mark and `ccba3a15` accepts pair-ready words in the tooltip. |
| 3. Tab strip, pairs, floor count, menu, file gestures and unique keys | **Pass on macOS for the original strip.** E1's three tab scales, Files course, and the retained focus/shortcut courses prove these original behaviors. Owner `c1a81e29` accepts duplication and amends activation and sizing; BPR-1206 implements that amendment, replacing the original floor count with scrolling. |
| 4. Tab/window comparisons, exact placement and later-thread routes, no off-head comment, restart | **Pass on macOS.** E1's tab course covers all three Show them routes and off-head Comment refusal; the 94.10-second public restart course preserves exact pairs and reading positions through duplicate, close, restart, Resume and reopen. The original exit requires later threads to be unplaced, counted and reachable; owner `2c856168` additionally chooses hiding them until Show them, carried by BPR-1205. |
| 5. Comment on a current line, selected-thread Reply, own-card Reply, explicit Publish | **Pass on macOS for the defined target paths.** BPR-1133's three-scale window course names and dispatches line 10 and thread 22, replies to another visible card's own thread and records zero publish commands. Its threaded-reply and publication-composition courses pass. Owner `1a095bfe`, `b3d1d32d` and `37c42e44` accept the default, menu and card Reply. The no-target editor defect and the owner's actual selected-line state remain unresolved on BPR-1207; see the distinction below. |
| 6. Measured open/folded floors, width/focus/accessibility/scale checks, documentation | **Pass for the macOS boundary.** E4's four appearance captures measure 872 folded / 838 open at 1280×1000 and the 28-pixel strip; E1 and retained title/focus/epoch courses cover 100/125/200 percent and the 900-pixel Mac floor. The documentation gate passes for this reconciliation. Windows/Linux measurements and native checks are carried below, not marked passed. |

**Comment's known defect is retained.** BPR-1207's screenshot transcription
names the editor's `selection_cleared` message, "Select a current-head diff
line to compose a review comment." Source inspection confirms that branch and
the existing action's unconditional opening when no target is selected. The
window course establishes the original selected-line and selected-thread
dispatch; it does not establish that the owner's exact fixture state had a
usable editor. No such reproduction is claimed here. BPR-1207 AC1 must still
exercise that fixture both without a selected change and with a current-head
line selected, then pin the usable editor and unavailable-action behavior in
the composed course. The owner's approved m2b places that investigation and
correction before closeout. Likewise BPR-1203 must diagnose the frozen
Compose review authority message; it is not treated as accepted.

**Required debt stays open.** Windows native appearance and Narrator;
Windows/Linux station courses; Linux measured heights (876/842 remain derived);
Windows's 902-pixel width sweep and measured 884/850 budget; applicable native
key delivery; all ten m2b cards and their separate owner questions. The Windows
850 open budget is unchanged. The existing macOS VoiceOver/Right Arrow verdict
`6ee64abe` is accepted only for the old tray candidate; Notes needs its new
listen. No release, live-provider or all-platform claim follows.

**Review state and next action.** Per the user's latest state relayed by the
coordinator, session `06ad5734` was consumed and closed, its server stopped and
its runtime directory deleted; `60bd1ed5` received the fourteen matching
imported verdicts and released its claims. The old m2 handoff's server and
waiter are stopped. Availability of the historical m2 evidence files is not
rechecked in this round. Do not
reopen either session or ask their answered questions again. The coordinator
lands this documentation checkpoint and dispatches a fresh m2b implementer:
BPR-1201 to BPR-1203 and BPR-1205 to BPR-1210, then BPR-1204 after its three
prerequisites. The m2b owner questions remain pending until reviewed on the
new candidate. m3 cannot begin before m2b's accepted exit.

## Owner direction on the second tray tab — 2026-09-25

This is the retained design direction. The verdict section below and the reconciliation
after it are the earlier record, and the sentence there that has
[[BPR-1204]]'s name waiting on a purpose discussion is superseded.

**The direction.** In chat, relayed by the coordinator, the owner settled what
BPR-1204's tab is for. BPR-1204 quotes the direction verbatim, and it says:

- the tab is the reviewer's private space for non-public notes and AI
  conversations and review feedback, and items are promoted from it to drafts
  in Comments, which publishes;
- a thread's controls move onto the thread;
- Dif leaves the right drawer and rests in the status bar's corner unless a
  screen gives him a prime spot;
- AI or Agent alone is rejected as the framing, and the name is one of Local,
  Notes or Personal.

**Delegation.** In three further relayed messages, the owner delegated the
tab's name ("We can always change it later if we don't like it"), the form of
the thread controls, and the Dif conflict with [[BPR-0966]] and [[BPR-0800]].
He asked that no open owner choice be left in this batch.

**Decisions made under that delegation.** Each card records its rationale.

| Choice | Decision | Card |
| --- | --- | --- |
| The tab's name | Notes | [[BPR-1204]] |
| Where a thread's defect controls go | Both forms: a context menu on every thread card, a badge button on a defect's card, and defect navigation beside Next new comment | [[BPR-1208]] |
| The Compose workshop slot against "corner by default" | Compose's workshop slot is a prime spot, and BPR-0966's verdict (`dba44cfd`) stands | [[BPR-1209]], [[BPR-0966]] |
| Where Dif's cues play | Wherever he is, with unchanged triggers, so RFC-BPR-0031 B7 is not amended | [[BPR-1209]] |
| BPR-0800 against a new card | BPR-0800 folds into BPR-1209, depends on it and closes on its evidence | [[BPR-1209]], [[BPR-0800]] |
| Order of BPR-0966 and BPR-1209 | No dependency; whichever lands second updates the Dif context course | [[BPR-1209]], [[BPR-0966]] |
| How promotion targets a draft | At Comment's target, one item at a time, keeping the source; RFC-BPR-0023 stays Deferred | [[BPR-1210]] |
| Plan placement of BPR-1208 to BPR-1210 | In `m2b-owner-verdict-follow-ups`, because AC-09 as amended cannot be met without them | this card |
| Order within m2b | BPR-1204 builds the header last. It depends on BPR-1208 to BPR-1210 and moves from Ready to Backlog until they are Done | [[BPR-1204]] |

BPR-0800's open question needed no delegated decision: "Move Dif out of the
right drawer entirely" answers it, because the corner replaces the drawer
slot.

**Promotion coverage.** Findings already promote to drafts through
RFC-BPR-0012's queue, and that flow is reused. Notes exist only in RFC-BPR-0007's
ledger, with no surface to write them and no promotion to a draft, and side-chat
replies cannot become drafts. [[BPR-1210]] adds that half.

**Text amended.** RFC-BPR-0052 AC-09, its tray design text, its status section,
its architecture and relation sections and its implementation table. The AC-09
row above follows the amended criterion. RFC-BPR-0031 records an owner
amendment of Dif's placement, keeping its criteria's accepted text.
RFC-BPR-0032's Home section points to that amendment. The
[plan](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/MILESTONES.md)'s
m2b adds the three cards, their exits and a rewritten BPR-1204 exit and
boundary, and MILESTONES.md is regenerated from the JSON with the installed
renderer. m1, m2 and m3 are unchanged. No product code changed.

**Plan checks.** The installed `milestone_plan.py check-amendment` refuses the
change with `amendment cannot change plan identity or authorized scope`, as it
refused the first amendment. It admits only refinements that keep the scope,
and the owner's delegation is what authorizes the wider one.
`milestone_plan.py check --target RFC-BPR-0052` reports the plan found and
structurally valid, with m1, m2, m2b and m3. `inspect --milestone
m2b-owner-verdict-follow-ups` lists all ten cards. BPR-1204's only unresolved
prerequisites are BPR-1208 to BPR-1210.

**Next at that checkpoint.** m2's exits still awaited judgment; the 2026-09-26
acceptance above now admits m2b. In m2b, BPR-1208, BPR-1209 and BPR-1210 are Ready and independent of one
another. BPR-1204 follows them, and it asks the owner one question about the
built Notes tab, not about its name.

## Owner verdicts on the m2 questions — 2026-09-25 (attended session 06ad5734)

These are the retained owner verdicts; the reconciliation below is the earlier record.

A fresh session, `06ad5734ed9f7c2fd6d0a62c8328fa0a` (packet
[owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)),
carried the fourteen questions verbatim from `60bd1ed5`, with identical
fingerprints and the same frozen app (`ba9d2723`, executable
`a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83`). The owner
answered all fourteen and handed back at sequence 54. Each decision is `actor:
human` with its fingerprint equal to the prepared one. Two candidates were
answered twice; the session keeps the later decision current and lists the
earlier one as superseded.

| Candidate | Decision, seq | Verdict, verbatim | Disposition |
|---|---|---|---|
| `BPR-1130-readiness` | `5b349ec3`, 44 | "Move to the track when open and summary when folded" | m2 exit 1's answer is recorded; [[BPR-1201]] |
| `BPR-1129-title` | `aee924e0`, 40 | "Keep this title row" | Accepted (AC-02, macOS) |
| `BPR-1129-bar` | `fd158272`, 48 (supersedes `8b8b310b`, 38, "Keep this review bar") | "Revise the review bar; describe the change": "I want to swap the order of file and change. File contains changes so file should be left of change." | Owner amendment to AC-03; [[BPR-1202]] |
| `BPR-1129-kept` | `011a2ebb`, 39 | **reject**: "When I click compose review I end up on this screen and I have no idea what's happening or what I'm meant to do." | [[BPR-1203]] |
| `BPR-1134-tray` | `6c6d2c52`, 52 | **reject**: "I have no idea what the hell findings is supposed to be." | [[BPR-1204]] |
| `BPR-1134-voiceover` | `6ee64abe`, 53 | "Yes, the announcement and Right Arrow both work" | Accepted (AC-09 native, macOS) |
| `BPR-1130-fold` | `74ee4e1c`, 41 | "Keep this fold and disclosure" | Accepted (AC-01, macOS) |
| `BPR-1130-history` | `2edcf8e2`, 42 | "The history mark is clear enough" (note below) | Accepted (AC-04, macOS) |
| `BPR-1130-pair-ready` | `ccba3a15`, 43 | "Keep pair-ready words in the tooltip" | Accepted (AC-04, macOS) |
| `BPR-1133-default-reply` | `1a095bfe`, 51 | "Reply is the right default" | Accepted (AC-08, macOS) |
| `BPR-1133-arrow` | `b3d1d32d`, 49 | "Keep the current arrow and menu", with the note "note: clicking comment takes me to this screen in the screenshot which seems completely broken, Doesn't seem like I can actually leave a comment yet." | Accepted; the page is [[BPR-1207]] |
| `BPR-1133-card-reply` | `37c42e44`, 50 | "Keep Reply on these cards" | Accepted (AC-08, macOS) |
| `BPR-1132-plus` | `c1a81e29`, 47 (supersedes `f9bfb3f9`, 46, same choice, no note) | "Plus should duplicate the active view": "Plus must switch focus to the new tab. Tabs should work like xcode/ghostty where they expand to fiill the available space and shrink to a minimum size before overflowing and allowing scrolling through." | Duplication accepted; owner amendment for focus and sizing, [[BPR-1206]] |
| `BPR-1132-later` | `2c856168`, 45 | "Hide later threads until Show them" | [[BPR-1205]] |

The note on `BPR-1130-history`, verbatim: "Metafeedback: Most of these attended
review questions seem like simple accept/reject review critera would be
sufficient, not sure why they are being presented as multiple option select
responses." It is now a rule for preparing attended questions in `AGENTS.md`.

The screenshots on `BPR-1129-kept`, `BPR-1133-arrow` and `BPR-1132-plus` are
kept with their decisions in the session's external state,
`/Users/jaeger/.mask/attended-review/owner-review-20260925-fresh-v001/`. They
are not committed.

**Where m2 stands.** Every owner question on macOS is answered, so exit 1's
owner answer is recorded and [[BPR-0795]]'s row may now change. The built
cards stay Done. The verdicts add seven required slices, [[BPR-1201]] to
[[BPR-1207]], all Ready and listed in this card's dependencies. Still owed:
the Windows native look and Narrator, the Windows and Linux station courses,
Linux's measured floors, and Windows's 902-pixel sweep and 850 open budget.
m3 is not eligible.

**The plan amendment, approved by the owner (2026-09-25).** The milestone plan
named only BPR-1129 to BPR-1135. In chat, relayed by the coordinator, the
owner approved amending it so that BPR-1201 to BPR-1207 come before m3. The
[plan](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/MILESTONES.md)
now has `m2b-owner-verdict-follow-ups` between m2 and m3. It holds the seven
cards, with one exit for each card and one shared verification exit, and m3
depends on it. Each of the seven cards links the plan in its metadata. The
definitions and exits of m1, m2 and m3 are unchanged, except that m3 now also
waits for m2b. The installed `milestone_plan.py check-amendment` refuses the
change (`amendment cannot change plan identity or authorized scope`) because
it admits only refinements that keep the scope. The owner's approval is what
authorizes the wider scope. `milestone_plan.py check --target RFC-BPR-0052`
then reports the amended plan found, structurally valid and fully linked, with
milestones m1, m2, m2b and m3. [[BPR-1197]] is Done and listed in this card's
dependencies, but it is still outside the plan; the owner's approval did not
name it.

**Decided and waiting inside m2b.** [[BPR-1206]]'s open point, the width
floor's count once tabs scroll, is decided without an owner question. The
owner delegated it ("pick one example and run with it, don't ask me"), and
Xcode's editor tabs are the one reference. One fill, shrink and scroll rule
holds at every width, the floor included, and there is no count or overflow
button. The card records the behaviour, and the RFC's design text says so.
[[BPR-1204]]'s tab name waits on a discussion with the owner about what the
tab is for. No name or alternative is prepared before that.

**Session `60bd1ed5` imported and released (2026-09-25).** Its recorded host
(port 63317, PID 97732), its later server (port 49975, PID 25339) and its
hand-back waiter (PID 97747) were no longer running, and nothing listened on
either port. The fourteen current decisions in the table above were exported
from `06ad5734` in the page's export shape, with only the `session` field
changed, and taken in through the skill's intake:

```sh
python3 .agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe/attended-review import --file <export>
python3 .agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe/attended-review resume-development --all
```

All fourteen fingerprints matched, and all fourteen were accepted (sequence 42
to 56). Each imported decision equals its original except for `saved_at`.
The superseded decisions `8b8b310b` and `f9bfb3f9` are not part of an export,
so they stay only in `06ad5734`. The other three `06ad5734` candidates
(BPR-0295, BPR-0641 and BPR-0966) do not exist in `60bd1ed5` and were skipped.
`resume-development --all` then released the session's 28 claims on the
frozen app and Qt (sequence 57), and the registry holds none for it. Its
server was not started. Its state stays at
`/Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe/attended-review/`,
uncommitted, with a stale host entry that `serve` would replace.

**Next.** m2's owner exit is now answered, so the next engineering round can
judge m2's exits and record its acceptance. The plan then admits m2b. Its
cards are Ready, but BPR-1204 waits for the owner's purpose discussion. m3
stays ineligible until m2 and m2b are accepted.

## Current reconciliation — partial, 2026-09-25

Assignment `rfc0052-20260925 / m3-reconciliation`, mandate
`owner-20260925-unattended-parallel`: reconcile existing evidence in the isolated
`codex/rfc0052-m3` worktree from `aa59606ae2352298dbd6545bacd294f7eef8af74`.
The coordinator confirmed that this is documentation-only partial work and does
not establish m3 eligibility. BPR-1135 stays **Backlog**, with no claim or finish:
the six implementation cards are Done on macOS, but m2's readiness-placement
exit and required human/native evidence are still pending. The installed
milestone binder refuses m3 with `milestone is unknown, accepted or has unmet
prerequisites`. No accepted-milestone flag was supplied to bypass that guard.
RFC-BPR-0052 and its index stay Proposed under the existing activation amendment.

The table replaces the stale “Not run; not started” entries with the actual Mac
checks and their limits. **AC-01 is not fully satisfied even on macOS:**
`test_the_epoch_row_spans_the_window_in_every_modality` explicitly allows
`reviewReadiness` while loading. The owner must decide its placement before
BPR-0795 changes; a passing loaded-state course does not accept “in any state.”

### Reused evidence and candidate identity

Exact command arrays, log hashes, source comparisons, measurements, pending
criteria and this round's checks are in
[m3-reconciliation-checkpoint.json](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/m3-reconciliation-checkpoint.json).
The [m3 handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-m3-2026-09-25.md)
is the current continuation. The earlier milestone records below remain history.

All E0–E4 courses ran on macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug,
offscreen and serial, during m2; the final product is
`0994aa0ee80e542e980c5b5b8682e1aac0161ec0`. This round re-read the actual logs
and four capture-facts files. It reuses unchanged passing inputs and records
failed intermediate runs rather than presenting them as an all-green matrix.

| Evidence | Actual result and use |
|---|---|
| E0: `verification.json`, `integration-tests.json`, `evidence/m2-integration.log` | Initial integration 50/52. The selected-diff request count and Home-during-Resume checks failed; E1/E2 supersede those results. The unchanged core/host/store, focus/title/epoch, conversation and geometry rows passed. |
| E1: `reopen-verification.json`, `evidence/m2-reopen-tests.log` | Final affected set 9/9, 179.47 s; includes window/tab scales, Files, exact-request workbench and full tab/restart/reopen (94.10 s). |
| E2: `correction-verification.json`, `evidence/m2-correction-integration.log` | 10/11, with full profile restoration passing in 557.70 s. Its tab-return failure and the later 5/6 gesture run's repeat-click failure are superseded by E1. |
| E3: `pin-verification.json`, `evidence/m2-final-pins.log` | Final architecture, tests-disabled Release composition and review round trip: 3/3, 237.29 s. |
| E4: `evidence/uiforetw/{light,dark,high-contrast-light,high-contrast-dark}/capture-facts.json` | All four: schema v7, initial fold true, comparison height 872, open timeline y=78/height=34, open comparison 838; document strip height 28. The capture course passed in E0. |
| E5: [[BPR-0898]] and its checkpoint | Combined source at `a39cfc91`, with pins committed by `f530e08b`: build and six serial refusal/editor/composition rows pass in 223.30 s. Source at this reconciliation is identical; the final pin contract is also identical to `f530e08b`. |

E0–E4 artifacts live in `/Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe`.
The later changes between m2 and this candidate are BPR-0898's typed publication
failure diagnostics, editor messages and exact-draft retry retention; the tab,
chrome and geometry inputs are unchanged. E5 covers the changed composition.
The 0052 Mac passes are therefore reused, with no fresh build or product-test
rerun for this documentation-only reconciliation. The m2 idle large-history
rerun also passed (309.55 s); its former follow-up obligation is resolved.

### Pending acceptance and retained session

One existing session, `60bd1ed56e768d60a40b104920641436`, was handed off at
cursor **42** with **14 ready / 0 decisions**. That is the m2 handoff snapshot,
not a fresh read of attendance. Its frozen app remains at
`ba9d272339ad235f1d99688c5c1af198b0c9c7ef`, executable SHA-256
`a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83`.
The coordinator owns the already armed hand-back watch and loopback server.
This round neither reads new verdicts nor prepares another candidate.

The 14 pending choices remain readiness placement; title row; review bar;
kept-draft mark; tray header; VoiceOver/Right Arrow; Updates fold; incomplete
history mark; pair-ready tooltip; default Comment/Reply; Comment arrow; Reply
beside code; plus duplication; and listed/hidden later threads. The pending
native VoiceOver listen is not supplied by QAccessible names or synthetic key
delivery. Windows appearance/Narrator, Windows/Linux courses, Linux measured
876/842 floors, Windows width sweep and measured 884/850 budget are still owed.
The owner deferred remote execution, not its evidence; never lower Windows'
850 open budget to pass. These obligations keep m2 and m3 unaccepted.

## Milestone checkpoints

### m1-title-row-bar-tab-model-tray: complete on macOS, 2026-09-25 (main, `7f36927a`)

The owner activated RFC-BPR-0052 on 2026-09-25 ahead of canonical order. The
plan is [milestone-plan.json](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json)
([MILESTONES.md](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/MILESTONES.md)).

| Card | Product commit | Finish | Record |
| --- | --- | --- | --- |
| [[BPR-1131]] document tabs in the core and the host | `e94e67f1` | `5b18a7d5` | Done on macOS |
| [[BPR-1129]] the title row and the review bar | `96a86433` | `1e8f0206` | Done on macOS |
| [[BPR-1134]] the right tray's tabs | `2770ebb1` | `7f36927a` | Done on macOS |

m2's cards ([[BPR-1130]], [[BPR-1132]], [[BPR-1133]]) moved to Ready in
`95a44560`.

- **Changed paths.** Core: `document_tabs.{hpp,cpp}` (new),
  `window_review_restoration.{hpp,cpp}`, `review_submission`'s `kept`.
  Application: `document_tab_host.{hpp,cpp}` (new), the window restoration
  host. SQLite: the restoration store's version four. Qt: `workbench_window`,
  `workbench_strip`, `epoch_line`, `epoch_comparison_control`,
  `review_submission_editor`. App: the UIforETW capture in
  `github_application.cpp` (test build only). Tests: the new core and host
  suites, the restoration suites, the Qt window, title-row, focus-order,
  composed and epoch-comparison courses,
  `application_uiforetw_selected_diff_contract.cmake`, and
  `application_review_round_trip_composition_contract.cmake` (resealed by
  [[BPR-1129]], with its record). Documentation: the workbench specification's
  layout, primary actions and conversation drawer, and the system
  architecture's tabs.
- **Integration pass at `7f36927a`** (macOS 26.6.2 arm64, Qt 6.11.2,
  `build/dev` Debug): 38 of 38 pass in 250.4 s, including
  `application_composition_contract`. The `core-only` preset builds, and its
  163 rows pass.

  ```sh
  ctest --test-dir build/dev -j8 -R "^(application_composition_contract|core_document_tabs|application_document_tab_host|core_window_review_restoration|application_window_review_restoration|sqlite_window_review_restoration_storage|sqlite_private_store_open|application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract|application_github_public_path_uiforetw_74|qt_workbench_title_row_scale_(100|125|200)|qt_workbench_window_scale_(100|125|200)|qt_workbench_focus_order_scale_(100|125|200)|qt_composed_github_scale_(100|125|200)|qt_epoch_comparison_scale_(100|125|200)|qt_review_submission_editor_scale_(100|125|200)|core_review_submission|qt_workbench_conversation_reveal|qt_github_conversation_panel|qt_conversation_cards_view|application_agent_finding_composition_contract|application_agent_invocation_composition_contract|application_workbench_composition_contract|test_network_isolation_manifest|architecture_boundaries|core_dependency_boundary)$"
  ```

- **Not this milestone's failure.** `application_agent_finding_dif_context_contract`
  fails as [[BPR-0966]] records, unchanged by these cards.
- **Owed before this card can close** (none blocks m2):
  1. The Windows and Linux station runs, with the exact commands in each
     card's "Owed elsewhere" section. [[BPR-1129]]'s includes the Linux
     height measurement (834 is derived; the measured value becomes the floor)
     and the Windows width sweep to 902 and height at the owner's 850. Windows
     had about 857 with the entry row, so the 28-pixel bar may take it below
     850. If it does, the owner decides; the floor is not lowered here.
  2. The owner's attended look, with the questions recorded on [[BPR-1129]]
     (four) and [[BPR-1134]] (two). No session was prepared.
- **Next action.** A fresh implementer takes m2: [[BPR-1130]] (the fold, the
  caption, the readiness line's words, which it asks the owner about, and the
  floors re-taken open and folded), [[BPR-1132]] (the tab strip, composing
  [[BPR-1131]]'s host over the epoch and workbench hosts), and [[BPR-1133]]
  (Comment's arrow and target, and Reply on every thread card).

### m2-fold-tab-strip-comment-reply: two of three cards done on macOS; stopped at the owner's wind-down, 2026-09-25

| Card | State | Product commit | Finish | Record |
| --- | --- | --- | --- | --- |
| [[BPR-1130]] the Updates line folds and loses its permanent caption | Done on macOS | `fb4b8364` | `7444aeb8` | On the card |
| [[BPR-1133]] Comment names its target, and every thread card replies | Done on macOS | `7c99a1eb` | `6db29f76` | On the card |
| [[BPR-1132]] the document tab strip in the comparison's header | In Progress (claimed `9d1b17a4`) | none on main | — | Checkpoint on the card |

- **Done.** [[BPR-1130]]: the Updates disclosure folds the timeline from the
  review bar and by keyboard (the View menu's Updates Line, Space on the
  disclosure; no new key); the fold is core state kept in each window's own
  layout record (settings store version ten), a new window starts folded, and
  nothing folds on its own; the line is 34 pixels with no caption, its statement
  is its tooltip and description, and incomplete or ambiguous history is a mark
  on the line and on the disclosure. [[BPR-0795]]'s readiness row is unchanged
  pending the owner's answer. [[BPR-1133]]: Comment answers the selected thread
  or comments on the current line and names the target in its tooltip, the Pull
  Request menu and its arrow's menu (the plain comment alone until
  RFC-BPR-0048); every card beside the code offers Reply through the Discuss
  path; nothing publishes without Publish. Both have negative controls, renders
  at 100, 125 and 200 percent, and their records on the cards.
- **Work in progress.** [[BPR-1132]] is a patch, not on main:
  the historical saved patch (consumed by the resumed product commit below)
  (`BPR-1132-document-tab-strip.patch` against `c0fdd70a`, SHA-256
  `830375e7239d9328585f0ba6dbbb3758200fa26288f6cb6c76fc37ba3f53eb0c`). It
  builds and passes the courses the README lists; the two
  `github_application.cpp` pins fail until resealed, and its Qt tests, composed
  course, format, tidy, specification and record are still to write.
- **Measured floors (macOS 26.6.2 arm64, 1280x1000, offscreen, all four
  appearance profiles).** On main at `fb4b8364`: 879 folded and 845 open (the
  menu bar 20, title row 30, review bar 28, diff status 15, 6 of spacing, status
  bar 22; open adds the timeline at 78+34), asserted by the capture and its
  course script. On the [[BPR-1132]] patch: 872 folded and 838 open (the
  28-pixel tab strip in place of the status row and its spacing). Linux is
  derived from [[BPR-1036]]'s stack (884 and 850 on main, 876 and 842 on the
  patch); Windows keeps the owner's 850 for the open line (884 folded) and is
  unmeasured. If Windows measures below 850, that is the owner's decision, and
  no floor is lowered here.
- **Owner asks (no session prepared; one question per criterion).** On
  [[BPR-1129]] (four; its fourth, the comparison 6 pixels shorter, is answered
  by the measurement: 879 folded and 845 open against 829), [[BPR-1134]] (two),
  [[BPR-1130]] (four: where the readiness words go, which the card must ask
  before [[BPR-0795]]'s row changes; the fold; the incomplete-history mark; the
  line's "pair ready" words now in the comparison menu's tooltip),
  [[BPR-1133]] (three: replying on a thread's line; the arrow's menu until
  RFC-BPR-0048; Reply on the cards beside the code), and two prepared on
  [[BPR-1132]]'s checkpoint for when it is done ("+" duplicating the tab; the
  older-pair notice above unplaced later threads).
- **Owed runs (the stations are idle; nothing was run on them).** Windows
  (bpr-main-win) and Linux (bpr-main-linux), each with the exact command on its
  card: [[BPR-1129]], [[BPR-1131]], [[BPR-1134]] (m1), [[BPR-1130]] (with both
  heights read from `capture-facts.json`: `comparison_rect_in_window.height` and
  `updates_line_open_comparison_height`), and [[BPR-1133]].
- **Not this milestone's failure.** `application_agent_finding_dif_context_contract`
  as [[BPR-0966]] records.
- **Next step.** Apply the [[BPR-1132]] patch at the current main, finish it
  (its checkpoint lists what remains), then run m2's integration pass and
  record it here; m3 follows once m2's exits hold.

### m2 resume checkpoint — macOS engineering verified; human/native pending, 2026-09-25

This supersedes the earlier m2 wind-down and next-action list. Assignment
`rfc0052-20260925 / m2-resume`, mandate `owner-20260925-m2-resume`, amended to
prepare one fresh scoped attended session after macOS engineering. The owner
explicitly deferred Windows/Linux runs. No remote station or live GitHub review
mutation was performed.

- **Product:** `0994aa0ee80e542e980c5b5b8682e1aac0161ec0` completes [[BPR-1132]] from the saved patch,
  alongside already completed [[BPR-1130]] and [[BPR-1133]]. The source and
  workflow root is `/Users/jaeger/Documents/GitHub/BetterPullReview`, `main`;
  unrelated RFC pack import `fc4b39e8` is preserved. The saved patch and its
  README are removed after consumption; their SHA and original state remain
  in the historical card record.
- **Implementation:** document tabs and their path/pair labels, narrow overflow,
  unique keys, file gestures, older-pair notice and per-window exact restoration.
  Integration corrected request reuse, failure ownership, Home cancellation,
  queued modality/file precedence and reopening from the retained Files cursor.
  The full source review and both composition-pin reseals are in the product
  commit; the mutation-branch digest is unchanged.
- **Verification:** build, changed-file format and tidy pass. The 52-row macOS
  integration passed 50 rows and exposed two lifecycle defects. The affected
  11-row run passed 10, including the full profile restoration course (557.70 s),
  and exposed request failure ownership on tab return. Its correction passed
  tab switching/restart but the strengthened final click exposed the retained
  list bridge. The final 9-row run passes all rows (179.47 s), including the full
  tab/restart/reopen course (94.10 s), exact-single-request composition, all tab
  and window scales, and Files tree. Final architecture and clean tests-disabled
  Release/composition-pin verification also pass. The idle large-history rerun
  passes in 309.55 s; no follow-up failure card is needed. Documentation gate
  and all 64 documentation regression tests pass.
- **Measurements:** all four UIforETW appearance profiles retain 872 folded /
  838 open at 1280×1000; the document strip is 28 pixels. Linux 876/842 remains
  derived; Windows 884/850 remains unmeasured and its owner budget is unchanged.
- **Evidence:** command/result inventories and original failure/correction logs
  are in `/Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe`:
  `verification.json`, `correction-verification.json`, `gesture-verification.json`,
  `reopen-verification.json`, `pin-verification.json`, `integration-tests.json`
  and `evidence/`. They are local runtime artifacts, not repository content.
- **Pending:** m2 is not fully accepted. Exit 1 still needs the owner's readiness
  wording placement before [[BPR-0795]] changes; its row is unchanged. The 14
  recorded owner decisions, macOS VoiceOver, Windows native appearance/Narrator,
  Windows/Linux station courses and measured platform floors remain owed. A
  platform failure remains the originating implementation card's defect. m3 is
  not eligible until the m2 exits and remaining closeout acceptance hold.
- **Review preparation:** fresh session `60bd1ed56e768d60a40b104920641436` at cursor 42
  has exactly 14 ready candidates and no decisions; it binds only those 14 decisions, one criterion per item with choices and Other/write-in,
  to the frozen app at revision `ba9d272339ad235f1d99688c5c1af198b0c9c7ef`
  and static renders. The loopback server is running (PID 25339); the coordinator
  owns the hand-back watch. Windows native appearance remains pending. Its final identity and command ownership
  are recorded in [the durable handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-m2-2026-09-25.md).
- **Workflow:** MASK preflight is `ready` / `ready_pinned` after the coordinator
  removed disposable installed bytecode; route `rfc-implementer`, `gpt-6-astra`,
  `xhigh`, `host_load: not_verified`. Skill CLIs use
  `PYTHONDONTWRITEBYTECODE=1`. The parent-created selection is preserved in
  `strategy-selection.json`; the final obligation binding is `m2-checkpoint.json`.

## m2b owner hand-back 49 — 2026-09-26

Fresh engineering assignment `/root/rfc0052_m2b_verdicts`, segment
`m2b-owner-verdicts-r2-20260926`, mandate
`owner-20260926-unattended-worktrees`, starts from integrated main `eb14f40e`.
[Exact twelve decisions](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/owner-handback49.json)
bind frozen source `8b5bc31c` and executable `96f2083c…125b`, not the newer
integrated baseline. Six accepts carry forward: readiness, tab sizing, Notes
purpose and spoken name, thread VoiceOver, note promotion. Six rejects remain:
bar grouping, Compose page and kept mark, Comment, thread controls and Dif size.

The rejected preparation exposed public preview and a missing classified defect.
The next packet requires agent-proved authenticated open-fixture actions and
Wizard steps with copyable launch/PR values. Navigation labels move before their
controls with visible group association; Dif becomes smaller under the delegated
design authority. BPR-1210 AC1 historical naming remains engineering pending;
BPR-1204 keeps its terminal dependencies. Native Windows/Linux station obligations,
BPR-1211 synchronous ledger I/O, and later BPR-1212/1213 scope remain unchanged.

### m2b successor preparation complete — 2026-09-26

The [second-round handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-m2b-r2-2026-09-26.md)
binds the repaired label/rail geometry and28px corner Dif to frozen source
`1971a9ab` and executable `b45b6420…12715`. Native proof now reaches editable
Compose and Comment and an actual critical/security defect through the open,
authenticated synthetic fixture. One shared setup Wizard and six criterion
Wizards accompany six ready Accept/Reject decisions in successor session
`0a3f0caf7998162c936e25e6d3f68672`, cursor25. Root owns the armed hand-back watch
and existing ACTIVE continuation heartbeat. The six earlier accepts remain;
BPR-1201/1206 are Done. BPR-1210 AC1 engineering debt, BPR-1204's dependency
guard and the native/closeout obligations above remain explicit.

## m2b Comment acceptance reconciled — 2026-09-26

Fresh assignment `rfc0052-20260925/m2b-handback12-20260926`, mandate
`owner-20260926-unattended-worktrees`, consumes the sole remaining prepared
question. The owner accepted BPR-1207 AC3 in session
`f613e282cad0d66a0813a7c85b3954a0`, decision
`8d7d2682-13bf-4ff1-970f-c4335f326b16`, event 11, hand-back 12, with no note
or attachment. The [exact receipt](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/owner-handback12.json)
binds source `c777a33d` and executable `8589abf9…b50ba`. This satisfies the
local-writing/no-publication owner criterion; AC1/AC2 retain the reproduced,
corrected public-path evidence on BPR-1207. Its three criteria are now met.

The earlier hand-back32 finished BPR-1202, BPR-1203, BPR-1208 and BPR-1209
plus carried BPR-0800. BPR-1201, BPR-1205 and BPR-1206 were already Done.
All prior exact decisions remain recorded. BPR-1210 AC1 is still blocked by
`historical-note-path-contract`; its promotion AC7 remains accepted. BPR-1204
keeps its terminal dependency and stays Backlog. Thus m2b and m3 remain
incomplete, with native Windows/Linux/station and full RFC evidence still owed.
The [current handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-m2b-r3-2026-09-26.md)
preserves cleanup, the consumed cursor and the separate BPR-1211/BPR-1214 debts.
