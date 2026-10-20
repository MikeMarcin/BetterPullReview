# RFC-BPR-0088 verification

Mandate `qt-6-12-r2`; baseline `1a7d74f003b656469f5c6822c655a314fb58f663`.
Upgrade source candidate: `b433d515f` (pins in `9f4f16997`, compatible drag fixture in `db072ffd7`, contained recipient fixture in `b433d515f`). Branch: `codex/qt-6-12`.

The owner explicitly accepted macOS 14.4+ for future Qt6.12 builds, then added
full macOS suite validation and a subsequent coordinated three-platform v0.3.1
release. This ledger covers the upgrade milestone; the release has separate
exact-artifact, trust, native and vulnerability obligations. Published v0.3.0
requirements remain13+. No product C++ or visual design change was required.

## Actual upstream inputs

The three official source downloads match their `.sha256` manifests; the
following values are also computed over downloaded bytes, not inferred.

| Input | Bytes | SHA-256 |
|---|---:|---|
| QtBase6.12.0 | 51220268 | `a951bd163c7b80fc6b8c88d7668fb56abf91c152373e13c10666763238131307` |
| QtSvg6.12.0 | 2341876 | `e4ab39534ec97987b1b9b60ef7f4d3253d5912a69f8c713a01753174a4029331` |
| QtImageFormats6.12.0 | 1848620 | `0943132b5db8de6b6a7f1d162682173e8763c7ed94e80fd39113292845f67762` |
| Official Linux ICU archive6.12.0-0-202609280346 | 9660660 | `1fdbcd67b1760f3a3f031c64637a9f19992aed2f35aa615e12a468f8c655d4da` |

