---
schema: "implement-rfc.card.v1"
id: "BPR-1021"
title: "Verify and close RFC-BPR-0028"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: ["BPR-0653","BPR-0746"]
closeout_for: ["RFC-BPR-0028"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"planning_reconciliation":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0028-closeout-planning.md","reservation":"docs/issues/_reservations/BPR-1020--BPR-1022--f3b33a48cfc4.json","unmet_dependencies":{"BPR-0653":"Parked on human-review for its Windows capture at 100% display scale (criteria 2 and 4; the 150% capture was accepted 2026-09-25, decision 1aba8610), which needs the owner to change the display scale first; criterion 3 also owes one unattended Windows station pass at a single current main revision"},"owner_decisions":"2026-09-24, attended session cbd48b91 (hand-back 589): AC-01 amended to admit the entry rows (decision 007a6eb5); BPR-0746 AC1/AC2 withdrawn by amendment (decision 14327a18); private Qt API accepted (decision 8dde5e4d); 2026-09-25, attended session df5859c7 (hand-back at sequence 6): BPR-0653's station capture accepted at 150% only (decision 1aba8610), 100% still owed, no amendment"}
---

# Verify and close RFC-BPR-0028

## Objective

Reconcile [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)
against direct evidence, all of its linked cards and the shipped workbench.
Close it when its acceptance criteria and explicit RFC dependencies are satisfied.
Index position is not a closeout gate.

RFC base: `58a853dd23cbbb7a7e4d28ff0ec39d405593d838`.

This is the commit before `08a621f7`, which locked the direction and carded its
first slices on 2026-09-06.

Plan: milestone `close-rfc-0028` of
[ux-redesign-pack-0028](../../rfcs/plans/ux-redesign-pack-0028/milestone-plan.json),
added by the [closeout planning record](../../rfcs/plans/ux-redesign-pack-0028/rfc-0028-closeout-planning.md).
It follows `windows-strip-title-row`.

## Why this card is in Backlog

Besides this card, 63 cards are linked to the RFC, and once BPR-0746 is
finished 62 of them are Done. Of this card's two dependencies, one is now Done
and one is not:

- [[BPR-0746]], the resize cursor over the seam, is **Done** by the owner's
  amendment of 2026-09-24 (attended session `cbd48b91`, decision `14327a18`),
  which withdrew its unobtainable AC1 and AC2. It is recorded as an amendment,
  not as a pass; the owner's report that the cursor works stays the evidence on
  [[BPR-0739]].
- [[BPR-0653]], the Windows title row, is **Parked on human-review** for its
  Windows capture at 100% display scale. On 2026-09-25 the owner accepted the
  station's capture at 150% (attended session `df5859c7`, decision
  `1aba8610`). The card binds the capture at 100% and 150%, and the verdict
  amends nothing, so criteria 2 and 4 stay pending on 100%. Its criterion 3
  also owes one unattended Windows station pass. The owner accepted Qt's
  private Windows native interface (decision `8dde5e4d`), and its criterion 1
  passes under the amended AC-01.

This card becomes Ready when BPR-0653 is Done. It does not reach past it.

## AC-01, amended by the owner (2026-09-24)

The conflict this section used to describe is settled. On main the chrome above
the comparison is the strip, the review surface's composition entry row
(Compose review and Draft comment or reply, RFC-BPR-0037's `cc038748`,
[[BPR-0929]]) and the epoch line; while a pull request loads, [[BPR-0795]]'s
readiness line sits there too. The owner chose to amend AC-01 and the chrome
paragraph to admit those rows (candidate `BPR-0653-entry-row-v001`, decision
`007a6eb5`, 2026-09-24T18:54:09Z), and asked for a UX design follow-up on where
the entries should finally sit. That follow-up is [[BPR-1106]]. It is
standalone and is not linked to this RFC, so it does not hold this closeout;
if it moves the entries, it amends AC-01 again. [[BPR-0940]]'s window course
already asserts the amended arrangement. [[BPR-1036]]'s height floor follows
from the same answer.

## Closeout checklist

