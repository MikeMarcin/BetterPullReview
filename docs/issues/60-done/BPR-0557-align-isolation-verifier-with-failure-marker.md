---
id: BPR-0557
title: Admit the exact inherited failure marker in the isolation verifier
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0552]
---

# Admit the exact inherited failure marker in the isolation verifier

BPR-0552 added `FAIL_REGULAR_EXPRESSION PUBLIC_PATH_FAILED` to every
ordinary isolated registration. The manifest verifier still rejects that
property on its process-exit safety tests, so the current configured manifest
fails despite using the required registration seam.

## Required final state

- Ordinary isolated safety tests accept only the exact inherited
  `PUBLIC_PATH_FAILED` failure expression. Other verdict overrides still fail.
- The designated live test retains its exact process-exit-only contract,
  including rejection of this expression.
- The verifier's positive/hostile tests and the configured manifest pass.

No launcher, live authority, sandbox policy, or product behavior changes.

## Verification — 2026-09-04

The validator allows the exact single-item failure-expression list only for
ordinary safety tests. Positive tests exercise every safety name; hostile
cases retain refusal of altered, expanded, or incorrectly typed expressions,
all other overrides, and the exact expression on the designated live lane.
The code-simplifier pass required no behavioral change.

`ctest --preset dev -R '^(test_network_isolation_manifest|test_network_isolation_structure_contract)$' --output-on-failure`
passed 2/2 in 3.61 seconds. The structure contract was rerun after test-only
formatting and passed in 0.35 seconds. The documentation gate and
`git diff --check` pass. Evidence belongs to this implementation commit;
the previous head was `5ea8d762`. The one transient log
`/tmp/bpr-0557-focused.log` was consumed and removed. No live lane ran.
