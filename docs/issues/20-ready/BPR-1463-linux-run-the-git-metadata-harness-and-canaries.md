---
schema: "implement-rfc.card.v1"
id: "BPR-1463"
title: "Linux: run the Git metadata differential harness, canary course and cost measurements"
status: "Ready"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455", "BPR-1456", "BPR-1457", "BPR-1458"]
priority: "P2"
platforms: ["linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
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
