---
schema: "implement-rfc.card.v1"
id: "BPR-1417"
title: "Verify and close RFC-BPR-0076"
status: "Ready"
rfcs: ["RFC-BPR-0076"]
depends_on: []
closeout_for: ["RFC-BPR-0076"]
metadata: {"milestone_plan":{"id":"file-change-navigation-v1","path":"docs/rfcs/plans/file-change-navigation-v1/milestone-plan.json"},"mandate_revision":"file-change-navigation-r1","rfc_base_sha":"d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1"}
---

# BPR-1417: Verify and close RFC-BPR-0076

RFC base: `d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1`.

Own all five criteria in [RFC0076](../../rfcs/RFC-BPR-0076-file-change-navigation-enters-review.md).
Preserve settled bounded entry and exact active-tab context; do not reopen
historical cards or change unrelated title-row work.

## Acceptance criteria

- **AC01:** Each of the four real navigation actions, invoked from each of
  Overview, Discuss and Threads, reveals Review and performs exactly its
  ordinary step when available, through clicks, menus and admitted shortcuts.
- **AC02:** First/last file, first/last change, no-open-comparison and
  no-changes cases reveal Review with the unchanged source position and exact
  three-second bound message. The no-tab first-file case follows “Enter Review
  and navigate its context.” Inactive workbenches remain disabled.
- **AC03:** When an active tab retains an older pair than the window's pair,
  navigation follows that tab's material and preserves sibling tabs. Delayed
  restoration performs one step on the originating context; replacement or
  unavailable material cannot produce a step on a different pair or overwrite
  later navigation.
- **AC04:** Entry restores Review's remembered drawer arrangement, including
  hidden drawers; repeated navigation inside Review does not reset it. Unsent
  work survives the round trip, and returning to discussion retains its
  existing reading and filtering state.
- **AC05:** Keyboard entry leaves focus on visible Review content or its
  visible fallback, and editable multi-line fields retain their native keys.
  Home, local Review Cycles and separate composition retain existing behavior.

## Continuation

Import prepared; claim before product edits. Candidate, commands and evidence
will be recorded here and in the [checkpoint](../_checkpoints/file-change-navigation-v1/mandate-r1.md).

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** Each of the four real navigation actions, invoked from each of Overview, Discuss and Threads, reveals Review and performs exactly its ordinary step when available, through clicks, menus and admitted shortcuts. | Pending implementation | pending | — |
| **AC02:** First/last file, first/last change, no-open-comparison and no-changes cases reveal Review with the unchanged source position and exact three-second bound message. The no-tab first-file case follows “Enter Review and navigate its context.” Inactive workbenches remain disabled. | Pending implementation | pending | — |
| **AC03:** When an active tab retains an older pair than the window's pair, navigation follows that tab's material and preserves sibling tabs. Delayed restoration performs one step on the originating context; replacement or unavailable material cannot produce a step on a different pair or overwrite later navigation. | Pending implementation | pending | — |
| **AC04:** Entry restores Review's remembered drawer arrangement, including hidden drawers; repeated navigation inside Review does not reset it. Unsent work survives the round trip, and returning to discussion retains its existing reading and filtering state. | Pending implementation | pending | — |
| **AC05:** Keyboard entry leaves focus on visible Review content or its visible fallback, and editable multi-line fields retain their native keys. Home, local Review Cycles and separate composition retain existing behavior. | Pending implementation | pending | — |
