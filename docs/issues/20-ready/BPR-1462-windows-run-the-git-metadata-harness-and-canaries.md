---
schema: "implement-rfc.card.v1"
id: "BPR-1462"
title: "Windows: extend the Git metadata canary course to K"
status: "Ready"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455", "BPR-1456", "BPR-1457", "BPR-1458"]
priority: "P2"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"git-metadata-readers-v1","path":"docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"},"mandate_revision":"git-metadata-readers-r1","rfc_base_sha":"9a4f61976bba23727acd77d10c23c041f87208f0","reservation":"docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json","amended":{"date":"2026-10-26","amendment":"docs/rfcs/assets/git-metadata-readers-v1/proposed-owner-amendment-2026-10-26.md","original_title":"Windows: run the Git metadata differential harness, canary course and cost measurements"}}
---

# BPR-1462: Windows: extend the Git metadata canary course to K

The Windows half of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md)'s H2 and H5 and of AC03 and
AC05, on a native Windows station. Nothing here is measured on macOS.

The canary course's FIFO and symlinked-rule-file canaries are POSIX-only: on
Windows the harness prints `GMR canary skipped`. This card extends the course
to Windows first, with equivalents that detect an open outside the root (for
example an `include.path` naming a named pipe or an outside file whose rules
would change the answers, and `.gitignore`/`.gitattributes` replaced by a
symbolic link or junction pointing outside the root), then records K's and L's
results.

## Station procedure

From a checkout of main at or after the commit that finished [[BPR-1460]]:

1. Configure the platform's dev preset with `BUILD_TESTING=ON` and build
   `test_review_cycle_git_readers`, `test_git_metadata_harness`,
   `bench_review_cycle_scale` and `test_better_pull_review_review_cycle_manifest_tests`.
2. Run `ctest -R "review_cycle_git_readers|git_metadata_harness|review_cycle_git_reader_scale|core_review_cycle_manifest" --output-on-failure`
   and record the harness's `GMR summary` lines. Every divergence must be
   classified; a new one is either a K defect to fix or a registry entry with
   its reason.
3. Configure a second tree **outside the checkout** with
   `-DBPR_EVALUATE_LIBGIT2=ON` (inside the checkout, `architecture_boundaries`
   scans the staged libgit2 sources under `build/`), build
   `test_git_metadata_harness`, `tool_libgit2_size_k`, `tool_libgit2_size_kl`
   and `libgit2package`, run the harness with L, and measure the clean libgit2
   build time and the stripped size delta as the macOS ledger did.
4. Record the results in [the verification ledger](../../verification/RFC-BPR-0083.md)
   and the decision record's pending H2 and H5 cells.

## Acceptance criteria

- **BPR-1462:AC1** *(amended 2026-10-26)*: On native Windows, the canary course has Windows equivalents of the FIFO and symlink canaries: an `include.path` naming a named pipe or an outside file whose rules would change the answers, and `.gitignore`/`.gitattributes` replaced by a symbolic link or junction pointing outside the root. It records containment, no-execution and no-write results for K, and a negative control shows that each canary fires. A containment failure is a K defect, fixed under this card or a card it names.
- **BPR-1462:AC2:** Dropped by owner amendment, 2026-10-26. libgit2 is not built or measured on Windows.

The original text of both criteria is in the [amendment record](../../rfcs/assets/git-metadata-readers-v1/proposed-owner-amendment-2026-10-26.md).

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.

## Narrowed by owner amendment, 2026-10-26

The owner accepted RFC-BPR-0083's [2026-10-26 amendment](../../rfcs/assets/git-metadata-readers-v1/proposed-owner-amendment-2026-10-26.md). L is
ineligible on every component, so its Windows canary and cost measurements
are dropped. Station procedure step 3 is not run, and step 4 records only
K's H2 cell. K's harness, divergence classification and reader contract
rows already pass in v0.3.4's Windows 11 Release and Debug suites at
`62bfc09f6`, against Git 2.55. `run_canary` reports "skipped" on `_WIN32`,
so the work left is the Windows canary course for K described in the
amended AC1.
