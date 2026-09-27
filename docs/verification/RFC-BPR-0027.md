# RFC-BPR-0027 advanced implementation evidence

Current result: all nine product criteria and the three human judgments are
accepted. The final engineering round below records the main landing and the
remaining canonical-order closeout hold. Earlier pending statements are dated
checkpoints, not the current human-review state.

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

M2 accepted and pushed on main as `8ff94031e1279c913cdb59512c2a9609e0779410`.
BPR-1216 is Done. Source behavior is `3381a02c`; the final handoff commit is
documentation only. All task-owned commands are finished. No review session,
server or protected candidate is transferred. Next: BPR-1217 / diagnostic-controls.

## M3 — diagnostic controls (BPR-1217)

Source candidate: `75700f4dd0ce88f86e59940cddf690a6a3e2f45b`.
Merged candidate: `6089117d7bd13ad52d2b34c49713b842cd0f3954`, preserving concurrent
main `2deb889b` (RFC-0022). Assignment `/root/rfc0027_m3`, sole writer in worktree
`57ed`, milestone `diagnostic-controls`, mandate r2; immutable RFC base unchanged.
Claim: `b56d0b2f`. No live service, sign-in or PR mutation was used.

### Behavior and direct evidence

Help → Diagnostics and profile Storage & privacy now expose persistent On/Off,
Standard/Verbose and independent Anonymized/Include locally choices, Open logs
and confirmed Clear. The application owns serialized effects and acknowledgements;
SQLite owns the private versioned preference store; Qt renders accepted state.
Preferences load before the first event. Unknown readable values preserve valid
Off and fall back to Standard/Anonymized; unavailable stores disable capture and
editing. Explicit identity disclosure can be cancelled without changing state.

JSONL v3 adds capture/detail policy and typed request/window joins to the existing
bounded schema. Raw numeric identities are zero by default and retained only
with Include locally; no account/repository/path or content fields were added.
The existing credential-owner sanitizer remains authoritative. Nonempty summaries
must have its sanitized classification. Each provider operation adds a fixed
Verbose start event; errors/warnings remain eligible in Standard. Metadata preview
accepts v1/v2/v3 and continues to omit every identity/prose field.

Normal ingress stops at 3,584 events/7 MiB, with warning/error room inside the
4,096-event/8 MiB total and separate bounded Quill queues. Smaller per-thread
queues may drop sooner. The priority-overflow test drains a real error after
normal-pressure loss. The recent sanitized ring is bounded to 512 events, 2 MiB
and three minutes; reads enforce expiry, and 100 ms backend maintenance retires
idle obsolete entries without waiting on the producer lock. Mode/privacy changes,
Off and Clear invalidate queued generations, reset ring/mappings and flush before
acknowledgement. Sign out revokes immediately, and delayed intermediate producers
carry their captured generation so they cannot repopulate revoked context.

Live Clear uses native admission and retention coordination, refuses another
active session before deletion, removes admitted segments and opens a fresh empty
segment. Busy, unavailable and partial results are distinct. Exports, preference
stores and other profile state remain outside Clear. Profile purge includes the
new preference store and exact sidecars through the existing owner.

The rendered public course uses the real desktop, Settings and provider fixture.
It verifies signed-out reachability, independent mode choices and disclosure
cancel/accept, exact profile folder handoff, Verbose file output, visible recent
context, the Accounts & hosts Sign out gesture, profile-window sharing, confirmed
Clear while Off, orderly retirement, restart-Off without startup bytes and invalid
preference recovery. The folder handler records the OS handoff URL; it does not
claim that a real file manager appeared. Direct/manual-executor courses cover
four detail/privacy combinations, forbidden-content canaries in files/ring,
pending acknowledgements, failed persistence, failed Open, concurrent ingress,
Clear while another logger is active, Off, delayed revoked events and shutdown.

### Commands and results

