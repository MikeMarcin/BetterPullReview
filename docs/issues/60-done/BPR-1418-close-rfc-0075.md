---
schema: "implement-rfc.card.v1"
id: "BPR-1418"
title: "Verify and close RFC-BPR-0075"
status: "Done"
rfcs: ["RFC-BPR-0075"]
depends_on: []
closeout_for: ["RFC-BPR-0075"]
metadata: {"milestone_plan":{"id":"title-row-publication-v1","path":"docs/rfcs/plans/title-row-publication-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"0f039e775900153001b4c7cce04431a5d4278d86","evidence":{"kind":"observation","detail":"Real public workbench_window publication enters loading with two received-page observations and normal core commands before accepting content. Identity and adjacent browser button appear at unchanged 1920/1280/1100/900 widths, before resize/Home gestures. New assertion fails against unchanged baseline product; existing direct-acceptance control passes.","refs":["docs/issues/_checkpoints/title-row-publication-v1/final-title-row.json","docs/issues/_checkpoints/title-row-publication-v1/candidate-verification.json","docs/issues/_checkpoints/title-row-publication-v1/render-observation.json","docs/issues/_checkpoints/title-row-publication-v1/quality-checks.json"]}},{"criterion":"AC02","outcome":"pass","revision":"0f039e775900153001b4c7cce04431a5d4278d86","evidence":{"kind":"observation","detail":"Same-PR title/context replacement, identical republication, new-authority PR replacement, removal of browser context and clear settle at unchanged width. Click dispatch names only the latest accepted markdown context; clear hides and empties identity and withdraws the browser action.","refs":["docs/issues/_checkpoints/title-row-publication-v1/final-title-row.json","docs/issues/_checkpoints/title-row-publication-v1/candidate-verification.json","docs/issues/_checkpoints/title-row-publication-v1/render-observation.json","docs/issues/_checkpoints/title-row-publication-v1/quality-checks.json"]}},{"criterion":"AC03","outcome":"pass","revision":"0f039e775900153001b4c7cce04431a5d4278d86","evidence":{"kind":"observation","detail":"Preserved content keeps Home, Discuss and the actual selected Files index/identity plus position. A retained local cycle keeps its own identity without browser action; return shows the updated PR row immediately, and a new PR replaces the cycle normally. Existing composed personal-discovery and Markdown/browser paths pass on unchanged product source.","refs":["docs/issues/_checkpoints/title-row-publication-v1/final-title-row.json","docs/issues/_checkpoints/title-row-publication-v1/candidate-verification.json","docs/issues/_checkpoints/title-row-publication-v1/render-observation.json","docs/issues/_checkpoints/title-row-publication-v1/quality-checks.json"]}},{"criterion":"AC04","outcome":"pass","revision":"0f039e775900153001b4c7cce04431a5d4278d86","evidence":{"kind":"observation","detail":"Registered 100/125/200 scale title-row courses pass, including exact order, accessibility, adjacent button, roomy/compact/macOS 900-point floor and pixel-by-pixel width yield sweep without overflow. Repeated same-content publication has identical geometry. Offscreen accepted-row captures generated at four widths per scale and inspected at roomy/floor sizes.","refs":["docs/issues/_checkpoints/title-row-publication-v1/final-title-row.json","docs/issues/_checkpoints/title-row-publication-v1/candidate-verification.json","docs/issues/_checkpoints/title-row-publication-v1/render-observation.json","docs/issues/_checkpoints/title-row-publication-v1/quality-checks.json"]}}]
---

# BPR-1418: Verify and close RFC-BPR-0075

Mandate `title-row-publication-r1` authorizes full implementation of
[RFC-BPR-0075](../../rfcs/RFC-BPR-0075-title-row-publication.md), adopted from
RFC-DRAFT-0014. 

RFC base: `d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1`.

## Acceptance

- AC01: Loading-to-acceptance and direct acceptance show the current identity and adjacent browser button without resize or Home round trip.
- AC02: Replacement, republication and clear use only the current text/browser context; absent context hides the button.
- AC03: Preserved Home, local-cycle, modality and selected-file navigation remains intact; returning presents the current row.
- AC04: Existing order, adjacency, accessibility and width yield pass rendered checks at 100%, 125% and 200%, roomy/compact/platform floor widths, without overflow or oscillation.

## Current frontier

AC01–AC04 pass on `0f039e775900153001b4c7cce04431a5d4278d86`; the
[verification ledger](../../verification/RFC-BPR-0075.md) records direct public
window checks, rendered scale/width checks, composed-path evidence and self-review.
Three broad composed-GitHub rows retain an unrelated baseline warning-length failure;
the baseline control reproduces it, and candidate source is restored byte for byte.
No new human criterion remains. All task commands have finished; temporary inspection
files are removed at closeout. Original unrelated dirty files and drafts remain.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** A first read that exposes its loading surface before acceptance, with normally offered workbench commands retained, shows the accepted identity and adjacent Open on GitHub button at a fixed roomy width. The observation occurs before any resize or Home round trip and fails against the investigated implementation; direct acceptance also succeeds. | [Criterion ledger](../../verification/RFC-BPR-0075.md) and public/render evidence | pass | `0f039e775900153001b4c7cce04431a5d4278d86` |
| **AC02:** Replacement, same-PR republication and clearing settle the row without a geometry change. Its text and browser command refer to the latest accepted publication only, and a PR without a browser context offers no title-row browser button. | [Criterion ledger](../../verification/RFC-BPR-0075.md) and public/render evidence | pass | `0f039e775900153001b4c7cce04431a5d4278d86` |
| **AC03:** Content publication preserves the defined Home, local-cycle and navigation behavior. Returning to a retained PR presents its current row immediately, without a second corrective gesture. | [Criterion ledger](../../verification/RFC-BPR-0075.md) and public/render evidence | pass | `0f039e775900153001b4c7cce04431a5d4278d86` |
| **AC04:** The corrected transition retains existing title-row order, adjacency, accessibility and width-yield behavior at the registered 100%, 125% and 200% scales; roomy, compact and platform-floor widths obey the existing contract without toolbar overflow or visibility oscillation. | [Criterion ledger](../../verification/RFC-BPR-0075.md) and public/render evidence | pass | `0f039e775900153001b4c7cce04431a5d4278d86` |
