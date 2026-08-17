---
id: BPR-0061
title: Reject stale preparation terminals before forming catalog context
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0045, BPR-0060]
completed: 2026-08-01
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

## Result

All preparation terminals now use one matching helper. It rejects absent or
non-preparing pending work and mismatched request/generation values before it
constructs the catalog-dependent expected context. A current request with a
malformed authority/revision/catalog state remains an identity mismatch, so
the existing stale-versus-mismatch policy is preserved.

The catalog-conflict regression now sends prepared, failed, and cancelled
terminals from the superseded preparation while the replacement catalog load
is active. Every terminal is diagnosed as stale and leaves the exact state
unchanged.

## Evidence

- Ubuntu 26.04 x86_64, Linux 7.0.0-28, GCC 15.2, and checked Debug
  libstdc++: `core_epoch_policy` passed 1/1 after the prior deterministic GDB
  trace stopped at `current_preparation_context` with an empty catalog.
- macOS arm64 Clang, ASan/UBSan, and the Qt-free `core-only` build each passed
  `core_epoch_policy` 1/1. The complete `core-only` matrix passed 27/27 in
  41.17 seconds.
- The macOS and Ubuntu GCC architecture/dependency groups each passed 5/5:
  `core_dependency_boundary`, `core_boundary_contract`,
  `architecture_boundaries`, `architecture_boundary_contract`, and
  `git_protocol_boundary`. The target-native QEMU group completed in 460.21
  seconds.
- The focused simplification review retained the small central matching
  helper: its explicit ordering is the fail-closed constraint and avoids
  duplicating the eager-context bug across three terminal handlers.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; tracked and on-disk GitHub Actions workflow counts are
  both zero; `git diff --check` passes.
