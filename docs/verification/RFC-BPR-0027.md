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

## M2 — session retention (BPR-1216)

Source candidate: `3381a02c5a78f4611fd7ab593cc0f6086ec32208`.
Assignment: `/root/rfc0027_m2`, `session-retention`, mandate r2, sole product
writer in worktree `57ed` on `codex/rfc0027-advanced`. Main `6e8ca8a8` was
fast-forwarded before the claim, preserving the concurrent RFC-0022 milestone.

The concrete Quill sink owns a native session-file service. Each logger creates
an exclusive `session-<128-bit-session>-<segment>.jsonl`; it never reopens a prior
session or appends to a crash tail. Defaults are 5 MiB per segment, 50 MiB total,
64 files (therefore at most 64 active sessions), and 30 days for inactive files.
The directory inventory has an independent 128-entry safety ceiling. A persistent
private `.retention.lock` serializes admission, pruning and writes. Current files
hold native lifetime locks; completed segments become reclaimable. All pruning
rechecks exact owner-private, single-link native identity. Unknown entries,
symlinks/reparse points, changed identities and unsafe permissions refuse storage.
POSIX operations use pinned directory descriptors; Windows uses the existing
private descriptor and retained parent handles with native file locks.

Native writes happen on the Quill backend, without a userspace file buffer.
Producer ingress retains its bounded dropping queues and try-lock. A contended
retention transaction drops and increments loss, while capacity/write/rotation
failure reports full/unavailable. Admission tries at most 100 one-millisecond
waits before busy. Normal close drains after producer quiescence. Kernel/device
stalls and crash durability remain outside the guarantee. Idle expired files
are pruned on the next startup/write; completed segments in a running session
may be pruned. External same-user modification is not constrained by a disk quota.

The existing metadata preview and cleanup recognize the shared closed session
filename grammar and legacy `diagnostics.log`. Each partial tail contributes one
omission, including a valid-looking legacy prefix missing only its newline.
Original input bytes count toward the 16 MiB read budget even if that tail is
removed from the in-memory classifier input. Clear/purge preserves the lock inode
and its directory while removing the prepared diagnostic files. Frozen metadata
export is still limited to 16 MiB input/65,536 records; larger selections refuse
instead of truncating. Richer scope/export policy remains M4.

### Commands and results

```sh
cmake --build --preset dev --target test_quill_logging test_session_retention test_application_logging test_release_maintenance_host test_release_maintenance_files test_release_maintenance_public_path test_profile_management_public_path test_profile_multi_window_public_path bench_better_pull_review_large_review -j 8
ctest --preset dev --output-on-failure -R '^(logging_session_retention|logging_session_concurrency|quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_logging|application_logging_cancel_startup|application_logging_full|application_logging_unavailable|application_release_maintenance|sqlite_release_maintenance_files|application_release_maintenance_public_path|application_profile_management_public_path|application_profile_multi_window_public_path|architecture_boundaries)$'
python3 _coding-standard/scripts/format.py --root . --dry-run src/adapters/logging/include/bpr/logging/quill_log.hpp src/adapters/logging/src/quill_log.cpp src/adapters/logging/src/session_files.cpp src/adapters/logging/src/session_files.hpp src/application/include/bpr/application/diagnostic_log_files.hpp src/adapters/sqlite/src/release_maintenance_files.cpp tests/support/diagnostic_files.hpp tests/logging/quill_logging_tests.cpp tests/logging/session_retention_tests.cpp tests/end_to_end/application_logging_tests.cpp tests/end_to_end/profile_management_public_path_tests.cpp tests/end_to_end/profile_multi_window_public_path_tests.cpp tests/end_to_end/large_review_benchmark.cpp tests/end_to_end/release_maintenance_public_path_tests.cpp tests/sqlite_contract/release_maintenance_files_tests.cpp
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/adapters/logging/src/quill_log.cpp src/adapters/logging/src/session_files.cpp src/adapters/sqlite/src/release_maintenance_files.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

Final build and 15-file format passed. Tidy passed all three product translation
units, reusing the unchanged session-file unit after its own passing rerun.
The documentation gate and its 64 tests passed. Final integration passed **15/15**
on that exact source candidate in 129.49 seconds; all task-owned build, test and
tidy commands completed. There is no attended session/server or protected candidate.

The early unchanged public logging/content checks passed 2/2. The initial build
found the old tests' numeric constructor arguments; these now use explicit
limits. First candidate testing passed 5/6: the public maintenance legacy fixture
needed owner-private creation now that the logger creates a different file.
The corrected fixture passed. New retention and public failure/cancellation
courses passed 6/7; an added assertion incorrectly expected a textual exported
outcome, and its first correction accessed the prepared context instead of its
Selection, failing compilation. A subsequent 14/15 run still used that older
binary and is not final-candidate evidence. Both assertion issues are corrected.
Tidy initially refused the enum exception; a fixed std::exception-derived refusal
resolved it. An intermediate read-budget edit omitted the local declaration and
failed compilation; the final helper compiles and preserves the original-byte cap.

The concise self-review separated native storage from record encoding, kept the
startup retry and multi-file snapshot loops in named helpers, removed obsolete
stdio dependencies, and checked lock lifetime, overflow, ownership and partial
record boundaries. It found and corrected the valid-looking legacy-tail issue
and retained-input budget before the final candidate.

Native evidence is macOS arm64 with the existing Debug Qt 6.11.2/Quill 13.0.0
cache. The direct storage course tests rotation, aggregate/count limits, expiry,
protected active sessions, refusal of linked/obstructed files, and an actual
kernel short write followed by RLIMIT_FSIZE failure, then restart. That is a real
native write-failure path, not an observed ENOSPC-filled filesystem. Four separate
processes exercise one directory and verify bounded files plus exact session
ownership. Public courses exercise provider-to-file success, full/unavailable
logging without disabling the application, cancellation, shutdown, profile/window
isolation, and real Settings preview/export/clear. No native Windows/Linux,
packaged-platform, human-control, live sign-in or crash-survival claim is made.
Those full-RFC obligations remain open for the later milestones.
