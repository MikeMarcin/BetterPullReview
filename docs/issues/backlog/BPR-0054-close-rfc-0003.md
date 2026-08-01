---
id: BPR-0054
title: Verify and close RFC-BPR-0003
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0035, BPR-0048, BPR-0049, BPR-0050, BPR-0051, BPR-0052, BPR-0053, BPR-0055, BPR-0056, BPR-0059, BPR-0067, BPR-0068, BPR-0069]
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

- The post-BPR-0066/BPR-0052 developer matrix passes 67/67, `core-only`
  passes 27/27, the focused sanitizer matrix passes 4/4, and the instrumented
  100,000-epoch SQLite stress binary completes directly after exceeding its
  ordinary CTest timeout. A Qt-enabled tests-disabled Release executable also
  builds and links successfully.
- The documentation gate, 104-case architecture mutation suite, four-case
  platform-configuration suite, original source-pack validator, tracked and
  on-disk workflow-absence scans, and diff hygiene are green. GitHub Actions
  workflow counts remain 0/0.
- BPR-0059 is done. Its owner-activated Cocoa/QMacStyle rerun directly passed
  active-focus, local/global Light-to-Dark transition, state, pixel,
  changed-render, and public-composition markers; inspection confirmed stable
  epoch/OID/Whole PR state in distinct 2200x1440 rasters, and the original
  System Dark setting was restored.
- The final critical review reproduced two non-Windows blockers: the selected
  application exposes no explicit maintenance command, and the repository
  actor/job/ingress residency is not actually bounded. BPR-0067 and BPR-0068
  own those repairs; BPR-0069 owns exact evidence, provenance, changed-path,
  and ledger reconciliation after they close.
- BPR-0035/BPR-0036 and BPR-0053 remain open for real Windows execution and
  canonical RFC-BPR-0002/RFC-BPR-0003 status transitions. This card remains in
  backlog and makes no `Auto-Verified` claim while any of those or the new
  critical-review dependencies remain open.
