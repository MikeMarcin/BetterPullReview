---
schema: "implement-rfc.card.v1"
id: "BPR-1463"
title: "Linux: run the Git metadata differential harness, canary course and cost measurements"
status: "Done"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455", "BPR-1456", "BPR-1457", "BPR-1458"]
priority: "P2"
platforms: ["linux"]
metadata: {"milestone_plan":{"id":"git-metadata-readers-v1","path":"docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"},"mandate_revision":"git-metadata-readers-r1","rfc_base_sha":"9a4f61976bba23727acd77d10c23c041f87208f0","reservation":"docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json","disposition":{"date":"2026-10-26","kind":"done-for-K-L-dropped-by-owner-amendment","amendment":"docs/rfcs/assets/git-metadata-readers-v1/proposed-owner-amendment-2026-10-26.md"}}
acceptance: [{"criterion":"BPR-1463:AC1","outcome":"pass","revision":"62bfc09f6","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0083.md","detail":"K only, as amended 2026-10-26: v0.3.4 Ubuntu 26.04 Release and Debug native suites at 62bfc09f6 (Git 2.53) pass git_metadata_harness, review_cycle_git_readers, review_cycle_git_reader_scale and core_review_cycle_manifest. The harness row runs the canary course (no --no-canary) and passes only with Unclassified == 0, Writes == 0 and a passing canary. CTest truncated the passing output at 1 KiB, so per-component GMR summary and canary lines are not retained. L's half is dropped by owner amendment."}}]
---

# BPR-1463: Linux: run the Git metadata differential harness, canary course and cost measurements

The Linux half of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md)'s H2 and H5 and of AC03 and
AC05, on a native Linux station. Nothing here is measured on macOS.

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

- **BPR-1463:AC1:** On native Linux, [[BPR-1454]]'s contract tests and the differential harness pass for K, with every divergence classified, and the canary course records containment, no-execution and no-write results for K and every L component evaluated.
- **BPR-1463:AC2:** With the evaluation option on, libgit2 builds with the existing Linux toolchain, and the stripped binary-size and build-time deltas are measured and recorded in the decision record.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.

## Closed, 2026-10-26

The owner accepted RFC-BPR-0083's [2026-10-26 amendment](../../rfcs/assets/git-metadata-readers-v1/proposed-owner-amendment-2026-10-26.md), which drops
L's Linux measurements. AC2 and AC1's L half are not run. AC1 for K passes
on evidence that already exists: `git_metadata_harness`,
`review_cycle_git_readers`, `review_cycle_git_reader_scale` and
`core_review_cycle_manifest` pass in v0.3.4's Ubuntu 26.04 Release and Debug
suites at `62bfc09f6`, against Git 2.53. That source includes
[[BPR-1460]]'s harness. The registered harness row runs the canary course
and passes only with no unclassified divergence, no write and a passing
canary. One limit: CTest truncated the passing output at 1 KiB, so the
per-component `GMR summary` and `GMR canary` lines were not kept. The
[ledger](../../verification/RFC-BPR-0083.md) records the same.
