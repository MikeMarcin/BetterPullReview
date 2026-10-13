---
schema: "implement-rfc.card.v1"
id: "BPR-1431"
title: "Search with RE2 instead of a hand-written regex engine"
status: "Done"
rfcs: []
depends_on: ["BPR-1432"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1431--BPR-1431--e2ce4383ccc3.json","origin":"owner request 2026-10-13 after the RFC-0077 critical review","follows":"RFC-BPR-0077"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"32ccfd8ad","evidence":{"kind":"observation","detail":"regex adapter implements core::source_regex_engine with pinned RE2 2025-11-05 and Abseil 20250814.2 (hash/size/license verified, Qt graph only); package_runtime, release_security_audit, binary_dependency_audit, binary_dependency_surface and dependency_modernization contracts pass.","path":"docs/issues/60-done/BPR-1431-search-with-re2.md"}},{"criterion":"AC2","outcome":"pass","revision":"32ccfd8ad","evidence":{"kind":"observation","detail":"Thompson VM, KMP path, 6,460-line Unicode tables, generator and third_party/unicode removed.","path":"docs/issues/60-done/BPR-1431-search-with-re2.md"}},{"criterion":"AC3","outcome":"pass","revision":"32ccfd8ad","evidence":{"kind":"observation","detail":"core_source_matcher pins a.* over 'ba a' as [3,4), TODO.* after refused xTODO, aa in 'aaa aa'; lookaround, backreferences and \\\\C refuse as unsupported.","path":"docs/issues/60-done/BPR-1431-search-with-re2.md"}},{"criterion":"AC4","outcome":"pass","revision":"32ccfd8ad","evidence":{"kind":"observation","detail":"15 core/host/public-path/performance search suites pass; warm matching 1,568,828 -> 348,163 us, cancel 3 ms, thresholds hold; boundary, docs, format and changed-line tidy gates pass.","path":"docs/issues/60-done/BPR-1431-search-with-re2.md"}}]
---

# Search with RE2 instead of a hand-written regex engine

RFC-BPR-0077 shipped Find and Find in Files on a hand-written Thompson VM,
a KMP literal path and 6,460 lines of generated Unicode tables in core. The
2026-10-13 critical review confirmed a silent false negative: with Regular
Expression and Whole Word, `a.*` over `ba a` returned a complete empty result
instead of `[3,4)`. The owner asked to replace the engine with a library and to
remove the hand-written code.

## Acceptance

- AC1: Pane Find and Find in Files match on pinned Google RE2 through core's
  `source_regex_engine` interface, implemented by a new `regex` adapter and
  supplied by the composition root. Core links no library; RE2 and Abseil are
  pinned, hash-verified archives with notices, provenance and inventory entries.
- AC2: The hand-written VM, KMP path, Unicode tables, their generator and
  `third_party/unicode` are removed.
- AC3: Whole Word resumes a refused match at the next start whose previous
  scalar is not a word scalar, so the review's counterexample and similar
  cases find their whole words. Lookaround, backreferences and `\C` stay
  refused before compilation.
- AC4: The existing core, host, public-path and performance courses pass on the
  new engine, and the committed performance thresholds still hold.

## Evidence

- AC1: `src/adapters/regex` implements `core::source_regex_engine` with RE2
  2025-11-05 and Abseil LTS 20250814.2 from `cmake/BprRe2Provider.cmake`
  (SHA-256, size and license digests checked; offline cache honoured; tests,
  install rules, ICU and PCRE off; C++23 as Abseil requires). Both are acquired
  only in the Qt graph, like Glaze, so the Qt-free core graph still needs only
  SQLite (`dependency_modernization_contract`). `third_party/re2`,
  `third_party/abseil`, `packaging/dependencies.json` and `NOTICE_FILES`
  carry the notices; `package_runtime_contract`, `release_security_audit_contract`,
  `binary_dependency_audit_contract` and `binary_dependency_surface` pass.
- AC2: `source_search_unicode_data.hpp` (6,460 lines), its generator and
  `third_party/unicode` are deleted; the VM and KMP code are gone from core.
- AC3: `core_source_matcher` pins `a.*` over `ba a` as `[3,4)`, `TODO.*` after a
  refused `xTODO`, and `aa` in `aaa aa`; lookaround, backreferences and `\C`
  (outside `[...]` and `\Q...\E` quoting) refuse as unsupported.
- AC4: `core_source_matcher`, `core_source_matcher_reference`,
  `core_source_search_policy`, `application_source_search_host`,
  `application_source_pr_search`, `application_epoch_host`,
  `application_source_find` (1x, 1.25x, 2x), `application_source_pr_find`
  (1x, 1.25x, 2x), `application_source_pr_search_scope`,
  `application_source_pr_search_retry` and
  `application_source_pr_search_performance` pass. On the large public course,
  warm matching fell from 1,568,828 µs to 348,163 µs (warm final 1,649 ms to
  436 ms), cancellation stayed 3 ms, peak RSS 208,257,024 bytes; thresholds hold.
- Gates: `core_dependency_boundary`, `core_boundary_contract`,
  `architecture_boundaries`, `architecture_boundary_contract` and the
  documentation gate pass; clang-format 22 applied; clang-tidy 22 reports no
  finding on changed lines.
