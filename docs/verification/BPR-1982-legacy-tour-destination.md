# BPR-1982: Saved legacy tour destination compatibility

Mandate `tour-destination-repairs-r1-legacy-destination-fix`; assignment
`legacy_destination_fix`, milestone `legacy-destination-fix`, baseline
`992ebba9c6ea35822a24ea6c315676a865fb5f65`. This post-review follow-up
preserves BPR-1981 and BPR-1979 historical acceptance.

Source/test candidate `4e21d72a50135e7226e4c0a3ab389cebff03bf20`.

## Regression and correction

Before the new confirmed Destination semantics, a completed replacement receipt
could retain imported Destination A while moving author-selected Mapped B into
Original B, clearing Mapped and setting Mapping to exact_original. Remote and
body baselines already named the replacement at B. The prior SQLite codec
preserves both source fields. Giving Destination unconditional precedence then
conflicted against the correct remote B after upgrade.

The shared core `published_tour_destination` recognizes exactly the completed
legacy shape (Remote, Original, no Mapped, exact_original) and uses Original.
Other states retain confirmed Destination precedence, independently of proposed
mappings. Session verification, destination comparison and observed source
deltas use the same rule. Before an ordinary local source edit or head
observation changes mapping state, core retains that effective confirmed subject
in Destination. Original and Mapped are not rewritten by this repair.
No codec/schema migration, remote capability or visual behavior was added.

The permanent production-session regression publishes a fixture review, constructs
the historical completed receipt, saves it through `make_review_tour_store`,
closes/reopens the actual SQLite store, asserts the entire receipt is equal,
then explicitly maintains the replacement with one body edit. It also tests a
modern receipt whose confirmed Destination differs from Original. Seven controls
per mode reject wrong node, actor, URL, review, kind, local confirmed source and
remote line without writes. The earlier mixed namespace regression remains in
the same session suite. Source ranges and remote discussion identity are checked
after maintenance. Core controls cover a later replacement C, selecting B again
as no replacement, no spurious source delta and head observation.

Runtime logs are external at `/tmp/bpr-legacy-destination-fix`; independent
reviewer reproduction remains at
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1981-review-o69kane5`.
No credential, raw transport log, build tree or authenticated URL is tracked.

## Verification

- Baseline new regression build passed. `ctest --preset dev -R '^staged_review_session$' --output-on-failure` failed at the new maintenance assertion after actual SQLite exact-reopen passed: CTest exit8, child exit1, 2.62seconds (`baseline-build.log`, `baseline.log`).
- Focused LLVM22 `tidy.py --build-dir build/dev --paths` for the six changed translation units with `--warnings-as-errors --jobs 3` passed (`tidy.log`).
- Initial core-only build selection exited1 before compiling because `test_review_tour` is not a target (`core-build.log`); corrected to `test_review_tour_core`.
- The first dev and core-only compiles failed on two new core test command initializers written out of declaration order (`fixed-build.log`, `core-build-2.log`); corrected field order. Final dev build of session, five core, SQLite and composed publication targets passes (`final-build.log`); final core-only build of five core targets passes (`core-build-final.log`).
- `ctest --preset dev -R '^(staged_review_session|staged_review_restart|sqlite_review_tour_storage|core_review_tour(|_update|_destination|_repair|_import)|application_review_tour_publication_(native|compat|repairs|mixed|file))$' --output-on-failure`: twelve courses pass; session reaches a new modern negative whose changed Destination disagrees with its Mapped source, so structural validation correctly refuses that fixture (`focused-tests.log`, 24.71seconds). Corrected the negative to retain a structurally consistent wrong confirmed/mapped source; the legacy arm and its demonstrated baseline failure are unchanged.
- After rebuilding that test (`session-build-final.log`), `ctest --preset dev -R '^staged_review_session$' --output-on-failure` passes, 2.39seconds (`session-tests-final.log`). This includes both SQLite reopen modes, fourteen zero-write negatives, exact one-edit maintenance and existing mixed namespace/restart/session controls. The other twelve passing integration courses have unchanged relevant inputs and are reused.
- Final LLVM22 format passes all seven changed C++ files (`format-final.log`). All six TUs pass warnings-as-errors tidy; unchanged source successes are in `tidy.log`, final core/session test successes in `core-tidy-final.log` and `session-tidy-final.log`. `git diff --check` passes.
- `python3 scripts/verify_documentation.py --root .` passes (`docs.log`); `python3 tests/scripts/verify_documentation_tests.py` passes 67/67 (`doc-tests.log`).
- `ctest --preset core-only -R '^(core_review_tour(|_import|_destination|_repair|_update)|architecture_boundaries|architecture_boundary_contract)$' --output-on-failure`: all seven pass (`core-tests-final.log`); no timing acceptance is inferred.

No independent/human/native acceptance is required by this correction mandate
or claimed by these checks. Existing separate live/native qualification gaps
retain their scope and status. No live remote mutation or desktop effect occurs.

## Criterion judgment and handoff

- AC1 passes: the new permanent regression fails on baseline after exact SQLite
  reopen, then passes on the candidate with the confirmed remote B and one body
  edit; it creates no replacement review or comment.
- AC2 passes: fourteen wrong identity/source controls refuse with zero writes,
  modern confirmed Destination stays independent of Original/Mapped, and core
  preserves B before later author-selected retarget/head transitions. Existing
  mixed overlapping-namespace and repaired publication courses pass.
- AC3 passes: focused integration/recheck, core-only and both architecture
  checks, LLVM22 format/tidy, documentation/67regressions and diff check pass
  against unchanged source/test bytes of the candidate. Actual initial compile
  and negative-fixture failures above remain part of the record.

All task-owned build/test/tidy/documentation commands have exited and results
are consumed. No review server, session/protection, task worktree or other writer
remains. Foreign `.codex-attachments/` is preserved. Source and guarded tracker
commits are recorded in the [checkpoint](../issues/_checkpoints/BPR-1982/handoff.md).
No push or external mutation occurred.
