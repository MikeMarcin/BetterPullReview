---
schema: "implement-rfc.card.v1"
id: "BPR-1599"
title: "A modality chosen during loading survives the pull request's arrival"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner-approved ordered handoff, 2026-10-17","reservation":"docs/issues/_reservations/BPR-1599--BPR-1599--c4402b131ede.json","related_cards":["BPR-0651","BPR-0661","BPR-0863","BPR-1588"]}
acceptance: []
---

# BPR-1599: A modality chosen during loading survives the pull request's arrival

The private-review course opens a pull request, presses Review once through the
same QAction a reviewer uses, and waits for the comparison and drawers. If the
press happens while the pull request loads, its eventual arrival chooses the
opening preference again. Overview replaces Review, and the course waits for a
choice it already made. A person can hit the same race and need a second press.
This is a product presentation bug; retrying the gesture in the fixture would
hide it.

## Acceptance

- AC1: A reviewer’s modality choice during the initial load survives a cached
  overview and the final pull request. A new load starts without the previous
  load’s choice, and the normal opening preference still applies when no choice
  was made.
- AC2: The deterministic regression test fails against the current-main product
  code, and passes with the fix restored. The original native private-review
  course is also checked as a negative control, with its actual result recorded.
- AC3: `application_private_review_native`, `application_private_review_file`
  and `application_epoch_portable_occurrence_two_catalogues` pass. The composed
  Qt tests at 100%, 125% and 200%, and the existing first-Review public-path
  contract, preserve their existing behavior. Format22, focused tidy22 and the
  documentation gate pass.
- AC4: The source-history cause, inherited WIP state, tested revision and
  negative-control restoration are recorded before the ordered landing.

## Cause and source history

The inherited working tree contained the fix, not the temporary reversion.
It was preserved in `5721c7061` and main through [[BPR-1598]] (`4305b7210`)
merged at `68c701ea5`.

The overriding arrival statement is older than the suspected October cards:
`252c825e91` ([[BPR-0651]], September 7) introduced the arrival’s unconditional
Overview entry; `c76ee6aa25` ([[BPR-0661]]) changed it to the configured opening
modality. `9c670d886` (September 19, “Open the review overview immediately with
bounded read progress”) exposed the workbench and loading Overview before the
request completes, while leaving that later reset intact. This is the loading
flow change that exposes a visible choice to the race. The one-press fixture
step was centralized in `7c6ba1a51` ([[BPR-0863]]).

This is a source-history attribution, not a full historical-build bisect.
The BPR-1588 first-Review commits are not ancestors of the supplied failing
base `6af3e4021`; they cannot explain that original failure. The focused
negative control checks the actual current-main reset rather than inferring a
failure from a commit name.

## Change

The window retains a choice made while an initial pull request is loading.
Its own loading, cached and final opening entries use that choice if present
without recording their defaults as user input. Completion, failure,
cancellation and a new load clear the temporary choice. The existing core
modality intent still controls drawer state.

The regression course presses the real strip action between loading and
arrival, checks a cached intermediate projection, and reuses a window to prove
that a later load does not inherit the choice. The no-choice control still
opens on Overview. The frontend-design skill was loaded for this Qt surface
work; the accepted design is preserved.

No unique `src/app` change is needed. Its composition fingerprints and the
passing composition evidence from [[BPR-1598]] remain applicable.

## Verification

The current-main negative control builds. The composed Qt row fails exactly at
“Review chosen while the pull request loaded is still Review when it arrives”.
The native course passes in 7.09 seconds on that run: the original asynchronous
stall did not recur with that arrival timing. The deterministic test establishes
the product failure independently of that timing. Both source files are restored
from exact saved bytes and verified by SHA-256 before the positive rebuild.

Positive tests and focused lint are in progress. The documentation gate passes.
The full integrated suite remains assigned to item 9 of the ordered handoff.