```sh
cmake --build --preset dev --target test_diagnostic_controls test_diagnostic_controls_public_path test_quill_logging test_session_retention test_application_logging test_release_maintenance_public_path test_better_pull_review_github_provider_tests
ctest --preset dev --output-on-failure -R '^(diagnostic_controls|diagnostic_context_age|application_diagnostic_controls_public_path|logging_session_retention|logging_session_concurrency|quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_logging|application_logging_cancel_startup|application_logging_full|application_logging_unavailable|application_release_maintenance|sqlite_release_maintenance_files|application_release_maintenance_public_path|application_profile_management_public_path|application_profile_multi_window_public_path|architecture_boundaries)$'
ctest --preset dev --output-on-failure -R '^(diagnostic_controls|github_provider|github_provider_content_policy|quill_logging|quill_logging_content_policy|logging_session_retention|logging_session_concurrency|quill_logging_backpressure|architecture_boundaries)$'
ctest --preset dev --output-on-failure -R '^(application_diagnostic_controls_public_path|application_logging|application_release_maintenance_public_path)$'
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/adapters/logging/src/quill_log.cpp src/adapters/logging/src/session_files.cpp src/adapters/qt/src/diagnostic_settings_panel.cpp src/adapters/qt/src/workbench_window.cpp src/adapters/qt/src/release_update_panel.cpp src/adapters/sqlite/src/diagnostic_preferences.cpp src/adapters/sqlite/src/release_maintenance_files.cpp src/app/src/diagnostic_controls.cpp src/app/src/github_application.cpp src/application/src/diagnostic_settings.cpp src/application/src/release_diagnostic_snapshot.cpp src/adapters/github/src/github_provider.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

The integration pass completed **16/18**: its two failures were older tests
expecting raw request IDs under the new Anonymized default. It passed actual
180-second age expiry, startup cancellation/full/unavailable, profile/window
isolation and maintenance courses. Unchanged long lifecycle/profile evidence is
reused. After corrections, affected backend/provider tests passed **9/9** in
24.21 seconds; final public courses passed **3/3** in 35.79 seconds. All 26 touched
C++ files passed `format.py --root . --dry-run` with clang-format 23.1.1. All 12
touched product translation units have passing tidy evidence (the final logger
and provider rerun passed); the documentation gate and its **64 tests** passed.

Earlier build failures were the folder path helper name, a manual-executor method
name and one nonexistent build-target name; each was corrected. Initial direct
preferences testing found missing SQLite runtime initialization and failed before
store loading; adding the existing runtime admission fixed it. Subsequent failures
exposed remaining raw-ID fixture assumptions, a provider assertion that prohibited
the new start event instead of only fabricated response events, and public tests
that left Settings hidden or searched the old Sign out parent. Corrected courses
exercise the actual visible Settings pages and current account card. Tidy found
a bounded `from_chars` pointer expression and missing vector reservation; both
were corrected. These failed runs are not acceptance evidence.

Self-review tightened summary classification, preserved accepted state on effect
failure, retired idle context, and rejected delayed intermediate generations after
Sign out. Context cleanup callbacks are disabled before logger destruction even
if Quill temporarily retains a sink. The decision lane stays live until pending
control effects quiesce. Architecture guards pass; no core policy, credential
owner, provider authority or Quill dependency version changed.

### Scope and remaining obligations

This supplies BPR-1217 AC1–AC3 and the controls/detail/privacy portions of full
RFC AC-01/03/04/06/07/08. It does not close any full RFC criterion. Rich anonymized
or identifying export, fresh export joins and explicit ring selection remain
BPR-1218. Windows/Linux native, installed-package, complete crash/disk-full and
attended control acceptance remain BPR-1219/BPR-0603/BPR-0601. Native evidence here
is macOS arm64 with the existing Debug Qt 6.11.2/Quill 13.0.0 build cache. Earlier
180-second expiry evidence verifies the unchanged public age cutoff; later idle
maintenance is additionally covered by logger lifecycle/direct checks. No physical
memory erasure or crash persistence guarantee is made. Historical files retain
their capture-time privacy, and current sanitized explanations remain contextual.

Owning documentation is updated for host/adapter boundaries, persistence,
operator controls, privacy/loss bounds and maintenance compatibility. No package
or dependency change needs new provenance for this milestone. The RFC/index and
full native/human checklist remain open. M3 creates no attended session/server or
protected candidate; preparation belongs to the native-closeout milestone.

Merged-candidate integration: rebuild passed and **5/5** checks passed in 35.90
seconds at `6089117d7bd13ad52d2b34c49713b842cd0f3954`:

```sh
cmake --build --preset dev --target test_diagnostic_controls_public_path test_application_logging test_diagnostic_controls test_better_pull_review_github_provider_tests
ctest --preset dev --output-on-failure -R '^(diagnostic_controls|application_diagnostic_controls_public_path|application_logging|github_provider|architecture_boundaries)$'
```

The merged documentation gate passed. All task-owned build/test/tidy commands
are complete; no source edits remain after that candidate. BPR-1217's three
criteria are accepted. The main landing is recorded below.

M3 accepted and pushed on main as `db4acde09b6346a3f88a251b2aeea6014bc7e0e3`.
BPR-1217 is Done. Source behavior is `75700f4d`; tested merged composition is
`6089117d`. Main/origin/main matched after the normal push. This final handoff
contains documentation only and releases the sole M3 writer. All owned commands
are complete; no review server/session/protected candidate remains. Next: fresh
BPR-1218 / diagnostic-export, with full native and human gaps preserved.

## M4 — frozen diagnostic export (BPR-1218)

Assignment `/root/rfc0027_m4`, sole engineering writer in worktree `57ed`,
`diagnostic-export`, mandate r2. Immutable RFC base remains
`55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`; claim `d234a24f` follows the preserved
M4 binding. Source/merged/accepted identities are recorded below after integration.

### Behavior and direct evidence

Profile Storage & privacy now prepares either anonymized or identifying previews
from retained sessions or the current session, with an explicit opt-in for the
bounded recent sanitized context. Preview flushes the profile logger, captures
its session/sequence cutoff and selected ring, then reads owner-admitted files.
The host freezes classified bytes, exact options, byte/record counts, time range,
category/session counts and capture-time omission counts. Saving consumes those
same bytes. Changing scope cancels preparation; later appends cannot enter the
saved selection. Files are local, exclusive new outputs; originals are unchanged.

Each export receives a fresh random namespace. Session/request/window joins are
remapped consistently. Anonymized output excludes original join identities;
identifying output can retain only numeric identities/session already present
at capture, requires a separate cancel-default disclosure, and cannot restore
capture-time omissions. Core rejects identifying export without the confirmation
and rejects prepared options that differ from the request.

Every selected field is parsed and classified again. JSONL v2/v3 operational
strings have field-specific fixed vocabularies; sanitized detail is admitted only
through the recognized explanation/code grammar and public query vocabulary.
Unknown prose gets an explicit omission marker. Invalid records and partial
tails count as omissions. Legacy v1 contributes classified numeric metadata
only. The old metadata-only API and `bpr-diagnostics-v1` format remain supported.
No opaque summary, source, comment, credential or arbitrary string is copied
through, including in identifying mode. The 16 MiB/65,536 input and
1,024-record/256 KiB output bounds remain; included ring input also has the
512-event/2 MiB bound. Duplicate file/ring records are counted once.

The public desktop fixture exercises signed-out reachability, two capture modes,
anonymized and identifying preview/export, cancel and accept of the additional
disclosure, session filtering, scope invalidation, explicit actual logger-ring
selection, hostile legacy input, later file mutation, byte-exact saves to new
Unicode paths, existing-path refusal and preservation of original files. It uses
credential-free fixture sign-in for the existing signed-in maintenance course;
there is no live sign-in, service request, upload or PR mutation. Direct tests
add field-by-field poison strings, useful unfamiliar-code detail, v2/v3 sessions,
epoch-zero range, fresh mappings and retained joins, cutoff/duplicate handling,
rich output bounds, unknown enums and mandatory core confirmation.

### Commands and results

```sh
ctest --preset dev --output-on-failure -R '^(application_release_maintenance_public_path|application_diagnostic_controls_public_path)$'
cmake --build --preset dev --target test_release_maintenance_core test_release_maintenance_host test_release_maintenance_files test_release_maintenance_panel test_release_maintenance_public_path test_quill_logging -j 8
ctest --preset dev --output-on-failure -R '^(core_release_maintenance|application_release_maintenance|sqlite_release_maintenance_files|qt_release_maintenance.*|application_release_maintenance_public_path|quill_logging|architecture_boundaries)$'
cmake --preset core-only -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build --preset core-only --target test_release_maintenance_core test_release_maintenance_host -j 8
ctest --preset core-only --output-on-failure -R '^(core_release_maintenance|application_release_maintenance|core_dependency_boundary|core_boundary_contract|architecture_boundaries|architecture_boundary_contract)$'
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/application/src/release_diagnostic_snapshot.cpp src/core/src/release_maintenance.cpp src/adapters/logging/src/quill_log.cpp src/adapters/sqlite/src/release_maintenance_files.cpp src/adapters/qt/src/release_maintenance_panel.cpp src/app/src/release_maintenance_controls.cpp src/app/src/github_application.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

