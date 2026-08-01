---
id: BPR-0061
title: Reject stale preparation terminals before forming catalog context
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0045, BPR-0060]
---

# Reject stale preparation terminals before forming catalog context

Repair the core transition defect exposed by BPR-0050's resumed Ubuntu 26.04
x86_64 GCC 15.2 matrix. After a preparation reports `catalog_conflict`, the
core clears the catalog and starts a fresh catalog load. A later terminal from
the superseded preparation is stale, but the preparation handlers currently
form an expected preparation context before rejecting the phase mismatch.
That eager context construction dereferences the intentionally absent catalog;
checked libstdc++ aborts while libc++ happened to leave readable optional
storage.

Centralize preparation-terminal matching so request/generation/phase are
rejected before catalog-dependent context construction. Preserve the existing
distinction between stale terminals and same-request identity mismatches.
Directly rerun `core_epoch_policy` under GCC x86_64, Clang arm64, and
ASan/UBSan, cover prepared/failed/cancelled terminals after conflict reload,
run architecture gates and `git diff --check`, and resume BPR-0050 only after
the remediation is committed.
