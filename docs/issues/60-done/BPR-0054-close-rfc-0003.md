---
id: BPR-0054
title: Verify and close RFC-BPR-0003
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0035, BPR-0048, BPR-0049, BPR-0050, BPR-0051, BPR-0052, BPR-0053, BPR-0055, BPR-0056, BPR-0059, BPR-0067, BPR-0068, BPR-0069, BPR-0070, BPR-0234]
---

# Verify and close RFC-BPR-0003

RFC base: `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`.

Replay the complete claim and architecture-conformance matrices from clean
state. Directly inspect core policy, host staging and persistence, real
installed-Git and native smart transport, cache/ref/catalog recovery,
force-push retention, exact aliases/selections, rendered Qt behavior,
security boundaries, package/runtime dependencies, and platform-native
evidence.

Resolve every documentation-ledger row, perform a skeptical final diff review,
move every RFC card to `60-done/`, and mark RFC-BPR-0003 Auto-Verified only when
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
- The focused sanitizer matrix has now been replayed at HEAD and passes 8/8 in
  7.06 seconds over the core epoch policy, the epoch host, and the three-scale
  epoch-comparison and diff-workbench cases, with the sanitizer confirmed
  active by cache flags, a linked ASan runtime, and 42 interceptor symbols. The
  tests-disabled Qt-enabled Release composition is built fresh and scanned by
  `application_composition_contract`, which passes at HEAD. The instrumented
  100,000-epoch SQLite catalog work has also been replayed at HEAD: running
  `sqlite_epoch_catalog_behavior_contract` directly out of the sanitizer tree
  reports `sqlite-epoch-catalog-behavior-tests:pass` in 45.82 seconds under
  ASan/UBSan. That closes the concern raised by BPR-0084 changing how SQLite
  enters the build.
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
- BPR-0035/BPR-0036 closed on 2026-08-28 with RFC-BPR-0002 Auto-Verified, so
  the canonical order gate is satisfied. BPR-0053's own work completed with
  matrix-10; it stays open only until this card's final replay consumes its
  procedure, which is the run recorded below. Every other blocking dependency
  is in `60-done/`. Nonblocking BPR-0086 is not a dependency.

## The closeout run — 2026-08-28

