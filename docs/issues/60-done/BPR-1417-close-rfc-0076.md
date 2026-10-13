---
schema: "implement-rfc.card.v1"
id: "BPR-1417"
title: "Verify and close RFC-BPR-0076"
status: "Done"
rfcs: ["RFC-BPR-0076"]
depends_on: []
closeout_for: ["RFC-BPR-0076"]
metadata: {"milestone_plan":{"id":"file-change-navigation-v1","path":"docs/rfcs/plans/file-change-navigation-v1/milestone-plan.json"},"mandate_revision":"file-change-navigation-r1","rfc_base_sha":"d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"aa3d71fe470627e1e78488eb51a9114a788f33bd","evidence":{"kind":"observation","detail":"The composed Git/cache review course invokes all four retained QActions from Overview, Discuss and Threads through toolbar clicks, View menu activation and admitted physical key events; each of 36 entries reveals Review and performs its ordinary step once.","refs":["docs/issues/_checkpoints/file-change-navigation-v1/m1-integrated-checks.json","docs/issues/_checkpoints/file-change-navigation-v1/m1-verification.json"]}},{"criterion":"AC02","outcome":"pass","revision":"aa3d71fe470627e1e78488eb51a9114a788f33bd","evidence":{"kind":"observation","detail":"The composed course covers first/last file and change, no-open-comparison, no-changes and no-tab first-file entry; bounds retain exact source/reading position and the existing three-second message, while inactive actions remain disabled.","refs":["docs/issues/_checkpoints/file-change-navigation-v1/m1-integrated-checks.json","docs/issues/_checkpoints/file-change-navigation-v1/m1-verification.json"]}},{"criterion":"AC03","outcome":"pass","revision":"aa3d71fe470627e1e78488eb51a9114a788f33bd","evidence":{"kind":"observation","detail":"The exact older/reverse active-tab pair is restored before navigation. Delayed file/change entry runs once; unavailable material and later command/tab/file/comparison/Home/composer gestures cancel without retargeting. The final sibling assertion restores its unchanged semantic reading position.","refs":["docs/issues/_checkpoints/file-change-navigation-v1/m1-integrated-checks.json","docs/issues/_checkpoints/file-change-navigation-v1/m1-verification.json"]}},{"criterion":"AC04","outcome":"pass","revision":"aa3d71fe470627e1e78488eb51a9114a788f33bd","evidence":{"kind":"observation","detail":"Hidden and remembered Review drawers restore on entry and retain later changes inside Review. The composed summary draft survives the round trip; the full Qt window course verifies discussion selection, reading, filters, replies and existing draft retention.","refs":["docs/issues/_checkpoints/file-change-navigation-v1/m1-integrated-checks.json","docs/issues/_checkpoints/file-change-navigation-v1/m1-verification.json"]}},{"criterion":"AC05","outcome":"pass","revision":"aa3d71fe470627e1e78488eb51a9114a788f33bd","evidence":{"kind":"observation","detail":"Physical key entry focuses visible source after exact asynchronous restoration or the originating visible arrow for empty/unavailable Review. Editable multi-line keys retain their native text/caret behavior; Home, local-cycle and composer behavior retain existing checks.","refs":["docs/issues/_checkpoints/file-change-navigation-v1/m1-integrated-checks.json","docs/issues/_checkpoints/file-change-navigation-v1/m1-verification.json"]}}]
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

## Execution record

AC01–AC05 pass on `aa3d71fe470627e1e78488eb51a9114a788f33bd`.
Product commit `40639f2f6` and the sibling-reading assertion `aa3d71fe4` are
integrated on main, preserving the title-row work through `a1d43c797`.
The [verification ledger](../../verification/RFC-BPR-0076.md) and
[checkpoint](../_checkpoints/file-change-navigation-v1/continuation.md) bind
all criteria to direct composed/window evidence and retain the negative control.
The final combined pass is 5/5 (74.815 seconds). New navigation code is tidy-clean;
14 composition and 6 broad Qt test diagnostics remain unchanged baseline findings.
No required human/native/independent criterion or review server remains.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** Each of the four real navigation actions, invoked from each of Overview, Discuss and Threads, reveals Review and performs exactly its ordinary step when available, through clicks, menus and admitted shortcuts. | [Criterion ledger](../../verification/RFC-BPR-0076.md) and exact combined course | pass | `aa3d71fe470627e1e78488eb51a9114a788f33bd` |
| **AC02:** First/last file, first/last change, no-open-comparison and no-changes cases reveal Review with the unchanged source position and exact three-second bound message. The no-tab first-file case follows “Enter Review and navigate its context.” Inactive workbenches remain disabled. | [Criterion ledger](../../verification/RFC-BPR-0076.md) and exact combined course | pass | `aa3d71fe470627e1e78488eb51a9114a788f33bd` |
| **AC03:** When an active tab retains an older pair than the window's pair, navigation follows that tab's material and preserves sibling tabs. Delayed restoration performs one step on the originating context; replacement or unavailable material cannot produce a step on a different pair or overwrite later navigation. | [Criterion ledger](../../verification/RFC-BPR-0076.md) and exact combined course | pass | `aa3d71fe470627e1e78488eb51a9114a788f33bd` |
| **AC04:** Entry restores Review's remembered drawer arrangement, including hidden drawers; repeated navigation inside Review does not reset it. Unsent work survives the round trip, and returning to discussion retains its existing reading and filtering state. | [Criterion ledger](../../verification/RFC-BPR-0076.md) and exact combined course | pass | `aa3d71fe470627e1e78488eb51a9114a788f33bd` |
| **AC05:** Keyboard entry leaves focus on visible Review content or its visible fallback, and editable multi-line fields retain their native keys. Home, local Review Cycles and separate composition retain existing behavior. | [Criterion ledger](../../verification/RFC-BPR-0076.md) and exact combined course | pass | `aa3d71fe470627e1e78488eb51a9114a788f33bd` |
