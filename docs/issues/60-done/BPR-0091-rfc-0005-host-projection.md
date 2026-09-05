---
schema: "implement-rfc.card.v1"
id: "BPR-0091"
title: "Implement host observation assembly and settings persistence"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0090"]
---

# Implement host observation assembly and settings persistence

Assemble bounded observations and incremental projections for the workbench, issue settings-persistence requests for drawer widths and layout state, publish changed domains, and handle cancellation and generation identity.

Stale, reordered, and duplicate terminals must not publish. Persistence restores across restart through the existing durable path.

## Implementation

Three narrow ports mirror the existing diff runtime idiom: an observation port
returning the ordered filtered actionable set, a settings port using
`std::expected`, and a projection port. `workbench_host_limits` bounds the
actionable file count, outstanding requests, and drawer width, with a
`constexpr valid()` contract and construction-time rejection.

Two ownership decisions are deliberate. The core owns drawer collapsed and
expanded state because it is semantic and gates gestures; the host owns pixel
widths because they carry none. Layout deltas therefore carry widths only and
drawer state travels in the core's own delta, so each meaning has one carrier.

The observation port reports `limit_exceeded` rather than truncating, because a
truncated order would hand the core false neighbour facts and the core trusts
those facts to decide legality.

Staleness is scoped per domain rather than by generation equality. A
generation-equality rule would let a fresh observation silently invalidate an
in-flight width persist whose payload was still correct, discarding its failure
report. `Generation` names the retained workbench incarnation for presentation
ordering; per-domain retirement is the staleness rule.

Cancellation and supersession stamp the outstanding record rather than erasing
it, because a cancelled request is still owed its terminal. Records are erased
on first delivery, so exactly-once falls out of the lookup and a duplicate
terminal takes the `unknown_request` path.

Publication diffs the retained published snapshot against current state and
engages only domains whose published meaning changed, so a bounded gesture does
not trigger a whole-state refresh.

## Direct verification

- `application_workbench_host` passes. `architecture_boundaries` and
  `architecture_boundary_contract` pass, so the host introduces no Qt or
  provider dependency.
- The 189-file format gate passes and strict `clang-tidy` with
  `--warnings-as-errors` passes on both new translation units.
- Non-vacuity, stale branch: weakening the retirement check so a superseded
  terminal publishes fails with "the superseded terminal publishes nothing".
- Non-vacuity, exactness branch: this was a real coverage gap found by
  independent verification after the slice was first reported complete.
  Weakening `domain != aDomain || context != aContext` to `&&` left the suite
  green, meaning nothing covered a terminal whose domain matched but whose
  context was forged. That is exactly the forged-session case the guard exists
  to catch. Two cases now cover it: a forged generation or session with a
  matching domain, and the mirror where the context matches exactly but the
  domain does not. Each was observed failing individually, with the case order
  swapped so the harness's exit-on-first-failure could not mask one behind the
  other. Re-applying the original `&&` weakening now fails with "forged
  generation: a forged terminal identity publishes nothing".
- Every non-vacuity result above sits after a build whose exit status was
  checked. This mattered: an early attempt used `if (false)`, which failed to
  compile under `-Werror` for unused variables, and CTest then reran the stale
  binary and reported a false pass. `${PIPESTATUS[0]}` is empty under this
  shell, which is a second quiet way to lose a build failure.

## Retained risks

- Restart persistence is proven across two host lifetimes over one shared fake
  durable store. Real durability needs the SQLite-backed adapter behind the
  settings port, which is a later card.
- Observation-order uniqueness is a port contract, not a host check. A port
  returning duplicate identities makes the host resolve the first occurrence.
  Verifying it would allocate a second copy of the bounded order per
  observation.
- A relative navigation step from a selection that has left the actionable
  order enters from the matching end rather than the vanished file's former
  neighbour. Retaining former positions is state no acceptance criterion asks
  for.
- No dedicated reentrant-projection-feedback lifecycle test exists in this
  slice, though the machinery mirrors the diff host.
- The domain-mismatch case white-box couples to request-id allocation order. If
  that order changes the case silently degrades to the `unknown_request` path.
- The suite exits on first failure, so simultaneous regressions in both
  exactness cases could mask one another.

## Result

The workbench host assembles bounded cursor facts for the core, publishes only
changed domains, persists and restores layout through a narrow port, and
refuses stale, superseded, duplicate, and forged terminals before publication.
Both disjuncts of the exactness guard now carry direct failing-if-wrong
evidence. BPR-0091 is complete.
