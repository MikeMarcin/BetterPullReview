---
schema: "implement-rfc.card.v1"
id: "BPR-1455"
title: "Build the differential harness that checks the Git metadata readers against Git"
status: "Done"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
acceptance: [{"criterion":"BPR-1455:AC1","outcome":"pass","revision":"9a8cde4f5","evidence":{"kind":"observation","detail":"ctest git_metadata_harness (test_git_metadata_harness, Git-gated, network-isolated) builds each corpus repository with /usr/bin/git 2.50.1 and asks the isolated oracle (GIT_CONFIG_NOSYSTEM=1, empty GIT_CONFIG_GLOBAL, empty HOME and XDG_CONFIG_HOME, GIT_OPTIONAL_LOCKS=0; RFC's ls-files/rev-parse/symbolic-ref/check-ignore/check-attr commands) and K (git_metadata_first_party.cpp, composed as capture composes the readers) C1-C6, plus a capture cross-check for C5; it reports agree/diverge/refused/skipped per component and case and writes a TSV with --report.","path":"docs/issues/60-done/BPR-1455-build-the-git-metadata-differential-harness.md"}},{"criterion":"BPR-1455:AC2","outcome":"pass","revision":"9a8cde4f5","evidence":{"kind":"observation","detail":"44 corpus cases cover the RFC list (SHA-1/SHA-256, index v2/v3/v4, manyFiles, skipHash, split, cone sparse index, skip-worktree, intent-to-add, unmerged, linked and relative worktrees, reftable, HEAD packed/loose/symbolic/too-deep/detached/unborn, include/includeIf/inline comments/quoting/escapes/subsections/ignoreCase both ways, POSIX classes, escapes, trailing spaces, negation, **, directory rules, CRLF, non-UTF-8 paths, attribute states and macros, BPR-0970 input, rule-file bounds); --large adds 250,000 entries, which agrees for C1-C6. Features the installed Git lacks are reported skipped with the version found; case-distinct-files is skipped on the case-insensitive file system.","path":"docs/issues/60-done/BPR-1455-build-the-git-metadata-differential-harness.md"}},{"criterion":"BPR-1455:AC3","outcome":"pass","revision":"9a8cde4f5","evidence":{"kind":"observation","detail":"Result for K: C1 38/0/5/1, C2 37/2/4/1, C3 38/1/4/1, C4 42/1/0/1, C5 34/3/6/1, C5-capture 33/2/7/1, C6 38/1/4/1 (agree/diverge/refused/skipped), unclassified=0; every divergence is in the H1 registry as fix, refusal or deliberate with its reason (listed in the ledger). Harness-found reader bugs were fixed: oversized rule files (and info files) now refuse capture with rule_file_bound instead of being dropped; symbolic references are followed to Git's depth of five so a two-level HEAD no longer looks unborn.","path":"docs/issues/60-done/BPR-1455-build-the-git-metadata-differential-harness.md"}},{"criterion":"BPR-1455:AC4","outcome":"pass","revision":"9a8cde4f5","evidence":{"kind":"observation","detail":"Negative control: with fix 4 reverted in bracket() the harness exits 1 with 17 unclassified divergences (e.g. C5 ignorecase-false 'a b' oracle=ignored answer=kept; C5-capture '7Q' captured); source restored byte for byte.","path":"docs/issues/60-done/BPR-1455-build-the-git-metadata-differential-harness.md"}}]
---

# BPR-1455: Build the differential harness that checks the Git metadata readers against Git

The harness of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) (Evaluation method, Oracle and
Corpus). Tests may run Git; the product may not. The baseline is the fixed
readers from [[BPR-1454]].

## Acceptance criteria

- **BPR-1455:AC1:** A workspace contract executable builds each corpus repository with the installed Git and asks Git (with `GIT_CONFIG_NOSYSTEM=1`, an empty global config, empty `HOME` and `XDG_CONFIG_HOME`, and `GIT_OPTIONAL_LOCKS=0`) and the first-party readers the C1–C6 questions, comparing the answers per path or reference and reporting agreement per component and case.
- **BPR-1455:AC2:** The corpus covers the RFC's object formats, index variants, index entries, repository layouts, HEAD states, config, ignore and attribute cases and the adversarial patterns. A case whose feature the installed Git lacks reports skipped with the version found, never a pass.
- **BPR-1455:AC3:** Every divergence is classified as a fix, an explicit refusal with a reason, or a documented deliberate choice; an unclassified divergence fails the row (H1 for K).
- **BPR-1455:AC4:** A negative control, a scratch build with one [[BPR-1454]] fix reverted, turns the harness red; the result is recorded.

## Continuation

Done on `9a8cde4f5` (macOS, Git 2.50.1); evidence in the
[verification ledger](../../verification/RFC-BPR-0083.md#bpr-1455-and-bpr-1456-the-differential-harness-and-canary-course).
L joins the same harness when the evaluation-only `bpr_libgit2_candidate`
target exists ([[BPR-1458]]). Windows and Linux runs are [[BPR-1462]] and [[BPR-1463]].
