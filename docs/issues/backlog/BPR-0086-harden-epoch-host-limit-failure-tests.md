---
id: BPR-0086
title: Harden epoch host limit and allocation-failure tests
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0068]
---

# Harden epoch host limit and allocation-failure tests

Add a deterministic allocation-failure seam around epoch-host admission and
exercise rollback at each recoverable allocation boundary. Extend the public
limit-validation table with zero deferred-ingress capacity and values above
both hard maxima. Require exact failure diagnostics, zero partial admission,
lease revocation, unchanged active authority, and shutdown quiescence.

This is nonblocking hardening identified by the BPR-0068 closeout review. The
current implementation has direct source review and executor-refusal evidence
for allocation-safe rollback plus positive and job-limit boundary coverage;
this card does not reopen BPR-0068 or block RFC-BPR-0003 closeout.
