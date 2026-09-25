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
Build and CTest execution await the coordinator's build-ownership release.
