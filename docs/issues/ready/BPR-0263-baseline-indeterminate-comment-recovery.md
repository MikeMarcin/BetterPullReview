---
id: BPR-0263
title: Baseline indeterminate comment recovery
status: Ready
rfc: RFC-BPR-0008
depends_on: [BPR-0255]
---

# Baseline indeterminate comment recovery

The BPR-0256 critical review proved that reconciliation can adopt a comment
that already matched actor, anchor, path, commit, line, and body before the
attempt. If POST then fails without transmission, the existing comment is
currently recorded as the new attempt's success.

This recovery ambiguity is independent of approval replay storage and
supervisor/capsule provenance. BPR-0255 already supplies the asynchronous host
and exact request identity that this repair extends, so BPR-0261 and BPR-0262
are not implementation prerequisites.

## Required final state

- Before durable admission and POST, observe a bounded exact-comment baseline
  without weakening the rule that viewer/target/head revalidation is the final
  authority sequence before POST.
- Persist the baseline identities with the exact unresolved request so restart
  recovery has the same evidence as the originating process.
- Post-attempt and restart reconciliation may confirm success only from exactly
  one matching server comment identity absent from the durable baseline.
- A pre-existing exact comment alone never resolves a new attempt. Multiple new
  candidates, pagination/limit ambiguity, missing baseline integrity, and
  definitely-no-transmission failure remain unresolved or typed refusal without
  another POST.
- Recovery schema/migration, capacity bounds, canonical fingerprints, and
  projection refresh remain fail-closed and backward compatible with explicitly
  supported stored versions.

## Non-goals

- Do not add a hidden comment marker, footer, private GitHub metadata, or a
  second mutation path.
- Do not weaken exact actor/target/head/anchor/body matching or permit automatic
  resubmission of an indeterminate write.
- Do not execute the designated live lane.

## Verification

- Provider tests preseed an exact C0, force a no-transmission POST failure,
  return only C0, and require no success; then add one new C1 and require only
  C1 to resolve. Ambiguous new candidates fail closed.
- Two-process SQLite recovery preserves the baseline across reopen and performs
  GET-only resolution with zero second POST.
- Provider, SQLite, application composition, architecture, format/tidy,
  documentation, and diff gates pass.
