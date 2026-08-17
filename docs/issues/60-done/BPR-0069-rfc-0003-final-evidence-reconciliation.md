---
id: BPR-0069
title: Reconcile final RFC-BPR-0003 evidence and documentation
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0051, BPR-0059, BPR-0067, BPR-0068]
---

# Reconcile final RFC-BPR-0003 evidence and documentation

Perform the final available-platform documentation and claim reconciliation
after the critical-review remediation slices. Replace every ambiguous
`Partially verified` ledger entry with an exact statement of what has direct
evidence and whether the remaining portion is solely target-native Windows
work. Correct every bounded-FIFO/resource claim to match the implemented and
flood-tested host capacities. Replace the documented absence of public
maintenance commands with their exact confirmation, consequence, and
end-to-end behavior.

Record reproducible provenance for the owner-activated BPR-0059 macOS probe:
the exact invocation and environment, executable source/build identity,
macOS/architecture/Qt versions, raster dimensions and hashes, restored System
Dark state, and primary-reviewer attestation that the two rendered frames show
the same loaded pull request, epoch identities, complete OIDs, Whole PR
selection, focus, and readable but distinct native palettes. Do not represent
transient rasters as durable checked-in artifacts.

Re-run immutable-base documentation impact classification over the complete
`a600ab2c29d2b4088a3f2598dbb817c35af2eafd..HEAD` path set, including every
path added after BPR-0051's 186-path pass. Reconcile README, architecture,
security, UX, build/operator guidance, fixture catalog, RFC verification
ledger, board dependencies, and closeout card without claiming final
RFC-BPR-0005 presentation polish or RFC-BPR-0006 selectable themes.

## Required direct evidence

- The documentation-impact gate passes against the immutable RFC base with
  every changed path classified and every no-impact disposition justified.
- Available-platform clean matrices, maintenance public-path evidence, host
  capacity floods, native macOS evidence, architecture/dependency scans,
  original-pack validation, tracker checks, workflow-absence scans, and
  `git diff --check` agree with the final claim matrix.
- Any remaining unverified statement names only its exact target-native
  Windows task and does not block later implementation by product-owner
  direction; it still blocks the affected Auto-Verified status transition
  unless the product owner explicitly changes that ordering policy.

## Claim reconciliation

All four `Partially verified` claim-matrix entries are replaced. Catalog/ref
commit and recovery, exact alias resolution, and stale/canceled publication now
state that they are verified on macOS arm64 and Ubuntu 26.04 x86_64 and that
target-native Windows execution is the single portion not directly executed.
The target-native platform row stays honestly incomplete but now names Windows
x64 as the sole remaining target instead of hedging.

The bounded-resource row carried a real overclaim. Its proof column asserted
that host *actor* limits were exceeded, and its result implied that BPR-0045
had exercised prepared-observation, recovery, and actor capacities. Direct
inspection shows only two epoch-host capacities have executed overflow
evidence, both through `epoch_host_limits`: the default-64/maximum-1,024 job
ledger enforced at `src/application/src/epoch_application_host.cpp:1219`, and
the default-64/maximum-1,024 ordinary reentrant ingress FIFO enforced at
`src/application/src/epoch_application_host.cpp:852`. The 100 prepared
observations (`:654`), 1,000 retained repository actors (`:2659`),
1,000-operation recovery scan in pages of 100, and 32-attempt source-identity
retry are admission refusals with source and enforcement-site evidence only;
no test drives them to capacity. One source-identity collision retry is
executed by `source_collision_retries_before_core_acceptance`. The ledger and
the performance plan now say exactly that, and nonblocking BPR-0086 continues
to own the extra allocation-failure and invalid-limit hardening.

The documented numbers themselves were confirmed correct against
`epoch_runtime_ports.hpp:14-21`, `epoch_application_host.cpp:23-26`,
`epoch_catalog_port.hpp:22-24`, and `core/epoch_model.hpp:249`. The
architecture statements at `docs/architecture/git-and-epoch-model.md:1019-1053`
and `docs/architecture/system-architecture.md:305-306` already matched the
implementation and needed no correction.

