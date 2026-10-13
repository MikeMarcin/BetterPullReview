---
schema: "implement-rfc.card.v1"
id: "BPR-1456"
title: "Extend the canary course with containment canaries for the Git metadata readers on macOS"
status: "Done"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
acceptance: [{"criterion":"BPR-1456:AC1","outcome":"pass","revision":"9a8cde4f5","evidence":{"kind":"observation","detail":"The canary course in test_git_metadata_harness (ctest git_metadata_harness) adds outside-scope canaries: an include.path naming a FIFO outside the root (a watcher thread records any reader), symlinked .gitignore and .gitattributes pointing outside the root with rules that would change the answers, alongside hooks, core.fsmonitor, filter clean/smudge/process and diff textconv drivers pointing at a logging script. A positive control under real Git fires the FIFO and fsmonitor canaries.","path":"docs/issues/60-done/BPR-1456-extend-the-git-metadata-canary-and-containment-course.md"}},{"criterion":"BPR-1456:AC2","outcome":"pass","revision":"9a8cde4f5","evidence":{"kind":"observation","detail":"For K on macOS, every component C1-C6: containment pass (FIFO never opened, outside rules not followed), no-execution pass, no-write pass; after every one of the 43 corpus cases the checkout and .git were byte-identical with no lock files (unchanged-checkouts=43, writes=0).","path":"docs/issues/60-done/BPR-1456-extend-the-git-metadata-canary-and-containment-course.md"}}]
---

# BPR-1456: Extend the canary course with containment canaries for the Git metadata readers on macOS

The Safety paragraph of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) for K on macOS.
Windows and Linux runs are [[BPR-1462]] and [[BPR-1463]].

## Acceptance criteria

- **BPR-1456:AC1:** Canaries outside the allowed scope detect any open, including a FIFO named by `include.path` and a symlinked `.gitignore` and `.gitattributes` pointing outside the root, alongside the existing hook and `core.fsmonitor` canaries.
- **BPR-1456:AC2:** After every harness case the checkout is byte-identical, unmodified and free of lock files, and no canary fired, for K on macOS.

## Continuation

Done for K on macOS on `9a8cde4f5`; evidence in the
[verification ledger](../../verification/RFC-BPR-0083.md#bpr-1455-and-bpr-1456-the-differential-harness-and-canary-course).
L's results from the same course are [[BPR-1458]]'s. Windows and Linux runs are
[[BPR-1462]] and [[BPR-1463]].
