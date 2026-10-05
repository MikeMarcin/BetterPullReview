---
schema: "implement-rfc.card.v1"
id: "BPR-1076"
title: "Implement the complete catalog and bounded lossless prefix recognizer"
status: "Done"
rfcs: ["RFC-BPR-0047"]
depends_on: ["BPR-1075"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-02","milestone":"semantics","acceptance":["R01","R02","R03","R04","R06","R40"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"command","detail":"Catalog equals LABEL-CATALOG.json field for field and in order; every label recognized identically in 3 forms x 3 cases x 6 decoration lists (648 combinations); all 48 parser-cases.json cases pass status/label/expectation/required diagnostics against the real core recognizer. FEEDBACK_CONVENTION_OK on macOS arm64 core-only Debug -Werror; negative controls (indent bound, last-token-wins, decoration bound, '(' requirement) each failed the test.","command":"ctest --test-dir build/core-only -R core_feedback_convention (tests/core/feedback_convention_tests.cpp against the pack's specs/LABEL-CATALOG.json and fixtures/parser-cases.json)"}},{"criterion":"AC2","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"command","detail":"288 generated bodies (24 wrappers x 12 labels: quote, fences, suggestion fence, indented/tab code, HTML, lists, headings, link, inline code, later paragraph, unsupported emphasis, NBSP, ZWSP) all plain with no label or token; corpus quoted/fenced/indented/tabbed/list/heading/html/later/inline-code/link cases plain; a tour envelope classifies only through map_tour_stop's Narrative.","command":"ctest --test-dir build/core-only -R core_feedback_convention (tests/core/feedback_convention_tests.cpp against the pack's specs/LABEL-CATALOG.json and fixtures/parser-cases.json)"}},{"criterion":"AC3","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"command","detail":"20 malformed shapes keep the raw token with one exact diagnostic and no decorations/spans; unknown labels stay unknown_label with unspecified expectation; body byte-identical after every call and digest equals SHA-256 of the body; limited at the 1024-byte budget+1 and the 33rd decoration, never truncated (60 KiB subject recognized); 1 MiB pathological inputs and 40,000 generated bodies inspect at most 1024 bytes. Core purity: core_dependency_boundary, core_boundary_contract, architecture_boundaries, architecture_boundary_contract pass.","command":"ctest --test-dir build/core-only -R core_feedback_convention (tests/core/feedback_convention_tests.cpp against the pack's specs/LABEL-CATALOG.json and fixtures/parser-cases.json)"}},{"criterion":"R01","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"artifact","detail":"Twelve first-class IDs with identical fields and own icon roles; aliases nit/typos/polishing/quibbles are not labels. The only advertised feature so far is the core catalog/recognizer; later consumers must reuse it.","path":"docs/verification/RFC-BPR-0047-0051.md"}},{"criterion":"R02","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"command","detail":"Plain, unknown-label, malformed and limited bodies are returned untouched (byte comparison and digest per call); no API rewrites Markdown.","command":"ctest --test-dir build/core-only -R core_feedback_convention (tests/core/feedback_convention_tests.cpp against the pack's specs/LABEL-CATALOG.json and fixtures/parser-cases.json)"}},{"criterion":"R03","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"command","detail":"Start-of-body subset (BOM, blank lines, 0-3 spaces, plain/strong forms, ASCII case) accepted; quoted/code/HTML/list/heading/link/later-paragraph false positives refused.","command":"ctest --test-dir build/core-only -R core_feedback_convention (tests/core/feedback_convention_tests.cpp against the pack's specs/LABEL-CATALOG.json and fixtures/parser-cases.json)"}},{"criterion":"R04","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"command","detail":"Custom decorations retained raw in order (including duplicates, with duplicate_decoration); if-minor typed separately; distinct standard tokens give conflicting with conflicting_decorations.","command":"ctest --test-dir build/core-only -R core_feedback_convention (tests/core/feedback_convention_tests.cpp against the pack's specs/LABEL-CATALOG.json and fixtures/parser-cases.json)"}},{"criterion":"R06","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"command","detail":"Guidance from the catalog, expectation only from decorations; note (blocking) keeps both plus advisory label_expectation_conflict; compile-time concepts show the annotation has no resolution, defect or verdict field.","command":"ctest --test-dir build/core-only -R core_feedback_convention (tests/core/feedback_convention_tests.cpp against the pack's specs/LABEL-CATALOG.json and fixtures/parser-cases.json)"}},{"criterion":"R40","outcome":"pass","revision":"dca6d277261eecf130d7451552337059beaf3e50","evidence":{"kind":"artifact","detail":"Recorded budgets: 1024-byte header window, 32 decorations, 32-byte label word; pathological 1 MiB inputs bounded; core links no effectful dependency; spans only, no rendering.","path":"docs/verification/RFC-BPR-0047-0051.md"}}]
---

# BPR-1076: Implement the complete catalog and bounded lossless prefix recognizer

Pack alias: **PACK-CC-02** (live `BPR-1076`, allocated at import).

## Outcome and work

Implement the twelve-ID catalog and the bounded grammar in the Qt-free core. Return raw/body spans and diagnostic facts; do not reconstruct Markdown. Add the supplied positive/negative corpus and size/pathological tests. Keep guidance separate from explicit expectation and retain custom decorations.

## Acceptance

- Every catalog label has identical feature capability and all documented parser cases pass against real core code.
- Quoted, code, HTML, list, heading and later-paragraph examples cannot classify the outer body.
- Malformed, unknown and limited input remains readable with no effects or destructive normalization.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation evidence, 2026-10-05

Product commit `dca6d277261eecf130d7451552337059beaf3e50` adds
`src/core/include/bpr/core/feedback_convention.hpp`, `src/core/src/feedback_convention.cpp`
and `core_feedback_convention` (`tests/core/feedback_convention_tests.cpp`). The test reads
the pack's own `specs/LABEL-CATALOG.json` and `fixtures/parser-cases.json`. The
[execution ledger](../../verification/RFC-BPR-0047-0051.md) records the grammar decisions
(1024-byte header budget, 32 decorations, 32-byte label word, guidance contradiction
table) and the full evidence.

- **Catalog and capability (R01).** The twelve entries equal the pack catalog field for
  field and in order, each with its own icon role. Each label is recognized in
  3 forms × 3 letter cases × 6 decoration lists (648 combinations) with identical
  results. Aliases (`nit`, `typos`, `polishing`, `quibbles` …) are not labels.
- **Documented cases (R02–R04).** All 48 corpus cases pass with their status, label,
  expectation and required diagnostics. Custom decorations keep their raw text and
  order. `if-minor` stays conditional, and distinct standard tokens give
  `conflicting`.
- **False positives (R03).** 288 bodies (24 wrappers × 12 labels) all stay `plain`:
  quote, fences, suggestion fence, indented and tab code, HTML, lists, headings,
  link, inline code, later paragraph, unsupported emphasis, NBSP and zero-width
  space.
- **Readable and non-destructive (R02, R40).** 20 malformed shapes each report their
  exact diagnostic and keep the raw token, with no decorations or spans. The body is
  byte-identical after every call. `limited` is tested at the budget and one byte
  past it; a 60 KiB subject is not truncated. Ten 1 MiB pathological inputs and
  40,000 generated bodies stay within 1024 inspected bytes and keep every span
  invariant.
- **Separate facts (R06).** Guidance comes from the catalog and expectation only from
  decorations. `note (blocking)` keeps both and adds an advisory diagnostic. The
  annotation type has no resolution, defect or verdict field; concepts in the test
  check this at compile time.
- **Managed segments.** A labelled tour stop is recognized only through
  `map_tour_stop(...).Narrative`. The whole envelope and an unmapped edited stop are
  `plain`, the latter with `classification_unavailable`. Permalink prose is
  recognized at its own offset.
- **Negative controls.** Four reverted mutations each failed the test at the expected
  check: indent bound, last-token-wins, decoration bound and the `(` requirement.

Limitations: this is the macOS arm64 AppleClang core-only Debug run only, and the
Linux/Windows station runs are owed. Header edits, roots, caches, the Qt span
mapping and the CLI belong to later cards. No fuzz selector was added; BPR-1093
covers adversarial verification.

**Note: Linux run at `d2187e3d1` (holds `887119820`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, the `core-only` preset, `build/bpr1076`).**
`test_feedback_convention` built with no GCC warning or error. Run as
written, all 4 rows were "Not Run": the command builds only
`test_feedback_convention`, and each network-isolated row needs
`better_pull_review_network_sandbox`, which is not built
([[BPR-1180]] and [[BPR-1185]] had the same gap). After building the whole
`core-only` tree, also with no warnings, 4 of 4 passed:
`core_feedback_convention` (2.2 s), `core_review_tour_markdown`,
`core_dependency_boundary` and `core_boundary_contract`. Nothing needed a
fix. `build/bpr1076` was removed. Done is not reopened.

### Windows core-only (bpr-main-win, 2026-10-05)

Windows 11 Home 10.0.26200, MSVC 14.51, at `34ab71337` (which holds
`8508f0c9a`): `cmake --preset core-only -B build/bpr1077`, the whole tree
built.

MSVC first refused `src/core/src/feedback_convention.cpp:500`, with errors
C2440 and C3535 on `const auto* found = std::ranges::find_if(catalog, …)`.
Over a `std::array`, MSVC's STL returns an iterator class, not the raw pointer
libstdc++ and libc++ return. This also broke every Windows build of `main`.
The fix, `const auto found`, is BPR-1077's MSVC fix.

With it, the whole core-only tree builds with no `warning C` or `warning LNK`
line. `ctest -R "core_feedback_|core_review_composition|core_review_tour_markdown|core_dependency_boundary|core_boundary_contract"`
passes **8 of 8**: `core_feedback_convention`, `core_feedback_header_edit`,
`core_feedback_conversation`, `core_feedback_partition`,
`core_review_composition`, `core_review_tour_markdown`,
`core_dependency_boundary` and `core_boundary_contract`.