The unchanged shortest public composition passed 2/2 before edits. Candidate
integration passed 9/9 in 34.91 seconds. Core-only build and six focused boundary,
parser and core gates passed in 114.59 seconds before the final factual-preview
corrections; subsequent dev core/parser checks passed 2/2 in 0.67 seconds.
All 18 changed C++ files passed `format.py --dry-run` with clang-format 23.1.1.
The documentation gate and its 64 tests passed. Final-source public/Qt and merged
integration results follow below. Passing unchanged units are reused.

Initial compilation exposed default aggregate members and an accidental extra
lifecycle constructor argument; both were corrected. Initial parser testing used
a source session equal to part of the build fingerprint, producing a false
identity-leak assertion; the fixture now distinguishes them. An initial public
identifying test changed capture privacy before flushing its seeded event, so
the acknowledged privacy barrier correctly dropped it; the fixture now flushes
before that transition. Tidy found one local identifier spelling, corrected to
`pagedOperations`; parser/core tidy then passed. These intermediate failures are
not acceptance evidence.

Self-review checked source-field classification, escaped detail handling,
immutable option/byte binding, native file admission and shared logger lifetime.
It corrected legacy-only category reporting and the exact minimum for timestamp
zero, with focused regression assertions. Named field masks replace numeric
literals in the new core/host policy. The preview intentionally retains its
explicit frozen selection until cancelled or consumed; live recent-ring expiry
and control barriers remain separate. No native Windows/Linux, packaged,
human-control or crash-survival acceptance is claimed. M5 retains those required
full-RFC obligations and must not infer them from the automated macOS fixture.

Final source candidate: `3fd1224ebe72e9f2f22707eadb04cacb5586d466`.
The final Qt/public run passed **4/4** in 20.85 seconds after the factual-preview
corrections; final panel tidy passed, completing clean evidence for all seven
product translation units. Main's accepted review-tour work `80a9551c` merged
without conflict as `9a46ddd802423c4749836a4e6094986452ea5092`, preserving its
composition, maintenance inventory and storage documentation additions.
The merged documentation gate passed. Integration commands:

```sh
cmake --build --preset dev --target test_release_maintenance_core test_release_maintenance_host test_release_maintenance_files test_release_maintenance_public_path -j 8
ctest --preset dev --output-on-failure -R '^(core_release_maintenance|application_release_maintenance|sqlite_release_maintenance_files|application_release_maintenance_public_path|architecture_boundaries)$'
```

Merged-source build passed; all **5/5** integration checks passed in 26.45
seconds at `9a46ddd8`, including native file maintenance and the full public
export course. Scoped board validation passed. No product edit followed that
verification. All task-owned build/test/tidy commands completed.

