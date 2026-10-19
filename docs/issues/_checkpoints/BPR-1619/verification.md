# BPR-1619 verification

Assignment `core-boundaries-v1/core-boundaries`, mandate `core-boundaries-r1`,
nondelegating leaf `/root/core_boundaries` in the main checkout. Baseline
`4bae9df701fdd70e16c0481a1c595e404b4298ce`; plan/reservation `a69668185`;
claim `30515b8b7`; production candidate `381ec842b`. Sibling integration
`530a7b94d` changes tour/epoch sources, outside this mandate. A final header
comment documents the existing admitted-blob/span preconditions; no computation
changed after the production candidate.

## Result and acceptance basis

- **AC1:** Core owns the provider-free line-matcher interface and stateless
  factory. The complete xdiff adapter/target and its application wiring are gone.
  Only `src/core/src/xdiff_line_matcher.cpp` calls the pinned xdiff DSO. The
  application aliases the value/interface names and still owns copied-input
  worker lifetime, scheduling, stop and abandonment. Provider C types stay private.
  The provider, archive, sources and dynamic library are unchanged apart from a
  placement comment and its corresponding packaging-input digest. Separate
  replacement, notices, corresponding source and relinking contracts remain.
- **AC2:** `<atomic>`, `<mutex>` and lexical `<filesystem>` paths are allowed.
  Tests admit passed atomic/mutex references, path construction/decomposition,
  `lexically_normal` and `lexically_relative`. Negative courses reject actual
  reads/writes, current-directory lookup, canonicalization and directory iteration,
  including namespace aliases and imports. Ordinary domain `copy`/`status` names
  remain usable. Review owns the no-hidden-global-state rule; this textual guard
  is not a complete C++ semantic/effect analyzer. Native path conventions still
  differ; no cross-platform identity or encoding equivalence is claimed.
- **AC3:** The unchanged identifier computation moves to core with the span/status
  value contracts and application aliases. Qt packaged grammar, template helper,
  resources, caches and jobs retain their owners. No syntax-definition revision,
  grammar, heuristic, colors, limits or cancellation semantics changed. The
  optional parser evaluation follows the relocated source/symbol; it was compiled,
  not remeasured or newly adopted.
- **AC4:** Focused checks below pass except the documented intentional Debug skip
  and stale historical intraline comparison; exact pre/post-wrapper evidence
  resolves that comparison for the placement mandate. Dependency integration passes. Broader original native/platform
  and RFC-closeout requirements remain on their existing cards.

## Direct computation, host and public composition

Commands are run from `/Users/jaeger/Documents/GitHub/BetterPullReview`.

- `cmake --preset core-only`, then targeted build of
  `test_core_computation_boundaries`,
  `test_better_pull_review_diff_compute_core_tests`,
  `test_better_pull_review_application_diff_host_tests` and
  `tool_diff_xdiff_equivalence`: pass. Final focused CTest selector
  `^(core_computation_boundaries|core_diff_compute|application_diff_host)$`:
  **3/3 pass**, 6.13 s after integration. The core-only direct course uses
  `BetterPullReview::Core` without the directory's application-backed test helper:
  exact matches, line-ending/whitespace corrections, empty final line, namespace
  and enumerator declarations/uses, repeat spans, span exhaustion and stop.
- `nm`/`otool -L` on that core-only executable show both core factories,
  undefined `xdl_diff` resolved through `@rpath/libbpr_xdiff.dylib`, and no Qt.
- Both moved production bodies equal baseline after only the prescribed include,
  namespace/type-qualification changes and clang-format22. No retained state or
  new process-global state was introduced. Existing host tests include the
  **10 ms** matcher-abandonment budget, owned input/matcher release, restart only
  while wanted and the same result after repeated abandonment.
- Regenerated RFC0082 corpus from cached pinned Git archives and committed
  fixtures: **86 pairs, 78 oracles, zero fixture identity mismatches**.
  Recompiled the exact pre-move xdiff wrapper from baseline, linked it to the same
  current engine/host and replayed both binaries: **86/86 output rows identical**,
  including segments, pairs, intraline, approximation and GitHub results; empty
  stderr. All **78/78** recorded GitHub hunk comparisons agree.
- The old `macOS-L-raw.json` comparator reports 29/86 completely equal rows:
  primary segments/pairs and GitHub agreement are preserved, but 57 rows have
  later intraline digests and one also differs in approximation. The identical
  pre-move-wrapper run proves these predate this amendment; the current
  word-token intraline implementation comes from `2429f4ea4` (BPR-1537).
  The historical record and comparator are preserved.
- `cmake --preset dev` and actual app/syntax/public-course target builds: pass.
  Focused public CTest: **6 pass, 1 intentional skip**, 22.54 s. Passing:
  `syntax_classification_contract`, `syntax_refinement_cancellation` (stop during
  refinement, no spans), `application_cpp_identifier_appearance` (real composed
  offscreen path), `application_large_review_active_work`,
  `application_large_review_pathological`, `binary_dependency_surface`.
  `application_large_review_cache_bytes` exits 77 because it requires an optimized
  build; Debug is diagnostic. No new memory benchmark or universal performance
  claim follows from this relocation.
- The sibling's resealed `application_review_round_trip_composition_contract`
  passes, 4.89 s, including the moved application factory/header wiring. Its
  unrelated source/read changes and historical reseal belong to that writer.
- The optional tree-sitter candidate and evaluation harness translation units
  compile against the new core symbol/value contracts and cached runtime headers.
  No new parser-library approval or platform measurement is inferred.

## Boundaries, packages and maintenance

- `core_dependency_boundary`, `core_boundary_contract` (**11 tests**),
  `architecture_boundaries` (41.08 s) and complete
  `architecture_boundary_contract` (191.51 s): pass. The targeted xdiff ownership
  counterexamples pass: private owner only, DSO only, no second source build or
  other linking target.
- `package_runtime_contract`, `release_source_assets_contract`,
  `release_package_audit_contract`, `release_security_audit_contract`: pass.
  `dependency_modernization_contract`: pass, 115.98 s; the complete focused
  structure/dependency/package group is **9/9 pass**, 352.80 s.
- `clang-format 22.1.8` checks all **15** touched C++ files; `clang-tidy22`
  passes both moved production translation units. `verify_documentation.py`:
  **DOCUMENTATION_OK**; `verify_documentation_tests.py`: **67 pass**;
  `git diff --check`: pass. Final board/checkpoint checks are recorded with the completion handoff.

## Failures corrected and remaining limits

Initial scripted relocation paused at the existing one-line CMake link list;
remaining edits were applied explicitly. Initial build target abbreviations were
not valid target names and were corrected. Test support previously inherited
application headers through AdapterXdiff: it now links Application explicitly,
while the dedicated direct course deliberately links core alone. The direct
course's relative test-support include was corrected accordingly. One architecture
negative fixture omitted the designated wrapper source and failed earlier than
its intended assertion; listing that source reaches the intended second-build
rejection. All corresponding focused rechecks pass.

No owner desktop/browser access, live GitHub mutation, push, native release,
attended session or human decision was needed. Original RFC0081 Windows/Linux
measurement and RFC0082 platform/closeout obligations remain separate. No release,
installed-package, all-platform or broader RFC acceptance is implied.

Unrelated writers were preserved throughout: tour-cli documentation, BPR-1377
continuation, tour_remote, epoch host and tour source contracts/implementations,
epoch-host tests, CLI loopback tests and the round-trip composition reseal.
Explicit owned-path commits exclude their work. Current command receipts,
selection, original binding and exact pre/post comparison remain in
`/tmp/bpr-core-boundaries` until final cleanup/handoff.