The final replay ran fresh on two hosts at code head `27e50ded` (every later
commit is docs/tracker-only, diff-verified): on macOS arm64, clean
reconfigured trees pass the complete dev matrix 218/218 in 1756.78 s, the
core-only matrix 98/98 in 282.04 s, and a fresh ASan/UBSan tree passes the
focused sanitizer matrix 8/8 with the instrumented 100,000-epoch catalog
contract green in 57.46 s; on the Ubuntu 26.04 WSL2 host, clean reconfigured
trees pass dev 208 of 212 in 996 s — every failure attributed to BPR-0310
(keyring trio, untested-here) or new BPR-0472 (the pre-existing
obstruction-course handoff hang, proven at BPR-0156's own baseline), none to
the post-`82c7d1c1` epoch commits — and core-only 96/96 in 439 s, with both
verify scripts green. The Windows surface is consumed from BPR-0053's
matrix-10/11 record with a commit-exact delta analysis showing no later
Windows-compiled change can affect an RFC-BPR-0003 claim. The dev matrix's
one same-day predecessor failure was `documentation_structure` correctly
rejecting this card's transient move to `30-in-progress/`; the closeout card
works from backlog, and the complete matrix re-ran green after the cure.
The Linux run corrected one recorded attribution by measurement — BPR-0156's
"slower host" story for the review-ledger contract — and that truth now lives
in BPR-0472 and the BPR-0156 postscript rather than being repeated here.
Per-criterion selector results, the sanitizer-active proof, gate results, and
both platform records are in the ledger's final-closeout-replay section.
Implemented by the integrator with the concurrent Linux session running its
host's matrices; the independent capability-critical review runs before the
status flip.

## Closeout checklist

Historical evidence is provenance only; every row requires final replay at the final closeout head.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [x] | Opening a fixture PR creates/reuses the correct app-owned bare repository. | App-owned bare-repository creation and reuse in git_cache_contract and application_epoch_host | `ctest --preset dev -R '^(git_cache_contract\|application_epoch_host)$' --output-on-failure` | Opening the fixture creates one repository under the app cache and reopening reuses that repository without touching a developer worktree. | Cache-retention-and-persistence evidence in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “Opening a fixture PR creates/reuses the correct app-owned bare repository.” | automated | No live blocker; the final replay is recorded in the ledger’s final-closeout-replay section. |
| [x] | Observed heads receive durable private refs and remain available after the simulated remote force-push removes them. | Private observed-head refs and cross-process epoch persistence for GitHub.com and GHES fixtures | `ctest --preset dev -R '^(git_cache_contract\|application_epoch_persistence_(github-com\|ghes-3.18))$' --output-on-failure` | Observed base/head OIDs remain reachable through private refs after the simulated remote force-push removes its public ref. | Durable private-ref and persistence rows in the claim matrix in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “Observed heads receive durable private refs and remain available after the simulated remote force-push removes them.” | automated | No live blocker; the final replay is recorded in the ledger’s final-closeout-replay section. |
| [x] | Base update, author upload, force-push, and base-branch-change cases receive distinct cause labels. | Epoch-cause transition policy and catalog contracts | `ctest --preset dev -R '^(core_epoch_policy\|application_epoch_contract\|application_epoch_catalog_contract)$' --output-on-failure` | Base update, author upload, force-push, and base-branch change each produce their distinct typed cause and exact OID pair. | Deterministic epoch-policy evidence in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “Base update, author upload, force-push, and base-branch-change cases receive distinct cause labels.” | automated | No live blocker; the final replay is recorded in the ledger’s final-closeout-replay section. |
| [x] | Left and right selectors reach all retained epochs through core-authorized bounded pages and show exact OIDs even when the selected anchors leave the resident page. | Bounded core history pages and three-scale Qt epoch selectors | `ctest --preset dev -R '^(core_epoch_policy\|qt_epoch_comparison_scale_(100\|125\|200))$' --output-on-failure` | Both selectors reach retained epochs beyond the resident page, retain selected anchors, and display the complete exact OIDs. | Epoch selector and bounded-history rows in the claim matrix in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “Left and right selectors reach all retained epochs through core-authorized bounded pages and show exact OIDs even when the selected anchors leave the resident page.” | automated | No live blocker; the final replay is recorded in the ledger’s final-closeout-replay section. |
| [x] | `Previous upload -> latest` and `My last reviewed -> latest` resolve to expected OIDs. | Core default-comparison resolver over retained epoch identities | `ctest --preset dev -R '^(core_epoch_policy)$' --output-on-failure` | Previous upload to latest and My last reviewed to latest resolve to the expected immutable left and right OIDs. | Default comparison-policy row in the claim matrix in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “`Previous upload -> latest` and `My last reviewed -> latest` resolve to expected OIDs.” | automated | No live blocker; the final replay is recorded in the ledger’s final-closeout-replay section. |
| [x] | Git process tests prove hooks, textconv, external diff, submodules, filters, and worktree execution are disabled. | Installed-Git process boundary on real Windows x64 plus git_process_contract | Run [BPR-0053](../60-done/BPR-0053-rfc-0003-windows-verification.md#run-rfc-bpr-0003-windows-x64-target-native-verification)'s clean Windows x64 Git process contract and inspect the child environment, argv, worktree, and process tree. | Hooks, textconv, external diff, submodules, filters, and worktree execution remain disabled on the target-native installed-Git path. | BPR-0053 target-native Git-process evidence summarized by the Git process row in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “Git process tests prove hooks, textconv, external diff, submodules, filters, and worktree execution are disabled.” | target-native | [BPR-0053](../60-done/BPR-0053-rfc-0003-windows-verification.md) closed with this card; the ledger’s final-closeout-replay section records the consumed matrix-10/11 record and its commit-exact delta analysis. |
| [x] | Repository cache deletion and ref pruning are explicit user/admin operations with documented consequences. | Explicit cache deletion and retained-ref pruning through the maintenance host and public executable path | `ctest --preset dev -R '^(epoch_maintenance_integration\|application_epoch_maintenance_public_path)$' --output-on-failure` | Only an explicit maintenance request deletes a cache or prunes refs, and the trace reports the documented loss of retained history. | Cache deletion and ref-pruning row in the claim matrix in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “Repository cache deletion and ref pruning are explicit user/admin operations with documented consequences.” | automated | No live blocker; the final replay is recorded in the ledger’s final-closeout-replay section. |
| [x] | Core epoch-selection tests run without Git or Qt; host tests prove Git/ref/time effects and stale completion rejection; Qt tests prove selector event translation and projection only. | Core epoch policy, host Git/ref/time staging, and three-scale Qt selector projection | `ctest --preset dev -R '^(core_epoch_policy\|application_epoch_host\|qt_epoch_comparison_scale_(100\|125\|200))$' --output-on-failure` | Core runs without Git/Qt, stale Git completions are refused by exact identity, and Qt selector events change only semantic selections/projections. | Core-host-Qt allocation row in the claim matrix in [RFC-BPR-0003 verification ledger](../../verification/RFC-BPR-0003.md#claim-matrix); final replay record keyed by “Core epoch-selection tests run without Git or Qt; host tests prove Git/ref/time effects and stale completion rejection; Qt tests prove selector event translation and projection only.” | automated | No live blocker; the final replay is recorded in the ledger’s final-closeout-replay section. |
