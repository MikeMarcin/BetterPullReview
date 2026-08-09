---
id: BPR-0235
title: Enforce default-deny network isolation for live mutation tests
status: Ready
rfc: RFC-BPR-0008
depends_on: [BPR-0221]
---

# Enforce default-deny network isolation for live mutation tests

The current isolation guard inventories selected port implementations,
symbols, and factory calls. It does not cover generic network calls, Python
tests, subprocesses, or production application binaries launched by tests.
That is sufficient to describe today's tree, which has no live mutation
provider, but not to discharge the standing claim after one lands.

## Required final state

- Automated tests run with network denied by default at the process boundary.
- Exactly one explicitly named live-test lane may opt in, and only after
  proving the designated repository and pull-request identity before launch.
- CTest executables, Python tests, helper subprocesses, and application-binary
  launches are all covered; a generic HTTPS or `gh` path cannot bypass the
  port-symbol scan.
- Positive counterexamples prove that direct HTTPS, a script subprocess, and a
  production-application launch fail outside the designated lane.
- BPR-0142 cannot introduce or use the live provider until this gate passes.

The existing structural scan remains useful defense in depth. It is no longer
described as complete isolation by itself.