M4 accepted and pushed on main as `5ef925d960c958509f535651e4ed3f4b72d8719a`;
BPR-1218 is Done. Source behavior is `3fd1224e`, tested merged source `9a46ddd8`.
Main and origin/main matched after push. The final checkpoint/observation is
documentation only and releases the writer. No review URL, session, event cursor,
protected candidate or server exists; all task-owned commands have finished.
Next: fresh BPR-1219 / `native-closeout`, preserving all full-RFC limitations.


## M5 — native qualification and attended closeout (BPR-1219)

Assignment `/root/rfc0027_m5`, milestone `native-closeout`, mandate r2, sole
engineering writer in worktree `57ed`. The inherited accepted main was
`806c117616dc54bbf2c243e31148a6bd7155b74f`; binding `ee8ace04`, claim `e535fa69`.
Source corrections are `92c983615d1ac23943a34f29327c2a78412c1c4c`,
`189d24f937d16cfedba7990aff3837bcc1a2c5b0`, and
`7ab119933439878d858100410083080ab4d1ea7b`. The full RFC is **not accepted**:
three attended judgments remain pending. The reconciled nine-criterion map is
[BPR-0601](../issues/30-in-progress/BPR-0601-close-rfc-0027.md).

### Corrections and direct failure evidence

- A refresh requested while a diagnostic capture was in flight was lost. The
  settings host now coalesces it and publishes a fresh observation, so reopening
  after Sign out cannot retain a stale recent-context count. A deterministic
  manual-executor test reproduces the ordering; the public course also passes.
- Epoch jobs now retain the logger context generation when submitted. A delayed
  producer cannot fill revoked recent context after a privacy/sign-out barrier.
- The composed failed-sign-in course injects an unfamiliar GraphQL error through
  the existing synthetic credential owner, real provider and real local logger.
  It reads the resulting JSONL directly: approved `__type` service-repetition
  explanation, an omitted/unclassified code, HTTP 200 and actual request/session
  joins are present. The UI still reports session failure with explicit retry;
  no session is granted and remote prose is not copied to the UI. This is fixture
  evidence, not a new live sign-in or root-cause diagnosis.
- Native Windows compilation exposed uninitialized enum-switch paths and a
  `bool` comparison warning treated as error. Invalid enum values now refuse;
  the serializer compares in its existing uint64 domain, preserving its bytes.
- The tests-disabled universal build exposed production review-source state and
  its clone helper hidden under `BPR_BUILD_TESTING`. Their declarations and
  accepted-handoff capture now compile in the production composition, preserving
  its existing authority checks. The cache serializer and review-tour public
  course pass after this correction.
- Native Windows ordinary readers could not open active log files because the
  append handle requested DELETE access. Append handles now omit that access.
  Retention, holding the inactive-session lock, separately opens a removal
  handle and verifies owner/private attributes, exact file identity and named
  path before disposition. Active ownership remains a lock beyond EOF. The
  native crash, file-read, retention and control courses exercise this behavior.
  Test roots are canonical native paths; the Open-folder assertion compares
  filesystem identity instead of slash spelling. The Qt course drains deferred
  profile destruction and explicitly removes its test root.

The native recovery child flushes one record, is forcibly killed, and restarts
against the same root. Previously flushed bytes remain unchanged, no shutdown
record is invented, and the restarted process creates its own session. It does
not claim unflushed events survive termination or power loss. The separate macOS
failure course fills only a mounted, owned **32 MiB HFS+ image**, validates its
separate device and bound, observes actual `ENOSPC` after 32,727,040 filler bytes,
checks unavailable status, frees space and restarts without altering the failed
session. The image is detached and removed. The host volume is never filled.

### Commands and exact candidate evidence

The Mac arm64 Debug cut on `92c98361` passed **17/17** in 68.81 seconds (logging,
startup/cancel/full/unavailable/failure, controls, exports, retention, concurrent
sessions, crash, backpressure, architecture and package contracts). The bounded
portability cut at `189d24f9` passed **5/5** in 51.52 seconds. The final Mac
controls/retention/crash cut at `7ab11993` passed **4/4** in 11.95 seconds.
Earlier direct epoch and window-host tests pass 1/1 each. Seven touched product
translation units have passing tidy; nine initial C++ files and subsequent
changed files were formatted with clang-format 23.1.1. Documentation and all
64 documentation tests pass; the final checkpoint records their last rerun.

