---
id: BPR-0557
title: Admit the exact inherited failure marker in the isolation verifier
status: In Progress
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