Origins are recorded in `packaging/dependencies.json`; official release:
[Qt6.12 release announcement](https://www.qt.io/blog/qt-6.12-released).
The Linux SDK metadata, actual QtCore ELF NEEDED entries and producer SPDX
all require ICU73.2 data/uc/i18n. All three raw files match the existing
recorded hashes byte for byte; the source-built hardening recipe, source,
notices and recipient helper stay unchanged. Actual macOS producer SPDX joins
six selected binary records and the Svg/WebP/libwebp attribution closure.
All 55 source notice files, including libwebp COPYING/PATENTS/AUTHORS, are
present in the new archives. These are metadata/source checks, not a delivered
package or native Linux runtime qualification.

## Verification

All paths below are relative to the checkout unless absolute. Runtime logs
are external in `/tmp/bpr-qt612-run` and are not committed.

| Criterion | Command or direct observation | Result |
|---|---|---|
| AC01 | Fresh-cache `cmake -P /tmp/bpr-qt612-run/provider-course.cmake`; actual `bpr_acquire_qt`/pinned aqt3.3.0 | PASS: official6.12.0, Svg/WebP validated |
| AC01 | Same provider with cached offline, empty offline, wrong6.11.2 root and missing WebP prefix | PASS: cached success; other three controls refuse |
| AC01 | `cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DBPR_QT_PROVIDER=ON -DBPR_QT_ROOT=/tmp/bpr-qt612-run/qt/6.12.0/macos` | PASS |
| AC01 | Pinned upstream Windows aqt revision `list-qt windows desktop --arch 6.12.0` | PASS: msvc2022_64 exists; native Windows acquisition/build unobserved |
| AC02 | Actual source/ICU downloads, hashes, raw ICU files and actual producer SPDX/notice closure | PASS as above |
| AC02 | `python3 tests/scripts/package_runtime_tests.py` | PASS: 29 cases, one native-Linux-only skip |
| AC02 | `python3 tests/scripts/release_source_assets_tests.py` | PASS: 23 cases |
| AC02 | `python3 tests/scripts/verify_release_security_tests.py` | PASS: 18 cases |
| AC03 | App/composed GitHub/Markdown/image-decoder targets via `cmake --build --preset dev --target ... --parallel 6` | PASS |
| AC03 | Focused CTest app self-test, composed hosts/GitHub/Markdown and image decoder | PASS:5/5 |
| AC03 | Full `cmake --build --preset dev --parallel 6` | PASS after drag-fixture repair |
| AC03 | `python3 scripts/run_test_suite.py --preset dev --output-dir /tmp/bpr-qt612-run/full-suite-r2` | PASS: complete 811-row manifest, 810 passed, one configured Debug skip, zero failures; exit0, 4407.875 seconds |
| AC04 | `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py` | PASS: DOCUMENTATION_OK, 67 controls |

The focused app build used:

```sh
cmake --build --preset dev --target app_better_pull_review test_better_pull_review_qt_github_tests test_better_pull_review_markdown_image_decoder_tests test_better_pull_review_composed_markdown_tests --parallel 6
ctest --test-dir build/dev -R '^(application_public_path|composed_application_hosts|qt_composed_github_scale_100|qt_composed_markdown_1_0|qt_markdown_image_decoder)$' --output-on-failure
```

The early preview build overlapped its reconfigure and temporarily lost the
KSyntaxHighlighting installed header prefix. Sequential pinned configure/build
passes without a product repair. The original failure is retained externally.
The initial full build then exposed deprecated QPoint drag-event constructors
in the Review Tour fixture under Qt6.12. The fixture now passes QPointF for
Qt6.12+ and retains QPoint for earlier caller-installed Qt; a standalone
constructor syntax check passes against both6.11.2 and6.12.0 with deprecations
as errors. Clang-format22 checks the touched fixture. No warning was disabled.

The first dependency contract run refused acquisition because the new pin was
absent from the shared CPM cache inside the network-denied test boundary. The
normal public provider populated that cache outside isolation; the full suite
re-exercises the unchanged contract there. Its original failure remains
retained in the external dependency-contract log.
Released aqt3.3.0 still cannot enumerate the Windows split repository; the
existing pinned upstream Windows installer revision succeeds and is retained.

Target-only plan/checkpoint discovery currently refuses an unrelated existing
plan's missing boundary fields. Explicit selected-plan binding succeeds; the
[durable checkpoint](../issues/_checkpoints/BPR-1665/handoff.md) preserves the
canonical binding and exact procedural limitation without changing that plan.

The first full CTest attempt was interrupted at268/811 after the host volume
ran out of space. Scratch-root creation and SQLite writes reported ENOSPC;
those rows establish no product failure or pass. Its incomplete log and timing
report are retained externally, with exit130. Only task-owned duplicate SDK
cache and 1281 object intermediates (2.36GiB) were removed; all executables,
libraries, prior releases and other worktrees remain intact. Free space before
the maintained rerun was7.8GiB. No build runs alongside that suite.

That first full attempt also found an existing containment-policy violation in
`xdiff_recipient_rebuild_tests.py`, introduced before this upgrade. With the
coordinator's full-gate repair authorization it now uses the established
`ContainedTemporaryDirectory`, retaining identical POSIX semantics and the
required Windows container ACL behavior. Focused usage6/6 and helper11/11
checks pass; the original failing observation remains in the interrupted log.

## Completed full-suite evidence and skips

The maintained runner used four independent jobs and validated all 421
GUI/end-to-end/performance rows as RUN_SERIAL. Final JUnit and timing records
have no missing or unexpected rows. The source candidate is
`b433d515f5a3a513bcecc0f520d8d04c38454c6a`; subsequent closeout edits change
documentation only. The developer app SHA-256 remained
`e49e34d24c350fc6d24a9017aa08e960a2add8bf7113e5f2c06c66a71abd017e`.
The suite also built and exercised its fresh tests-disabled Release composition
graph. These are available-host engineering observations, not installed release
qualification. No competing build ran during either performance section.

The single CTest skip is `application_large_review_cache_bytes`, which exits77
in Debug by its existing optimized-only policy. It is not a pass. The release
owner must run its registered CTest row in a tests-enabled Release tree on the
integrated candidate; the v0.3.0 RSS exception does not apply to v0.3.1. Exact
configure/build/test commands are in the [checkpoint](../issues/_checkpoints/BPR-1665/handoff.md#optimized-release-obligation).

Three inner unittest cases were skipped: native Linux CPack baseline, native
Windows poisoned-PSModulePath Authenticode, and comparing multiple installed
KSyntaxHighlighting configurations in this single-configuration Debug tree.
The Git-match corpus additionally reports six case-distinct filename subcases
unsupported by this case-folding filesystem and two invalid-repository cases
refused by installed Git. These are not additional passing observations.
The exact twelve records, including the CTest skip, are retained externally in
`full-suite-r2/skip-inventory.json`. The stale build `LastTestsFailed.log` still
names the earlier dependency-acquisition failure; its copy is explicitly named
`stale-prior-LastTestsFailed.log`. Final JUnit/timings establish zero failures.

Actual full-suite reports are retained in external `/tmp/bpr-qt612-run`:
`full-suite-r2/{manifest.json,junit.xml,ctest.log,timings.json,LastTest.log}`.
The task build and private SDK remain available by coordinator request for the
separate scrolling repair and release work. No task-owned command remains
active. The tracked execution-input manifest covers 3062 files; final closeout
must preserve their exact Git identities.

Final documentation structure and all 67 documentation controls pass. A
concurrent task advanced the shared allocation high water to BPR-1667; the
documentation and finish guard correctly refused this branch's stale counter.
Importing only that existing main counter/receipt commit restores consistency
without allocating another ID or changing execution inputs. The canonical
finish helper closes BPR-1665 and the RFC/index on this task branch.

## Remaining evidence boundaries

The available host is macOS arm64. Offscreen checks do not establish owner
visual judgment, Intel execution, Windows/Linux native execution, clean-account
authentication/vault/installation/removal, crash privacy, vulnerability
applicability, signing/notarization or release approval. Existing M8/M9/native
and vulnerability gaps remain explicit. The coordinator owns subsequent
three-platform v0.3.1 release work; no release was published by this milestone.
The owner's separately reported Threads scrolling/flicker problem remains a
release blocker assigned to another implementer. This upgrade does not claim
to fix it.