```sh
cmake --build --preset dev --target test_quill_logging test_session_retention test_diagnostic_controls test_application_logging test_diagnostic_controls_public_path test_release_maintenance_public_path -j 8
ctest --preset dev --output-on-failure -R '^(diagnostic_controls|application_diagnostic_controls_public_path|logging_session_retention|logging_session_concurrency|logging_crash_recovery|quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_logging|application_logging_cancel_startup|application_logging_full|application_logging_unavailable|application_logging_capability-failure|application_release_maintenance_public_path|architecture_boundaries|package_runtime_contract|release_source_assets_contract)$'
cmake --build --preset dev --target test_cached_review_overview_storage test_review_tour_composition_public_path -j 8
ctest --preset dev --output-on-failure -R '^(sqlite_cached_review_overview|application_review_tour_composition_public_path|application_logging_capability-failure|application_diagnostic_controls_public_path|application_release_maintenance_public_path)$'
ctest --preset dev --output-on-failure -R '^(diagnostic_controls|application_diagnostic_controls_public_path|logging_session_retention|logging_crash_recovery)$'
python3 -B tests/logging/verify_logging_disk_full.py /Users/jaeger/.codex/worktrees/57ed/BetterPullReview/build/dev/tests/logging/session_retention
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/application/src/diagnostic_settings.cpp src/application/src/epoch_application_host.cpp src/application/src/github_window_read_host.cpp src/app/src/github_public_path_fixture.cpp src/app/src/github_application.cpp src/adapters/sqlite/src/cached_review_overview_store.cpp src/adapters/logging/src/session_files.cpp
```

The native station is `aito-main-win` / JaegAW. Isolated clones
`C:/code/rfc0027-m5` and `/home/mmarcin/rfc0027-m5` were created from the station's
existing repositories, fetched exact source bundles and used detached commits.
Shared station checkouts were untouched. Builds used their existing verified
CPM caches with `BPR_DEPENDENCY_OFFLINE=ON`. No credential/live-service action ran.

Linux is actual WSL2 Ubuntu **26.04**, not a cross-build. Its native Qt/logging
composition at `189d24f9` passed **15/15** in 58.98 seconds: the same focused
logging, lifecycle, controls, export, crash, concurrency, backpressure and
architecture courses. `7ab11993` changes production code only within the Windows
branch; this Linux production evidence is reused with that limit. Ubuntu 26.04
is not a declared distribution baseline: no Ubuntu 24.04/RHEL10 installer,
GLIBC baseline or Linux desktop-installation qualification is inferred.

Windows uses native `windows-dev`/Debug and the pinned Qt 6.11.2 MSVC package.
The initial selected build omitted its sandbox helper; adding it exposed native
AppContainer child initialization failure `0xc0000142` for the static boundary
script. The same read-only boundary command is run directly and its outcome is
recorded separately; the failed containment run is not a pass. Logging/UI tests
are explicitly registered `WINDOWS_UNCONTAINED`, as their owner-private ACL setup
requires. Native results are appended below.

Initial Mac public sign-out testing found the stale refresh once and passed on
repeat; the deterministic regression established the cause before the fix.
The failure fixture initially supplied no credential boundary and then asserted
the raw message rather than the approved classified prose. Both were corrected
without loosening the sanitizer. The first Windows/public/retention/crash runs
failed on root spelling and DELETE sharing as described above. Intermediate
failures remain observations, not passing acceptance. The copied attended
candidate was released while these corrections were made and will be refreshed.

### Pinned dependency and unpublished package

The existing publish-release skill and documented unpublished preparation path
were used on clean `189d24f9`, after the tests-disabled compile repair:

```sh
CMAKE_BUILD_PARALLEL_LEVEL=8 tools/release.sh --version 0.1.3 --prepare-only --skip-tests --output-dir build/releases/rfc0027-m5-189d24f9
```

This passed the universal arm64/x86_64 production build (`BUILD_TESTING=OFF`),
restricted Qt deployment, both-slice/deployment-floor checks, offline ad-hoc
sealing, relocated native launch, notices/source binding and package audit.
Focused tests above supply the separate test evidence; the command deliberately
skips a redundant full test suite. No signing identity/Keychain/Apple service,
tag, release or publication action occurred. The final Windows-only source
correction leaves these Mac production/dependency/package paths unchanged;
the package evidence remains explicitly bound to `189d24f9`.

Artifacts remain local under
`build/releases/rfc0027-m5-189d24f9/` (not committed):

| Artifact | SHA-256 |
|---|---|
| `BetterPullReview-0.1.3-macos-universal-local.zip` | `4dcaadf6dd1b9149324cfe2ce7df54cef8184b5b21709dd75bfa7bcd95983b58` |
| `BetterPullReview-0.1.3-macos-universal-local.spdx.json` | `e5ce58d292ef3fe979460bb4bdb737a8bd389c32fd48ee28f74b2cbf0dc7f2ae` |
| `BetterPullReview-0.1.3-macos-universal-corresponding-source.zip` | `2532ba6aaa2137deb709f201faa453214425e52cc3bec5389cbe898210663329` |

Installed `ThirdParty/THIRD-PARTY-NOTICES.txt` contains the Quill MIT and fmt
licenses; the installed source provenance binds the separate corresponding-source
asset to the digest above. SPDX records Quill **13.0.0**, compiled headers,
MIT, pinned codeload URL and SHA-256
`88b4a1542125577a4d51cf444c51e34d63618c422ba6a4fa9bd23894b49d696b`.
The common dependency manifest and runtime notices generator include Quill for
every platform; there is no added runtime Quill DLL/framework. Windows installer
and supported-baseline Linux package execution were not qualified here. Existing
release trust, recipient source access, retention/legal and platform baseline
obligations remain explicit; offline audit is not release approval.


Final Windows `7ab11993`: all **12/12** registered native logging/control courses
pass in 64.34 seconds, including normal/failed sign-in, cancellation,
full/unavailable sink isolation, export, controls, retention, canaries and killed
process recovery. The combined CTest command reports 12/13 because the separate
AppContainer architecture wrapper failed as described above. The identical
static gate passed natively outside that failed container, exit 0:

