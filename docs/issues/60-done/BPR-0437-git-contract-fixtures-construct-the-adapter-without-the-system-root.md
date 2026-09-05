---
schema: "implement-rfc.card.v1"
id: "BPR-0437"
title: "The Git contract fixtures construct the adapter without the Windows system root"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: []
---

# The Git contract fixtures construct the adapter without the Windows system root

Uncontained on the Windows host, at `22b991c`:

```
git_process_contract       FAIL: construct installed Git adapter, failure=0
git_cache_contract         diagnostic cache result: value=0 code=2 detail=[]
                           FAIL: cache open preserves the exact redacted Git bootstrap diagnostic
```

`make_installed_git_process` (`src/adapters/git/src/installed_git_process.cpp`)
requires, under `Q_OS_WIN`, a `WindowsSystemRoot` in its configuration and
returns `invalid_configuration` — failure code 0 — without one. The contract's
`make_adapter` (`tests/git_contract/git_process_contract_tests.cpp:897-903`)
passes only `GitExecutable` and `IsolationRoot`, the two fields a POSIX
adapter needs. The fixture was written where the third field does not
exist, and no run before [[BPR-0412]] could reach it.

`git_cache_contract`'s failure is in the same family by shape — the cache is
built on the same adapter — but its assertion is about a redacted bootstrap
diagnostic, and it is listed here to be separated, not assumed.

## Required final state

- The Git contract fixtures supply `WindowsSystemRoot` on Windows from the
  same source the application uses, and the adapter constructs.
- `git_cache_contract`'s failure is attributed on its own evidence; if it is
  the same omission it closes here, otherwise it gets its own card.
- Both contracts pass on Windows as registered, and are unchanged on macOS
  and Linux.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^(git_process_contract|git_cache_contract)$'
```

## Measured and repaired, layer by layer — 2026-08-23

The omission the card named was the first layer. Each fix exposed the next
failure, and each was measured by its own probe before it was touched.

In the product:

1. **No `WindowsSystemRoot`.** `tests/git_contract/windows_system_root.hpp`
   supplies it to every construction in the three contracts that build the
   adapter or the cache — eleven sites in the process contract, three in
   the cache contract, the real cache in the maintenance integration —
   from `GetWindowsDirectoryW`, the application's source.
2. **The uncanonical spelling.** The API says `C:\WINDOWS`; the directory
   is `C:\Windows`; `canonical_existing` accepts only the canonical form.
   The helper canonicalises, and so does the application's own
   `windows_system_root` in `github_application.cpp` ([[BPR-0441]] saw the
   same refusal in the shipped epoch pipeline).
3. **Nothing was ever "missing".** MSVC's `symlink_status` reports
   `not_found` *and* sets a `system_category()` error code, and the
   adapters compared it for equality with the `generic_category()` code
   from `std::make_error_code` — never equal there. Five sites now compare
   against `std::errc::no_such_file_or_directory`, the `error_condition`
   comparison every category answers. Until then no isolation or cache
   directory could ever be created.
4. **`core.longpaths`.** A ref under
   `refs/betterpullreview/v1/stage/<sha-256>/` beneath a `%TEMP%` or
   `%LOCALAPPDATA%` cache crosses `MAX_PATH` by design — the first measured
   lock path was 261 characters. Git reads the setting from the isolation's
   *global* configuration; the system one is disabled by the adapter's own
   `GIT_CONFIG_NOSYSTEM`. On Windows `config-global` carries exactly
   `[core]\n\tlongpaths = true\n`, and the isolation's file contents are a
   per-file table the exact-content check reads too.
5. **The process itself.** Windows grants long paths only to a manifested
   process under an enabled host policy. `cmake/bpr_long_paths.manifest`
   declares `longPathAware`, `bpr_add_executable` links it into every
   first-party executable under MSVC, and the host's `LongPathsEnabled` is
   1. Without it the adapters' own `std::filesystem` walks failed with
   `ERROR_PATH_NOT_FOUND` on the very refs Git had just written.
6. **Abort with an open sink.** The pack spool's receiving file was opened
   with no sharing; aborting an operation renames and unlinks it while the
   sink still holds it — free on POSIX, refused on Windows. The handle now
   carries `FILE_SHARE_DELETE` and nothing else; the file lives in an
   owner-private directory, so only this user can reach it.
7. **A hard link was invisible.** The POSIX file rule requires exactly one
   name (`st_nlink == 1`); the Windows branches checked attributes alone.
   Both adapters' `private_regular_file` now read `nNumberOfLinks` through
   an attribute-only handle, which sharing checks exempt.

In the contracts:

8. **The fakes wrote and read CRLF.** Both contracts run their own
   executable as the fake Git; the CRT's text mode rewrote the planted
   bootstrap diagnostic, the pack it received on stdin, and the invocation
   log it wrote. stdin, stdout, and stderr are binary in both `main`s, the
   log writer is binary, and the logged working directory is spelled
   natively.
9. **The fake's copies could not load.** `make_fake_git_executable` copies
   the Qt-linked test executable into `fake-exec/`, launched with the
   adapter's PATH of that directory alone: the runtime DLLs travel with
   every copy, the hostile contract uses the shared helper instead of an
   inline copy, and the hook canary is written on Windows too (its refusal
   needs only the entry).
10. **Two POSIX-only adversaries.** A group-read mode bit and an
    unprivileged file symlink have no Windows expression on this host; both
    carry the suite's self-describing `SKIP` lines, the same boundary
    [[BPR-0425]] records, and the junction cases still cover reparse
    substitution.

`git_cache_contract`'s bootstrap-diagnostic case was layers 8–9; its
remaining failures, `git_process_contract`'s, and [[BPR-0441]]'s
`EPOCH_CACHE_FAILURE` shapes in the application tests were layers 3–7.

### Evidence

| claim | command | result |
|---|---|---|
| both contracts pass on Windows as registered (uncontained, `qprocess-named-pipe`) | `ctest --preset windows-dev -R '^(git_process_contract\|git_cache_contract)$'` | Passed — process 43.8 s (`gpc-4.log`), cache 55.4 s (`cards-run-29.log`), 2026-08-23; the one `Not Run` before it was the [[BPR-0424]] wait on the freshly linked image |
| the real maintenance composition constructs and resolves its epoch | `ctest --preset windows-dev -R '^epoch_maintenance_integration$'` | Passed, 7.7 s, same runs |
| macOS and Linux are unchanged | the helper returns `std::nullopt` there; the `error_condition` comparison is the equality it replaces; the global-config content, manifest, share mode, link rule, and DLL copies are `Q_OS_WIN`/`MSVC` only; binary mode is `_WIN32` only | by construction; the Linux lane's next run is the check |

Implemented by the integrator; the pack-spool share mode and the
owner-private file rule are storage-boundary changes and are marked for
the capability-critical closeout review with [[BPR-0436]]'s.

## Independent review — 2026-08-23

Reviewed read-only by `rfc-reviewer` with the session's other
capability-critical changes: confirmed sound, no blocking findings. The
review's non-blocking risks and verification gaps are recorded on
[[BPR-0036]]'s procedure and [[BPR-0053]]'s ledger.
