# RFC-BPR-0027 advanced implementation evidence

Mandate r2, 2026-09-26: implement the full advanced phase and land each accepted
milestone on main. Immutable RFC base:
`55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`. The prior deferral is superseded;
BPR-0971's closed provider sanitizer and historical acceptance remain intact.
No live sign-in, GitHub read or PR mutation is authorized by this work.

## M1 — structured events (BPR-1215)

Source candidate: `ac64e5ad0dc4694dad4c6db3a40f12479b11503e`.
Assignment: `/root/rfc0027_m1`, `structured-events`, mandate r2, sole product
writer in worktree `57ed` on `codex/rfc0027-advanced`.

New records are canonical JSONL v2, encoded into fixed 16 KiB storage before
Quill receives a view that it copies. The adapter supplies UTC milliseconds,
monotonic elapsed microseconds, a random 128-bit logger session, increasing
sequence, stable event/category/severity and a source/toolchain build identity.
Existing window/request, operation, HTTP, failure and sanitized summary fields
remain typed. Invalid framing bytes are refused and counted before enqueue.
File admission, caps, dropping queues and lifecycle ownership are unchanged.

The existing maintenance preview reads both the legacy v1 prefix and canonical
v2 JSONL. It reconstructs the same small metadata export; no free text, timestamp,
build, session or request identity is copied. Unknown, malformed, duplicate-key,
unsupported-version and partial JSON records are omitted with counts. This is
compatibility for the current export, not the richer BPR-1218 export policy.

### Direct verification

Native environment: macOS arm64, Debug, pinned Qt 6.11.2 and Quill 13.0.0.
The offline configure reused the existing pinned source cache:

```sh
cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build --preset dev --target test_quill_logging test_application_logging test_release_maintenance_host test_release_maintenance_files test_release_maintenance_public_path test_profile_management_public_path test_profile_multi_window_public_path bench_better_pull_review_large_review -j 8
ctest --preset dev --output-on-failure -R '^(quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_logging|application_logging_full|application_logging_unavailable|application_release_maintenance|sqlite_release_maintenance_files|application_release_maintenance_public_path|application_profile_management_public_path|application_profile_multi_window_public_path|architecture_boundaries)$'
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/adapters/logging/src/quill_log.cpp src/application/src/release_diagnostic_snapshot.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

The initial public-path foundation run passed `application_logging` and
`quill_logging_content_policy` (2/2). Candidate focused testing passed the
composed success/full/unavailable and sanitizer/backpressure courses; the first
`quill_logging` run failed an old dynamically assembled plaintext assertion.
That assertion now reads the JSON field. The independent schema, timing,
escaping and malformed-record checks had already passed before that failure.
The tidy helper's first invocation used positional paths and exited at argument
parsing; the corrected `--paths` invocation passed both product translation units.

Final integration: 12/12 passed in 131.16 seconds on the unchanged source
candidate. The concurrent main merge contained documentation only; source
and test inputs remained identical.
The large-review consumer compiled; no performance/benchmark acceptance is claimed.
Format checking passed all nine touched C++ files with clang-format 23.1.1.
The documentation gate and its 64 tests passed. Their expected negative-fixture
error messages are not repository documentation failures.

Tests parse actual file bytes using Qt's independent JSON parser and verify all
25 fields, fresh sessions after reopen, sequence/monotonic progress, contemporary
UTC, severity, stable build identity, maximum escaped summaries and exact omission
counts. Host/file/public maintenance tests retain the legacy export contract.
The public application test signs in only through the credential-free fixture,
loads its synthetic PR, observes a controlled cache refusal, drains shutdown and
reads the real profile log. Forbidden-content canaries exercise the unchanged
credential-owner sanitizer through the real sink and exporter. File/full/loss and
blocked-stderr courses verify refusal, concurrent owners and bounded drainage.

### Scope and limitations

This milestone establishes the structured event portion of AC-02, AC-03 and
AC-06 through AC-08. It does not close any full RFC criterion. File rotation,
aggregate/age retention, priority queues, recent context, settings/privacy barriers,
expanded operation coverage, rich anonymized export and the complete native
platform matrix remain BPR-1216 through BPR-1219. The current fixed file can still
stop at its 16 MiB ceiling. A crash can leave an incomplete tail; JSONL adds no
crash-durability guarantee. Windows/Linux native evidence and attended controls
remain pending. No new independently reviewed or installed-package claim is made.

The build identity covers first-party C++ sources/headers/source CMake files,
version and compiler/platform facts plus build configuration. It is not a full
binary/package digest and excludes resources, dependencies and other build flags.
The schema uses fixed ASCII sanitized explanations; unsupported input is omitted
or refused rather than copied. Existing v1 bytes and their privacy limits persist.

Documentation impact: system ownership and maintenance parsing contracts, storage
privacy/limits and troubleshooting are updated. GitHub authority/UI semantics,
credential handling, persistence databases and package dependencies have no changed
behavior; provider sanitizer and core source are unchanged. The selected Quill
version and existing provenance remain valid for this slice.

M1 integration: `857d93624aa3acb1e7f590969ae25aecf93a8993` fast-forwarded the
clean primary main checkout and pushed normally to origin/main. Concurrent
review-tour documentation through `1248c0b1` was merged and preserved; no source
or test input changed. The documentation gate passed again after integration.