```powershell
cmake --build --preset windows-dev --target tool_better_pull_review_network_sandbox test_quill_logging test_session_retention test_diagnostic_controls test_application_logging test_diagnostic_controls_public_path test_release_maintenance_public_path --parallel 10
ctest --preset windows-dev --output-on-failure -R "^(diagnostic_controls|application_diagnostic_controls_public_path|logging_session_retention|logging_crash_recovery|quill_logging|quill_logging_content_policy|application_logging|application_logging_cancel_startup|application_logging_full|application_logging_unavailable|application_logging_capability-failure|application_release_maintenance_public_path|architecture_boundaries)$"
cmake -DBPR_ROOT=C:/code/rfc0027-m5 -P C:/code/rfc0027-m5/cmake/VerifyArchitectureBoundaries.cmake
```

For Linux, the same target cut adds `tool_better_pull_review_network_sandbox`
and uses `dev`; its CTest expression also includes `logging_session_concurrency`
and `quill_logging_backpressure`. Native Windows lacks those separately
registered POSIX courses; Windows concurrency/ownership is exercised within
retention and controls. The actual ENOSPC course is macOS only. No Windows
power-loss or separate full-volume failure claim is made.

### Attended candidate and continuation

Session `c1565558e5fd3fdc277e62622604873b`, final prepared event cursor **19**.
Candidates `BPR-1219-controls`, `BPR-1219-files`, `BPR-1219-export` each ask one
criterion with Accept/Reject and a note for changes. No verdict has been supplied.
The copied review app is macOS arm64 Debug, source `7ab11993`, binary SHA-256
`3875a0f1704eef5c0655de82579960050acb9683ecd23257d2c1be753341b2f5`.
It uses the shipped UI with the tests-enabled explicit data-root override;
that override is absent from the independently qualified production package.
Each candidate has a separate empty owner-private profile, stays signed out,
and uses an absolute launch command. No service or remote PR is part of review.

Runtime manifest, identity, copied app and offline HTML are under
`/Users/jaeger/.codex/worktrees/57ed/BetterPullReview/build/rfc0027-m5/review/`.
The session state is `.mask/attended-review/session.json`; its loopback host is
PID 8289, command session 64739, port 63107. The token-bearing complete URL is
returned directly to the coordinator, not committed. Protected source paths are
listed in the manifest; the immutable copied app is a shared versioned resource.
Query conflicts and release affected candidates before changing protected input.

The coordinator takes the durable watch before the human handoff:

```sh
python3 -B /Users/jaeger/.codex/worktrees/57ed/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/.codex/worktrees/57ed/BetterPullReview wait --after 19 --kind handed-back --timeout 43200
```

The next engineering round reads `status --after 19`, applies only recorded
verdicts to the exact candidates, resolves rejected criteria and remaining
qualification limits, then reconciles BPR-1219/BPR-0603/BPR-0601 and RFC/index.
An actual accepted M5 must land on main under mandate r2. No M5 acceptance or
main integration is claimed while the attended criteria are pending.


## M5 post-attended UI revision

Assignment `/root/rfc0027_m5_revision`, mandate r2, sole nondelegating writer.
Source slice `e0a870b3`, integrated with
main `f1e0c652` as `0fb249a2499fa4752c645f29d19e1c768e9472e4`.

### Owner decisions and design

The owner handed back session `c1565558e5fd3fdc277e62622604873b` at cursor 23.
`BPR-1219-files` was accepted with no note (decision `413e836b-5898-4aeb-a48c-43b21595d54b`).
Controls were rejected in favor of AllTheThings’ setting selectors; export was
rejected in favor of its simpler Help actions. Both controls attachments and
the Help screenshot were decoded locally and viewed. The read-only reference
at `2ba1b33c9886a3e20865801581a26b5ba73f2700` confirms native segmented detail
choices and separate Help exports. No AllTheThings file was modified.

The revised Settings uses native exclusive paired buttons, short explanations,
and an optional About disclosure. Open/Clear behavior and confirmations retain
the accepted code. Help now offers anonymized and identifying exports directly;
the profile owner composes the same maintenance host without displaying Settings.
A compact dialog automatically freezes a preview, presents scope/range/size and
omissions, and offers Save/Cancel. Options and exact records expand on request.
Changing options cancels the old snapshot; Update preview re-admits it. The
identifying dialog defaults to Cancel; its explicit Save supplies the core’s
existing confirmation bit for the exact displayed descriptor. Capture policies,
barriers, parser, file admission and local-only effects are unchanged.

### Verification and integration

The unchanged public path passed 2/2 before edits. The first revised build and
architecture/Qt/public cut passed 6/6 in 42.00 seconds. Render inspection led to
arrow disclosures replacing empty collapsed frames and removal of duplicated
summary prose. The final affected Qt/public cut passed **5/5** in 34.25 seconds.
All five changed product translation units passed tidy; the two polished panels
were checked again. All 12 changed C++ files passed formatting. Documentation
and its 64 tests pass.

