---
id: BPR-0052
title: Simplify the RFC-BPR-0003 change set
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0048, BPR-0049, BPR-0050, BPR-0051, BPR-0066]
---

# Simplify the RFC-BPR-0003 change set

Run the code-simplifier skill over the completed RFC slice. Remove duplicated
epoch policy, authority drift, broad Git/filesystem/persistence access,
unnecessary serialization, retained whole-history copies, full Qt model
resets, unsafe process/config convenience paths, stale comments, and
avoidable complexity without changing behavior.

Replay core, host, real-Git, smart-transport, catalog/recovery, Qt,
composition, security, sanitizer, documentation, and target-native
available-platform verification after simplification.

## Audit progress

- Removed a redundant direct `BetterPullReview::Application` link from the
  real maintenance integration target. Both adapter dependencies already
  publish that application dependency; the generated link command now names
  the static application library once and the real core/host/Git/SQLite test
  passes 1/1 in 29.10 seconds.
- Simplified stable-row move/update handling by retaining the exact current
  index instead of scanning the model a second time, and removed an identical
  second insertion-index clamp. The scale-specific Qt comparison matrix passes
  3/3 with its persistent-index, exact-signal, and zero-routine-reset checks.
- Kept the explicit core reducers, host recovery phases, installed-Git policy,
  cache/catalog identity checks, platform filesystem branches, and SQLite CAS
  paths intact. Independent mechanical and semantic audits agree that their
  verbosity encodes fail-closed behavior rather than removable duplication.
- The semantic audit found that accepted history pages can accumulate to the
  catalog's 100,000-epoch limit in the Qt selector, producing up to 200,000
  anchor rows and repeated linear scans. This requires a bounded/virtual
  selector design and direct integration evidence, not a behavior-preserving
  micro-refactor. Done BPR-0066 now resolves that finding with one
  100-epoch/200-anchor page, core-authorized newest/older navigation,
  stale-page invalidation, exact out-of-window selection, 1,000+ page core/Qt/
  SQLite traversal, and real rendered/restart evidence.
- Removed the never-produced `epoch_catalog_projection::Replacement` vector.
  `Replace` is emitted only for an exact clear; ordinary accepted pages already
  use the typed `HistoryPage` projection and stable-ID reconciliation.
- Extracted the automated timeout evidence snapshot from the large application
  state-machine tick. The helper preserves every captured status, OID, row,
  model-count, command-state, core-status, render, and shutdown field while
  leaving stage policy unchanged.
- Deliberately retained the small two-site Qt OID formatter, typed core
  reducers, host/catalog recovery phases, explicit invalidation state, and
  platform-specific filesystem/process branches. Consolidating them would add
  coupling or obscure fail-closed behavior rather than simplify it.

## Verification

- The focused Qt/application replay passes 5/5, covering all three rendered
  scale cases plus the public composition and application render path.
- A clean Qt-enabled developer build passes 67/67 tests in 605.70 seconds.
  This includes core, host, real Git, native smart transport, cache, SQLite,
  Qt, public composition, architecture, dependency, runtime, documentation,
  persistence-restart, and rendered integration coverage.
- A clean Qt-free `core-only` build passes 27/27 tests in 76.69 seconds.
- The sanitizer build passes the focused core, host, bounded Qt model, and
  rendered application cases 4/4. Its 100,000-epoch SQLite stress case exceeded
  the ordinary 180-second CTest timeout under instrumentation, then completed
  successfully when run directly from the same sanitizer build with no
  sanitizer diagnostic.
- A tests-disabled Release configuration with Qt enabled builds and links the
  real `better_pull_review` executable in 46 steps. `otool -L` shows the
  expected Qt and macOS system frameworks; the extracted helper therefore
  compiles without any test-only field or definition.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`, and `git diff --check` passes.

## Documentation disposition

The simplification changes no product, architecture, security, persistence,
packaging, or UX contract. This issue card records the removed unused
projection surface, the behavior-preserving state-machine extraction, the
intentionally retained fail-closed structure, and the replay evidence. The
RFC ledger and governing documents require no semantic update for this slice.

## Result

RFC-BPR-0003's completed implementation is smaller without weakening typed
authority, stale-result rejection, bounded history residency, incremental Qt
projection, or real composition behavior. BPR-0066 resolved the only blocking
simplification finding before this card closed.
