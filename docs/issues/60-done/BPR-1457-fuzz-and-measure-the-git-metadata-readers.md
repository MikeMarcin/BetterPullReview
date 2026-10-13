---
schema: "implement-rfc.card.v1"
id: "BPR-1457"
title: "Fuzz oracles, adversarial timing and the 250,000-entry bench for the Git metadata readers"
status: "Done"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
acceptance: [{"criterion":"BPR-1457:AC1","outcome":"pass","revision":"561604015","evidence":{"kind":"observation","detail":"fuzz_git_output keeps targets for every K reader (names/config selector 0, index selector 1, HEAD/refs/packed-refs selector 2, ignore/attributes/matcher selector 3) and now asserts oracles beyond crashes: index serialize-then-parse round trip with zero and hashed trailers (test-only writer tests/support/git_metadata_writers.hpp), config canonical write-back round trip, and matcher properties (escaped path matches exactly itself, ** matches all, * matches exactly slash-free paths). Corpus replay fuzz_git_output_corpus passes on 31 seeds (5 new); a 600 s campaign (seed 1457, ASan+UBSan, Apple clang + LLVM 23 libFuzzer runtime) ran 5,284,057 inputs with no crash, oracle failure or timeout. Round trips also run in core_review_cycle_manifest.","path":"docs/issues/60-done/BPR-1457-fuzz-and-measure-the-git-metadata-readers.md"}},{"criterion":"BPR-1457:AC2","outcome":"pass","revision":"561604015","evidence":{"kind":"observation","detail":"bench_review_cycle_scale --adversarial (Debug -O0, loaded machine): BPR-0970 shape 0.52 ms (folded 0.46), ** chain 0.16, **/a* chain 70, */ segments 0.11, class brackets 0.17, 1 MiB ignore file load 135 and one path 1.3 ms (1,963 ms before the byte-set prefilter added here), 100,000 unskippable rules 174 load / 450 one path, 50,001-link macro chain 594, 65,535-entry config 230 ms: all under 1 s. ctest row review_cycle_git_reader_scale runs these cases.","path":"docs/issues/60-done/BPR-1457-fuzz-and-measure-the-git-metadata-readers.md"}},{"criterion":"BPR-1457:AC3","outcome":"pass","revision":"561604015","evidence":{"kind":"observation","detail":"bench_review_cycle_scale --git /usr/bin/git --git-index-entries 250000: a 21.2 MB SHA-1 index that git ls-files reads; index parse median of five 787 ms (174 ms under index.skipHash; a more loaded first run 1,619 ms), ignore walk of 250,000 paths against a 20-rule .gitignore median of five 358 ms, peak RSS 135 MB; recorded in docs/verification/RFC-BPR-0083.md for rule 3, not asserted in ctest.","path":"docs/issues/60-done/BPR-1457-fuzz-and-measure-the-git-metadata-readers.md"}}]
---

# BPR-1457: Fuzz oracles, adversarial timing and the 250,000-entry bench for the Git metadata readers

The Fuzzing and Performance paragraphs of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) for K
on macOS.

## Acceptance criteria

- **BPR-1457:AC1:** Every reader that stays keeps a target in the existing fuzz harness; the index target asserts a serialize-then-parse round trip, and the config and matcher targets assert properties beyond the absence of a crash.
- **BPR-1457:AC2:** The BPR-0970 input and the adversarial corpus each finish within 1 s on the fixed readers (H6 baseline).
- **BPR-1457:AC3:** The scale bench builds a 250,000-entry checkout and reports the median of five runs of index parsing and an ignore walk, plus peak memory, recorded for the decision record without asserting it in ctest.

## Continuation

Done on `561604015` (macOS, Debug). Evidence in the
[verification ledger](../../verification/RFC-BPR-0083.md#bpr-1457-fuzz-oracles-adversarial-timing-and-scale).
The differential oracle against a reference matcher belongs to [[BPR-1459]].
Candidates are compared with these K numbers only in the same build and run.