The real native macOS app was also inspected through Help → Export anonymized
diagnostics → Cancel and Help → Diagnostics. The signed-out window showed the
two direct export entries, a ready frozen preview, Cancel as default, and blue
selected Standard/Anonymized paired buttons under the native dark appearance.
The app used an isolated owner-private profile and quit normally. This is
objective presentation evidence, not the owner’s replacement verdict.

The render-only executable run initially used `/var` aliases for two save
locations while native admission requires canonical parents. The ordinary CTest
course already supplies canonical temporary roots and passed. The public fixture
now canonicalizes all three export paths consistently; no file-admission policy
was loosened. The intermediate render failures are not claimed as passing tests.

Main integration preserved its newer tour-code destination and published
conversation state while retaining the M5 production state declarations.
The exact merged build and architecture, controls, export and tour public
composition passed **4/4** in 50.44 seconds at `0fb249a2`.

```sh
cmake --build --preset dev --target app_better_pull_review test_release_maintenance_panel test_release_maintenance_public_path test_diagnostic_controls_public_path -j 8
ctest --preset dev --output-on-failure -R '^(qt_release_maintenance.*|application_release_maintenance_public_path|application_diagnostic_controls_public_path)$'
cmake --build --preset dev --target app_better_pull_review test_release_maintenance_panel test_diagnostic_controls_public_path test_release_maintenance_public_path test_review_tour_composition_public_path -j 8
ctest --preset dev --output-on-failure -R '^(application_diagnostic_controls_public_path|application_release_maintenance_public_path|application_review_tour_composition_public_path|architecture_boundaries)$'
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/adapters/qt/src/diagnostic_settings_panel.cpp src/adapters/qt/src/release_maintenance_panel.cpp src/adapters/qt/src/workbench_window.cpp src/app/src/release_maintenance_controls.cpp src/app/src/github_application.cpp
python3 -B scripts/verify_documentation.py --root .
python3 -B tests/scripts/verify_documentation_tests.py
```

The native UI recheck is bounded to the two changed public controls/export
courses on the same isolated Windows/Linux clones. Linux passed **2/2** in
13.56 seconds at `0fb249a2`. The Windows build exposed MSVC warning C4804 in
incoming main's tour codec numeric bound comparison with bool. The one-line
uint64_t cast at `41cce4ba` follows the already-used cached overview serializer
pattern and preserves encoding/admission. Its local storage test passed **1/1**
in 0.42 seconds, and header formatting passed. Windows compiled the repaired
library and both public executables. The Windows controls/export courses passed
**2/2** in 29.05 seconds at `41cce4ba` (native uncontained execution). The
additional optional Windows tour-storage
test target failed to compile because its existing CMake target cannot find the
platform private-object header; it is not counted as a pass and does not alter
the two required UI course targets.

```sh
cmake --build --preset dev --target test_review_tour_storage -j 8
ctest --preset dev --output-on-failure -R '^sqlite_review_tour_storage$'
# Isolated Linux checkout /home/mmarcin/rfc0027-m5, source 0fb249a2:
cmake --build --preset dev --target test_diagnostic_controls_public_path test_release_maintenance_public_path -j 8
ctest --preset dev --output-on-failure -R '^(application_diagnostic_controls_public_path|application_release_maintenance_public_path)$'
# Isolated Windows checkout C:/code/rfc0027-m5, source 41cce4ba:
cmake --build --preset windows-dev --target test_diagnostic_controls_public_path test_release_maintenance_public_path test_review_tour_storage --parallel 10
ctest --preset windows-dev --output-on-failure -R '^(application_diagnostic_controls_public_path|application_release_maintenance_public_path)$'
```
 The original logging,
retention, crash, actual ENOSPC and Quill dependency/package evidence above is
reused within its exact recorded limits. The unpublished package remains bound
to `189d24f9`; no newly signed, installed or released package is claimed.

### Replacement attended candidate

Same session and server, cursor **31**. Only `BPR-1219-controls` and
`BPR-1219-export` are new pending questions. Their source is `0fb249a2` and copied
binary SHA-256 is `9c4ae94b217cfd20585c648dca0bd5e74ce53adad91fc40a8cb4966a459e5186`.
The separate app, manifest, identity and offline HTML are in
`build/rfc0027-m5/review-v2/`. Each launch names an absolute isolated data path;
no sign-in, live service or remote PR operation is requested.

The original files verdict, revision and old copied app remain intact; its
binary digest was rechecked as `3875a0f1704eef5c0655de82579960050acb9683ecd23257d2c1be753341b2f5`.
The conflict-release helper combines path and candidate selectors, so the shared
panel path also released the files protection flag. Its recorded accept remains
valid for the original immutable app, which stays reserved by this checkpoint.
No replacement files question is created. Both revised candidates are protected.
The final checkpoint on BPR-0601 transfers server PID 8289/session 64739 and the
complete local URL to the coordinator; M5 remains unaccepted until actual verdicts.


## M5 final engineering round — mandate r2

Assignment `/root/rfc0027_closeout`, `native-closeout`, sole writer in worktree
`57ed`, branch `codex/rfc0027-advanced`. Incoming checkpoint was
`f9185a22ff14281f4309df7717a175616dcf6408`. The source remains `41cce4ba` with
UI source `0fb249a2`; this round changes documentation and board state only.
The latest main checked and fetched was `f1e0c652`, already merged at `0fb249a2`.
The coordinator confirmed the hand-back watcher exited 0 at cursor 34.

