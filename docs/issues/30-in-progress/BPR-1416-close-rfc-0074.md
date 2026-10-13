---
schema: "implement-rfc.card.v1"
id: "BPR-1416"
title: "Verify and close RFC-BPR-0074"
status: "In Progress"
rfcs: ["RFC-BPR-0074"]
depends_on: ["BPR-1415"]
closeout_for: ["RFC-BPR-0074"]
priority: "P1"
metadata: {"mandate_revision":"header-source-navigation-r4","milestone_plan":{"id":"header-source-navigation-v1","path":"docs/rfcs/plans/header-source-navigation-v1/milestone-plan.json"},"rfc_base_sha":"6a310fc57ae2d52360bfec252cf25aa1350d8aa2","rfc_criteria":["AC01","AC02","AC03","AC04","AC05","AC06"]}
---

# BPR-1416: Verify and close RFC-BPR-0074

RFC base: `6a310fc57ae2d52360bfec252cf25aa1350d8aa2`.

Owner amendment r4 settles Q1; no chooser or cycling.

## Acceptance criteria

- **AC01:** In the composed PR review, the shortcut jumps from
  `src/widget.cpp` to the sole `inc/widget.h` counterpart and back, with both
  split and unified presentation. Supported suffix and case behavior follows
  “Match files already in the review”; an unrelated or outside-review file
  cannot become a target.
- **AC02:** The agreed multiple-candidate behavior distinguishes same-folder
  candidates from cross-folder candidates and handles ambiguity predictably.
  A unique closest candidate jumps; equal best scores change no reading state.
  The directory examples and same-directory preference follow owner amendment r4.
- **AC03:** A counterpart hidden by the Files filter or a private receipt
  remains reachable without changing the filter, receipt or actionable order.
  No-match, unsupported, no-selection and unavailable-catalogue states produce
  the distinct behavior specified under “Command and keyboard behavior.”
- **AC04:** A jump retains an active tab's older comparison and leaves sibling
  tabs unchanged. A source/tab/PR/catalogue/comparison change before commitment
  rejects the old resolution, and a late diff cannot replace the latest selection.
  A missing target at that comparison is reported without fallback.
- **AC05:** The real QAction and physical key-event path reach the same
  selection operation, with a unique assembled shortcut and accessible menu
  and status text. Editable Option/Alt input and inactive modalities
  retain their behavior; successful navigation keeps focus in the source comparison.
- **AC06:** A large accepted review performs indexed counterpart lookup without
  a full-catalogue scan per gesture, speculative candidate reads or model reset.
  Core matching is verifiable without QApplication, and the existing
  architecture boundaries continue to pass.

## Verification and execution record

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** In the composed PR review, the shortcut jumps from `src/widget.cpp` to the sole `inc/widget.h` counterpart and back, with both split and unified presentation. Supported suffix and case behavior follows “Match files already in the review”; an unrelated or outside-review file cannot become a target. | [Criterion ledger](../../verification/RFC-BPR-0074.md) and composed/core evidence | pass | `53988db27a909baebf1af4546132610ff7e59052` |
| **AC02:** The agreed multiple-candidate behavior distinguishes same-folder candidates from cross-folder candidates and handles ambiguity predictably. A unique closest candidate jumps; equal best scores change no reading state. The directory examples and same-directory preference follow owner amendment r4. | [Criterion ledger](../../verification/RFC-BPR-0074.md) and composed/core evidence | pass | `53988db27a909baebf1af4546132610ff7e59052` |
| **AC03:** A counterpart hidden by the Files filter or a private receipt remains reachable without changing the filter, receipt or actionable order. No-match, unsupported, no-selection and unavailable-catalogue states produce the distinct behavior specified under “Command and keyboard behavior.” | [Criterion ledger](../../verification/RFC-BPR-0074.md) and composed/core evidence | pass | `53988db27a909baebf1af4546132610ff7e59052` |
| **AC04:** A jump retains an active tab's older comparison and leaves sibling tabs unchanged. A source/tab/PR/catalogue/comparison change before commitment rejects the old resolution, and a late diff cannot replace the latest selection. A missing target at that comparison is reported without fallback. | [Criterion ledger](../../verification/RFC-BPR-0074.md) and composed/core evidence | pass | `53988db27a909baebf1af4546132610ff7e59052` |
| **AC05:** The real QAction and physical key-event path reach the same selection operation, with a unique assembled shortcut and accessible menu and status text. Editable Option/Alt input and inactive modalities retain their behavior; successful navigation keeps focus in the source comparison. | [Criterion ledger](../../verification/RFC-BPR-0074.md) and composed/core evidence | pass | `53988db27a909baebf1af4546132610ff7e59052` |
| **AC06:** A large accepted review performs indexed counterpart lookup without a full-catalogue scan per gesture, speculative candidate reads or model reset. Core matching is verifiable without QApplication, and the existing architecture boundaries continue to pass. | [Criterion ledger](../../verification/RFC-BPR-0074.md) and composed/core evidence | pass | `53988db27a909baebf1af4546132610ff7e59052` |

## Baseline and upkeep

The [scoped baseline review](../_checkpoints/header-source-navigation-v1/m2-baseline-review.json) accounts for M1 and M2 together, preserving owner amendment r4. All six amended criteria pass through the final composed/core evidence. Full composition tidy remains explicitly qualified for unchanged baseline naming violations; no explicit human or independent criterion exists.

## Automated milestone outcome

Product `720c52c69` and final key/input course `53988db27` are integrated on main. [M2 verification](../_checkpoints/header-source-navigation-v1/m2-verification.json) records exact candidates/hashes, direct tests, initial failures/corrections and known limits. The [renewed binding](../_checkpoints/header-source-navigation-v1/m2-binding-renewed.json) changes only canonical documentation syntax, preserving six criteria and both milestone exits.
