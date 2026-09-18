---
schema: "implement-rfc.card.v1"
id: "BPR-0921"
title: "Investigate No updates loaded after opening a PR"
status: "Done"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0029", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
acceptance: [{"criterion":"Reproduce initial PR open with recorded GHES/CLI fixtures and trace the first missing or refused update-history transition.","outcome":"pass","revision":"5b95485c9432cd365e8cedffc3aee0ea7d52b164","evidence":{"kind":"observation","detail":"The composed application course traced the refused smart-Git acquire into an unavailable epoch state while preserving exact request authority and recording the live-cause limitation."}},{"criterion":"For available history, display the actual update sequence after PR load; loading, genuine empty, unavailable and failed states are distinguishable.","outcome":"pass","revision":"5b95485c9432cd365e8cedffc3aee0ea7d52b164","evidence":{"kind":"command","detail":"application_setup_destination_public_path passed with one retained initial epoch followed by a reuse launch showing two real epochs and the updated head; existing focused epoch and overview contracts also passed."}},{"criterion":"Refresh and PR/account replacement cannot retain another PR's epochs or install stale history; record whether the source-loading failure shares the cause.","outcome":"pass","revision":"5b95485c9432cd365e8cedffc3aee0ea7d52b164","evidence":{"kind":"artifact","detail":"milestone-source-update-review-entry-evidence.md classifies the shared recorded acquisition boundary without inferring the owner's cause; epoch, multi-window, overview-since-review, and workbench composition selectors passed."}}]
---

# Investigate No updates loaded after opening a PR

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-04. Earlier RFC references retain their governing contracts.

## Owner feedback

“Updates says ‘No updates loaded’ at the top where epochs should be.”

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Trace the first loaded PR through history acquisition, persistence, update
identity and epoch-line projection. Determine whether history was never
requested, was refused/failed, is pending, or exists but is not projected.
Do not infer an empty remote history from the displayed text, and do not invent
updates to fill the strip. Investigate correlation with [[BPR-0922]] without
assuming the two symptoms share a cause.

Starting points: `src/adapters/qt/src/epoch_line.cpp`,
`src/adapters/qt/src/epoch_history_model.cpp`, and the composed history host.

## Acceptance

- Reproduce initial PR open with recorded GHES/CLI fixtures and trace the first missing or refused update-history transition.
- For available history, display the actual update sequence after PR load; loading, genuine empty, unavailable and failed states are distinguishable.
- Refresh and PR/account replacement cannot retain another PR’s epochs or install stale history; record whether the source-loading failure shares the cause.

## Status

Accepted at source revision `5b95485c9432cd365e8cedffc3aee0ea7d52b164`.
The recorded CLI/GHES refusal identifies an unavailable acquisition boundary;
supported single/reuse launches over one durable store retain and project the
actual two-epoch sequence. No empty history is invented, and the owner's live
cause remains unobserved.
