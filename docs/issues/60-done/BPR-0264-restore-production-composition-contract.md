---
schema: "implement-rfc.card.v1"
id: "BPR-0264"
title: "Restore the production composition contract after recovery repair"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0263"]
---

# Restore the production composition contract after recovery repair

The complete macOS `dev` suite exposed two regressions outside BPR-0263's
focused closeout selector. The production-composition test still calls the
testing one-shot grant factory with its older signature, and the tests-disabled
Release binary fails the composed-host inventory on a recovery diagnostic
anchor. The focused recovery matrix cannot substitute for this public
composition evidence.

## Required final state

- The language-sealing contract uses the exact current testing-grant API and
  continues to prove that production translation units cannot call or forge
  either testing or attended authority issuers.
- The tests-disabled Release application passes the production composed-host
  inventory without hiding a fixture-only provider, credential seam, or
  testing grant, while retaining the generic reconciliation decision used by
  the live post-mutation projection.
- Any verifier change distinguishes a legitimate production recovery contract
  from a fixture-only implementation by source and selected-symbol evidence; it
  must not merely delete or weaken a failing forbidden anchor.
- The isolated `application_composition_contract` passes, followed by the
  remaining complete macOS `dev` suite, repository-wide format/tidy,
  documentation, and diff gates.

## Non-goals

- Do not change mutation recovery, replay, or supervisor authority semantics.
- Do not execute the attended live lane or use a credential, approval, network
  target, or real GitHub mutation.
- Do not claim target-native Windows, Linux, or attended evidence.

## Verification

- Rebuild the affected composition/application target and run
  `application_composition_contract` in isolation.
- Run the complete `dev` CTest graph from the first previously unexecuted test
  through completion, then run the full selector if the repair can affect an
  earlier surface.
- Run repository-wide `coding-format-check` and `coding-tidy`, the
  documentation gate and tests, and `git diff --check`.
- Review the final diff independently as capability-critical production
  composition and mutation-authority work.

## Closeout

The language-sealing fixture and hostile probes now call the current
target-bound one-shot testing-grant factory. The composed-host verifier treats
generic reconciliation as a shared production/fixture decision, retains
opposite live-versus-fixture provider inventories, and explicitly requires or
forbids both testing-grant factories by profile. The latter closes the
independent review's non-blocking observation that a future source split could
otherwise expose the one-shot factory without tripping the production symbol
denylist.

Direct macOS evidence passed the 42-test verifier unit suite, both composed
application host profiles, the language-sealing contract, and the fresh
tests-disabled Release `application_composition_contract` after the verifier
hardening. Repository format, focused tidy, documentation, and diff gates
passed for the implementation slice. An independent capability-critical
review returned `ACCEPT` with no blocking or verification finding.

The remaining complete-suite failures were reproduced outside this change and
split by causal surface into BPR-0265 through BPR-0268. They do not invalidate
the exact composition inventory repaired here and make no live, attended,
Windows, or Linux claim.
