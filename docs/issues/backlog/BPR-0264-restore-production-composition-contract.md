---
id: BPR-0264
title: Restore the production composition contract after recovery repair
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0263]
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
  inventory without hiding a fixture-only provider, reconciliation decision,
  credential seam, or testing grant in production.
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
