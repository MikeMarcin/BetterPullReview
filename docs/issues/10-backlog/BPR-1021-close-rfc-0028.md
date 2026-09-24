---
schema: "implement-rfc.card.v1"
id: "BPR-1021"
title: "Verify and close RFC-BPR-0028"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: ["BPR-0653","BPR-0746"]
closeout_for: ["RFC-BPR-0028"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"planning_reconciliation":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0028-closeout-planning.md","reservation":"docs/issues/_reservations/BPR-1020--BPR-1022--f3b33a48cfc4.json","unmet_dependencies":{"BPR-0653":"Parked on human-review for its Windows attended capture only (snap layouts, a drag, the caption buttons), from a tests-disabled build at or after e08a0409; asks 2 and 3 were answered 2026-09-24"},"owner_decisions":"2026-09-24, attended session cbd48b91 (hand-back 589): AC-01 amended to admit the entry rows (decision 007a6eb5); BPR-0746 AC1/AC2 withdrawn by amendment (decision 14327a18); private Qt API accepted (decision 8dde5e4d)"}
---

# Verify and close RFC-BPR-0028

## Objective

Reconcile [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)
against direct evidence, all of its linked cards and the shipped workbench.
Close it only under the canonical index order. RFC-BPR-0028 is the next RFC in
that order; every RFC before it is Auto-Verified or Deferred.

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
- [[BPR-0653]], the Windows title row, is **Parked on human-review** for one
  ask: its attended capture on the Windows station (snap layouts, a drag, the
  caption buttons' look), from a tests-disabled build at or after `e08a0409`.
  The owner accepted Qt's private Windows native interface (decision
  `8dde5e4d`), and its criterion 1 passes under the amended AC-01.

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
| AC-01 The chrome above the comparison is the strip, the review surface's entry rows and the epoch line, in that order, with no scroll area and no separate title bar, at every window size the workbench supports; the strip is the row the window controls sit in. The entry rows are the composition entry row (Compose review, Draft comment or reply) and, while a pull request loads, the readiness line; nothing else sits between the strip and the epoch line (owner amendment, 2026-09-24). | `qt_workbench_window_scale_{100,125,200}` (strip order, width sweep to the floor, the epoch row directly beneath the entry rows); `qt_workbench_title_row_scale_{100,125,200}`; `application_windows_title_row` on the real Windows platform at 1280x852 and 1920x1080; the macOS title row from [[BPR-0646]]; the owner's amendment above | Not run; BPR-0653's attended capture is open | None |
| AC-02 The epoch line selects any two uploads by drag, click, ⌘-click, menu and keyboard stepping, shows my last review, and drives the comparison the existing epoch host already resolves. | Epoch control and window courses from [[BPR-0646]], [[BPR-0676]], [[BPR-0725]], [[BPR-0737]] and [[BPR-0743]] (the pull request's own updates, RFC-BPR-0029), and the composed epoch public paths | Not run | None |
| AC-03 The four modalities set the drawers as described, persist per window, never change on their own unless a configured transition fires; in Overview a drawer gesture is refused with a stated reason and every drawer stays collapsed until another modality is chosen ([[BPR-0727]]); a collapsed drawer is its rail while the drawer controls are shown and no column while they are hidden ([[BPR-0721]]). | `qt_workbench_window_scale_*` modality, rail and drawer courses; `qt_workbench_overview` (the Overview refusal); [[BPR-0662]], [[BPR-0674]], [[BPR-0724]], [[BPR-0920]] evidence | Not run | None |
| AC-04 The overview shows every file with the listed columns from projections the host already publishes, plus participants and the since-my-last-review summary, and Continue review lands on the first unseen change. | `qt_workbench_overview`; the overview courses of [[BPR-0647]], [[BPR-0654]] and [[BPR-0921]] | Not run | None |
| AC-05 Read shows this file's threads aligned to their lines with unread state and drafts, in both presentations; in one pane a thread names a side and a line and sits against the row carrying that identity, so a thread on a removed line sits against its struck row and a thread on an added line against the added row, and threads that would collide keep document order and are pushed down with their leader still on the anchored row; Discuss shows the list, map and feed with the listed filters. | `qt_workbench_conversation_reveal` and the window's Read and Discuss courses; the aligned-card, one-pane and Discuss courses of [[BPR-0648]], [[BPR-0649]], [[BPR-0729]], [[BPR-0734]] and [[BPR-0745]] | Not run; on Windows `qt_workbench_conversation_reveal` and `qt_workbench_window_scale_*` abort in the Discuss course at `38cc26d7` (a card is requested) | None |
| AC-06 Home is the whole centre while nothing is open, with none of the review chrome, and the strip then shows only Home, account and settings; a Home button (⌘0) at the strip's left returns to Home from any review (owner decision, 2026-09-07: Home is a view with its button, not a fifth modality); it shows the grouped action list from the queue and ledger projections; with no account it funnels to sign-in, and with no pull request open it funnels to the queue or the locator, rather than showing empty panes. | `qt_home_list_scale_*`, `qt_workbench_home_discovery_scale_*`, the Home courses of `qt_composed_github_scale_*`; [[BPR-0650]], [[BPR-0669]], [[BPR-0923]] evidence | Not run | None |
| AC-07 Sign-in remains reachable in every state without scrolling. | The composed sign-in courses in `qt_composed_github_scale_*` and `qt_device_authorization_scale_*`; [[BPR-0646]], [[BPR-0924]] evidence | Not run | None |
| AC-09 The repositories and files drawers render the trees above from the queue, ledger and snapshot projections and collapse independently. | The drawer and tree courses of [[BPR-0651]], [[BPR-0658]], [[BPR-0659]] and [[BPR-0926]]; `qt_composed_resident_drawer_filters` | Not run | None |
| AC-10 The three fixed Settings pages exist with live controls, and every other page names where its settings will live. | `qt_settings_window`, `qt_settings_effect_boundary`; [[BPR-0652]] evidence | Not run | None |
| AC-08 A recorded conversation with more than twenty posts and three branches reads correctly in Read and Discuss (the owner's stated stress case). | The recorded stress conversation courses of [[BPR-0648]], [[BPR-0649]] and [[BPR-0925]] | Not run | None |

## Completion rules

Record the exact command, result and producing commit for each criterion. Every
linked card must be Done first, including any allocated later. BPR-0746 may be
Done only through a recorded owner amendment of its unobtainable criteria.

Use focused changed-surface checks, one integration-focused pass over the
composed workbench and Settings selectors, and the real Windows title-row
course. Limit attended, native and platform claims to the evidence actually
taken. A superseded or amended criterion is recorded as such, never as passed.

Run the documentation gate and `git diff --check`. Reconcile the RFC's status
and its index row only when the canonical order and the dependencies allow.
