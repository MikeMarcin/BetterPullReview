---
id: BPR-0054
title: Verify and close RFC-BPR-0003
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0035, BPR-0048, BPR-0049, BPR-0050, BPR-0051, BPR-0052, BPR-0053, BPR-0055, BPR-0056, BPR-0059, BPR-0067, BPR-0068, BPR-0069, BPR-0070]
---

# Verify and close RFC-BPR-0003

Replay the complete claim and architecture-conformance matrices from clean
state. Directly inspect core policy, host staging and persistence, real
installed-Git and native smart transport, cache/ref/catalog recovery,
force-push retention, exact aliases/selections, rendered Qt behavior,
security boundaries, package/runtime dependencies, and platform-native
evidence.

Resolve every documentation-ledger row, perform a skeptical final diff review,
move every RFC card to `done/`, and mark RFC-BPR-0003 Auto-Verified only when
all claims agree and RFC-BPR-0002 has already closed in canonical order.

Closeout requires fresh `dev`, `core-only`, sanitizer, tests-disabled Release,
macOS, Linux, and real Windows matrices; safe fixture and hostile-canary
evidence; architecture/runtime/binary-dependency gates; immutable-base
documentation review; original-pack validation; workflow absence; and
`git diff --check`.

## Closeout preparation

- The BPR-0069 replay supersedes the earlier snapshot. On macOS 15.7.7 build
  24G720 arm64 with Qt 6.11.1, clean reconfigured trees pass 88/88 on `dev` in
  684.92 seconds and 33/33 on `core-only` in 79.36 seconds. Both counts grew
  with the RFC-BPR-0004 pack, BPR-0067, BPR-0068, and BPR-0085.
- The documentation gate, architecture-boundary and boundary-contract suites,
  documentation-structure and issue-graph contracts, platform-configuration
  contract, 181-file format gate, strict first-party `clang-tidy` audit,
  `BINARY_DEPENDENCIES_OK`, `RUNTIME_SURFACE_OK`, original source-pack
  validator, workflow-absence scans, and diff hygiene are green. GitHub Actions
  workflow counts remain 0/0.
- The focused sanitizer matrix, the 100,000-epoch SQLite stress binary, and the
  tests-disabled Qt-enabled Release composition were not re-executed by
  BPR-0069. They passed at the earlier snapshot and remain this card's
  responsibility to replay from clean state before closeout, which matters
  because BPR-0084 changed how SQLite enters the build.
- BPR-0059 is done. Its owner-activated Cocoa/QMacStyle run is bound to commit
  `63df236` and is not reproducible at HEAD; its invocation, source identity,
  raster hashes, restored System Dark state, and attestation limits are
  recorded in the RFC-BPR-0003 probe-provenance section.
- The final critical review reproduced two non-Windows blockers, and both are
  now closed. Done BPR-0067 exposes prune and whole-cache deletion through the
  selected application under one secondary `Local history maintenance` menu,
  each capturing its exact target before a default-Cancel confirmation and
  leaving durable tombstones. Done BPR-0068 makes the retained job vector the
  global admission bound at 64 by default and 1,024 at most, separately caps
  ordinary reentrant ingress, and gives invalidation and shutdown fixed
  terminal priority. Done BPR-0069 completed the evidence, provenance,
  changed-path, and ledger reconciliation and corrected the one claim that had
  overstated which host capacities are actually flood-tested.
- BPR-0035/BPR-0036 and BPR-0053 remain open for real Windows execution and
  canonical RFC-BPR-0002/RFC-BPR-0003 status transitions. Every other blocking
  dependency of this card is now in `done/`: BPR-0070 closed the prune
  command-projection refinement, and BPR-0069 closed the evidence and
  documentation reconciliation. Nonblocking BPR-0086 is not a dependency. This
  card remains in backlog and makes no `Auto-Verified` claim while real Windows
  x64 evidence is outstanding.