### Consumed human acceptance

Session `c1565558e5fd3fdc277e62622604873b` handed back at cursor **34**, with
no pending decisions. All three outcomes are explicit human Accept, blank notes:

| Candidate | Source | Decision ID | Fingerprint |
|---|---|---|---|
| `BPR-1219-controls` | `0fb249a2499fa4752c645f29d19e1c768e9472e4` | `87de7415-823d-4efc-b07c-03e9491fec5f` | `86f20e8f40401168249b2c48b610d8d364af07d88f5d08fdae341cecbb377cd8` |
| `BPR-1219-export` | `0fb249a2499fa4752c645f29d19e1c768e9472e4` | `1e877215-b39e-4e6c-a83e-fd338022d415` | `31b491264e3f3dfaaee7861c3de28adaf06b5ee39d7e712ad16fe2fbc0859d4a` |
| `BPR-1219-files` | `7ab119933439878d858100410083080ab4d1ea7b` | `413e836b-5898-4aeb-a48c-43b21595d54b` | `6eabaa3590e43c125423db6cd31edba6d685f2f919e94d55b7bd1a7cc0836ab7` |

The controls/export copied binary is SHA-256
`9c4ae94b217cfd20585c648dca0bd5e74ce53adad91fc40a8cb4966a459e5186`;
the original files binary is
`3875a0f1704eef5c0655de82579960050acb9683ecd23257d2c1be753341b2f5`.
Both executable digests were rechecked from the retained copied apps and match.
The accepted files behavior did not change in the UI revision. No copied app,
review attachment, manifest or recorded verdict was rewritten. The current
documentation paths had no attended protection conflicts.

### Closeout disposition

All nine RFC product criteria pass within the exact evidence in BPR-0601.
Actual Mac, Windows and Linux developer logging/lifecycle/control checks meet
AC-09; the package proof binds pinned Quill, installed notices, SPDX and the
separate corresponding-source artifact at `189d24f9`. Those observations do not
qualify release distribution. The Windows AppContainer wrapper failed startup;
its direct architecture gate passed. Ubuntu 26.04 evidence does not qualify
Ubuntu 24.04/RHEL10 packages. The universal Mac package is unpublished and
ad-hoc sealed, with no release trust or source-offer approval. Crash evidence
preserves only explicitly flushed records. The optional unrelated Windows tour
storage target's missing platform include remains unclaimed.

The current user-supplied AGENTS.md requires sequential canonical RFC closeout.
Earlier canonical RFC-0020, RFC-0021 and RFC-0023 are Deferred; RFC-0022 is
Proposed. RFC-0027's listed dependencies 0001/0002 are Auto-Verified. Although
historical BPR-0750 applied ordering only to its declared dependencies, mandate
r2 supplies no explicit exception to the current sequential rule. Therefore the
accepted implementation lands on main while BPR-0601 and RFC-0027 terminal
status remain pending that ordering requirement. No product criterion is waived
and no additional human product review is requested. The coordinator may obtain
an explicit ordering amendment, or resume closeout after those predecessors.

Documentation impact: existing product, host, privacy, settings, resource and
package documentation already owns the final behavior. This round updates
acceptance and removes stale active-review wording; no API, persistence,
threading, provider authority, native launch or package behavior changes.


### Final focused verification

At `f0104f8007a401f7eb417e82f7050d8aeeff09fe` (product inputs identical to
`41cce4ba`), the explicit three-target dev rebuild passed. The focused public
integration passed **4/4** in **46.21 seconds**:

```sh
cmake --build --preset dev --target test_application_logging test_diagnostic_controls_public_path test_release_maintenance_public_path -j 8
ctest --preset dev --output-on-failure -R '^(application_logging_capability-failure|application_diagnostic_controls_public_path|application_release_maintenance_public_path|architecture_boundaries)$'
```

This exercises the real provider-to-file failure, signed-out control and export
compositions through their fixture transports. No live sign-in ran. Unchanged
native/package, format and tidy results above remain bound to their actual
producing revisions; no broad matrix was repeated for this documentation round.

The documentation gate reports `DOCUMENTATION_OK`; `git diff --check` passes.
The prior 64 documentation-regression tests remain valid because neither gate
nor schema changed. The attended-review `stop` command returned `stopped: true`
after decision intake. Session 64739/PID 8289 is no longer transferred; copied
apps, verdicts and protections remain retained for inspection. The coordinator's
watch 47764 completed with exit 0. All final build/test commands have completed.


### Main landing and final board state

The accepted M5 implementation and consumed human evidence landed/pushed as
`e2eec1d69340bbf0f519b6cae28eb462fb40966e`; main and origin/main matched, and the
primary checkout remained clean. BPR-1219 finished at `e42cc7e9` and BPR-0603 at
`f4545095`. The final bookkeeping lands separately without changing any product
input. BPR-0601 alone remains Blocked on `canonical-rfc-order`; its nine product
records pass and its separate sequencing record remains pending. RFC/index stay
Proposed. The authored parent milestone is not falsely marked complete: its
remaining BPR-0601/RFC terminal obligation is preserved in the final observation.
All task-owned commands and the attended server are stopped. Accepted copied apps
and local evidence remain retained. No release publication or live service ran.