Public maintenance commands are no longer documented as absent. README, the
workbench specification, the appearance and accessibility document, and the
operator guidance each state the exact confirmation, its default and Escape
Cancel action, the captured target, the durable tombstone consequence, and
that neither GitHub nor a developer worktree is modified. The two remaining
"absence" sentences are historical critical-review findings inside issue cards;
the BPR-0054 closeout card now records that BPR-0067 and BPR-0068 closed them.

## BPR-0059 provenance

The verification ledger gains a dedicated provenance section binding the
owner-activated macOS probe to commit `63df236`, `src/app/main.cpp` blob
`787ba735278b4f22834fab0e3b8e20f7a9df5515`, the `dev`-preset executable, the
documented rollback-safe invocation and environment, the accepted marker set,
2200x1440 raster geometry, both raster SHA-256 values, and the restored System
Dark `true` setting.

Two limits are recorded rather than papered over. The macOS 15.7.7 build
24G720 arm64 and Qt 6.11.1 host identity was confirmed on 2026-08-03 from the
same machine and build tree; the run log itself captured no version strings, so
that row is a post-run confirmation and is labeled as one. The two rasters were
transient and are not checked-in artifacts, so the attestation about matching
loaded content, epoch identities, complete OIDs, `Whole PR` selection, focus,
and readable distinct palettes is attributed to the primary reviewer who
inspected them during the BPR-0059 run rather than restated as a fresh
inspection. The durable substitutes are the recorded hashes, the marker set,
and the checked-in `native_appearance_probe_contract` rejection guard.

A rerun at HEAD is explicitly not a reproduction: `src/app/main.cpp` changed in
five later commits and the probe now waits for the RFC-BPR-0004 selected-file
workbench result before collecting evidence, so a fresh run renders different
content and different hashes. Owner-authorized native System-theme evidence for
the current composition belongs to BPR-0083 under RFC-BPR-0004.

Operator guidance gained the two facts a reproducer needs and did not have: the
`NATIVE_APPEARANCE_ACTIVE_FOCUS_OK` marker is now in the required marker list,
and the Cocoa owner-focus wait is documented, including that the readiness loop
cannot progress until the desktop owner clicks the window and that an
unattended run is a clean unverified attempt rather than evidence.

## Immutable-base classification

The complete `a600ab2c29d2b4088a3f2598dbb817c35af2eafd..HEAD` set is 374 paths,
198 added and 176 modified, against BPR-0051's earlier 186-path pass. Every
path resolves to a tracked `AGENTS.md`: 86 to `docs/`, 79 to `tests/`, 67 to
the repository root, 36 to `src/application/`, 29 to `src/adapters/github/`,
26 each to `src/adapters/qt/` and `src/core/`, 12 each to `src/adapters/` and
`src/app/`, and 1 to `packaging/`. The per-scope dispositions are recorded in
the verification ledger.

An independent semantic scout re-examined every RFC-BPR-0003-owned surface for
changes made after BPR-0051 and found no new documentation obligation. Its
attribution shows `epoch_catalog_port.hpp`, `git_smart_transport_port.hpp`,
`git_pack_stream.hpp`, and `github_epoch_handoff.hpp` changed only through the
BPR-0085 formatting pass, and every remaining semantic change traces to a
closeout card that already updated its owning document.

Two conclusions the scout left open were checked directly rather than assumed.
BPR-0084 touched only SQLite acquisition: `git show --stat bcde3b9` changes
`src/adapters/sqlite/CMakeLists.txt`, the vendored tree, and provenance
documentation, and leaves `src/adapters/sqlite/src/` and `include/` untouched,
so the catalog schema, VFS, and storage contract are unchanged at the pinned
3.53.4 version. The ledger's "closed 18-table/16-index schema" claim was also
counted directly against the adapter source and is exactly 18 tables and 16
indexes.

