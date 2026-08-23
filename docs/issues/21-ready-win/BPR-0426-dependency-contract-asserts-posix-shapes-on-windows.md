---
id: BPR-0426
title: The dependency modernization contract asserts POSIX shapes on Windows
status: Ready
rfc: RFC-BPR-0013
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