Each row names the check planned for closeout and the cards that carry its
evidence today. Nothing has been run for this closeout.

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01 The chrome above the comparison is the strip, the review surface's entry rows and the epoch line, in that order, with no scroll area and no separate title bar, at every window size the workbench supports; the strip is the row the window controls sit in. The entry rows are the composition entry row (Compose review, Draft comment or reply) and, while a pull request loads, the readiness line; nothing else sits between the strip and the epoch line (owner amendment, 2026-09-24). | `qt_workbench_window_scale_{100,125,200}` (strip order, width sweep to the floor, the epoch row directly beneath the entry rows); `qt_workbench_title_row_scale_{100,125,200}`; `application_windows_title_row` on the real Windows platform at 1280x852 and 1920x1080; the macOS title row from [[BPR-0646]]; the owner's amendment above | Not run; BPR-0653's Windows capture at 100% and its criterion 3 station pass are open (150% accepted 2026-09-25) | None |
| AC-02 The epoch line selects any two uploads by drag, click, ⌘-click, menu and keyboard stepping, shows my last review, and drives the comparison the existing epoch host already resolves. | Epoch control and window courses from [[BPR-0646]], [[BPR-0676]], [[BPR-0725]], [[BPR-0737]] and [[BPR-0743]] (the pull request's own updates, RFC-BPR-0029), and the composed epoch public paths | Not run | None |
| AC-03 The four modalities set the drawers as described, persist per window, never change on their own unless a configured transition fires; in Overview a drawer gesture is refused with a stated reason and every drawer stays collapsed until another modality is chosen ([[BPR-0727]]); a collapsed drawer is its rail while the drawer controls are shown and no column while they are hidden ([[BPR-0721]]). Amended 2026-10-06 ([[BPR-1295]]): there are three modalities, Overview, Review and Discuss; Review restores the arrangement it last had in the window, hidden drawers included, and its preset only on the window's first entry or ⌘2 in Review; ⌘B and its icon hide every open drawer and restore exactly the same set, refused in Overview, disabled in Discuss, and yielded to a focused composer. Amended 2026-10-06 ([[BPR-1293]], with the owner's answers): there are four modalities, Overview, Review, Conversations and Discuss on ⌘1 to ⌘4; ⌘J goes straight to Conversations; while it shows the drawers are hidden without changing their states, and ⌘B is disabled there. Amended 2026-10-06 by the owner ("can we swap discuss to 3 and conversations to 4"): Discuss is ⌘3 and Conversations ⌘4. Amended 2026-10-08 by the owner ([[BPR-1341]]): Conversations is renamed Threads, so the fourth modality, ⌘4, is Threads and ⌘J goes to Threads. | `qt_workbench_window_scale_*` modality, rail and drawer courses; `qt_workbench_overview` (the Overview refusal); [[BPR-0662]], [[BPR-0674]], [[BPR-0724]], [[BPR-0920]] evidence | Not run | None |
| AC-04 The overview shows every file with the listed columns from projections the host already publishes, plus participants and the since-my-last-review summary, and Continue review lands on the first unseen change. | `qt_workbench_overview`; the overview courses of [[BPR-0647]], [[BPR-0654]] and [[BPR-0921]] | Not run | None |
| AC-05 Review (Read until [[BPR-1295]] merged it into Review, 2026-10-06) shows this file's threads aligned to their lines with unread state and drafts, in both presentations; in one pane a thread names a side and a line and sits against the row carrying that identity, so a thread on a removed line sits against its struck row and a thread on an added line against the added row, and threads that would collide keep document order and are pushed down with their leader still on the anchored row; Discuss shows the list, map and feed with the listed filters. Amended 2026-10-06 by the owner's verdict on [[BPR-1290]]'s inline drawer prototype ("I like it"): the open thread is pinned at its anchored row and the threads around it make room above and below it, with the same document order; Discuss shows the list with the listed filters, each thread opening in place, and no map or separate feed. | `qt_workbench_conversation_reveal` and the window's Read and Discuss courses; the aligned-card, one-pane and Discuss courses of [[BPR-0648]], [[BPR-0649]], [[BPR-0729]], [[BPR-0734]] and [[BPR-0745]] | Not run; the Windows abort in the Discuss course recorded at `38cc26d7` is fixed by [[BPR-1029]] (`8edd961a`), after which `qt_workbench_conversation_reveal` and `qt_workbench_window_scale_*` pass on the Windows station | None |
| AC-06 Home is the whole centre while nothing is open, with none of the review chrome, and the strip then shows only Home, account and settings; a Home button (⌘0) at the strip's left returns to Home from any review (owner decision, 2026-09-07: Home is a view with its button, not a fifth modality); it shows the grouped action list from the queue and ledger projections; with no account it funnels to sign-in, and with no pull request open it funnels to the queue or the locator, rather than showing empty panes. | `qt_home_list_scale_*`, `qt_workbench_home_discovery_scale_*`, the Home courses of `qt_composed_github_scale_*`; [[BPR-0650]], [[BPR-0669]], [[BPR-0923]] evidence | Not run | None |
| AC-07 Sign-in remains reachable in every state without scrolling. | The composed sign-in courses in `qt_composed_github_scale_*` and `qt_device_authorization_scale_*`; [[BPR-0646]], [[BPR-0924]] evidence | Not run | None |
| AC-09 The repositories and files drawers render the trees above from the queue, ledger and snapshot projections and collapse independently. | The drawer and tree courses of [[BPR-0651]], [[BPR-0658]], [[BPR-0659]] and [[BPR-0926]]; `qt_composed_resident_drawer_filters` | Not run | None |
| AC-10 The three fixed Settings pages exist with live controls, and every other page names where its settings will live. | `qt_settings_window`, `qt_settings_effect_boundary`; [[BPR-0652]] evidence | Not run | None |
| AC-08 A recorded conversation with more than twenty posts and three branches reads correctly in Review and Discuss (the owner's stated stress case; Read until [[BPR-1295]] merged it into Review, 2026-10-06). Amended 2026-10-06 by the owner's verdict on [[BPR-1290]]'s inline drawer prototype ("I like it"): it reads in place, every post in posting order with its complete body, and a reply that answers anything but the first post names whom it answers. Navigating its structure is [[BPR-1293]]'s. | The recorded stress conversation courses of [[BPR-0648]], [[BPR-0649]] and [[BPR-0925]] | Not run | None |

## Completion rules

Record the exact command, result and producing commit for each criterion. Every
linked card must be Done first, including any allocated later. BPR-0746 may be
Done only through a recorded owner amendment of its unobtainable criteria.

Use focused changed-surface checks, one integration-focused pass over the
composed workbench and Settings selectors, and the real Windows title-row
course. Limit attended, native and platform claims to the evidence actually
taken. A superseded or amended criterion is recorded as such, never as passed.

Run the documentation gate and `git diff --check`. Reconcile the RFC's status
and its index row only when its acceptance criteria and explicit dependencies allow.

## Closeout assessment, 2026-09-25 (main, Mac)

This is bookkeeping only. Nothing was run for this closeout, and main was at
`81d8da35`. **RFC-BPR-0028 cannot close yet.** The completion rules require
every linked card to be Done, and one is not. This card stays in Backlog.

### What still blocks it

1. **[[BPR-0653]], criteria 2 and 4 at 100% display scale.** The owner
   accepted the station's capture at 150% on 2026-09-25: session
   `df5859c784ba447497026d6cd81af810`, candidate
   `BPR-0653-windows-capture-150-v001`, decision `1aba8610`. The card binds the
   capture at 100% and 150%. The question told the owner that 100% stays owed,
   and the verdict amends nothing. The steps are:
   - the owner sets JaegAW's display scale to 100%, a system setting;
   - the station runs `windows_caption_capture.ps1`;
   - the owner reviews that capture.
2. **[[BPR-0653]], criterion 3.** Its two Windows reds are fixed, by
   [[BPR-1029]] and [[BPR-0869]]. But no single station pass covers its focused
   selectors and the manifest gate since the window code last changed. One
   unattended Windows pass at a current main revision clears it, and it needs
   no owner.
3. **This card's own checks.** Every row of the checklist is still Not run.
   They follow BPR-0653 and cannot be recorded before it is Done.

Only the owner can shorten item 1, by amending BPR-0653's criteria 2 and 4 to
the accepted 150% capture. No such amendment has been made. If one is, item 2
is BPR-0653's last obligation.

### What does not block it

- **[[BPR-0746]]** is Done by the owner's amendment (decision `14327a18`),
  recorded as an amendment, not as a pass.
- **AC-01's entry rows** are admitted by the owner's amendment of 2026-09-24.
- **Canonical order.** Every RFC before RFC-BPR-0028 in the index is
  Auto-Verified or Deferred, checked at `81d8da35`.
- **The linked cards.** 64 cards name RFC-BPR-0028: 62 are Done, BPR-0653 is
  Parked and this card is in Backlog. None has been allocated since.
- **The follow-ups.** [[BPR-1106]] and [[BPR-1022]] are standalone and Done.

### RFC-BPR-0052

The coordinator reports that the owner has activated
[RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md). At
`81d8da35` the repository does not record that yet:

- it is Proposed;
- its text and index row say it starts after RFC-BPR-0028 to RFC-BPR-0038
  close (decision `1d9104f8`);
- [[BPR-1129]] to [[BPR-1135]] are in Backlog.

Recording the activation belongs with its implementer. RFC-BPR-0052's own text
decides what the activation changes here.

- **No criterion changes.** RFC-BPR-0052 supersedes RFC-BPR-0028's chrome where
  they conflict: the composition entry row, the strip's navigation, progress
  and details controls, the modality's place, and the Updates line's permanent
  caption. It also says RFC-BPR-0028 "closes on its own criteria, with AC-01
  as amended on 2026-09-24". So the activation adds no blocker and removes
  none. It does not stand in for BPR-0653's 100% capture.
- **The evidence window narrows.** [[BPR-1129]] replaces the composition entry
  row and moves those strip controls and the modality. [[BPR-1130]] folds the
  Updates line and drops its caption. Once either lands on main, main no longer
  draws AC-01's arrangement: the strip, the entry rows, then the epoch line.
  This closeout then has two choices for AC-01, and for any other row whose
  check measures a superseded element:
  - bind its checks to a main revision from before those slices landed; or
  - record it as superseded by RFC-BPR-0052, never as passed.

  Today, main's head is still such a revision.
- **BPR-0653's remaining evidence.** The 150% accept is bound to image
  `6aa27c8d…98ac4` (source `85e522d8`), which predates RFC-BPR-0052. A 100%
  capture from the same image shows the same title row, whatever RFC-BPR-0052
  lands meanwhile. Criterion 3's station pass is best taken before
  [[BPR-1129]] lands. After that it would measure RFC-BPR-0052's title row
  instead.