The `src/adapters/github/` disposition needed correction during this pass. Four
commits touch that scope in the range, and two of them are RFC-BPR-0003 work
rather than RFC-BPR-0002 formatting: `edd762e` adds the native Git smart
transport and its lease boundary, and `15e7842` adds the accepted-snapshot
outlet for BPR-0047. The ledger records that split.

Two documents named by this card had genuine gaps rather than wording drift.
The fixture catalog described the BPR-0055 and BPR-0056 additions to
`application_epoch_host` but never recorded BPR-0068's residency cases at all;
it now documents the injected-capacity floods, the counters the manual
executors expose, and the fact that only two capacities are driven to overflow.
The security threat model's resource-exhaustion section predated BPR-0068 and
described deferred ingress without any global bound; it now states the job
ledger as the admission bound, the ingress FIFO, and the fixed
invalidation/shutdown priority, so a looping producer meets a bounded refusal
rather than unbounded residency.

## Direct verification

The replay ran serially on macOS 15.7.7 build 24G720 arm64 with Qt 6.11.1 and
Git 2.39.5. `build/dev` and `build/core-only` were removed and reconfigured so
the matrices exercise BPR-0084's CPM SQLite acquisition rather than stale
output; the verified source cache was preserved, so the configure resolved
3.53.4 offline.

- `cmake --build --preset dev` and `ctest --preset dev --output-on-failure`:
  100% of 88 tests passed in 684.92 seconds.
- `cmake --build --preset core-only` and `ctest --preset core-only
  --output-on-failure`: 100% of 33 tests passed in 79.36 seconds.
- The architecture-boundary, architecture-boundary-contract,
  documentation-structure, documentation-issue-graph, and
  platform-configuration gates pass inside both matrices.
- `cmake --build --preset dev --target coding-format-check coding-tidy`: the
  format gate passes across 181 files and the strict first-party `clang-tidy`
  audit passes.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`.
- `python3 scripts/verify_binary_dependencies.py
  build/dev/src/app/better_pull_review` reports `BINARY_DEPENDENCIES_OK`;
  `python3 scripts/verify_runtime_surface.py` on the same executable reports
  `RUNTIME_SURFACE_OK`.
- The original source-pack validator reports `OK: 14 RFCs, 17 normative
  documents, architecture v2 enforced`.
- Tracked and on-disk GitHub Actions workflow counts are 0/0, and
  `git diff --check` is clean.

Test counts grew from the 67 and 27 of the earlier snapshot because the
RFC-BPR-0004 pack, BPR-0067, BPR-0068, and BPR-0085 added cases. The earlier
record is superseded rather than contradicted.

This slice deliberately did not re-execute the focused sanitizer matrix, the
instrumented 100,000-epoch SQLite stress binary, or the tests-disabled
Qt-enabled Release composition. Those are named in BPR-0054's closeout list and
that card now records that they must be replayed from clean state, which
matters because BPR-0084 changed how SQLite enters the build. No fresh claim
about them is made here.

No owner-authorized System-theme probe was run. BPR-0059's evidence is recorded
as provenance, not reproduced, and current-composition System-theme evidence
remains BPR-0083's owner-authorized gate.

## Documentation disposition

The verification ledger, fixture catalog, security threat model, appearance and
accessibility document, operator guidance, board frontier text, and the
BPR-0054 closeout card are updated. README, the workbench specification, the
system architecture, and the Git/epoch model already matched the implementation
and needed no change; the architecture capacity numbers were checked against
the source and confirmed correct. No final RFC-BPR-0005 presentation-polish or
RFC-BPR-0006 selectable-theme claim is made.

## Result

RFC-BPR-0003's recorded evidence now matches what was actually executed. The
one material overclaim found during this pass, that host actor and preparation
capacities had been exceeded by test, is corrected in both the claim matrix and
the performance plan, and the residual hardening stays with nonblocking
BPR-0086. Every remaining unverified RFC-BPR-0003 statement names target-native
Windows and its exact task, BPR-0053. BPR-0069 is complete.
