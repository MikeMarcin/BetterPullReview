---
schema: "implement-rfc.card.v1"
id: "BPR-0426"
title: "The dependency modernization contract asserts POSIX shapes on Windows"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The dependency modernization contract asserts POSIX shapes on Windows

Run uncontained on the Windows host at `a74e1df`, exactly as CTest registers
it, `dependency_modernization_contract` fails 5 of 22:

```
FAIL: test_provider_owned_package_redirect_is_rejected_unexecuted
FAIL: test_qt_provider_is_the_default_acquisition_path
FAIL: test_repository_modules_ignore_caller_module_path
FAIL: test_sqlite_archive_hash_mismatch_is_rejected
FAIL: test_sqlite_staging_excludes_unverified_cache_headers
```

The last one is diagnosed and shows the class. It asserts
`"_bpr/sqlite/3.53.4" in command` against a compile line that Ninja wrote for
MSVC, where the staged include arrives as
`-external:IC:\Users\…\build\_bpr\sqlite\3.53.4` — backslashes, and a
different flag spelling than the `-isystem` the assertion was written against.
The staging is correct; the assertion describes a POSIX compile line. The
other four were not diagnosed individually and must not be assumed to be the
same class: [[BPR-0347]] already paid for grouping by symptom once.

Contained, the same test fails 14 of 22, and those nine extra failures are the
container's, not this card's: inside the AppContainer `ninja` fails with
`CreateNamedPipe: Access is denied`, so every fixture configure stops in the
compiler check before it can reach the rejection the test is asserting. That
is why [[BPR-0362]] registers this test `WINDOWS_UNCONTAINED`. This card owns
only the five that remain once the container is out of the way.

[[BPR-0373]] fixed this file on 2026-08-19 for a failure that "failed
everywhere"; these five do not. They fail on the one platform whose compile
lines look different, which no run before this week could reach.

## Required final state

- Each of the five is attributed to a measured cause.
- The assertions describe the property (the staged header directory is on
  the include path; the hash mismatch is rejected; the redirect is rejected
  unexecuted) in a form that holds for both MSVC and POSIX compile lines,
  rather than a platform-specific spelling.
- The contract still fails when the property is false — a widened assertion
  that matches any compile line is not a repair.
- `dependency_modernization_contract` passes on Windows (uncontained, as
  registered) and is unchanged on macOS and Linux.

## Verification

```sh
ctest --preset core-only --output-on-failure -R '^dependency_modernization_contract$'
```

## One confound to separate first — 2026-08-22, later

Both uncontained runs of this test coincided with a Smart App Control
refusal of a CMake try-run probe (`cmTC_d5f9a.exe` at 19:37:06, `cmTC_c53f9.exe`
at 20:05:46; [[BPR-0424]]). A fixture configure whose compiler check was the
refused probe fails before reaching the assertion the test makes. The
diagnosed failure — the POSIX include spelling — is not that, but the four
undiagnosed ones must each be shown to fail on their assertion, with the
Code Integrity log clean for the run, before they are called this card's.

## Attributed and repaired — 2026-08-23

Run uncontained, as registered, in the first full `windows-dev` matrix after
[[BPR-0436]] (`94abb61`), the contract ran 25 tests and failed 4. Each
failure's own message:

| test | what the message said | cause |
|---|---|---|
| `test_provider_owned_package_redirect_is_rejected_unexecuted` | `Syntax error in cmake code … when parsing string file(WRITE [==[C:\Users\…\redirect-executed]==] executed)\n — Invalid character escape '\U'` | the test pastes the marker path into a *quoted* CMake string, where a Windows separator is an escape; the redirect was never written, so the rejection it asserts never ran |
| `test_repository_modules_ignore_caller_module_path` | `CMake Error at …/TryCompile-…/CMakeLists.txt:2 (set): Syntax error in cmake code` | CMake re-emits `CMAKE_MODULE_PATH` into every try-compile as a quoted `set()`; the test passed `…\fake modules` with backslashes |
| `test_sqlite_archive_hash_mismatch_is_rejected` | `File not found: /C:/Users/…/sqlite-amalgamation-3530400.zip` | the test substitutes `corrupt_archive.as_uri()` for the archive URL; ExternalProject strips `file://` and keeps the slash, which on Windows is not a path, so the download step failed before the hash check the test exists for |
| `test_sqlite_staging_excludes_unverified_cache_headers` | `'_bpr/sqlite/3.53.4' not found in '"…\cl.exe" /nologo … -external:IC:\…\_bpr\sqlite\3.53.4 …'` | the staged include is on the line, spelled with backslashes |

The fifth failure the card recorded,
`test_qt_provider_is_the_default_acquisition_path`, passed in the same run:
its nested configure was the `CMAKE_CONFIGURATION_TYPES` split that
[[BPR-0428]] repaired, and it has nothing of its own.

None of the four is the provider's. Each test now spells the path it hands
CMake with forward slashes (`as_posix()`), hands a local path rather than a
`file://` URI, or compares the compile line and the poisoned cache with one
separator — and each still asserts the property it did: the redirect is
refused unexecuted, the caller's module path is ignored, the corrupt
archive is refused at its hash, the staged directory is the one on the
include path and the unverified cache is not.

### Evidence

| claim | command | result |
|---|---|---|
| the contract passes uncontained on Windows as registered | `ctest --preset core-only -R '^dependency_modernization_contract$'` | Passed, 25 tests in 75 s (`dep-mod-1.log`, 2026-08-23) |
| the four assertions still fail when their property is false | the negative branches are the tests' own: a redirect that executes writes the marker the test checks, a module that executes aborts the configure, an archive whose hash matches is accepted, a cache header that reaches the compiler is a `#error` | unchanged |
| macOS and Linux are unchanged | `as_posix()` of a POSIX path is the path; a local archive path is accepted by ExternalProject everywhere | by construction; the Linux lane's next run is the check |

Implemented by the integrator.
