---
schema: "implement-rfc.card.v1"
id: "BPR-0898"
title: "A node the account cannot see and a genuinely broken server are reported identically"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-006 worker on BPR-0879","found_at":"5c8ee790","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0898: A node the account cannot see and a genuinely broken server are reported identically

## Defect

`map_contract_failure` folds `malformed_response` into `unavailable`. Both
refuse, so nothing unsafe happens — this is a diagnostic loss, not a boundary
gap, which is why it is P3.

But the two facts are different in the way that matters when something goes
wrong. "The account cannot see that node" is an authority or staleness answer
the person can act on. "The server sent something I could not parse" is an
availability or compatibility answer they cannot. Reporting both as
`unavailable` costs exactly the distinction an investigation needs, and
[[BPR-0879]] is an example of the kind of investigation that wants it: the new
pre-flight observation returns nothing for an invisible thread, and nothing for
a broken server, and the caller cannot tell which it met.

## Scope

- Separate the two at the point where the information still exists, and check
  what the callers upstream do with `unavailable` before adding a term — a new
  failure value that every caller maps straight back to "unavailable" buys
  nothing.
- Prefer widening the diagnostic over widening the refusal: both cases must
  still refuse.
- Check whether the same fold appears elsewhere in the contract's failure
  mapping; a single-site fix that leaves the same shape twice over is not worth
  a card.

## Acceptance

- An invisible node and an unparseable response are distinguishable in what the
  product reports, and both still refuse.
- A contract row covers each, so the distinction cannot silently collapse again.
- No caller's refusal behaviour changes.

## Assignment and plan — 2026-09-25

Run `bpr0898-20260925-parallel`, segment `s1`, mandate
`owner-20260925-unattended-parallel`; isolated branch
`codex/bpr0898-diagnostic`, base `fc4b39e8`. The bounded plan and current
continuation are in [the card checkpoint](../_checkpoints/BPR-0898/checkpoint.json).
The installed implement-rfc plan discovery returned absent and its checkpoint
binder requires an adopted milestone. This independent card has no RFC; the
coordinator authorized the narrow board claim/finish procedure. No RFC is invented.

Preserve null-node versus malformed-response diagnostics in thread preflight
and reply reconciliation, carry them to product presentation, and retain all
refusal and retry behavior. Verify the parser, public mutation session and
presentation with recorded fixtures; no live GitHub mutation is authorized.
The coordinator released build ownership after RFC-0052's resource-sensitive checks;
the focused isolated verification below is complete.

## Implementation and verification — 2026-09-25

Source candidate `9a2a8dd3` plus the isolated retry hunk `8a250c39`, verified
at branch revision `664b544f`. An explicit null thread node now reports
`target_unavailable`; invalid JSON or required structure reports
`malformed_response`. Both the thread preflight and reply reconciliation keep
the distinction. The native HTTPS mutation mapping preserves malformed responses
as well. The host/coordinator forward the typed failure, resolution reports its
stable name, and the real Qt draft editor gives distinct access and server-response
messages. The app retains the existing safe retry identity for both new values.

| Criterion | Focused evidence | Result |
|---|---|---|
| Different product diagnostics; both refuse | Contract and mutation-session rows, coordinator diagnostic names, actual Qt editor label assertions | Pass |
| A contract row for each | Null node, invalid JSON, absent/mistyped/incomplete node; preflight and reply baseline | Pass |
| Refusal behavior retained | Focused refusal/recovery suites pass; combined app retry/composition checks remain with the coordinator | Pending integration |

The six focused test executables build successfully on macOS arm64 with
AppleClang 21.0.0 and Qt 6.11.2. The ten serial CTest rows listed in the
checkpoint pass (124.61 seconds), including both architecture gates, native
HTTPS loopback behavior, SQLite-backed restart recovery and the Qt editor.
The 11 changed C++ files pass clang-format 23.1.1. The documentation gate and
all 64 documentation-verifier tests pass.

The raw nine-translation-unit clang-tidy 23.1.1 command exits 1 on 13 existing
diagnostics: Glaze's lowercase option members, the unchanged Home uint8_t
bitwise expression, the existing `exact_current` local name, and existing
`#pragma once` headers. Every reported line is unchanged and present at base
`fc4b39e8`; none intersects a changed line. The checkpoint names every finding.
No rule was disabled and no unrelated source was changed. This is a failing
raw tidy gate, not a claimed clean run.

Self-review confirmed that enum values are appended, required node fields remain
required, authority mismatches keep their prior refusal, and native transmission
classification and durable recovery remain intact. Documentation impact is
recorded in [Review Round Trip](../../architecture/review-round-trip.md).
Core decisions, endpoints, credentials, persistence and automatic retries have
no semantic change.

## Integration handoff

The card remains **In Progress** by coordinator direction. Its final primary
candidate must include RFC-0052's separate composition changes. The coordinator
owns the source-digest reseals in
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`,
`application_composition_contract`, and
`application_review_round_trip_composition_contract`; these have not been
claimed passing on this branch. Disposition the baseline tidy findings before
final card acceptance. The app-only commit names the exact retry predicate to
reconcile in `github_application.cpp`.

All task-owned commands are finished. No live GitHub mutation, attended server,
push, or primary-checkout edit occurred. The durable checkpoint records source
and base identities, changed paths, full commands, evidence, remaining gates
and the coordinator's next action.
